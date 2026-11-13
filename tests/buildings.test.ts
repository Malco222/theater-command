// Towns you build up, in the grand campaign. The chief place of every province has building plots, more the bigger
// it is; each plot takes one line of building, raised in three levels. Commerce pays money and helps trade, farming
// feeds the depots, learning quickens research, training and medicine serve the army, administration brings tax and
// makes conquered ground pay. The history campaigns have none of it.
import { describe, expect, it } from 'vitest';
import data from '../data/campaign/buildings.json';
import {
  CIVIC, campXp, civicAt, civicDrafts, civicLevel, civicOffers, civicSum, hallShare, hospitalNear, hospitalWorth, orderCivic, plotsAt, plotsOf, researchSpeed, whyNotCivic,
} from '../src/campaign/buildings';
import { ledger, provinceYield } from '../src/campaign/economy';
import { allied, createGame } from '../src/campaign/game';
import { keepsStores } from '../src/campaign/resources';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { townSize } from '../src/campaign/towns';
import { housesOf } from '../src/campaign/trade';
import { beginHuman, buildingTips, endHuman, humanUp, orderBuilding, orderCancelBuild, runTurnAI, stepAI, townBuildings } from '../src/campaign/turn';
import type { Game, Region } from '../src/campaign/types';

/** A grand campaign with this country at the table, at its first month of orders. */
function playing(nat: string, seed = 1): Game {
  const g = startOpen(createGame(seed, nat));
  let guard = 0;
  while (!humanUp(g) && guard++ < 200) stepAI(g);
  beginHuman(g);
  return g;
}

/** Ends the player's month and plays on to his next one. */
function month(g: Game): void {
  endHuman(g);
  let guard = 0;
  while (!humanUp(g) && guard++ < 400) stepAI(g);
  beginHuman(g);
}

const built = (g: Game, line: string, r: Region, level: number): void => { g.flags['cv:any'] = 1; g.flags[`cv:${line}:${r.id}`] = level; };
const home = (g: Game, nat: string, want: (r: Region) => boolean): Region => g.regions.find((r) => r.ctrl === nat && r.owner === nat && want(r))!;
const lineOf = (id: string) => CIVIC.lines.find((l) => l.id === id)!;

describe('the data', () => {
  it('has six lines of three levels, each with a price and a time for every level', () => {
    expect(CIVIC.lines.map((l) => l.id)).toEqual(['market', 'farm', 'school', 'camp', 'hospital', 'hall']);
    for (const l of CIVIC.lines) {
      expect(l.levels, l.id).toHaveLength(3);
      expect(l.cost, l.id).toHaveLength(3);
      expect(l.months, l.id).toHaveLength(3);
      expect(l.blurb.length, l.id).toBeGreaterThan(20);
      // A higher level never costs less or goes up faster than the one before.
      for (let i = 1; i < 3; i++) { expect(l.cost[i]).toBeGreaterThanOrEqual(l.cost[i - 1]); expect(l.months[i]).toBeGreaterThanOrEqual(l.months[i - 1]); }
    }
    expect(data.plots).toEqual({ village: 1, town: 2, city: 3, capital: 4 });
  });
});

describe('the history campaigns', () => {
  it('have no town buildings at all', () => {
    const g = createGame(4, 'GER');
    const berlin = g.byName['Berlin'];
    expect(townBuildings(g, berlin.id)).toBeNull();
    expect(plotsAt(g, berlin.id)).toEqual([]);
    expect(whyNotCivic(g, 'GER', berlin, 'market')).toMatch(/grand campaign/);
    expect(orderCivic(g, 'GER', berlin.id, 'market')).toBeNull();
    expect(orderBuilding(g, berlin.id, 'market')).toBeNull();
    expect(buildingTips(g)).toEqual([]);
    // Even a flag left by mistake does nothing there.
    built(g, 'market', berlin, 3); built(g, 'school', berlin, 3);
    expect(civicSum(g, 'GER').count).toBe(0);
    expect(researchSpeed(g, 'GER')).toBe(1);
    expect(civicDrafts(g, 'GER')).toBe(1);
    expect(ledger(g, 'GER').income.map((l) => l.key)).not.toContain('commerce');
  });
});

describe('plots', () => {
  it('number one in a village, two in a town, three in a city and four in the capital', () => {
    const g = playing('GER');
    for (const r of g.regions) {
      const size = townSize(g, r);
      expect(plotsOf(g, r), r.name).toBe(data.plots[size]);
      expect(plotsAt(g, r.id), r.name).toHaveLength(data.plots[size]);
    }
    const t = townBuildings(g, g.nations.GER.capital)!;
    expect(t.size).toBe('capital');
    expect(t.plots).toHaveLength(4);
    expect(t.free).toBe(4);
    expect(t.mine).toBe(true);
    expect(t.offers.map((o) => o.line)).toEqual(CIVIC.lines.map((l) => l.id));
    // Somebody else's town shows its plots and offers nothing.
    const paris = townBuildings(g, g.nations.FRA.capital)!;
    expect(paris.mine).toBe(false);
    expect(paris.offers).toEqual([]);
  });

  it('take one line each, and a full town takes no new line', () => {
    const g = playing('GER');
    const village = home(g, 'GER', (r) => townSize(g, r) === 'village' && lineOf('farm').ground!.includes(r.terrain));
    expect(whyNotCivic(g, 'GER', village, 'camp')).toBeNull();
    built(g, 'camp', village, 1);
    expect(whyNotCivic(g, 'GER', village, 'farm')).toMatch(/Every building plot/);
    // The line that stands there can still be raised.
    expect(whyNotCivic(g, 'GER', village, 'camp')).toBeNull();
    const plots = plotsAt(g, village.id);
    expect(plots).toHaveLength(1);
    expect(plots[0]).toMatchObject({ line: 'camp', level: 1, label: 'Drill ground', building: false });
    expect(plots[0].does.length).toBeGreaterThan(10);
  });
});

describe('ordering a building', () => {
  it('is paid for at once, takes its months, and is announced when it is finished', () => {
    const g = playing('GER');
    const berlin = g.regions[g.nations.GER.capital];
    const l = lineOf('market');
    const before = g.nations.GER.pp;
    const b = orderBuilding(g, berlin.id, 'market')!;
    expect(b).toMatchObject({ what: 'civic', item: 'market', region: berlin.id, cost: l.cost[0], eta: g.turn + l.months[0] });
    expect(b.name).toBe('Market hall at Berlin');
    expect(g.nations.GER.pp).toBeCloseTo(before - l.cost[0], 6);
    // The builders of a town do one thing at a time.
    expect(whyNotCivic(g, 'GER', berlin, 'school')).toMatch(/builders at Berlin are busy/);
    expect(orderBuilding(g, berlin.id, 'school')).toBeNull();
    const going = townBuildings(g, berlin.id)!;
    expect(going.free).toBe(3);
    expect(going.plots[0]).toMatchObject({ line: 'market', level: 0, building: true, due: b.eta });
    expect(civicLevel(g, 'market', berlin.id)).toBe(0);
    for (let m = 0; m < l.months[0]; m++) month(g);
    expect(civicLevel(g, 'market', berlin.id)).toBe(1);
    expect(g.headlines.some((h) => /market hall at Berlin is finished/.test(h.text))).toBe(true);
    const done = townBuildings(g, berlin.id)!;
    expect(done.plots[0]).toMatchObject({ line: 'market', level: 1, label: 'Market hall', building: false });
    expect(done.offers.find((o) => o.line === 'market')).toMatchObject({ level: 2, label: 'Merchant quarter', has: 1, cost: l.cost[1], months: l.months[1], why: null });
    // The money shows in the ledger.
    const line = ledger(g, 'GER').income.find((x) => x.key === 'commerce')!;
    expect(line.amount).toBeCloseTo(civicSum(g, 'GER').commerce, 6);
    expect(line.amount).toBeGreaterThan(0);
  });

  it('can be cancelled in the month it was ordered, for the money back', () => {
    const g = playing('GER');
    const berlin = g.regions[g.nations.GER.capital];
    const before = g.nations.GER.pp;
    const b = orderBuilding(g, berlin.id, 'hall')!;
    expect(orderCancelBuild(g, b.id)).toBeTruthy();
    expect(g.nations.GER.pp).toBeCloseTo(before, 6);
    expect(townBuildings(g, berlin.id)!.free).toBe(4);
  });

  it('is refused with a reason where it cannot stand', () => {
    const g = playing('GER');
    const berlin = g.regions[g.nations.GER.capital];
    const paris = g.regions[g.nations.FRA.capital];
    expect(whyNotCivic(g, 'GER', paris, 'market')).toMatch(/not yours/);
    expect(orderCivic(g, 'GER', paris.id, 'market')).toBeNull();
    // Farming needs farmland, forest or hills.
    expect(berlin.terrain).toBe('urban');
    expect(whyNotCivic(g, 'GER', berlin, 'farm')).toMatch(/ground here will not carry it/);
    // Learning needs a city, commerce at least a town.
    const village = home(g, 'GER', (r) => townSize(g, r) === 'village');
    expect(whyNotCivic(g, 'GER', village, 'school')).toMatch(/Needs a city/);
    expect(whyNotCivic(g, 'GER', village, 'market')).toMatch(/Needs a town/);
    // The third level is the last.
    built(g, 'hall', berlin, 3);
    expect(whyNotCivic(g, 'GER', berlin, 'hall')).toMatch(/as far as this goes/);
    // An empty treasury: the offer says so, and the order is not taken.
    g.nations.GER.pp = 1;
    const o = civicOffers(g, 'GER', berlin.id).find((x) => x.line === 'market')!;
    expect(o).toMatchObject({ poor: true, why: 'The treasury cannot cover it.' });
    expect(orderCivic(g, 'GER', berlin.id, 'market')).toBeNull();
    expect(g.nations.GER.queue.filter((b) => b.what === 'civic')).toHaveLength(0);
  });

  it('on conquered ground is only administration and medicine', () => {
    const g = playing('GER');
    const taken = g.byName['Warsaw'];
    taken.ctrl = 'GER';
    expect(whyNotCivic(g, 'GER', taken, 'market')).toMatch(/Only administration and medicine/);
    expect(whyNotCivic(g, 'GER', taken, 'school')).toMatch(/Only administration and medicine/);
    expect(whyNotCivic(g, 'GER', taken, 'hall')).toBeNull();
    expect(whyNotCivic(g, 'GER', taken, 'hospital')).toBeNull();
    expect(orderCivic(g, 'GER', taken.id, 'hall')).not.toBeNull();
  });

  it('survives a save, both what stands and what is on order', () => {
    const g = playing('GER');
    const berlin = g.regions[g.nations.GER.capital];
    const hamburg = g.byName['Hamburg'];
    built(g, 'school', hamburg, 2);
    const b = orderBuilding(g, berlin.id, 'market')!;
    const back = loadGame(saveGame(g))!;
    expect(civicLevel(back, 'school', hamburg.id)).toBe(2);
    expect(back.nations.GER.queue.find((x) => x.id === b.id)).toMatchObject({ what: 'civic', item: 'market', region: berlin.id, eta: b.eta });
    expect(townBuildings(back, berlin.id)!.plots[0]).toMatchObject({ line: 'market', building: true });
    expect(researchSpeed(back, 'GER')).toBeCloseTo(researchSpeed(g, 'GER'), 9);
  });
});

describe('what the buildings are worth', () => {
  it('commerce pays more in a rich place than a poor one, and half on conquered ground', () => {
    const g = playing('GER');
    const l = lineOf('market');
    const berlin = g.regions[g.nations.GER.capital];
    built(g, 'market', berlin, 2);
    const two = civicSum(g, 'GER').commerce;
    expect(two).toBeGreaterThan(l.money! * 2 * CIVIC.place.base);
    built(g, 'market', berlin, 3);
    expect(civicSum(g, 'GER').commerce).toBeCloseTo((two * 3) / 2, 6);
    // The same market in poorer ground pays less.
    const g2 = playing('GER');
    const poor = home(g2, 'GER', (r) => r.terrain !== 'urban' && townSize(g2, r) !== 'village');
    built(g2, 'market', poor, 2);
    expect(civicSum(g2, 'GER').commerce).toBeLessThan(two);
    // A market taken with the town still pays its new master, at the occupied share.
    const g3 = playing('GER');
    const warsaw = g3.byName['Warsaw'];
    built(g3, 'market', warsaw, 2);
    const polish = civicSum(g3, 'POL').commerce;
    warsaw.ctrl = 'GER';
    expect(civicSum(g3, 'POL').commerce).toBe(0);
    expect(civicSum(g3, 'GER').commerce).toBeGreaterThan(0);
    expect(civicSum(g3, 'GER').commerce).toBeLessThan(polish);
    // But it adds nothing to the conqueror's trade.
    expect(civicSum(g3, 'GER').trade).toBe(0);
  });

  it('commerce adds to the worth of trade, and a merchant quarter in the capital or a harbour opens a trading house', () => {
    const g = playing('GER');
    const l = lineOf('market');
    const berlin = g.regions[g.nations.GER.capital];
    const houses = housesOf(g, 'GER');
    built(g, 'market', berlin, 1);
    expect(civicSum(g, 'GER').trade).toBeCloseTo(l.trade!, 9);
    expect(housesOf(g, 'GER')).toBe(houses);
    built(g, 'market', berlin, 2);
    expect(housesOf(g, 'GER')).toBe(houses + 1);
    // A harbour town does the same; an inland town does not.
    const harbour = home(g, 'GER', (r) => r.port && r.id !== berlin.id && townSize(g, r) !== 'village');
    built(g, 'market', harbour, 2);
    expect(housesOf(g, 'GER')).toBe(houses + 2);
    const inland = home(g, 'GER', (r) => !r.port && r.id !== berlin.id && townSize(g, r) !== 'village');
    built(g, 'market', inland, 3);
    expect(housesOf(g, 'GER')).toBe(houses + 2);
    // However many markets, the gain to trade stops at its cap.
    for (const r of g.regions) if (r.ctrl === 'GER' && r.owner === 'GER') built(g, 'market', r, 3);
    expect(civicSum(g, 'GER').trade).toBe(CIVIC.caps.trade);
    expect(civicSum(g, 'GER').houses).toBeLessThanOrEqual(CIVIC.caps.houses);
  });

  it('farming feeds the depots of a power that keeps stocks, and pays money to one that does not', () => {
    const g = playing('GER');
    const l = lineOf('farm');
    expect(keepsStores(g, 'GER')).toBe(true);
    const field = home(g, 'GER', (r) => l.ground!.includes(r.terrain));
    built(g, 'farm', field, 2);
    expect(civicSum(g, 'GER', true)).toMatchObject({ sup: l.sup! * 2, farms: 0 });
    // A small country sells the produce instead.
    expect(keepsStores(g, 'BEL')).toBe(false);
    const meadow = home(g, 'BEL', (r) => l.ground!.includes(r.terrain));
    built(g, 'farm', meadow, 3);
    const s = civicSum(g, 'BEL', false);
    expect(s.sup).toBe(0);
    expect(s.farms).toBeCloseTo(l.moneyElse! * 3, 9);
    expect(ledger(g, 'BEL').income.find((x) => x.key === 'farms')!.amount).toBeCloseTo(s.farms, 6);
  });

  it('learning quickens research, up to a cap', () => {
    const g = playing('GER');
    const l = lineOf('school');
    const city = home(g, 'GER', (r) => townSize(g, r) === 'city');
    built(g, 'school', city, 3);
    expect(researchSpeed(g, 'GER')).toBeCloseTo(1 + l.research! * 3, 9);
    for (const r of g.regions) if (r.ctrl === 'GER') built(g, 'school', r, 3);
    expect(researchSpeed(g, 'GER')).toBeCloseTo(1 + CIVIC.caps.research, 9);
  });

  it('training gives a corps raised there more experience and speeds the replacements', () => {
    const g = playing('GER');
    const l = lineOf('camp');
    const r = home(g, 'GER', () => true);
    expect(campXp(g, r.id)).toBe(0);
    built(g, 'camp', r, 2);
    expect(campXp(g, r.id)).toBeCloseTo(l.xp! * 2, 9);
    expect(civicDrafts(g, 'GER')).toBeCloseTo(1 + l.drafts! * 2, 9);
    for (const x of g.regions) if (x.ctrl === 'GER') built(g, 'camp', x, 3);
    expect(civicDrafts(g, 'GER')).toBeCloseTo(1 + CIVIC.caps.drafts, 9);
  });

  it('a hospital serves a corps resting in its province or beside it, on friendly ground only', () => {
    const g = playing('GER');
    const l = lineOf('hospital');
    const c = g.corps.find((x) => x.nat === 'GER' && !x.dead)!;
    const here = g.regions[c.reg];
    const friendly = (holder: string): boolean => holder === 'GER' || allied(g, 'GER', holder);
    expect(hospitalNear(g, c, friendly)).toBe(0);
    const beside = g.regions[here.nb.find((j) => g.regions[j].ctrl === 'GER')!];
    built(g, 'hospital', beside, 2);
    expect(hospitalNear(g, c, friendly)).toBe(2);
    built(g, 'hospital', here, 3);
    expect(hospitalNear(g, c, friendly)).toBe(3);
    expect(hospitalWorth(3)).toEqual({ drafts: 1 + l.rest! * 3, org: l.org! * 3 });
    // An enemy's hospital next door is no use.
    built(g, 'hospital', here, 0);
    beside.ctrl = 'POL';
    expect(hospitalNear(g, c, friendly)).toBe(0);
  });

  it('administration brings tax and political capital at home, and makes conquered ground pay more', () => {
    const g = playing('GER');
    const l = lineOf('hall');
    const berlin = g.regions[g.nations.GER.capital];
    built(g, 'hall', berlin, 2);
    expect(civicSum(g, 'GER')).toMatchObject({ tax: l.tax! * 2, pc: l.pc! * 2 });
    expect(ledger(g, 'GER').income.find((x) => x.key === 'tax')!.amount).toBeCloseTo(l.tax! * 2, 6);
    const warsaw = g.byName['Warsaw'];
    warsaw.ctrl = 'GER';
    const bare = provinceYield(g, warsaw, 'GER').total;
    built(g, 'hall', warsaw, 2);
    expect(hallShare(g, warsaw)).toBeCloseTo(l.occupiedShare! * 2, 9);
    expect(provinceYield(g, warsaw, 'GER').total).toBeGreaterThan(bare);
    // A governor's office abroad collects no home tax.
    expect(civicSum(g, 'GER').tax).toBeCloseTo(l.tax! * 2, 9);
    // Political capital from many halls stops at its cap.
    for (const r of g.regions) if (r.ctrl === 'GER' && r.owner === 'GER') built(g, 'hall', r, 3);
    expect(civicSum(g, 'GER').pc).toBe(CIVIC.caps.pc);
  });

  it('political capital from the halls reaches the government each month', () => {
    const g = playing('GER');
    const plain = playing('GER');
    built(g, 'hall', g.regions[g.nations.GER.capital], 3);
    g.nations.GER.pc = plain.nations.GER.pc = 0;
    month(g); month(plain);
    expect(g.nations.GER.pc - plain.nations.GER.pc).toBeCloseTo(lineOf('hall').pc! * 3, 6);
  });
});

describe('the staff\'s picks', () => {
  it('name, for each line, the best of the player\'s places that can take it now', () => {
    const g = playing('GER');
    const tips = buildingTips(g, 6);
    expect(tips.map((t) => t.offer.line)).toEqual(CIVIC.lines.map((l) => l.id));
    for (const t of tips) {
      expect(g.regions[t.region].ctrl, t.town).toBe('GER');
      expect(t.offer.why, t.town).toBeNull();
      expect(orderBuilding(g, t.region, t.offer.line), t.town).not.toBeNull();
      // One order at a time per town: undo it so the next tip for the same place can be tried.
      orderCancelBuild(g, g.nations.GER.queue[g.nations.GER.queue.length - 1].id);
    }
  });
});

describe('computer-run nations', () => {
  it('build up their towns from a share of their income, in a mix of lines, and leave the player\'s alone', () => {
    const g = startOpen(createGame(2, null));
    for (let m = 0; m < 30; m++) runTurnAI(g);
    const levels = (nat: string): Record<string, number> => {
      const out: Record<string, number> = {};
      for (const r of g.regions) if (r.ctrl === nat) for (const { line, level } of civicAt(g, r.id)) out[line.id] = (out[line.id] ?? 0) + level;
      return out;
    };
    const ger = levels('GER');
    const total = Object.values(ger).reduce((a, b) => a + b, 0);
    expect(total).toBeGreaterThanOrEqual(8);
    expect(Object.keys(ger).length).toBeGreaterThanOrEqual(3);
    expect(ger.market ?? 0).toBeGreaterThan(0);
    // No town holds more lines than it has plots, and no line stands above the third level.
    for (const r of g.regions) {
      expect(civicAt(g, r.id).length, r.name).toBeLessThanOrEqual(plotsOf(g, r));
      for (const { level } of civicAt(g, r.id)) expect(level).toBeLessThanOrEqual(3);
    }
    // A gang of builders for every dozen home provinces, and one more: never more orders than that.
    for (const n of Object.values(g.nations)) {
      if (!n.alive) continue;
      const homes = g.regions.filter((r) => r.owner === n.id).length;
      expect(n.queue.filter((b) => b.what === 'civic').length, n.id).toBeLessThanOrEqual(1 + Math.floor(homes / CIVIC.ai.gang));
    }
    // With a player at the table, nobody builds for him.
    const h = playing('GER', 2);
    for (let m = 0; m < 8; m++) month(h);
    expect(civicSum(h, 'GER').count).toBe(0);
    expect(h.nations.GER.queue.filter((b) => b.what === 'civic')).toHaveLength(0);
    expect(Object.keys(h.flags).some((k) => k.startsWith('cv:') && k !== 'cv:any')).toBe(true);
  }, 60_000);
});
