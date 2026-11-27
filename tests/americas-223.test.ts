// Ruling 223 (the design director's log b365, 17 November 2026), Brian's ask in the project chat at 18:46Z: "i also
// think the axis would try and convince south america countries or mexico to join then to open up the front at home
// for the USA". Once the United States leans to the Allies or is at war with the Axis, the Axis courts its neighbours
// and Argentina at every stop of the dial, in the news; whether it can win one over is one roll a campaign (none by
// the book or close to history, about 3 in 100 at history's shape, Mexico about 1 in 10 when anything goes); the
// United States courts back at no cost; a country won over keeps its own small army. Figures in diplomacy.json
// 'americas'.
import { describe, expect, it } from 'vitest';
import { whyNotBase } from '../src/campaign/air';
import { DIPLOMACY, americasOpen, americasShare, americasWon, canCourt, court, freeCourt, monthlyDiplomacy } from '../src/campaign/diplomacy';
import { homeAlarms } from '../src/campaign/danger';
import { addCorps, atWar, bloc, createGame, declareWar, thornGround } from '../src/campaign/game';
import { whyNotShip } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, orderDesk, stepAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

const A = DIPLOMACY.americas!;

/** The grand campaign on the wider world at its first month (September 1939), played as this country. */
function playing(human = 'USA', seed = 3, stop?: number): Game {
  const g = startOpen(createGame(seed, human));
  if (stop) g.flags.hist = stop;
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
/** Of 100 campaigns (seeds), in how many the Axis can win this country over at this stop. */
function wonOf(g: Game, id: string): number {
  const keep = g.seed;
  let n = 0;
  for (let s = 1; s <= 100; s++) { g.seed = s; if (americasWon(g, id)) n++; }
  g.seed = keep;
  return n;
}

describe('the Axis courts the Americas (ruling 223)', () => {
  it('the figures sit in the data: the twelve, the shares by stop and by readiness, the months, one a campaign', () => {
    expect(A).toMatchObject({ backer: 'USA', months: 6, most: 1, join: { 1: 0, 2: 0, 3: 0.03, 4: 0.06, 5: 0.1 }, readiness: { MEX: 1, ARG: 1, CHL: 1 } });
    expect(A.of).toEqual(['MEX', 'CUB', 'HAI', 'DOM', 'GUA', 'ELS', 'HON', 'NIC', 'COS', 'PAN', 'ARG', 'CHL']);
    // (b391: the eight of the Caribbean and Central America, at war with the Axis from December 1941, a third.)
    const g = playing();
    g.flags.hist = 5;
    expect(americasShare(g, 'MEX')).toBe(0.1);
    expect(americasShare(g, 'CHL')).toBe(0.1);
    expect(americasShare(g, 'CUB')).toBeCloseTo(0.0333, 3);
  });

  it('it opens on the wider world while America leans to the Allies, and never in a history campaign', () => {
    const g = playing();
    expect(g.flags.world).toBe(1);
    expect(americasOpen(g)).toBe(true);
    expect(americasOpen(createGame(3, 'GER'))).toBe(false);
  });

  it('by the book and close to history none of 100 campaigns can win one over; at history\'s shape none more than about 3; anything goes, Mexico about 1 in 10', () => {
    const g = playing();
    for (const stop of [1, 2]) {
      g.flags.hist = stop;
      for (const id of A.of) expect(wonOf(g, id)).toBe(0);
    }
    g.flags.hist = 3;
    for (const id of A.of) { expect(americasShare(g, id)).toBeLessThanOrEqual(0.03); expect(wonOf(g, id)).toBeLessThanOrEqual(7); }
    g.flags.hist = 5;
    const mex = wonOf(g, 'MEX');
    expect(mex).toBeGreaterThanOrEqual(5);
    expect(mex).toBeLessThanOrEqual(15);
  });

  it("the month: Germany's envoys go to the Americas, the news says so once, and America courts back at no cost", () => {
    const g = playing();
    const G = g.nations.GER, U = g.nations.USA;
    G.pc = 30;
    U.pc = 0;
    const h = g.headlines.length;
    monthlyDiplomacy(g);
    const went = A.of.filter((id) => g.flags[`court:GER:${id}`] === g.turn);
    expect(went.length).toBe(1);
    const T = g.nations[went[0]];
    const told = g.headlines.slice(h).filter((x) => / envoys are courting /.test(x.text));
    expect(told.map((x) => x.text)).toEqual([`German envoys are courting ${g.regions[T.capital].name}.`]);
    // (The player's America: the press is free for that country, and the row says so.)
    expect(freeCourt(g, 'USA', T.id)).toBe(true);
    expect(canCourt(g, 'USA', T.id)).toMatchObject({ ok: true });
    const before = T.opinion;
    expect(court(g, 'USA', T.id)).toBe(true);
    expect(U.pc).toBe(0);
    expect(T.opinion).toBeLessThan(before);
    // (A neutral the Axis has not courted costs what courting always costs, and a neutral America is in no bloc.)
    const other = A.of.find((id) => id !== T.id && !freeCourt(g, 'USA', id))!;
    expect(canCourt(g, 'USA', other).ok).toBe(false);
    // A second month is not news again.
    g.turn++;
    const h2 = g.headlines.length;
    G.pc = 30;
    monthlyDiplomacy(g);
    expect(g.headlines.slice(h2).filter((x) => x.text === told[0].text)).toEqual([]);
  });

  it('a computer-run America courts back each one the Axis courts, at no cost', () => {
    const g = playing('GER');
    const U = g.nations.USA, G = g.nations.GER;
    expect(g.human).toBe('GER');
    // (The player's Germany courts by hand; the computer's envoys stay home.)
    U.pc = 0;
    G.pc = 30;
    expect(court(g, 'GER', 'MEX')).toBe(true);
    monthlyDiplomacy(g);
    expect(g.flags['court:USA:MEX']).toBe(g.turn);
    expect(U.pc).toBeLessThanOrEqual(0 + 3);
    expect(A.of.filter((id) => id !== 'MEX' && g.flags[`court:GER:${id}`] === g.turn)).toEqual([]);
  });

  it('a country the roll favours joins the Axis after six months of courting; one it does not, never by courting', () => {
    const g = playing('USA', 3, 5);
    let yes = 0, no = 0;
    for (let s = 1; s < 200 && (!yes || !no); s++) { g.seed = s; if (americasWon(g, 'MEX')) yes ||= s; else no ||= s; }
    for (const [seed, joins] of [[yes, true], [no, false]] as const) {
      const h = playing('USA', 3, 5);
      h.seed = seed;
      const M = h.nations.MEX;
      for (let m = 0; m < 8; m++) {
        h.nations.GER.pc = 30;
        h.flags['court:GER:MEX'] = -9;
        expect(court(h, 'GER', 'MEX') || bloc(M) === 'axis').toBe(true);
        if (bloc(M) === 'axis') break;
        h.turn++;
      }
      expect(bloc(M) === 'axis').toBe(joins);
      if (joins) expect(h.flags['americas:months:MEX']).toBe(A.months);
      // (Pushed all the way by drift, a country the roll did not favour still does not join the Axis.)
      if (!joins) { M.opinion = 95; M.home = 95; monthlyDiplomacy(h); expect(bloc(M)).not.toBe('axis'); }
    }
  });

  it('a country won over keeps its own army: no computer-run corps is shipped there and no air group based there', () => {
    const g = playing('USA', 3, 5);
    const M = g.nations.MEX, Vera = g.byName.Veracruz;
    M.align = 'axis';
    g.flags['americas:in:MEX'] = g.turn;
    expect(thornGround(g, Vera, 'GER')).toBe(true);
    expect(thornGround(g, Vera, 'MEX')).toBe(false);
    // Its enemies are not barred from it: the United States ships to and flies from what it takes (the cities
    // version's review).
    expect(thornGround(g, Vera, 'USA')).toBe(false);
    expect(thornGround(g, Vera, 'UK')).toBe(false);
    const port = Object.values(g.byName).find((r) => r.owner === 'GER' && r.port && !r.overseas)!;
    const c = addCorps(g, 'GER', 'Test infantry', 'inf', 2, port.id, {});
    expect(whyNotShip(g, c, Vera)).toBe('Mexico keeps its own army: no corps of its allies is shipped there.');
    const f = g.airFleets.find((x) => x.nat === 'GER' && x.ship === undefined)!;
    expect(whyNotBase(g, f, Vera, true)).toBe('Mexico keeps its own air force: no group of its allies is based there.');
    // (Played as Germany, the player's own are his affair.)
    const p = playing('GER', 3, 5);
    p.nations.MEX.align = 'axis';
    p.flags['americas:in:MEX'] = p.turn;
    expect(thornGround(p, p.byName.Veracruz, 'GER')).toBe(false);
  });

  it('once America is at war with the Axis, a country won over is at war with it too, and its army on American ground raises the alarm', () => {
    const g = playing('USA', 3, 5);
    const M = g.nations.MEX;
    M.align = 'axis';
    g.flags['americas:in:MEX'] = g.turn;
    monthlyDiplomacy(g);
    expect(atWar(g, 'MEX', 'USA')).toBe(false);
    declareWar(g, 'GER', 'USA');
    monthlyDiplomacy(g);
    expect(atWar(g, 'MEX', 'USA')).toBe(true);
    const home = Object.values(g.byName).find((r) => r.owner === 'USA' && !r.overseas && r.nb.some((j) => g.regions[j].owner === 'MEX'))!;
    addCorps(g, 'MEX', 'Northern military zones (test)', 'inf', 1, home.id, {});
    expect(homeAlarms(g, 'USA').some((a) => a.region === home.id && a.by === 'MEX')).toBe(true);
  });

  it('no more than one country of the Americas joins the Axis in a campaign (b391)', () => {
    const g = playing('USA', 3, 5);
    g.flags['americas:in:ARG'] = g.turn - 1;
    g.nations.ARG.align = 'axis';
    let seed = 0;
    for (let s = 1; s < 400 && !seed; s++) { g.seed = s; if (americasWon(g, 'MEX')) seed = s; }
    g.seed = seed;
    g.flags['americas:months:MEX'] = A.months - 1;
    g.nations.GER.pc = 30;
    expect(court(g, 'GER', 'MEX')).toBe(true);
    expect(bloc(g.nations.MEX)).not.toBe('axis');
    expect(g.flags['americas:in:MEX']).toBeUndefined();
  });

  it("the words: each capital reads as a city, and a player's America that hands diplomacy to its staff hears it courted back", () => {
    const g = playing();
    expect(A.of.map((id) => g.regions[g.nations[id].capital].name)).toEqual(['Mexico City', 'Havana', 'Port-au-Prince', 'Ciudad Trujillo', 'Guatemala City', 'San Salvador', 'Tegucigalpa', 'Managua', 'San Jose', 'Panama City', 'Buenos Aires', 'Santiago']);
    g.flags['court:GER:MEX'] = g.turn;
    g.nations.USA.pc = 0;
    orderDesk(g, 'diplomacy', true);
    const said = g.headlines.filter((h) => h.desk === 'diplomacy' && h.turn === g.turn).map((h) => h.text).join(' ');
    expect(said).toMatch(/courted Mexico back against the German envoys, at no cost\./);
    expect(g.nations.USA.pc).toBe(0);
  });
});
