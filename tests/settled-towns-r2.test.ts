// Conquered towns settle (release 2; Brian 24 November 2026 18:12Z and 18:13Z): a town taken from another country can
// put up every kind of building, and the military works that go only on a country's own ground (factories, shipyards
// and their slips, fortifications), once it has been held a few months; its buildings then serve its holder as at
// home. One rule for every country, read from the ground alone. Off, nothing changes.
import { describe, expect, it } from 'vitest';
import { CIVIC, civicOffers, civicSum, orderCivic, plotsAt, setSettledTowns, settleLeft, settledFor, settledTownsOn, whyNotCivic } from '../src/campaign/buildings';
import { canLay, deliverArsenal, factoriesHeld, offers, orderOffer } from '../src/campaign/arsenal';
import { fortCost } from '../src/campaign/economy';
import { capitulate, createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Build, Game, Region } from '../src/campaign/types';

/** A grand campaign with this country at the table, at its first month of orders. */
function playing(nat: string, seed = 1): Game {
  const g = startOpen(createGame(seed, nat));
  let guard = 0;
  while (!humanUp(g) && guard++ < 200) stepAI(g);
  beginHuman(g);
  return g;
}

/** Hands a town to a conqueror, taken 'ago' months before this one. */
function take(g: Game, r: Region, nat: string, ago: number): Region {
  r.ctrl = nat;
  r.since = g.turn - ago;
  return r;
}

const WAIT = CIVIC.settle.months;
const offerAt = (g: Game, nat: string, r: Region, key: string) => offers(g, nat, r.id).find((o) => o.key === key)!;

describe('off (the switch not thrown)', () => {
  it('opens no campaign with it, and a conquered town is as it was', () => {
    const g = playing('GER');
    expect(settledTownsOn(g)).toBe(false);
    const warsaw = take(g, g.byName['Warsaw'], 'GER', 12);
    expect(settledFor(g, 'GER', warsaw)).toBe(false);
    expect(whyNotCivic(g, 'GER', warsaw, 'school')).toMatch(/Only administration and medicine/);
    expect(offerAt(g, 'GER', warsaw, 'work:factory').why).toBe('Factories are built in the home country.');
    expect(offerAt(g, 'GER', warsaw, 'work:fort').why).toBe('Fortifications are built on home ground.');
    expect(fortCost(g, 'GER', warsaw)).toBeNull();
  });
});

describe('on', () => {
  it('a town just taken is still settling: administration and medicine now, everything else after the wait', () => {
    const g = playing('GER');
    setSettledTowns(g, true);
    const warsaw = take(g, g.byName['Warsaw'], 'GER', 0);
    expect(settleLeft(g, 'GER', warsaw)).toBe(WAIT);
    expect(whyNotCivic(g, 'GER', warsaw, 'school')).toMatch(new RegExp(`still settling.*in ${WAIT} months`));
    expect(whyNotCivic(g, 'GER', warsaw, 'hall')).toBeNull();
    expect(whyNotCivic(g, 'GER', warsaw, 'hospital')).toBeNull();
    expect(offerAt(g, 'GER', warsaw, 'work:factory').why).toMatch(/still settling/);
    expect(fortCost(g, 'GER', warsaw)).toBeNull();
    // One month short is still short.
    take(g, warsaw, 'GER', WAIT - 1);
    expect(settleLeft(g, 'GER', warsaw)).toBe(1);
    expect(whyNotCivic(g, 'GER', warsaw, 'market')).toMatch(/in 1 month\./);
  });

  it('once settled, every line of building can go up, and is ordered as at home', () => {
    const g = playing('GER');
    setSettledTowns(g, true);
    const warsaw = take(g, g.byName['Warsaw'], 'GER', WAIT);
    expect(settledFor(g, 'GER', warsaw)).toBe(true);
    for (const line of ['market', 'school', 'camp', 'hospital', 'hall']) expect(whyNotCivic(g, 'GER', warsaw, line), line).toBeNull();
    g.nations.GER.pp = 1000;
    expect(orderCivic(g, 'GER', warsaw.id, 'school')).not.toBeNull();
  });

  it('a settled town serves its holder: learning counts, its pay stays at the conquered share, taxes stay with home ground', () => {
    const g = playing('GER');
    setSettledTowns(g, true);
    const warsaw = take(g, g.byName['Warsaw'], 'GER', 0);
    g.flags['cv:any'] = 1;
    g.flags[`cv:school:${warsaw.id}`] = 2;
    g.flags[`cv:hall:${warsaw.id}`] = 1;
    const before = civicSum(g, 'GER'); // still settling: as without the switch
    take(g, warsaw, 'GER', WAIT);
    const after = civicSum(g, 'GER');
    const school = CIVIC.lines.find((l) => l.id === 'school')!;
    expect(after.research - before.research).toBeCloseTo(Math.min(CIVIC.caps.research, before.research + school.research! * 2) - before.research, 9);
    expect(after.research).toBeGreaterThan(before.research);
    // The hall on conquered ground adds the conquered share (economy.ts), never its home taxes or political capital.
    expect(after.tax).toBeCloseTo(before.tax, 9);
    expect(after.pc).toBeCloseTo(before.pc, 9);
    expect(after.commerce).toBeCloseTo(before.commerce, 9);
  });

  it('factories and fortifications go up in a settled town, and a factory built there counts against the industry', () => {
    const g = playing('GER');
    setSettledTowns(g, true);
    const warsaw = take(g, g.byName['Warsaw'], 'GER', WAIT);
    g.nations.GER.pp = 5000;
    const fac = offerAt(g, 'GER', warsaw, 'work:factory');
    expect(fac.why).toBeNull();
    expect(fortCost(g, 'GER', warsaw)).not.toBeNull();
    expect(offerAt(g, 'GER', warsaw, 'work:fort').why).toBeNull();
    // A factory finished there is the province's, and the builder's count of factories carries it.
    const held0 = factoriesHeld(g, 'GER').held, fac0 = warsaw.fac;
    const b: Build = { id: g.nextBuild++, what: 'work', item: 'factory', name: 'Factory at Warsaw', region: warsaw.id, ordered: g.turn, eta: g.turn, cost: fac.cost };
    deliverArsenal(g, 'GER', b);
    expect(warsaw.fac).toBe(fac0 + 1);
    expect(factoriesHeld(g, 'GER').held).toBe(held0 + 1);
    // Lost again, it no longer counts for the one who built it.
    warsaw.ctrl = 'POL';
    expect(factoriesHeld(g, 'GER').held).toBe(held0);
  });

  it('a settled port takes a shipyard and has its slips', () => {
    const g = playing('GER');
    const port = g.regions.find((r) => r.owner !== 'GER' && r.port && r.seas.length > 0 && r.owner === 'POL') ?? g.regions.find((r) => r.owner !== 'GER' && r.port && r.seas.length > 0)!;
    take(g, port, 'GER', WAIT);
    g.flags[`wk:yard:${port.id}`] = 1;
    expect(canLay(g, 'GER', port, 1)).toBe(false);
    expect(offerAt(g, 'GER', port, 'work:yard').why).toBe('Shipyards are built in the home country.');
    g.flags.settledTowns = 1; // as a campaign opened with the switch on
    expect(canLay(g, 'GER', port, 1)).toBe(true);
    g.nations.GER.pp = 5000;
    expect(offerAt(g, 'GER', port, 'work:yard').why).toBeNull();
  });

  it('is one rule for every country: the same town, held as long, answers alike for any holder', () => {
    for (const [nat, town] of [['GER', 'Warsaw'], ['SOV', 'Warsaw'], ['HUN', 'Warsaw'], ['POL', 'Berlin']] as const) {
      const g = playing('GER');
      setSettledTowns(g, true);
      const r = take(g, g.byName[town], nat, WAIT);
      expect(settledFor(g, nat, r), `${nat} ${town}`).toBe(true);
      expect(whyNotCivic(g, nat, r, 'school'), `${nat} ${town}`).toBeNull();
      take(g, r, nat, 0);
      expect(whyNotCivic(g, nat, r, 'school'), `${nat} ${town}`).toMatch(/still settling/);
    }
  });

  it('ground handed over when a country falls starts settling that month, as ground won in battle does', () => {
    for (const on of [false, true]) {
      const g = playing('GER');
      setSettledTowns(g, on);
      g.turn = 9; // long after the start, so that a town that never changed hands would read as settled
      const held = g.regions.filter((r) => r.owner === 'POL' && r.ctrl === 'POL' && !r.sov);
      expect(held.length).toBeGreaterThan(0);
      capitulate(g, 'POL', 'GER');
      const handed = held.filter((r) => r.ctrl === 'GER');
      expect(handed.length).toBeGreaterThan(0);
      for (const r of handed) {
        if (on) expect(settleLeft(g, 'GER', r), r.name).toBe(WAIT);
        else expect(g.flags[`hand:${r.id}`], r.name).toBeUndefined();
      }
      if (on) {
        g.turn += WAIT;
        expect(settledFor(g, 'GER', handed[0])).toBe(true);
      }
    }
  });

  it('a training ground in a settled town speeds replacements and promises nothing it cannot give', () => {
    const g = playing('GER');
    setSettledTowns(g, true);
    const warsaw = take(g, g.byName['Warsaw'], 'GER', WAIT);
    const camp = civicOffers(g, 'GER', warsaw.id).find((o) => o.line === 'camp')!;
    expect(camp.why).toBeNull();
    expect(camp.effect).toMatch(/replacements/i);
    expect(camp.effect).not.toMatch(/corps raised here/i);
    expect(camp.effect).not.toMatch(/corps raised here/);
    g.flags['cv:any'] = 1;
    g.flags[`cv:camp:${warsaw.id}`] = 1;
    const plot = plotsAt(g, warsaw.id).find((p) => /Drill|Training|Barracks/.test(JSON.stringify(p)));
    expect(plot, "the drill ground stands in the town plan").toBeDefined();
    expect(JSON.stringify(plot)).not.toMatch(/corps raised here/i);
  });

  it('works ordered in a settled town are delivered even if the switch is turned off before they finish, and still count', () => {
    const g = playing('GER');
    setSettledTowns(g, true);
    const warsaw = take(g, g.byName['Warsaw'], 'GER', WAIT);
    g.nations.GER.pp = 5000;
    const held0 = factoriesHeld(g, 'GER').held, fac0 = warsaw.fac;
    const b = orderOffer(g, 'GER', 'work:factory', warsaw.id)!;
    expect(b).not.toBeNull();
    expect(b.conq).toBe(true);
    setSettledTowns(g, false);
    deliverArsenal(g, 'GER', b);
    expect(warsaw.fac).toBe(fac0 + 1);
    expect(factoriesHeld(g, 'GER').held).toBe(held0 + 1);
  });

  it('turned on in a campaign under way, ground already held by another starts its wait then', () => {
    const g = playing('GER');
    g.turn = 12;
    const warsaw = take(g, g.byName['Warsaw'], 'GER', 9); // taken long ago, but the switch was off
    warsaw.since = 0; // as a country's fall leaves it
    setSettledTowns(g, true);
    expect(settleLeft(g, 'GER', warsaw)).toBe(WAIT);
    g.turn += WAIT;
    expect(settledFor(g, 'GER', warsaw)).toBe(true);
  });

  it('a reason the wait will not clear is said first', () => {
    const g = playing('GER');
    setSettledTowns(g, true);
    const inland = g.regions.find((r) => r.owner === 'POL' && !r.port && !r.sov)!;
    take(g, inland, 'GER', 0);
    expect(offerAt(g, 'GER', inland, 'work:yard').why).toBe(`${inland.name} has no harbour.`);
  });

  it('home ground is as it always was, with the switch on or off', () => {
    const g = playing('GER');
    const berlin = g.regions[g.nations.GER.capital];
    const off = whyNotCivic(g, 'GER', berlin, 'school');
    setSettledTowns(g, true);
    expect(settleLeft(g, 'GER', berlin)).toBe(0);
    expect(settledFor(g, 'GER', berlin)).toBe(false);
    expect(whyNotCivic(g, 'GER', berlin, 'school')).toBe(off);
  });
});
