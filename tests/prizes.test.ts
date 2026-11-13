// Prizes outside the town, and scorched earth (the grand campaign). A province's oilfield, factory district,
// airfield, harbour and great rail junction are places of their own. Those outside the walls go with the country
// round the town: the army that wins the field has the use of them once it has held them a month, while the town
// still holds. A harbour lies inside the defences of a city, a capital or a fortress, and goes with the town. A prize
// can be wrecked by order, which is a corps' work for the month, and burned by an army driven off the ground if its
// nation has ordered scorched earth there. Time mends it, and engineers mend it faster. None of this is in the
// historical scenarios. The numbers are in data/campaign/prizes.json.
import { describe, expect, it } from 'vitest';
import { whyNotBase } from '../src/campaign/air';
import { income, ledger, provinceYield } from '../src/campaign/economy';
import { addCorps, createGame, declareWar, relocate } from '../src/campaign/game';
import { whyNotShip } from '../src/campaign/navy';
import {
  PRIZES, PRIZE_KINDS, airfieldOpenTo, burnChance, burnOrder, harbourOpen, hasPrize, junctionOpenTo, mendPrizes, orderWreckPrize, prizeHolder, prizeIncome,
  prizeInside, prizeMarks, prizeOut, prizeRules, prizeSettling, prizeTaker, prizeUser, prizesAt, prizesOnLoss, scorchPolicy, setBurnOrder, setScorchPolicy,
  whyNotWreck, willBurn, worksOutside, wreckPrize, type PrizeKind,
} from '../src/campaign/prizes';
import { mendRail, railCut, railOpen, railsAt, workingFactories } from '../src/campaign/rails';
import { fieldYield, stores } from '../src/campaign/resources';
import { loadGame, saveGame } from '../src/campaign/save';
import { arrive, besieged, fedBySea, fieldCtrl, garrisonOf, lift } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import {
  beginHuman, endHuman, humanUp, myPrizeIncome, orderBurn, orderScorchPolicy, orderWreck, prizesOf, prizesOnMap, runTurnAI, scorchPolicyNow, stepAI, whyNotBurnOrder,
  whyNotWreckPrize,
} from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';

const free = (seed: number, human: string | null = null): Game => startOpen(createGame(seed, human));
/** A grand campaign with the player's first month open. */
function playing(human = 'GER', seed = 3): Game {
  const g = free(seed, human);
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
/** Sets a province's field armies down somewhere else, for a test. */
function clearField(g: Game, r: Region, to: Region): void {
  for (const u of [...r.units]) relocate(g, u, to);
}
/** A corps of this nation set down in the field of a province, as if it had just won the battle there. */
function comeUp(g: Game, nat: string, r: Region, kind: 'inf' | 'arm' | 'eng' = 'inf'): Corps {
  const c = addCorps(g, nat, `Test ${kind} ${r.name}`, kind, 3, r.id, {});
  arrive(g, r, nat);
  return c;
}
/** A siege that began so many months ago. */
const heldFor = (g: Game, r: Region, months: number): void => { g.flags[`siege:${r.id}`] = g.turn - months; };
const kindsAt = (g: Game, r: Region): PrizeKind[] => PRIZE_KINDS.filter((k) => hasPrize(g, r, k));

/** Germany in September 1939 with one of her corps round Warsaw: the Polish field army is set down in Lublin. */
function warsaw(): { g: Game; town: Region; ger: Corps } {
  const g = playing('GER');
  const town = g.byName['Warsaw'];
  clearField(g, town, g.byName['Lublin']);
  const ger = comeUp(g, 'GER', town);
  return { g, town, ger };
}

describe('prizes are for the grand campaign', () => {
  it('the historical scenarios have none: everything goes with the province as it always did', () => {
    const g = createGame(3, 'GER');
    const ruhr = g.byName['Ruhr'];
    expect(prizeRules(g)).toBe(false);
    expect(kindsAt(g, ruhr)).toEqual([]);
    expect(prizesAt(g, ruhr.id)).toBeNull();
    expect(prizeMarks(g)).toEqual([]);
    expect(prizeUser(g, ruhr, 'works')).toBe('GER');
    expect(workingFactories(g, ruhr)).toBe(ruhr.fac);
    expect(wreckPrize(g, ruhr, 'works', 6)).toBe(false);
    expect(whyNotWreck(g, 'GER', ruhr.id, 'works')).toMatch(/only in the grand campaign/);
    expect(willBurn(g, 'SOV', g.byName['Kiev'])).toBe(false);
    expect(prizeIncome(g, 'GER').total).toBe(0);
  });

  it('the grand campaign has them, and a saved game keeps what was wrecked and what was ordered', () => {
    const g = playing('GER');
    const ruhr = g.byName['Ruhr'];
    expect(prizeRules(g)).toBe(true);
    wreckPrize(g, ruhr, 'works', 6);
    setBurnOrder(g, 'GER', ruhr, 1);
    setScorchPolicy(g, 'GER', 'conquered');
    const h = loadGame(saveGame(g))!;
    expect(prizeOut(h, h.byName['Ruhr'], 'works')).toBe(6);
    expect(burnOrder(h, 'GER', h.byName['Ruhr'])).toBe(1);
    expect(scorchPolicy(h, 'GER')).toBe('conquered');
    expect(prizesAt(h, ruhr.id)).toEqual(prizesAt(g, ruhr.id));
  });
});

describe('what a province has, and where it lies', () => {
  it('oilfields, factory districts, airfields, harbours and great junctions are on the map', () => {
    const g = playing('GER');
    const count = (k: PrizeKind): number => g.regions.filter((r) => hasPrize(g, r, k)).length;
    expect(count('oil')).toBe(5);
    expect(count('works')).toBe(37);
    expect(count('junction')).toBe(32);
    expect(count('airfield')).toBeGreaterThan(100);
    expect(count('harbour')).toBeGreaterThan(80);
    expect(kindsAt(g, g.byName['Warsaw'])).toEqual(['works', 'airfield', 'junction']);
    expect(kindsAt(g, g.byName['Ploiesti'])).toEqual(['oil', 'airfield']);
    expect(kindsAt(g, g.byName['Maikop'])).toEqual(['oil']);
  });

  it('half the factories of an industrial province stand outside the walls, and a province with one factory has no district', () => {
    const g = playing('GER');
    expect(worksOutside(g.byName['Ruhr'])).toBe(2);
    expect(worksOutside(g.byName['Moscow'])).toBe(3);
    expect(worksOutside(g.byName['Warsaw'])).toBe(1);
    expect(worksOutside(g.byName['Bremen'])).toBe(0);
    expect(hasPrize(g, g.byName['Bremen'], 'works')).toBe(false);
  });

  it('a junction is where at least five lines meet', () => {
    const g = playing('GER');
    for (const r of g.regions) expect(hasPrize(g, r, 'junction'), r.name).toBe(railsAt(r.id).length >= PRIZES.junction.lines);
  });

  it('the harbour of a city, a capital or a fortress lies inside the defences; the quays of a small place lie outside', () => {
    const g = playing('GER');
    expect(prizeInside(g, g.byName['Hamburg'], 'harbour')).toBe(true);
    expect(prizeInside(g, g.byName['London'], 'harbour')).toBe(true);
    // Odessa is a small place, but a fortress.
    expect(prizeInside(g, g.byName['Odessa'], 'harbour')).toBe(true);
    expect(prizeInside(g, g.byName['Bremen'], 'harbour')).toBe(false);
    expect(prizeInside(g, g.byName['Antwerp'], 'harbour')).toBe(false);
    // Nothing else is ever inside.
    for (const r of g.regions) for (const k of ['oil', 'works', 'airfield', 'junction'] as const) expect(prizeInside(g, r, k)).toBe(false);
  });

  it('a panel reads each prize with who holds it and how it stands', () => {
    const g = playing('GER');
    const v = prizesAt(g, g.byName['Warsaw'].id)!;
    expect(v.prizes.map((p) => p.name)).toEqual(['Factory district', 'Airfield', 'Rail junction']);
    expect(v.prizes[0].detail).toBe('1 of 2 factories outside the walls');
    for (const p of v.prizes) {
      expect(p.holder).toBe('POL');
      expect(p.user).toBe('POL');
      expect(p.out).toBe(0);
      expect(p.state).toBe('At work.');
    }
    expect(prizesAt(g, g.byName['Lodz'].id)?.prizes.map((p) => p.kind) ?? []).not.toContain('works');
  });
});

describe('the prizes outside the walls go with the field', () => {
  it('an army that wins the country round a town holds them at once, and has the use of them after a month', () => {
    const { g, town } = warsaw();
    expect(besieged(g, town)).toBe(true);
    // The player is told what changed hands whole. An airfield alone is not news.
    expect(g.headlines.some((h) => h.text === 'The factory district, the airfield and the rail junction outside Warsaw are in German hands, though the town holds. They work for Germany after a month.')).toBe(true);
    const lodz = g.byName['Lodz'];
    clearField(g, lodz, g.byName['Lublin']);
    comeUp(g, 'GER', lodz);
    expect(g.headlines.some((h) => /outside Lodz/.test(h.text))).toBe(false);
    expect(town.ctrl).toBe('POL');
    expect(fieldCtrl(g, town)).toBe('GER');
    for (const k of ['works', 'airfield', 'junction'] as const) {
      expect(prizeHolder(g, town, k)).toBe('GER');
      expect(prizeSettling(g, town, k)).toBe(true);
      expect(prizeUser(g, town, k)).toBeNull();
      expect(prizeTaker(g, town, k)).toBeNull();
    }
    expect(prizesAt(g, town.id)!.prizes[0].state).toMatch(/Taken by German troops this month/);
    heldFor(g, town, PRIZES.settle);
    for (const k of ['works', 'airfield', 'junction'] as const) {
      expect(prizeSettling(g, town, k)).toBe(false);
      expect(prizeUser(g, town, k)).toBe('GER');
    }
    expect(prizesAt(g, town.id)!.prizes[0].state).toMatch(/In German hands with the country round the town/);
  });

  it('the factory district pays the army that holds it, and the map marks the province', () => {
    const { g, town } = warsaw();
    const before = income(g, 'GER');
    expect(prizeIncome(g, 'GER').total).toBe(0);
    heldFor(g, town, 1);
    const p = prizeIncome(g, 'GER');
    expect(p.works).toBeGreaterThan(0);
    expect(p.count).toBe(1);
    expect(income(g, 'GER')).toBeGreaterThan(before);
    const line = ledger(g, 'GER').income.find((l) => l.key === 'prizes')!;
    expect(line.count).toBe(1);
    expect(line.amount).toBeCloseTo(p.total, 6);
    expect(myPrizeIncome(g)!.total).toBeCloseTo(p.total, 6);
    // The town's own holder gets next to nothing from a province under siege, as before.
    expect(provinceYield(g, town).fac).toBeLessThan(0.5);
    expect(prizeMarks(g).find((m) => m.region === town.id)).toEqual({ region: town.id, by: 'GER', burning: [], seized: ['works', 'airfield', 'junction'] });
    expect(prizesOnMap(g).some((m) => m.region === town.id)).toBe(true);
  });

  it('nothing is paid for a prize while the siege is not a month old, or once the besiegers are gone', () => {
    const { g, town, ger } = warsaw();
    expect(prizeIncome(g, 'GER').count).toBe(0);
    heldFor(g, town, 2);
    expect(prizeIncome(g, 'GER').count).toBe(1);
    relocate(g, ger, g.byName['Masuria']);
    expect(prizeIncome(g, 'GER').count).toBe(0);
    expect(prizeHolder(g, town, 'works')).toBe('POL');
  });

  it('the oil of a field goes to the army that holds the wells, though the town holds', () => {
    const g = playing('GER');
    declareWar(g, 'GER', 'ROM');
    const field = g.byName['Ploiesti'];
    clearField(g, field, g.byName['Bucharest']);
    const oilOf = (nat: string): number => stores(g, nat)!.oil.income.filter((l) => l.key === 'taken' || l.key === 'own').reduce((s, l) => s + l.amount, 0);
    const before = oilOf('GER');
    comeUp(g, 'GER', field);
    expect(field.ctrl).toBe('ROM');
    // The month it changes hands it yields to nobody.
    expect(prizeTaker(g, field, 'oil')).toBeNull();
    expect(oilOf('GER')).toBeCloseTo(before, 6);
    heldFor(g, field, 1);
    expect(prizeTaker(g, field, 'oil')).toBe('GER');
    expect(oilOf('GER')).toBeCloseTo(before + fieldYield(g, field, 'GER'), 6);
    expect(prizeIncome(g, 'GER').oil).toBeGreaterThan(0);
  });

  it('aircraft fly from an airfield taken with the field, once it has been held a month', () => {
    const { g, town } = warsaw();
    const wing = g.airFleets.find((f) => f.nat === 'GER' && f.ship === undefined)!;
    expect(airfieldOpenTo(g, 'GER', town)).toBe(false);
    expect(whyNotBase(g, wing, town)).toBe('Airfields taken this month are not yet in use.');
    heldFor(g, town, 1);
    expect(airfieldOpenTo(g, 'GER', town)).toBe(true);
    expect(airfieldOpenTo(g, 'POL', town)).toBe(false);
    const why = whyNotBase(g, wing, town);
    expect(why === null || /Too far|already changed base/.test(why)).toBe(true);
    const polish = g.airFleets.find((f) => f.nat === 'POL' && f.ship === undefined && f.base !== town.id);
    if (polish) expect(whyNotBase(g, polish, town)).toMatch(/enemy holds the country round the town/);
  });

  it('trains run through a great junction held with the field, though the station in the town is shut', () => {
    const { g, town } = warsaw();
    // The next station down the line is in German hands, with nobody round it.
    const next = railsAt(town.id).map((j) => g.regions[j]).find((r) => r.name !== 'Lublin')!;
    clearField(g, next, g.byName['Lublin']);
    next.ctrl = 'GER';
    expect(railsAt(town.id)).toContain(g.byName['Lublin'].id);
    expect(junctionOpenTo(g, 'GER', town)).toBe(false);
    expect(railOpen(g, 'GER', town.id, next.id)).toBe(false);
    heldFor(g, town, 1);
    expect(junctionOpenTo(g, 'GER', town)).toBe(true);
    expect(junctionOpenTo(g, 'POL', town)).toBe(false);
    expect(railOpen(g, 'GER', town.id, next.id)).toBe(true);
    expect(railOpen(g, 'POL', town.id, g.byName['Lublin'].id)).toBe(false);
    // A province with no great junction stays shut to both while its town holds.
    const lodz = g.byName['Lodz'];
    expect(hasPrize(g, lodz, 'junction')).toBe(false);
    clearField(g, lodz, g.byName['Lublin']);
    comeUp(g, 'GER', lodz);
    if (besieged(g, lodz)) { heldFor(g, lodz, 3); expect(junctionOpenTo(g, 'GER', lodz)).toBe(false); }
  });

  it('the quays of a small port are lost with the field: its garrison is no longer fed by sea', () => {
    const g = playing('GER');
    declareWar(g, 'GER', 'BEL');
    const port = g.byName['Antwerp'];
    expect(harbourOpen(g, port)).toBe(true);
    clearField(g, port, g.byName['Brussels']);
    comeUp(g, 'GER', port);
    expect(besieged(g, port)).toBe(true);
    expect(prizeHolder(g, port, 'harbour')).toBe('GER');
    expect(harbourOpen(g, port)).toBe(false);
    expect(fedBySea(g, port)).toBe(false);
    expect(prizesAt(g, port.id)!.prizes.find((p) => p.kind === 'harbour')!.detail).toBe('the quays lie outside the town');
  });

  it('the harbour of a city stays with the town, whoever holds the country round it', () => {
    const g = playing('GER');
    const port = g.byName['Marseille'];
    clearField(g, port, g.byName['Lyon']);
    comeUp(g, 'GER', port);
    expect(besieged(g, port)).toBe(true);
    heldFor(g, port, 3);
    expect(prizeHolder(g, port, 'harbour')).toBe('FRA');
    expect(prizeUser(g, port, 'harbour')).toBe('FRA');
    expect(harbourOpen(g, port)).toBe(true);
    expect(prizeIncome(g, 'GER').harbour).toBe(0);
    expect(prizesAt(g, port.id)!.prizes.find((p) => p.kind === 'harbour')!.state).toBe('Held with the town, and still in use.');
  });
});

describe('wrecking a prize by order', () => {
  it('is refused in peacetime, without troops on the spot, and for what is not yours', () => {
    const g = playing('GER');
    const ruhr = g.byName['Ruhr'];
    expect(whyNotWreck(g, 'SWE', g.byName['Stockholm'].id, 'airfield')).toMatch(/peacetime/);
    clearField(g, ruhr, g.byName['Westphalia']);
    for (const c of [...garrisonOf(ruhr)]) relocate(g, c, g.byName['Westphalia']);
    expect(whyNotWreck(g, 'GER', ruhr.id, 'works')).toMatch(/None of your troops stand at the factory district of Ruhr/);
    expect(whyNotWreck(g, 'GER', g.byName['Warsaw'].id, 'works')).toMatch(/not in your hands/);
    expect(whyNotWreck(g, 'GER', g.byName['Lodz'].id, 'oil')).toMatch(/Lodz has no oilfield/);
    expect(orderWreckPrize(g, 'GER', ruhr.id, 'works')).toBe(false);
  });

  it('is a corps\' work for the month: the factory district stands idle for months, and the news says so', () => {
    const g = playing('GER');
    const ruhr = g.byName['Ruhr'];
    const c = addCorps(g, 'GER', 'Test corps', 'inf', 3, ruhr.id, {});
    const news = g.headlines.length;
    expect(whyNotWreckPrize(g, ruhr.id, 'works')).toBeNull();
    expect(orderWreck(g, ruhr.id, 'works')).toBe(true);
    expect(c.ap).toBe(0);
    expect(prizeOut(g, ruhr, 'works')).toBe(PRIZES.wreck.works);
    expect(workingFactories(g, ruhr)).toBe(ruhr.fac - worksOutside(ruhr));
    expect(prizeUser(g, ruhr, 'works')).toBeNull();
    expect(g.headlines.slice(news).some((h) => /German troops blow up the factory district of Ruhr: its 2 factories stand idle for 6 months/.test(h.text))).toBe(true);
    expect(whyNotWreckPrize(g, ruhr.id, 'works')).toMatch(/already wrecked/);
    expect(prizeMarks(g).find((m) => m.region === ruhr.id)).toEqual({ region: ruhr.id, by: null, burning: ['works'], seized: [] });
    // The same corps has no month left for the airfield.
    expect(whyNotWreckPrize(g, ruhr.id, 'airfield')).toMatch(/month's work/);
  });

  it('engineers do the work for choice', () => {
    const g = playing('GER');
    const ruhr = g.byName['Ruhr'];
    const inf = addCorps(g, 'GER', 'Test corps', 'inf', 3, ruhr.id, {});
    const eng = addCorps(g, 'GER', 'Test pioneers', 'eng', 1, ruhr.id, {});
    expect(orderWreck(g, ruhr.id, 'airfield')).toBe(true);
    expect(eng.ap).toBe(0);
    expect(inf.ap).toBeGreaterThan(0);
  });

  it('a wrecked airfield serves no aircraft, a wrecked harbour no ship, a burning oilfield gives no oil and a blown junction no trains', () => {
    const g = playing('GER');
    const hamburg = g.byName['Hamburg'], bremen = g.byName['Bremen'];
    const wing = g.airFleets.find((f) => f.nat === 'GER' && f.ship === undefined && f.base !== bremen.id)!;
    expect(wreckPrize(g, bremen, 'airfield', 3)).toBe(true);
    expect(wreckPrize(g, bremen, 'airfield', 3)).toBe(false);
    expect(whyNotBase(g, wing, bremen)).toMatch(/airfield there is wrecked: it will be 3 months/);
    expect(airfieldOpenTo(g, 'GER', bremen)).toBe(false);

    const c = addCorps(g, 'GER', 'Test corps', 'inf', 3, hamburg.id, {});
    const whole = whyNotShip(g, c, bremen);
    expect(wreckPrize(g, bremen, 'harbour', 5)).toBe(true);
    expect(whyNotShip(g, c, bremen)).toMatch(/harbour of Bremen is wrecked: no ship will use it for 5 months/);
    expect(whole === null || !/wrecked/.test(whole)).toBe(true);
    expect(harbourOpen(g, bremen)).toBe(false);
    expect(provinceYield(g, bremen).port).toBe(0);

    const maikop = g.byName['Maikop'];
    const full = fieldYield(g, maikop, 'SOV');
    expect(full).toBeGreaterThan(0);
    expect(wreckPrize(g, maikop, 'oil', 8)).toBe(true);
    expect(fieldYield(g, maikop, 'SOV')).toBe(0);
    expect(prizeOut(g, maikop, 'oil')).toBe(8);
    expect(provinceYield(g, maikop).oil).toBe(0);

    const berlin = g.byName['Berlin'];
    expect(wreckPrize(g, berlin, 'junction', 0)).toBe(true);
    expect(railCut(g, berlin.id)).toBe(true);
    expect(prizeOut(g, berlin, 'junction')).toBe(99);
    expect(prizesAt(g, berlin.id)!.prizes.find((p) => p.kind === 'junction')!.state).toBe('The line is cut until engineers relay it.');
    mendRail(g, berlin.id);
    expect(prizeOut(g, berlin, 'junction')).toBe(0);
  });

  it('a besieger can wreck what he holds outside the walls, but not the harbour inside them', () => {
    const { g, town, ger } = warsaw();
    ger.ap = 4;
    expect(whyNotWreckPrize(g, town.id, 'works')).toBeNull();
    expect(orderWreck(g, town.id, 'works')).toBe(true);
    expect(workingFactories(g, town)).toBe(town.fac - 1);
    const port = g.byName['Marseille'];
    clearField(g, port, g.byName['Lyon']);
    comeUp(g, 'GER', port);
    expect(whyNotWreckPrize(g, port.id, 'harbour')).toMatch(/not in your hands/);
  });

  it('time mends a wrecked prize, and engineers standing there mend it a month faster each month', () => {
    const g = playing('GER');
    const ruhr = g.byName['Ruhr'];
    wreckPrize(g, ruhr, 'works', 6);
    g.turn++;
    expect(prizeOut(g, ruhr, 'works')).toBe(5);
    mendPrizes(g, 'GER');
    expect(prizeOut(g, ruhr, 'works')).toBe(5);
    addCorps(g, 'GER', 'Test pioneers', 'eng', 1, ruhr.id, {});
    mendPrizes(g, 'GER');
    expect(prizeOut(g, ruhr, 'works')).toBe(4);
    g.turn += 4;
    expect(prizeOut(g, ruhr, 'works')).toBe(0);
    expect(workingFactories(g, ruhr)).toBe(ruhr.fac);
    expect(prizeUser(g, ruhr, 'works')).toBe('GER');
  });
});

describe('scorched earth', () => {
  it('the player burns nothing until he says so; each computer nation has a policy of its own', () => {
    const g = playing('GER');
    expect(scorchPolicyNow(g)).toBe('never');
    expect(scorchPolicy(g, 'SOV')).toBe('everywhere');
    expect(scorchPolicy(g, 'UK')).toBe('conquered');
    expect(scorchPolicy(g, 'FRA')).toBe('never');
    // The same nation in the player's hands starts with none, and a computer Germany burns only what it conquered.
    expect(scorchPolicy(playing('SOV'), 'SOV')).toBe('never');
    const h = playing('UK');
    expect(scorchPolicy(h, 'GER')).toBe('conquered');
    expect(willBurn(h, 'GER', h.byName['Ruhr'])).toBe(false);
    h.byName['Warsaw'].ctrl = 'GER';
    expect(willBurn(h, 'GER', h.byName['Warsaw'])).toBe(true);
    expect(willBurn(h, 'SOV', h.byName['Kiev'])).toBe(true);
    expect(willBurn(h, 'FRA', h.byName['Paris'])).toBe(false);
  });

  it('a policy covers conquered ground or all of it, and an order for one province overrides the policy', () => {
    const g = playing('GER');
    const ruhr = g.byName['Ruhr'], warsawR = g.byName['Warsaw'];
    warsawR.ctrl = 'GER';
    expect(orderScorchPolicy(g, 'conquered')).toBe(true);
    expect(willBurn(g, 'GER', ruhr)).toBe(false);
    expect(willBurn(g, 'GER', warsawR)).toBe(true);
    expect(orderBurn(g, warsawR.id, -1)).toBe(true);
    expect(willBurn(g, 'GER', warsawR)).toBe(false);
    expect(orderBurn(g, ruhr.id, 1)).toBe(true);
    expect(willBurn(g, 'GER', ruhr)).toBe(true);
    expect(prizesOf(g, ruhr.id)).toMatchObject({ order: 1, burns: true, policy: 'conquered' });
    expect(orderBurn(g, ruhr.id, 0)).toBe(true);
    expect(willBurn(g, 'GER', ruhr)).toBe(false);
    expect(orderScorchPolicy(g, 'everywhere')).toBe(true);
    expect(willBurn(g, 'GER', ruhr)).toBe(true);
    expect(whyNotBurnOrder(g, g.byName['Paris'].id)).toMatch(/not in your hands/);
    expect(orderBurn(g, g.byName['Paris'].id, 1)).toBe(false);
  });

  it('the chance is high with troops still at hand, low with none, and lower when fast troops took the ground', () => {
    const g = playing('GER');
    const town = g.byName['Warsaw'];
    clearField(g, town, g.byName['Lublin']);
    addCorps(g, 'GER', 'Test corps', 'inf', 3, town.id, {});
    // Polish troops next door in Lublin.
    expect(burnChance(g, town, 'GER', 'POL')).toBeCloseTo(PRIZES.burn.chance, 6);
    addCorps(g, 'GER', 'Test panzers', 'arm', 3, town.id, {});
    expect(burnChance(g, town, 'GER', 'POL')).toBeCloseTo(PRIZES.burn.chance - PRIZES.burn.fastCaptor, 6);
    // Nobody at hand: no garrison inside and no Polish corps in any neighbour.
    for (const j of town.nb) for (const u of [...g.regions[j].units]) if (u.nat === 'POL') relocate(g, u, g.byName['Lwow']);
    for (const u of [...garrisonOf(town)]) relocate(g, u, g.byName['Lwow']);
    expect(burnChance(g, town, 'GER', 'POL')).toBeCloseTo(PRIZES.burn.unguarded - PRIZES.burn.fastCaptor, 6);
  });

  it('an army driven off the field burns what lies outside the walls, about as often as the chance says', () => {
    const g = playing('GER');
    const town = g.byName['Warsaw'];
    clearField(g, town, g.byName['Lublin']);
    addCorps(g, 'GER', 'Test corps', 'inf', 3, town.id, {});
    // No order, no fire.
    expect(prizesOnLoss(g, town, 'GER', 'POL', 'outside')).toEqual([]);
    setBurnOrder(g, 'POL', town, 1);
    const p = burnChance(g, town, 'GER', 'POL');
    let works = 0, fields = 0;
    const N = 400, t0 = g.turn;
    for (let k = 0; k < N; k++) {
      g.turn = t0 + k;
      for (const key of Object.keys(g.flags)) if (key.startsWith('pzo:')) delete g.flags[key];
      const burnt = prizesOnLoss(g, town, 'GER', 'POL', 'outside');
      if (burnt.includes('works')) works++;
      if (burnt.includes('airfield')) fields++;
      // The railway keeps its own rule (rails.ts): a junction is cut by order, not by scorched earth.
      expect(burnt).not.toContain('junction');
    }
    expect(railCut(g, town.id)).toBe(false);
    for (const n of [works, fields]) {
      expect(n / N).toBeGreaterThan(p - 0.07);
      expect(n / N).toBeLessThan(p + 0.07);
    }
  });

  it('burned prizes are out for fewer months than wrecked ones, and the news names them', () => {
    const g = playing('GER');
    const town = g.byName['Warsaw'];
    clearField(g, town, g.byName['Lublin']);
    addCorps(g, 'GER', 'Test corps', 'inf', 3, town.id, {});
    setBurnOrder(g, 'POL', town, 1);
    let burnt: PrizeKind[] = [];
    const news = g.headlines.length;
    for (let k = 0; k < 40 && !burnt.includes('works'); k++) { g.turn++; burnt = prizesOnLoss(g, town, 'GER', 'POL', 'outside'); }
    expect(burnt).toContain('works');
    expect(prizeOut(g, town, 'works')).toBe(PRIZES.burn.months.works);
    expect(PRIZES.burn.months.works).toBeLessThan(PRIZES.wreck.works);
    expect(g.headlines.slice(news).some((h) => /^Scorched earth: Polish troops wreck .*the factory district.* at Warsaw as they are driven off\. Germany will get nothing from/.test(h.text))).toBe(true);
    // Nothing is burned twice.
    expect(prizesOnLoss(g, town, 'GER', 'POL', 'outside')).not.toContain('works');
  });

  it('when the field is lost only what lies outside burns; the harbour inside burns when the town falls', () => {
    const g = playing('GER');
    const port = g.byName['Marseille'];
    setBurnOrder(g, 'FRA', port, 1);
    clearField(g, port, g.byName['Lyon']);
    addCorps(g, 'GER', 'Test corps', 'inf', 3, port.id, {});
    let outside: PrizeKind[] = [], inside: PrizeKind[] = [];
    for (let k = 0; k < 60; k++) {
      g.turn++;
      for (const key of Object.keys(g.flags)) if (key.startsWith('pzo:')) delete g.flags[key];
      outside = outside.concat(prizesOnLoss(g, port, 'GER', 'FRA', 'outside'));
      for (const key of Object.keys(g.flags)) if (key.startsWith('pzo:')) delete g.flags[key];
      inside = inside.concat(prizesOnLoss(g, port, 'GER', 'FRA', 'inside'));
    }
    expect(outside).toContain('airfield');
    expect(outside).not.toContain('harbour');
    expect(inside).toContain('harbour');
    expect(inside).not.toContain('airfield');
  });

  it('the rules call it: a field lost burns at once, and a besieger driven off by a relieving army burns what he held', () => {
    // The Soviet Union burns everywhere: German troops come up to Kiev and find the works wrecked, in most wars.
    let burnedOnInvest = 0, burnedOnRelief = 0;
    for (let seed = 1; seed <= 12; seed++) {
      const g = playing('GER', seed);
      declareWar(g, 'GER', 'SOV');
      const kiev = g.byName['Kiev'];
      clearField(g, kiev, g.byName['Poltava']);
      const c = comeUp(g, 'GER', kiev);
      expect(besieged(g, kiev)).toBe(true);
      if (prizeOut(g, kiev, 'works') > 0) burnedOnInvest++;
      // A second province, whole: Germany holds its field for two months under a burn order, then is driven off.
      const tula = g.byName['Tula'];
      clearField(g, tula, g.byName['Moscow']);
      const d = comeUp(g, 'GER', tula);
      for (const key of Object.keys(g.flags)) if (key.startsWith(`pzo:${tula.id}`)) delete g.flags[key];
      heldFor(g, tula, 2);
      setBurnOrder(g, 'GER', tula, 1);
      relocate(g, d, g.byName['Orel']);
      lift(g, tula, 'SOV');
      if (prizeOut(g, tula, 'works') > 0 || prizeOut(g, tula, 'airfield') > 0) burnedOnRelief++;
      expect(besieged(g, tula)).toBe(false);
      expect(c.dead).toBeFalsy();
    }
    expect(burnedOnInvest).toBeGreaterThanOrEqual(6);
    expect(burnedOnRelief).toBeGreaterThanOrEqual(4);
  });

  it('a besieger who walks away, or who held the field less than a month, burns nothing', () => {
    const g = playing('GER');
    declareWar(g, 'GER', 'SOV');
    const tula = g.byName['Tula'];
    setScorchPolicy(g, 'SOV', 'never');
    clearField(g, tula, g.byName['Moscow']);
    const d = comeUp(g, 'GER', tula);
    setBurnOrder(g, 'GER', tula, 1);
    heldFor(g, tula, 3);
    relocate(g, d, g.byName['Orel']);
    lift(g, tula);
    for (const k of kindsAt(g, tula)) expect(prizeOut(g, tula, k)).toBe(0);
  });
});

describe('the player\'s orders', () => {
  it('are his only in his own month', () => {
    const g = playing('GER');
    endHuman(g);
    expect(humanUp(g)).toBe(false);
    const ruhr = g.byName['Ruhr'];
    expect(whyNotWreckPrize(g, ruhr.id, 'works')).toBe('It is not your month.');
    expect(orderWreck(g, ruhr.id, 'works')).toBe(false);
    expect(whyNotBurnOrder(g, ruhr.id)).toBe('It is not your month.');
    expect(orderBurn(g, ruhr.id, 1)).toBe(false);
    expect(orderScorchPolicy(g, 'everywhere')).toBe(false);
    expect(scorchPolicyNow(g)).toBe('never');
  });

  it('a province with nothing to burn takes no order', () => {
    const g = playing('GER');
    const bare = g.regions.find((r) => r.ctrl === 'GER' && kindsAt(g, r).length === 0)!;
    expect(prizesOf(g, bare.id)).toBeNull();
    expect(whyNotBurnOrder(g, bare.id)).toMatch(/nothing there to burn/);
  });
});

describe('a whole war with nobody at the table', () => {
  it('armies seize prizes round towns they have not taken, nations burn what they leave, and what was wrecked comes back', () => {
    const g = free(7);
    let seized = 0, burning = 0, scorched = 0, paid = 0, news = 0;
    const out = new Map<string, number>();
    let mended = 0;
    while (g.turn <= 40) {
      runTurnAI(g);
      for (const m of prizeMarks(g)) { if (m.seized.length) seized++; if (m.burning.length) burning++; }
      for (const h of g.headlines.slice(news)) if (/^Scorched earth/.test(h.text)) scorched++;
      news = g.headlines.length;
      for (const n of Object.values(g.nations)) paid += prizeIncome(g, n.id).total;
      for (const r of g.regions) for (const k of PRIZE_KINDS) {
        if (k === 'junction' || !hasPrize(g, r, k)) continue;
        const key = `${r.id}:${k}`, left = prizeOut(g, r, k);
        expect(left).toBeLessThanOrEqual(24);
        if (left > 0) out.set(key, left); else if (out.has(key)) { out.delete(key); mended++; }
      }
    }
    expect(seized).toBeGreaterThan(10);
    expect(paid).toBeGreaterThan(0);
    expect(burning).toBeGreaterThan(0);
    expect(scorched + burning).toBeGreaterThan(0);
    expect(mended).toBeGreaterThan(0);
  }, 120_000);
});
