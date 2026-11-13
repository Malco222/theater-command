// The arsenal of the grand campaign: corps of every kind, aircraft by the wing, warships and troopships, and the
// works a province can be given: factories, shipyards, anti-aircraft guns and airfields. Heavy equipment needs
// industry. The historical scenarios offer what they always offered and nothing more.
import { describe, expect, it } from 'vitest';
import arsenal from '../data/campaign/arsenal.json';
import budget from '../data/campaign/budget.json';
import { AIR, airUpkeep, assessAir, crowding, forecastAir, hasAirfield } from '../src/campaign/air';
import { ARSENAL, aiArsenal, canStation, factoriesBuilt, isCity, offers, orderOffer, workLevel, yardLevel, type Offer } from '../src/campaign/arsenal';
import { techOf } from '../src/campaign/tech';
import { BUDGET, PRODUCTION, cancelBuild, canRaiseKind, deliver, income, isHome, ledger, needsOf, orderCorps, raiseSite, upkeepTotal } from '../src/campaign/economy';
import { DUG_IN, GUNS, addCorps, assess, createGame } from '../src/campaign/game';
import { fleetsOf, shipCount } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { arsenalOffers, orderArsenal, runTurnAI } from '../src/campaign/turn';
import { FORT_MUL, type Build, type Game, type Region } from '../src/campaign/types';

const grand = (seed = 3, human: string | null = null): Game => startOpen(createGame(seed, human));
const offer = (g: Game, nat: string, key: string, sel = -1): Offer => offers(g, nat, sel).find((o) => o.key === key)!;
const rich = (g: Game, nat: string, pp = 500): void => { g.nations[nat].pp = pp; };
/** Brings an order due at once and hands it over. */
const arrive = (g: Game, nat: string, b: Build | null): void => { expect(b).not.toBeNull(); b!.eta = g.turn; deliver(g, nat); };

describe('the arsenal data', () => {
  it('prices everything, and nothing is free', () => {
    for (const w of Object.values(arsenal.air)) { expect(w.cost).toBeGreaterThan(0); expect(w.months).toBeGreaterThan(0); expect(w.count).toBeGreaterThan(0); expect(w.blurb.length).toBeGreaterThan(20); }
    for (const s of Object.values(arsenal.ships)) { expect(s.cost).toBeGreaterThan(0); expect(s.months).toBeGreaterThan(0); expect(s.yard).toBeGreaterThanOrEqual(1); expect(s.yard).toBeLessThanOrEqual(3); }
    for (const k of ['yard', 'aa', 'field'] as const) { expect(arsenal.works[k].cost).toHaveLength(4); expect(arsenal.works[k].cost[1]).toBeGreaterThan(0); expect(arsenal.works[k].cost[3]).toBeGreaterThan(arsenal.works[k].cost[1]); }
    expect(arsenal.land.art.cost).toBeGreaterThan(0);
    expect((budget.upkeep.corps as Record<string, number>).art).toBeGreaterThan(0);
    // Capital ships take longest and need the biggest yards; a battleship outlasts a destroyer on the slip threefold.
    expect(arsenal.ships.bb.months).toBeGreaterThan(arsenal.ships.dd.months * 3);
    expect(arsenal.ships.bb.yard).toBe(3);
    expect(arsenal.ships.dd.yard).toBe(1);
    // Heavier equipment needs more industry.
    expect(arsenal.needs.inf).toBe(0);
    expect(arsenal.needs.art).toBeGreaterThanOrEqual(1);
    expect(arsenal.needs.arm).toBeGreaterThan(arsenal.needs.mot - 1);
  });

  it('puts every historical shipyard in a real port', () => {
    const g = grand();
    for (const [name, lv] of Object.entries(arsenal.yards)) {
      const r = g.byName[name];
      expect(r, name).toBeDefined();
      expect(r.port, name).toBe(true);
      expect(yardLevel(g, r), name).toBe(lv);
    }
    // Every nation with a navy at the start has somewhere to build a destroyer.
    for (const nat of new Set(g.fleets.filter((f) => f.base >= 0).map((f) => f.nat))) expect(offer(g, nat, 'ship:dd').at, nat).toBeGreaterThanOrEqual(0);
  });
});

describe('a historical scenario', () => {
  it('offers the four classic corps and fortifications, as it always did', () => {
    const g = createGame(3, 'GER');
    const keys = offers(g, 'GER', g.nations.GER.capital).map((o) => o.key).sort();
    expect(keys).toEqual(['corps:arm', 'corps:inf', 'corps:mot', 'corps:mtn', 'work:fort']);
    expect(offers(g, 'GER').map((o) => o.key).sort()).toEqual(['corps:arm', 'corps:inf', 'corps:mot', 'corps:mtn']);
    for (const o of offers(g, 'GER')) expect(o.upkeep).toBe(0);
    rich(g, 'GER');
    for (const key of ['air:ftr', 'ship:dd', 'work:aa', 'work:factory', 'corps:art']) expect(orderOffer(g, 'GER', key, g.nations.GER.capital), key).toBeNull();
    expect(orderCorps(g, 'GER', 'art', g.nations.GER.capital)).toBeNull();
  });

  it('raises tanks anywhere at home, with no factories asked for', () => {
    const g = createGame(3, 'GER');
    rich(g, 'GER');
    const bare = g.regions.find((r) => isHome(g, 'GER', r) && !r.overseas && r.fac === 0)!;
    expect(needsOf(g, 'arm')).toBe(0);
    expect(canRaiseKind(g, 'GER', bare, 'arm')).toBe(true);
    expect(orderCorps(g, 'GER', 'arm', bare.id)).not.toBeNull();
  });

  it('builds nothing from the arsenal in a year of history', () => {
    const g = createGame(2, null);
    for (let t = 0; t < 12; t++) runTurnAI(g);
    expect(Object.keys(g.flags).filter((k) => k.startsWith('wk:'))).toEqual([]);
    for (const n of Object.values(g.nations)) expect(n.queue.every((b) => b.what === 'corps' || b.what === 'fort')).toBe(true);
    expect(g.corps.some((c) => c.kind === 'art')).toBe(false);
    expect(g.airFleets.every((f) => f.extra === undefined)).toBe(true);
  });
});

describe('heavy equipment needs industry', () => {
  it('raises infantry anywhere at home, but tanks only where there are factories', () => {
    const g = grand();
    rich(g, 'GER');
    const bare = g.regions.find((r) => isHome(g, 'GER', r) && !r.overseas && r.fac === 0)!;
    const berlin = g.byName['Berlin'];
    expect(canRaiseKind(g, 'GER', bare, 'inf')).toBe(true);
    expect(canRaiseKind(g, 'GER', bare, 'mtn')).toBe(true);
    expect(canRaiseKind(g, 'GER', bare, 'art')).toBe(false);
    expect(canRaiseKind(g, 'GER', bare, 'mot')).toBe(false);
    expect(canRaiseKind(g, 'GER', bare, 'arm')).toBe(false);
    expect(canRaiseKind(g, 'GER', berlin, 'arm')).toBe(true);
    expect(orderCorps(g, 'GER', 'arm', bare.id)).toBeNull();
    // Asked for at a province without industry, the tanks form up where the factories are, and the offer says so.
    const o = offer(g, 'GER', 'corps:arm', bare.id);
    expect(o.at).toBe(berlin.id);
    expect(o.why).toBeNull();
    expect(o.note).toContain(bare.name);
    const b = orderOffer(g, 'GER', 'corps:arm', bare.id)!;
    expect(b.region).toBe(berlin.id);
    expect(raiseSite(g, 'GER', 'inf', bare.id)).toBe(bare.id);
  });

  it('a nation with no factory province cannot raise tanks at all, and is told what to build', () => {
    const g = grand();
    const nat = Object.keys(g.nations).find((id) => g.nations[id].alive && g.regions.some((r) => isHome(g, id, r)) && !g.regions.some((r) => isHome(g, id, r) && r.fac >= ARSENAL.needs.arm))!;
    expect(nat).toBeDefined();
    rich(g, nat);
    // The tank arm is researched: only the industry is missing.
    if (!techOf(g, nat)!.done.includes('tanks')) techOf(g, nat)!.done.push('tanks');
    const o = offer(g, nat, 'corps:arm');
    expect(o.at).toBe(-1);
    expect(o.why).toMatch(/factor/);
    expect(orderOffer(g, nat, 'corps:arm')).toBeNull();
    expect(offer(g, nat, 'corps:inf').why).toBeNull();
  });

  it('a new factory makes a province able to raise what it could not before, and pays into the treasury', () => {
    const g = grand(3, 'GER');
    rich(g, 'GER');
    const town = g.regions.find((r) => isHome(g, 'GER', r) && isCity(r) && r.fac === 1)!;
    expect(canRaiseKind(g, 'GER', town, 'arm')).toBe(false);
    const before = income(g, 'GER');
    const o = offer(g, 'GER', 'work:factory', town.id);
    expect(o.why).toBeNull();
    expect(o.cost).toBe(ARSENAL.works.factory.cost);
    const paid = g.nations.GER.pp;
    const b = orderOffer(g, 'GER', 'work:factory', town.id)!;
    expect(g.nations.GER.pp).toBeCloseTo(paid - o.cost, 9);
    expect(b.eta).toBe(g.turn + ARSENAL.works.factory.months);
    // The next one in the same city costs more.
    expect(offer(g, 'GER', 'work:factory', town.id).cost).toBe(ARSENAL.works.factory.cost + ARSENAL.works.factory.step);
    arrive(g, 'GER', b);
    expect(town.fac).toBe(2);
    expect(factoriesBuilt(g, town.id)).toBe(1);
    expect(canRaiseKind(g, 'GER', town, 'arm')).toBe(true);
    expect(income(g, 'GER')).toBeGreaterThan(before);
    expect(g.headlines.some((h) => h.text.includes('new factory') && h.text.includes(town.name))).toBe(true);
  });

  it('builds factories only in cities, only at home, and only so many', () => {
    const g = grand();
    rich(g, 'GER', 5000);
    const field = g.regions.find((r) => isHome(g, 'GER', r) && !isCity(r))!;
    expect(offer(g, 'GER', 'work:factory', field.id).why).toMatch(/cities/);
    expect(offer(g, 'GER', 'work:factory', g.byName['Paris'].id).why).not.toBeNull();
    expect(offer(g, 'GER', 'work:factory').why).toMatch(/Pick/);
    const berlin = g.byName['Berlin'];
    for (let k = 0; k < ARSENAL.works.factory.most.city; k++) expect(orderOffer(g, 'GER', 'work:factory', berlin.id), `factory ${k + 1}`).not.toBeNull();
    expect(offer(g, 'GER', 'work:factory', berlin.id).why).toMatch(/no more/);
    expect(orderOffer(g, 'GER', 'work:factory', berlin.id)).toBeNull();
  });
});

describe('artillery', () => {
  /** A Polish province on the German border, with a Polish garrison in it and German infantry next door. */
  const front = (g: Game): { target: Region; from: Region } => {
    const target = g.regions.find((r) => r.ctrl === 'POL' && r.units.some((c) => c.nat === 'POL') && r.nb.some((j) => g.regions[j].ctrl === 'GER' && g.regions[j].owner === 'GER'))!;
    const from = g.regions[target.nb.find((j) => g.regions[j].ctrl === 'GER' && g.regions[j].owner === 'GER')!];
    return { target, from };
  };

  it('is a corps like any other: ordered, paid for, kept, and named', () => {
    const g = grand(3, 'GER');
    rich(g, 'GER');
    const o = offer(g, 'GER', 'corps:art', g.byName['Berlin'].id);
    expect(o.group).toBe('army');
    expect(o.cost).toBe(arsenal.land.art.cost);
    expect(o.upkeep).toBeCloseTo(arsenal.land.art.divs * BUDGET.upkeep.corps.art, 9);
    const before = upkeepTotal(g, 'GER');
    const b = orderArsenal(g, 'corps:art', g.byName['Berlin'].id)!;
    expect(b.what).toBe('corps');
    expect(b.name).toMatch(/Artillery Corps$/);
    arrive(g, 'GER', b);
    const c = g.corps.find((x) => x.kind === 'art')!;
    expect(c.nat).toBe('GER');
    expect(c.reg).toBe(g.byName['Berlin'].id);
    expect(upkeepTotal(g, 'GER')).toBeGreaterThan(before);
  });

  it('breaks up fortifications and trenches in the attack', () => {
    const g = grand();
    const { target, from } = front(g);
    target.fort = { lv: 2, vs: ['GER'] };
    for (const d of target.units) d.dug = true;
    const inf = addCorps(g, 'GER', 'Test Infantry Corps', 'inf', 3, from.id, {});
    const guns = addCorps(g, 'GER', 'Test Artillery Corps', 'art', 2, from.id, {});
    const alone = assess(g, [inf], target), gunsOnly = assess(g, [guns], target), both = assess(g, [inf, guns], target);
    expect(alone.lines.some((l) => l.label === 'Artillery')).toBe(false);
    const line = both.lines.find((l) => l.label === 'Artillery')!;
    expect(line).toBeDefined();
    const soften = Math.min(GUNS.softenMost, GUNS.soften * 2);
    expect(line.effect).toContain(`${Math.round(soften * 100)}%`);
    // The infantry gets through works worth less: its share of the attack grows by exactly what the guns took off them.
    const fort = FORT_MUL[2], softened = 1 + (fort - 1) * (1 - soften);
    expect(both.A - gunsOnly.A).toBeCloseTo(alone.A * (fort / softened), 6);
    // And the trenches are worth less too.
    expect(both.D).toBeCloseTo(alone.D * ((1 + (DUG_IN - 1) * (1 - soften)) / DUG_IN), 6);
    expect(both.odds).toBeGreaterThan(alone.odds);
  });

  it('never takes more off the works than the most the guns can do', () => {
    const g = grand();
    const { target, from } = front(g);
    target.fort = { lv: 3, vs: ['GER'] };
    const cs = [addCorps(g, 'GER', 'Test Infantry Corps', 'inf', 3, from.id, {})];
    for (let k = 0; k < 6; k++) cs.push(addCorps(g, 'GER', `Test Artillery Corps ${k}`, 'art', 2, from.id, {}));
    const line = assess(g, cs, target).lines.find((l) => l.label === 'Artillery')!;
    expect(line.effect).toContain(`${Math.round(GUNS.softenMost * 100)}%`);
  });

  it('thickens a defence it stands in, and is weak caught alone', () => {
    const g = grand();
    const { target, from } = front(g);
    const att = [addCorps(g, 'GER', 'Test Infantry Corps', 'inf', 3, from.id, {})];
    const before = assess(g, att, target);
    const guns = addCorps(g, 'POL', 'Test Artillery Corps', 'art', 2, target.id, {});
    const after = assess(g, att, target);
    const thicken = Math.min(GUNS.thickenMost, GUNS.thicken * 2);
    expect(after.lines.find((l) => l.label === 'Artillery')!.effect).toContain(`${Math.round(thicken * 100)}%`);
    // Everyone else in the defence fights that much harder; the guns add little of their own.
    target.units.splice(target.units.indexOf(guns), 1);
    const own = (() => { const keep = target.units; target.units = [guns]; const d = assess(g, att, target).D; target.units = keep; return d; })();
    target.units.push(guns);
    expect(after.D - own).toBeCloseTo(before.D * (1 + thicken), 6);
    expect(own).toBeLessThan(before.D / Math.max(1, target.units.length - 1));
  });
});

describe('aircraft by the wing', () => {
  it('a wing joins the air fleet based where it is stationed, and stays on its strength', () => {
    const g = grand(3, 'GER');
    rich(g, 'GER');
    const f = g.airFleets.find((x) => x.nat === 'GER')!;
    const base = f.base, had = f.ftr, est = f.est.ftr;
    const o = offer(g, 'GER', 'air:ftr', base);
    expect(o.at).toBe(base);
    expect(o.note).toContain(f.name);
    expect(o.upkeep).toBeCloseTo(ARSENAL.air.ftr.count * BUDGET.upkeep.air.ftr, 9);
    const b = orderArsenal(g, 'air:ftr', base)!;
    expect(b.eta).toBe(g.turn + ARSENAL.air.ftr.months);
    arrive(g, 'GER', b);
    expect(f.ftr).toBe(had + ARSENAL.air.ftr.count);
    expect(f.est.ftr).toBe(est + ARSENAL.air.ftr.count);
    // The staff's monthly reshuffle does not send the new wing back to the reserve.
    airUpkeep(g, 'GER');
    expect(f.est.ftr).toBeGreaterThanOrEqual(est + ARSENAL.air.ftr.count);
    expect(f.ftr).toBeGreaterThanOrEqual(had + ARSENAL.air.ftr.count);
  });

  it('needs an aircraft industry', () => {
    const g = grand();
    const nat = Object.keys(g.nations).find((id) => g.nations[id].alive && g.regions.some((r) => isHome(g, id, r)) && !g.regions.some((r) => isHome(g, id, r) && r.fac >= ARSENAL.air.ftr.factories))!;
    rich(g, nat);
    const o = offer(g, nat, 'air:ftr');
    expect(o.at).toBe(-1);
    expect(o.why).toMatch(/aircraft industry/);
    expect(orderOffer(g, nat, 'air:ftr')).toBeNull();
  });

  it('needs airfields to be stationed on: build them and a new air group can be formed there', () => {
    const g = grand(3, 'GER');
    rich(g, 'GER');
    // An airfield is a place: most of the country has none.
    const r = g.regions.find((x) => isHome(g, 'GER', x) && !hasAirfield(g, x.id))!;
    expect(canStation(g, 'GER', r)).toBe(false);
    expect(offer(g, 'GER', 'air:bmr', r.id).at).not.toBe(r.id);
    arrive(g, 'GER', orderArsenal(g, 'work:field', r.id));
    expect(workLevel(g, 'field', r.id)).toBe(1);
    expect(canStation(g, 'GER', r)).toBe(true);
    const o = offer(g, 'GER', 'air:bmr', r.id);
    expect(o.at).toBe(r.id);
    expect(o.note).toMatch(/new air group/);
    const fleets = g.airFleets.length;
    arrive(g, 'GER', orderArsenal(g, 'air:bmr', r.id));
    expect(g.airFleets.length).toBe(fleets + 1);
    const f = g.airFleets[g.airFleets.length - 1];
    // It is a bomber group, numbered after the nation's others, under a command named for the place.
    expect(f.kind).toBe('bmr');
    expect(f.name).toMatch(/^Kampfgeschwader \d+$/);
    expect(f.parent).toBe(`${r.name} Air Command`);
    expect(f.base).toBe(r.id);
    expect(f.bmr).toBe(ARSENAL.air.bmr.count);
    airUpkeep(g, 'GER');
    expect(f.est.bmr).toBe(ARSENAL.air.bmr.count);
  });

  it('each level of airfields lets more aircraft fly from a province', () => {
    const g = grand();
    const f = g.airFleets.find((x) => x.nat === 'GER')!;
    f.ftr = AIR.airfields.holds; f.bmr = 500;
    for (const x of g.airFleets) if (x !== f && x.base === f.base) { x.ftr = 0; x.bmr = 0; }
    expect(crowding(g, f)).toBeCloseTo(AIR.airfields.holds / (AIR.airfields.holds + 500), 9);
    g.flags[`wk:field:${f.base}`] = 1;
    expect(crowding(g, f)).toBe(1);
  });
});

describe('anti-aircraft guns', () => {
  it('stop part of the bombing and bring down more of the bombers', () => {
    const g = grand();
    const warsaw = g.byName['Warsaw'];
    const f = g.airFleets.filter((x) => x.nat === 'GER' && x.bmr > 0).sort((a, b) => assessAir(g, [b], 'bomb', warsaw.id).B - assessAir(g, [a], 'bomb', warsaw.id).B)[0];
    // With no fighters to meet them the bombers' losses are the guns' alone, well short of the most a raid can lose.
    for (const x of g.airFleets) if (x.nat === 'POL') x.ftr = 0;
    const bare = assessAir(g, [f], 'bomb', warsaw.id);
    expect(bare.why).toBeNull();
    expect(bare.aa).toBe(0);
    expect(bare.lines.some((l) => l.label === 'Anti-aircraft guns')).toBe(false);
    const open = forecastAir(g, bare, 'bomb', warsaw.id);
    g.flags[`wk:aa:${warsaw.id}`] = 2;
    const gunned = assessAir(g, [f], 'bomb', warsaw.id);
    expect(gunned.aa).toBe(2);
    expect(gunned.bomberLoss).toBeCloseTo(bare.bomberLoss + 2 * ARSENAL.works.aa.flak, 9);
    expect(gunned.lines.find((l) => l.label === 'Anti-aircraft guns')!.effect).toContain(`${Math.round(2 * ARSENAL.works.aa.cut * 100)}%`);
    const shut = forecastAir(g, gunned, 'bomb', warsaw.id);
    expect(shut.cut).toBeLessThan(open.cut * (1 - 2 * ARSENAL.works.aa.cut) + 1e-9);
    expect(shut.nation).toBeLessThan(open.nation);
    // Guns do nothing for the fight for the sky itself.
    expect(assessAir(g, [f], 'superiority', warsaw.id).aa).toBe(0);
  });

  it('can be built on any ground a nation holds, level by level, and cost upkeep to man', () => {
    const g = grand(3, 'GER');
    rich(g, 'GER');
    const r = g.byName['Hamburg'];
    const before = upkeepTotal(g, 'GER');
    for (let lv = 1; lv <= 3; lv++) {
      const o = offer(g, 'GER', 'work:aa', r.id);
      expect(o.size).toBe(`level ${lv}`);
      expect(o.cost).toBe(ARSENAL.works.aa.cost[lv]);
      arrive(g, 'GER', orderArsenal(g, 'work:aa', r.id));
      expect(workLevel(g, 'aa', r.id)).toBe(lv);
    }
    expect(offer(g, 'GER', 'work:aa', r.id).why).toMatch(/as heavy as/);
    expect(orderArsenal(g, 'work:aa', r.id)).toBeNull();
    expect(upkeepTotal(g, 'GER') - before).toBeCloseTo(3 * ARSENAL.works.aa.upkeep, 9);
    const line = ledger(g, 'GER').upkeep.find((l) => l.key === 'works:aa')!;
    expect(line.count).toBe(3);
    expect(offer(g, 'GER', 'work:aa', g.byName['Paris'].id).why).toMatch(/not in your hands/);
  });

  it('a computer-run nation rings its industry with guns once it has been bombed', () => {
    const g = grand();
    rich(g, 'GER');
    aiArsenal(g, 'GER');
    expect(g.nations.GER.queue.some((b) => b.what === 'work')).toBe(false);
    g.raids.GER = { turn: g.turn, share: 0.05 };
    aiArsenal(g, 'GER');
    const b = g.nations.GER.queue.find((x) => x.what === 'work' && x.item === 'aa')!;
    expect(b).toBeDefined();
    const most = Math.max(...g.regions.filter((r) => isHome(g, 'GER', r)).map((r) => r.fac));
    expect(g.regions[b.region].fac).toBe(most);
    // One order at a time.
    aiArsenal(g, 'GER');
    expect(g.nations.GER.queue.filter((x) => x.what === 'work' && x.item === 'aa')).toHaveLength(1);
  });
});

describe('ships and shipyards', () => {
  it('lays a flotilla down in a home port and adds it to the fleet when it is finished', () => {
    const g = grand(3, 'GER');
    rich(g, 'GER');
    const o = offer(g, 'GER', 'ship:dd');
    expect(o.at).toBeGreaterThanOrEqual(0);
    expect(yardLevel(g, g.regions[o.at])).toBeGreaterThanOrEqual(1);
    expect(o.upkeep).toBeCloseTo(ARSENAL.ships.dd.count * BUDGET.upkeep.ships.dd, 9);
    const dd = (): number => fleetsOf(g, 'GER').reduce((s, f) => s + f.ships.dd, 0);
    const before = dd();
    const b = orderArsenal(g, 'ship:dd')!;
    expect(b.eta).toBe(g.turn + ARSENAL.ships.dd.months);
    arrive(g, 'GER', b);
    expect(dd()).toBe(before + ARSENAL.ships.dd.count);
    // Submarines go to the submarine arm, not to the battle fleet.
    const arm = fleetsOf(g, 'GER').sort((x, y) => y.ships.ss - x.ships.ss)[0], had = arm.ships.ss;
    arrive(g, 'GER', orderArsenal(g, 'ship:ss'));
    expect(arm.ships.ss).toBe(had + ARSENAL.ships.ss.count);
  });

  it('capital ships need a great yard: a small navy has to enlarge its yard first', () => {
    const g = grand();
    rich(g, 'POL', 2000);
    const port = g.byName['Danzig Corridor'];
    expect(yardLevel(g, port)).toBe(1);
    expect(offer(g, 'POL', 'ship:dd').why).toBeNull();
    expect(offer(g, 'POL', 'ship:ca').why).toMatch(/level 2/);
    expect(offer(g, 'POL', 'ship:bb').why).toMatch(/level 3/);
    expect(orderOffer(g, 'POL', 'ship:bb')).toBeNull();
    arrive(g, 'POL', orderOffer(g, 'POL', 'work:yard', port.id));
    expect(yardLevel(g, port)).toBe(2);
    expect(offer(g, 'POL', 'ship:ca').why).toBeNull();
    arrive(g, 'POL', orderOffer(g, 'POL', 'work:yard', port.id));
    expect(yardLevel(g, port)).toBe(3);
    expect(offer(g, 'POL', 'work:yard', port.id).why).toMatch(/as large as/);
    const ships = (): number => fleetsOf(g, 'POL').reduce((s, f) => s + shipCount(f.ships), 0);
    const before = ships();
    arrive(g, 'POL', orderOffer(g, 'POL', 'ship:bb'));
    expect(ships()).toBe(before + 1);
    expect(fleetsOf(g, 'POL').some((f) => f.ships.bb === 1)).toBe(true);
  });

  it('shipyards go only in ports, and a nation with no port builds no ships', () => {
    const g = grand();
    rich(g, 'GER');
    expect(offer(g, 'GER', 'work:yard', g.byName['Berlin'].id).why).toMatch(/no harbour/);
    const landlocked = Object.keys(g.nations).find((id) => g.nations[id].alive && g.regions.some((r) => isHome(g, id, r)) && !g.regions.some((r) => isHome(g, id, r) && r.port))!;
    expect(landlocked).toBeDefined();
    expect(offer(g, landlocked, 'ship:dd').at).toBe(-1);
  });

  it('troopships add to the sea lift, up to what the ports can handle', () => {
    const g = grand();
    rich(g, 'UK', 2000);
    const lift = g.nations.UK.sealift;
    for (let k = 0; k < ARSENAL.lift.most; k++) arrive(g, 'UK', orderOffer(g, 'UK', 'ship:tp'));
    expect(g.nations.UK.sealift).toBe(lift + ARSENAL.lift.most);
    expect(offer(g, 'UK', 'ship:tp').why).toMatch(/troopships/);
    expect(orderOffer(g, 'UK', 'ship:tp')).toBeNull();
  });
});

describe('ordering', () => {
  it('lists the same offers for both interfaces, each with a price, a place and a reason when it cannot be had', () => {
    const g = grand(3, 'GER');
    const list = arsenalOffers(g, g.byName['Hamburg'].id);
    expect(list.map((o) => o.key)).toEqual(['elite', 'corps:mtn', 'corps:inf', 'corps:art', 'corps:mot', 'corps:arm', 'corps:eng', 'para', 'air:ftr', 'air:bmr', 'ship:dd', 'ship:ss', 'ship:tp', 'ship:ca', 'ship:cv', 'ship:bb', 'work:factory', 'work:yard', 'work:aa', 'work:field', 'work:fort']);
    for (const o of list) {
      expect(o.label.length, o.key).toBeGreaterThan(3);
      expect(o.blurb.length, o.key).toBeGreaterThan(20);
      expect(o.cost, o.key).toBeGreaterThan(0);
      expect(o.months, o.key).toBeGreaterThan(0);
      expect(['army', 'air', 'navy', 'works']).toContain(o.group);
      // Germany begins the war with an empty treasury: everything it could build is out of reach for want of money alone.
      // (Its special formation, the heavy tanks, waits for its year: national abilities have their own tests.)
      // (And carriers wait on a technology: the tree has its own tests too.)
      if (o.why !== null && o.key !== 'work:yard' && o.key !== 'elite' && !o.tech) expect(o.poor, o.key).toBe(true);
      if (o.tech) expect(o.key).toBe('ship:cv');
    }
    expect(arsenalOffers(createGame(3, null))).toEqual([]);
  });

  it('refuses what the treasury cannot cover, and takes an order back for a full refund in the month it was placed', () => {
    const g = grand(3, 'GER');
    g.nations.GER.pp = ARSENAL.air.ftr.cost - 0.01;
    expect(offer(g, 'GER', 'air:ftr').poor).toBe(true);
    expect(orderArsenal(g, 'air:ftr')).toBeNull();
    g.nations.GER.pp = ARSENAL.air.ftr.cost;
    const b = orderArsenal(g, 'air:ftr')!;
    expect(g.nations.GER.pp).toBeCloseTo(0, 9);
    expect(cancelBuild(g, 'GER', b.id)).toBe(true);
    expect(g.nations.GER.pp).toBeCloseTo(ARSENAL.air.ftr.cost, 9);
    expect(g.nations.GER.queue).toHaveLength(0);
  });

  it('works lost to the enemy before they are finished are not built', () => {
    const g = grand(3, 'GER');
    rich(g, 'GER');
    const r = g.byName['Hamburg'];
    const b = orderArsenal(g, 'work:aa', r.id)!;
    r.ctrl = 'UK';
    arrive(g, 'GER', b);
    expect(workLevel(g, 'aa', r.id)).toBe(0);
  });

  it('survives a save: works, factories built, wings bought and everything still on order', () => {
    const g = grand(3, 'GER');
    rich(g, 'GER', 2000);
    const r = g.byName['Hamburg'], fac = r.fac;
    arrive(g, 'GER', orderArsenal(g, 'work:factory', r.id));
    arrive(g, 'GER', orderArsenal(g, 'work:aa', r.id));
    arrive(g, 'GER', orderArsenal(g, 'work:field', r.id));
    arrive(g, 'GER', orderArsenal(g, 'air:ftr', r.id));
    orderArsenal(g, 'ship:bb');
    orderArsenal(g, 'corps:art', r.id);
    const h = loadGame(saveGame(g))!;
    const hr = h.byName['Hamburg'];
    expect(hr.fac).toBe(fac + 1);
    expect(factoriesBuilt(h, hr.id)).toBe(1);
    expect(workLevel(h, 'aa', hr.id)).toBe(1);
    expect(workLevel(h, 'field', hr.id)).toBe(1);
    const f = h.airFleets.find((x) => x.base === hr.id && x.nat === 'GER')!;
    expect(f.extra).toEqual({ ftr: ARSENAL.air.ftr.count, bmr: 0 });
    expect(h.nations.GER.queue.map((b) => `${b.what}:${b.item ?? b.kind}`)).toEqual(['ship:bb', 'corps:art']);
    expect(income(h, 'GER')).toBeCloseTo(income(g, 'GER'), 9);
    expect(upkeepTotal(h, 'GER')).toBeCloseTo(upkeepTotal(g, 'GER'), 9);
    // And a second save of the loaded game is the same save.
    expect(saveGame(h)).toBe(saveGame(g));
  });

  it('a whole war with the computer playing every nation stays sound', () => {
    const g = grand(5, null);
    for (let t = 0; t < 30; t++) runTurnAI(g);
    for (const [id, n] of Object.entries(g.nations)) {
      expect(Number.isFinite(n.pp), id).toBe(true);
      for (const b of n.queue) expect(['corps', 'fort', 'work']).toContain(b.what);
    }
    for (const k of Object.keys(g.flags).filter((x) => x.startsWith('wk:aa:'))) expect(g.flags[k]).toBeLessThanOrEqual(3);
    expect(PRODUCTION.builds.art).toBeUndefined();
  });
});
