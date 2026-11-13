// Oil and supplies (the grand campaign). Money builds an army, oil moves it, supplies keep it fighting: the great
// powers keep a stock of each, fed from home, from deals with their neighbours and along shipping lanes an enemy
// can raid or shut. When a stock runs dry the forces get only what came in that month. Every number is in
// data/campaign/resources.json. The historical scenarios know none of it.
import { describe, expect, it } from 'vitest';
import { aiProduction, income, ledger } from '../src/campaign/economy';
import { assess, checkCollapse, collapseScore, createGame, startPhase } from '../src/campaign/game';
import { fleetPower, laneLoss, zoneOf } from '../src/campaign/navy';
import {
  GOOD_NAME, RES, fieldYield, fuelAir, fuelFleet, fuelMet, fuelSlows, keepsStores, lanesTo, needOf, oilMonths, openStores, resRules, resourcesTick, seaLanes,
  shortAttack, shortDefence, shortOfOil, standing, stores, storesOf, supDrafts, supIncome, suppliesMet, willLost,
} from '../src/campaign/resources';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { TECH, staffPick, techOf, techRule } from '../src/campaign/tech';
import { depots, runTurnAI, shippingLanes } from '../src/campaign/turn';
import type { Fleet, Game } from '../src/campaign/types';

const grand = (human: string | null = null, seed = 3): Game => startOpen(createGame(seed, human));
const history = (human: string | null = null, seed = 3): Game => createGame(seed, human);
const POWERS = RES.nations;

function war(g: Game, a: string, b: string): void {
  if (!g.nations[a].atWar.includes(b)) g.nations[a].atWar.push(b);
  if (!g.nations[b].atWar.includes(a)) g.nations[b].atWar.push(a);
}
function peace(g: Game, a: string, b: string): void {
  g.nations[a].atWar = g.nations[a].atWar.filter((x) => x !== b);
  g.nations[b].atWar = g.nations[b].atWar.filter((x) => x !== a);
}
const lane = (g: Game, nat: string, id: string) => lanesTo(g, nat).find((l) => l.id === id)!;
const line = (g: Game, nat: string, good: 'oil' | 'sup', key: string) => stores(g, nat)![good].income.find((l) => l.key === key);
/** Sends a nation's fleets to the given sea in the given stance. */
function sail(g: Game, nat: string, sea: string, stance: Fleet['stance']): Fleet[] {
  const fs = g.fleets.filter((f) => f.nat === nat);
  for (const f of fs) { f.zone = zoneOf(g, sea); f.stance = stance; }
  return fs;
}

describe('the data', () => {
  it('names powers, seas, oilfields and sellers that exist', () => {
    const g = grand();
    for (const nat of POWERS) expect(g.nations[nat], nat).toBeTruthy();
    for (const name of Object.keys(RES.oil.fields)) if (name !== '_default') expect(g.byName[name]?.oil, name).toBe(true);
    for (const r of g.regions) if (r.oil) expect(RES.oil.fields[r.name], `${r.name} has a yield of its own`).toBeGreaterThan(0);
    const ids = new Set<string>();
    for (const l of RES.lanes) {
      expect(ids.has(l.id), l.id).toBe(false); ids.add(l.id);
      expect(POWERS, l.id).toContain(l.to);
      for (const z of [...l.zones, ...(l.draw ?? [])]) expect(zoneOf(g, z), `${l.id}: ${z}`).toBeGreaterThanOrEqual(0);
      for (const p of [...(l.past ?? []), ...(l.field ? [l.field] : []), ...(l.via ? [l.via] : [])]) expect(g.byName[p], `${l.id}: ${p}`).toBeTruthy();
      for (const n of [...(l.shutBy ?? []), ...(l.allies ?? []), ...(l.source ? [l.source] : [])]) expect(g.nations[n], `${l.id}: ${n}`).toBeTruthy();
      expect(l.oil + l.sup, l.id).toBeGreaterThan(0);
      expect(l.text.length, l.id).toBeGreaterThan(20);
    }
    for (const d of RES.deals) {
      expect(ids.has(d.id), d.id).toBe(false); ids.add(d.id);
      expect(POWERS, d.id).toContain(d.to);
      expect(!!d.field || !!d.seller || !!d.ore, d.id).toBe(true);
      if (d.field) expect(g.byName[d.field]?.oil, d.id).toBe(true);
      if (d.seller) expect(g.nations[d.seller], d.id).toBeTruthy();
    }
  });

  it('keeps every share and every penalty inside sane bounds', () => {
    const S = RES.oil.short, P = RES.sup.short;
    for (const x of [S.attack, S.defence, S.air, S.fleet, S.slowAt, P.fight, P.drafts, P.income, RES.oil.occupied, RES.sup.occupied, RES.sup.overseas]) {
      expect(x).toBeGreaterThan(0); expect(x).toBeLessThan(1);
    }
    for (const nat of POWERS) expect(RES.sup.ratio[nat] ?? RES.sup.ratio._default, nat).toBeGreaterThan(0.3);
    // Britain cannot feed or fuel itself: that is the point of the Atlantic lane.
    expect(RES.sup.ratio.UK).toBeLessThan(1);
    expect(RES.oil.home.UK).toBe(0);
    expect(RES.will.per * 12).toBeLessThan(1);
  });
});

describe('the historical scenarios', () => {
  it('know nothing of oil and supplies', () => {
    const g = history('GER');
    expect(resRules(g)).toBe(false);
    for (let k = 0; k < 6; k++) runTurnAI(g);
    for (const nat of POWERS) {
      expect(g.nations[nat].res, nat).toBeUndefined();
      expect(stores(g, nat), nat).toBeNull();
      expect(fuelMet(g, nat)).toBe(1);
      expect(suppliesMet(g, nat)).toBe(1);
      expect(willLost(g, nat)).toBe(0);
    }
    expect(seaLanes(g)).toEqual([]);
    expect(depots(g)).toBeNull();
    expect(shippingLanes(g)).toEqual([]);
    // Even a state left over from somewhere else changes nothing there.
    g.nations.GER.res = { oil: 0, sup: 0, oilMet: 0.2, supMet: 0.2, hunger: 9 };
    const tank = g.corps.find((c) => c.nat === 'GER' && c.kind === 'arm')!;
    expect(shortAttack(g, tank)).toBe(1);
    expect(fuelAir(g, 'GER')).toBe(1);
    expect(fuelFleet(g, 'GER')).toBe(1);
    expect(supIncome(g, 'GER')).toBe(1);
    expect(supDrafts(g, 'GER')).toBe(1);
    expect(willLost(g, 'GER')).toBe(0);
  }, 120000);
});

describe('the depots', () => {
  it('open with so many months of war in hand for each great power, and none for a small country', () => {
    const g = grand('GER');
    for (const nat of POWERS) {
      const st = g.nations[nat].res!;
      expect(st, nat).toBeTruthy();
      expect(st.oilMet).toBe(1); expect(st.supMet).toBe(1);
      expect(st.oil).toBeCloseTo((RES.oil.start[nat] ?? RES.oil.start._default) * needOf(g, nat, 'oil', true), 6);
      expect(st.sup).toBeCloseTo((RES.sup.start[nat] ?? RES.sup.start._default) * needOf(g, nat, 'sup', true), 6);
    }
    for (const nat of ['POL', 'SWE', 'ROM', 'HUN']) {
      expect(keepsStores(g, nat), nat).toBe(false);
      expect(g.nations[nat].res, nat).toBeUndefined();
      expect(stores(g, nat), nat).toBeNull();
      expect(fuelMet(g, nat)).toBe(1);
    }
  });

  it('gain what comes in and lose what goes out, month by month, up to what they can hold', () => {
    const g = grand('GER');
    const v = stores(g, 'GER')!;
    expect(v.oil.net).toBeCloseTo(v.oil.made - v.oil.need, 9);
    expect(v.oil.made).toBeCloseTo(v.oil.income.reduce((s, l) => s + l.amount, 0), 9);
    expect(v.oil.need).toBeCloseTo(v.oil.spend.reduce((s, l) => s + l.amount, 0), 9);
    const before = g.nations.GER.res!.oil;
    resourcesTick(g, 'GER');
    expect(g.nations.GER.res!.oil).toBeCloseTo(Math.min(v.oil.cap, before + v.oil.net), 6);
    // The tanks hold only so much.
    g.nations.GER.res!.oil = v.oil.cap * 3;
    resourcesTick(g, 'GER');
    expect(g.nations.GER.res!.oil).toBeCloseTo(stores(g, 'GER')!.oil.cap, 6);
  });

  it('count a need that is met in part when they run dry, and say so to the player', () => {
    const g = grand('UK');
    war(g, 'UK', 'GER');
    const st = g.nations.UK.res!;
    // Nothing gets through, and nothing is left in the tanks.
    sail(g, 'UK', 'nsea', 'port');
    for (const f of sail(g, 'GER', 'natl', 'raid')) f.ships.ss = 4000;
    st.oil = 0;
    const v = stores(g, 'UK')!;
    expect(v.oil.net).toBeLessThan(0);
    const news = g.headlines.length;
    resourcesTick(g, 'UK');
    expect(st.oil).toBe(0);
    expect(st.oilMet).toBeCloseTo(v.oil.made / v.oil.need, 6);
    expect(st.oilMet).toBeLessThan(0.6);
    expect(g.headlines.slice(news).some((h) => /oil has run out/.test(h.text))).toBe(true);
    const short = stores(g, 'UK')!.oil;
    expect(short.effects.length).toBeGreaterThanOrEqual(3);
    expect(short.effects.join(' ')).toMatch(/Tanks and lorried troops attack \d+% weaker/);
    expect(standing(short)).toMatch(/short|dry/);
    // The raiders go home and the tankers arrive: the need is met again and the news says so.
    for (const f of g.fleets.filter((x) => x.nat === 'GER')) f.stance = 'port';
    const again = g.headlines.length;
    resourcesTick(g, 'UK');
    expect(st.oilMet).toBe(1);
    expect(g.headlines.slice(again).some((h) => /oil enough again/.test(h.text))).toBe(true);
  });

  it('warn the player while there is still time', () => {
    const g = grand('UK');
    war(g, 'UK', 'GER');
    sail(g, 'UK', 'nsea', 'port');
    for (const f of sail(g, 'GER', 'natl', 'raid')) f.ships.ss = 600;
    const v = stores(g, 'UK')!;
    expect(v.oil.net).toBeLessThan(0);
    g.nations.UK.res!.oil = -v.oil.net * 3.5;
    const news = g.headlines.length;
    resourcesTick(g, 'UK');
    expect(g.headlines.slice(news).some((h) => /^Oil for \d more months?/.test(h.text))).toBe(true);
    expect(g.nations.UK.res!.oilMet).toBe(1);
    expect(oilMonths(g, 'UK')).toBeLessThan(RES.ai.armourAt);
    expect(shortOfOil(g, 'UK')).toBe(true);
  });

  it('are read by the turn API for the player and for any power', () => {
    const g = grand('UK');
    const mine = depots(g)!;
    expect(mine.nat).toBe('UK');
    expect(mine.oil.good).toBe('oil'); expect(mine.sup.good).toBe('sup');
    expect(mine.lanes.map((l) => l.id)).toEqual(['atlantic', 'gulf']);
    expect(depots(g, 'GER')!.nat).toBe('GER');
    expect(depots(g, 'POL')).toBeNull();
    expect(depots(grand(null))).toBeNull();
    expect(GOOD_NAME.oil).toBe('Oil');
  });
});

describe('oilfields', () => {
  it('give their owner the most, a conqueror less, and whoever holds them the oil', () => {
    const g = grand('GER');
    const baku = g.byName['Kirovabad'];
    expect(fieldYield(g, baku)).toBe(RES.oil.fields.Kirovabad);
    expect(fieldYield(g, baku, 'GER')).toBeCloseTo(RES.oil.fields.Kirovabad * RES.oil.occupied, 9);
    expect(fieldYield(g, g.byName['Berlin'])).toBe(0);
    const before = stores(g, 'SOV')!.oil.made, ger = stores(g, 'GER')!.oil.made;
    expect(line(g, 'SOV', 'oil', 'fields')!.count).toBe(3);
    baku.ctrl = 'GER';
    expect(stores(g, 'SOV')!.oil.made).toBeCloseTo(before - RES.oil.fields.Kirovabad, 6);
    expect(stores(g, 'GER')!.oil.made).toBeCloseTo(ger + RES.oil.fields.Kirovabad * RES.oil.occupied, 6);
    expect(line(g, 'GER', 'oil', 'taken')!.count).toBe(1);
  });
});

describe('deals overland', () => {
  it('bring Romanian oil to Germany until the field is in enemy hands', () => {
    const g = grand('GER');
    const d = lane(g, 'GER', 'ploiesti');
    expect(d.kind).toBe('land');
    expect(d.state).toBe('open');
    expect(d.oil).toBe(RES.deals.find((x) => x.id === 'ploiesti')!.oil);
    expect(line(g, 'GER', 'oil', 'lane:ploiesti')!.amount).toBe(d.oil);
    // Romania at war with Germany sells it nothing.
    war(g, 'GER', 'ROM');
    expect(lane(g, 'GER', 'ploiesti').state).toBe('cut');
    expect(lane(g, 'GER', 'ploiesti').oil).toBe(0);
    peace(g, 'GER', 'ROM');
    // The Red Army at Ploiesti: no oil for Germany, and the reason names who holds it.
    war(g, 'GER', 'SOV');
    g.byName['Ploiesti'].ctrl = 'SOV';
    const lost = lane(g, 'GER', 'ploiesti');
    expect(lost.state).toBe('cut');
    expect(lost.why).toMatch(/Ploiesti is in the hands of the Soviet Union/);
    // Taken by Germany itself, the field is its own production and the purchase lapses.
    g.byName['Ploiesti'].ctrl = 'GER';
    expect(lane(g, 'GER', 'ploiesti').state).toBe('idle');
    expect(line(g, 'GER', 'oil', 'taken')!.amount).toBeCloseTo(RES.oil.fields.Ploiesti * RES.oil.occupied, 9);
  });

  it('send Soviet oil and grain to Germany while the two are at peace, out of the Soviet books', () => {
    const g = grand('GER');
    const pact = RES.deals.find((x) => x.id === 'pact')!;
    expect(lane(g, 'GER', 'pact').state).toBe('open');
    expect(line(g, 'SOV', 'oil', 'sold:pact')!.amount).toBe(-pact.oil);
    expect(stores(g, 'SOV')!.sup.income.find((l) => l.key === 'sold:pact')!.amount).toBe(-pact.sup);
    const ger = stores(g, 'GER')!;
    war(g, 'GER', 'SOV');
    expect(lane(g, 'GER', 'pact').state).toBe('cut');
    expect(line(g, 'SOV', 'oil', 'sold:pact')).toBeUndefined();
    const after = stores(g, 'GER')!;
    expect(after.oil.made).toBeCloseTo(ger.oil.made - pact.oil, 6);
    expect(after.sup.made).toBeCloseTo(ger.sup.made - pact.sup, 6);
  });

  it('bring Swedish ore to Germany for as long as the ore traffic runs', () => {
    const g = grand('GER');
    expect(lane(g, 'GER', 'ore').state).toBe('open');
    expect(lane(g, 'GER', 'ore').sup).toBe(RES.deals.find((x) => x.id === 'ore')!.sup);
    war(g, 'GER', 'SWE');
    expect(lane(g, 'GER', 'ore').state).toBe('cut');
    expect(lane(g, 'GER', 'ore').sup).toBe(0);
  });
});

describe('shipping lanes', () => {
  it('bring Britain most of its oil and half its supplies across the Atlantic', () => {
    const g = grand('UK');
    const row = RES.lanes.find((l) => l.id === 'atlantic')!;
    const a = lane(g, 'UK', 'atlantic');
    expect(a.kind).toBe('sea');
    expect(a.zones.map((z) => g.seas[z].key)).toEqual(row.zones);
    expect(a.full.oil).toBeCloseTo(row.oil * needOf(g, 'UK', 'oil'), 6);
    expect(a.full.sup).toBeCloseTo(row.sup * needOf(g, 'UK', 'sup'), 6);
    expect(a.oil).toBeCloseTo(a.full.oil * a.flow, 9);
    // At full flow the lanes cover a little more than the need: the stock grows slowly if nothing is sunk.
    const full = RES.lanes.filter((l) => l.to === 'UK').reduce((s, l) => s + l.oil, 0);
    expect(full).toBeGreaterThan(1); expect(full).toBeLessThan(1.3);
  });

  it('lose cargoes to raiders and keep them with escorts', () => {
    const g = grand('UK');
    war(g, 'UK', 'GER');
    for (const f of g.fleets) f.stance = 'port';
    const calm = lane(g, 'UK', 'atlantic');
    expect(calm.flow).toBe(1);
    expect(calm.state).toBe('open');
    // Submarines in the North Atlantic.
    for (const f of sail(g, 'GER', 'natl', 'raid')) f.ships.ss += 40;
    const raided = lane(g, 'UK', 'atlantic');
    expect(raided.state).toBe('raided');
    expect(raided.raid).toBeGreaterThan(0);
    expect(raided.flow).toBeLessThan(0.9);
    expect(raided.why).toMatch(/Raiders are sinking \d+% of the cargoes/);
    expect(raided.flow).toBeCloseTo(1 - laneLoss(g, 'UK', raided.zones).loss, 9);
    expect(stores(g, 'UK')!.oil.made).toBeLessThan(stores(g, 'UK')!.oil.income.reduce((s, l) => s + (l.lane === 'atlantic' ? calm.oil : l.amount), 0));
    // The Royal Navy on patrol in the same seas.
    sail(g, 'UK', 'natl', 'patrol');
    const guarded = lane(g, 'UK', 'atlantic');
    expect(guarded.guard).toBeGreaterThan(raided.guard);
    expect(guarded.flow).toBeGreaterThan(raided.flow);
    // Raiders in a sea the lane does not cross sink nothing on it.
    sail(g, 'GER', 'baltic', 'raid');
    expect(lane(g, 'UK', 'atlantic').flow).toBe(1);
  });

  it('are shut by whoever holds the narrows, and by a blockade', () => {
    const g = grand('ITA');
    expect(lane(g, 'ITA', 'italian').state).toBe('open');
    war(g, 'ITA', 'UK');
    const shut = lane(g, 'ITA', 'italian');
    expect(shut.state).toBe('cut');
    expect(shut.flow).toBe(0);
    expect(shut.why).toMatch(/United Kingdom holds Gibraltar/);
    // The Rock in Italian hands opens the door again.
    g.byName['Gibraltar'].ctrl = 'ITA';
    expect(lane(g, 'ITA', 'italian').state).not.toBe('cut');
    // Germany is blockaded from the first day of its war with Britain, and trades freely without one.
    expect(lane(g, 'GER', 'german').state).toBe('cut');
    expect(lane(g, 'GER', 'german').why).toMatch(/blockade of the United Kingdom/);
    peace(g, 'GER', 'UK');
    expect(lane(g, 'GER', 'german').state).toBe('open');
    expect(lane(g, 'GER', 'german').oil).toBeGreaterThan(0);
  });

  it('run to the Soviet Union only once it fights beside the western Allies', () => {
    const g = grand('SOV');
    expect(lane(g, 'SOV', 'arctic').state).toBe('idle');
    expect(lane(g, 'SOV', 'arctic').oil).toBe(0);
    g.nations.SOV.align = 'allies';
    war(g, 'SOV', 'GER');
    const a = lane(g, 'SOV', 'arctic');
    expect(a.state).not.toBe('idle');
    expect(a.sup).toBeGreaterThan(0);
  });

  it('depend on the oilfield or the seller they come from', () => {
    const g = grand('UK');
    expect(lane(g, 'UK', 'gulf').state).toBe('open');
    war(g, 'UK', 'GER');
    g.byName['Iraq'].ctrl = 'GER';
    expect(lane(g, 'UK', 'gulf').state).toBe('cut');
    expect(lane(g, 'UK', 'gulf').why).toMatch(/Iraq is in the hands of Germany/);
  });

  it('are given to an interface with points to draw them through', () => {
    const g = grand('UK');
    const all = shippingLanes(g);
    expect(all.map((l) => l.id)).toEqual(expect.arrayContaining(['atlantic', 'french', 'italian', 'german', 'arctic']));
    for (const l of all) {
      expect(l.kind).toBe('sea');
      expect(l.path.length, l.id).toBeGreaterThanOrEqual(3);
      for (const [x, y] of l.path) { expect(Number.isFinite(x) && Number.isFinite(y), l.id).toBe(true); }
      expect(l.flow).toBeGreaterThanOrEqual(0); expect(l.flow).toBeLessThanOrEqual(1);
      expect(['open', 'raided', 'cut', 'idle']).toContain(l.state);
    }
    // The Atlantic lane ends at a British port on the Irish Sea.
    const end = all.find((l) => l.id === 'atlantic')!.path.at(-1)!;
    const port = g.regions.find((r) => r.x === end[0] && r.y === end[1])!;
    expect(port.owner).toBe('UK');
    expect(port.port).toBe(true);
  });
});

describe('a shortage of oil', () => {
  it('weakens tanks and lorried troops in attack and defence, and no one else', () => {
    const g = grand('GER');
    const tank = g.corps.find((c) => c.nat === 'GER' && c.kind === 'arm' && !c.dead)!;
    const inf = g.corps.find((c) => c.nat === 'GER' && c.kind === 'inf' && !c.dead)!;
    const lodz = g.byName['Lodz'];
    const before = assess(g, [tank], lodz);
    expect(before.lines.some((l) => l.label === 'Oil and supplies')).toBe(false);
    g.nations.GER.res!.oilMet = 0.4;
    expect(shortAttack(g, tank)).toBeCloseTo(1 - RES.oil.short.attack * 0.6, 9);
    expect(shortDefence(g, tank)).toBeCloseTo(1 - RES.oil.short.defence * 0.6, 9);
    expect(shortAttack(g, inf)).toBe(1);
    const after = assess(g, [tank], lodz);
    expect(after.odds).toBeLessThan(before.odds);
    const said = after.lines.find((l) => l.label === 'Oil and supplies')!;
    expect(said.value).toMatch(/Attackers have fuel for 40% of what their tanks and lorries need/);
    expect(said.effect).toMatch(/Attack cut to as little as 70%/);
    expect(assess(g, [inf], lodz).lines.some((l) => l.label === 'Oil and supplies')).toBe(false);
  });

  it('takes a move from fast troops when less than half their fuel arrives', () => {
    const g = grand('GER');
    const tank = g.corps.find((c) => c.nat === 'GER' && c.kind === 'arm' && !c.dead)!;
    const inf = g.corps.find((c) => c.nat === 'GER' && c.kind === 'inf' && !c.dead)!;
    g.nations.GER.res!.oilMet = 0.3;
    expect(fuelSlows(g, tank)).toBe(true);
    expect(fuelSlows(g, inf)).toBe(false);
    g.nations.GER.res!.oilMet = 0.7;
    expect(fuelSlows(g, tank)).toBe(false);
  });

  it('grounds aircraft and keeps the fleet in harbour in proportion', () => {
    const g = grand('UK');
    const f = g.fleets.find((x) => x.nat === 'UK')!;
    const power = fleetPower(g, f);
    g.nations.UK.res!.oilMet = 0.5;
    expect(fuelAir(g, 'UK')).toBeCloseTo(1 - RES.oil.short.air * 0.5, 9);
    expect(fuelFleet(g, 'UK')).toBeCloseTo(1 - RES.oil.short.fleet * 0.5, 9);
    expect(fleetPower(g, f)).toBeCloseTo(power * fuelFleet(g, 'UK'), 6);
    expect(fuelAir(g, 'GER')).toBe(1);
  });
});

describe('a shortage of supplies', () => {
  it('weakens every corps, slows the drafts and idles the factories', () => {
    const g = grand('GER');
    const inf = g.corps.find((c) => c.nat === 'GER' && c.kind === 'inf' && !c.dead)!;
    const earned = income(g, 'GER');
    g.nations.GER.res!.supMet = 0.5;
    expect(shortAttack(g, inf)).toBeCloseTo(1 - RES.sup.short.fight * 0.5, 9);
    expect(shortDefence(g, inf)).toBeCloseTo(1 - RES.sup.short.fight * 0.5, 9);
    expect(supDrafts(g, 'GER')).toBeCloseTo(1 - RES.sup.short.drafts * 0.5, 9);
    expect(income(g, 'GER')).toBeCloseTo(earned * (1 - RES.sup.short.income * 0.5), 6);
    const L = ledger(g, 'GER');
    expect(L.gross).toBeCloseTo(income(g, 'GER'), 6);
    const idle = L.changes.find((l) => l.key === 'supplies')!;
    expect(idle.amount).toBeLessThan(0);
    expect(idle.label).toMatch(/idle for want of supplies/);
    const said = assess(g, [inf], g.byName['Lodz']).lines.find((l) => l.label === 'Oil and supplies')!;
    expect(said.value).toMatch(/Attackers have 50% of the supplies they need/);
  });
});

describe('privation at home', () => {
  it('builds month by month while the depots are empty and mends when they are filled', () => {
    const g = grand('UK');
    const st = g.nations.UK.res!;
    const base = collapseScore(g, 'UK');
    expect(willLost(g, 'UK')).toBe(0);
    war(g, 'UK', 'GER');
    sail(g, 'UK', 'nsea', 'port');
    for (const f of sail(g, 'GER', 'natl', 'raid')) f.ships.ss = 4000;
    st.oil = 0; st.sup = 0;
    resourcesTick(g, 'UK');
    const one = st.hunger!;
    expect(one).toBeCloseTo(RES.will.sup * (1 - st.supMet) + RES.will.oil * (1 - st.oilMet), 6);
    resourcesTick(g, 'UK');
    expect(st.hunger!).toBeGreaterThan(one);
    expect(willLost(g, 'UK')).toBeCloseTo(st.hunger! * RES.will.per, 9);
    expect(collapseScore(g, 'UK')).toBeCloseTo(base + willLost(g, 'UK'), 9);
    expect(stores(g, 'UK')!.willLost).toBe(willLost(g, 'UK'));
    // The lanes open again: the people recover, slower than they suffered.
    for (const f of g.fleets.filter((x) => x.nat === 'GER')) f.stance = 'port';
    st.oil = 500; st.sup = 500;
    const worst = st.hunger!;
    resourcesTick(g, 'UK');
    expect(st.hunger ?? 0).toBeCloseTo(Math.max(0, worst - RES.will.heal), 9);
  });

  it('chokes a country that is starved for long enough', () => {
    const g = grand('GER');
    war(g, 'UK', 'GER');
    const st = g.nations.UK.res!;
    st.hunger = (g.nations.UK.resilience / RES.will.per) * 0.6;
    checkCollapse(g, 'UK', 'GER');
    expect(g.nations.UK.alive).toBe(true);
    st.hunger = g.nations.UK.resilience / RES.will.per + 0.5;
    checkCollapse(g, 'UK', 'GER');
    expect(g.nations.UK.alive).toBe(false);
  });
});

describe('synthetic fuel', () => {
  it('makes oil at home for every working factory once it is researched', () => {
    const g = grand('GER');
    const t = TECH.techs.find((x) => x.id === 'synthetics')!;
    expect(t.oil).toBeGreaterThan(0);
    expect(techRule(t)).toMatch(/Every working factory at home also makes [\d.]+ oil a month/);
    expect(line(g, 'GER', 'oil', 'tech')).toBeUndefined();
    const made = stores(g, 'GER')!.oil.made;
    techOf(g, 'GER')!.done.push('synthetics');
    const plants = line(g, 'GER', 'oil', 'tech')!;
    expect(plants.count).toBeGreaterThan(20);
    expect(plants.amount).toBeCloseTo(t.oil! * plants.count!, 9);
    expect(stores(g, 'GER')!.oil.made).toBeCloseTo(made + plants.amount, 6);
    expect(TECH.techs.find((x) => x.id === 'hydrogenation')!.needs).toEqual(['synthetics']);
  });

  it('comes first for a staff whose oil is running down, and last for a country that keeps no stock', () => {
    const g = grand('UK');
    const calm = staffPick(g, 'GER');
    expect(calm).not.toBe('synthetics');
    // The Soviet deliveries end and Ploiesti falls: Germany is living on its stock.
    war(g, 'GER', 'SOV');
    g.byName['Ploiesti'].ctrl = 'SOV';
    expect(oilMonths(g, 'GER')).toBeLessThan(RES.ai.synthAt);
    expect(staffPick(g, 'GER')).toBe('synthetics');
    // Sweden buys its oil abroad: fuel chemistry is the last thing its staff would pick.
    const swe = techOf(g, 'SWE')!;
    const open = TECH.techs.filter((t) => !swe.done.includes(t.id) && !swe.work.some((w) => w.id === t.id) && t.needs.every((x) => swe.done.includes(x)));
    expect(open.some((t) => t.id === 'synthetics')).toBe(true);
    expect(staffPick(g, 'SWE')).not.toBe('synthetics');
  });
});

describe("the computer's staffs", () => {
  it('raise no more tanks while the oil is running out', () => {
    const g = grand('UK', 5);
    const n = g.nations.GER;
    n.queue = []; n.pp = 96; n.corpsNo = 2;
    g.turn = 30;
    war(g, 'GER', 'SOV');
    g.byName['Ploiesti'].ctrl = 'SOV';
    n.res!.oil = 5;
    expect(shortOfOil(g, 'GER')).toBe(true);
    aiProduction(g, 'GER');
    expect(n.queue.length).toBeGreaterThan(0);
    expect(n.queue.some((b) => b.kind === 'arm')).toBe(false);
  });

  it('fight a whole war with stocks that stay sane', () => {
    const g = grand(null, 7);
    let shortSeen = false;
    for (let k = 0; k < 48; k++) {
      runTurnAI(g);
      for (const nat of POWERS) {
        const st = g.nations[nat].res;
        if (!st) continue;
        expect(Number.isFinite(st.oil) && st.oil >= 0, `${nat} oil turn ${g.turn}`).toBe(true);
        expect(Number.isFinite(st.sup) && st.sup >= 0, `${nat} supplies turn ${g.turn}`).toBe(true);
        expect(st.oilMet).toBeGreaterThanOrEqual(0); expect(st.oilMet).toBeLessThanOrEqual(1);
        expect(st.supMet).toBeGreaterThanOrEqual(0); expect(st.supMet).toBeLessThanOrEqual(1);
        if (st.oilMet < 1 || st.supMet < 1) shortSeen = true;
        if (g.nations[nat].alive) {
          const v = stores(g, nat)!;
          expect(v.oil.stock).toBeLessThanOrEqual(v.oil.cap * 1.5 + 1e-6);
          expect(v.lanes.every((l) => l.flow >= 0 && l.flow <= 1)).toBe(true);
        }
      }
    }
    // Small countries were never touched.
    for (const n of Object.values(g.nations)) if (!POWERS.includes(n.id)) expect(n.res, n.id).toBeUndefined();
    expect(typeof shortSeen).toBe('boolean');
  }, 240000);
});

describe('a month of the campaign', () => {
  it('settles the depots at the start of each nation\'s turn, before the money is counted', () => {
    const g = grand('GER');
    const st = g.nations.GER.res!;
    const v = stores(g, 'GER')!;
    const before = st.oil;
    startPhase(g, 'GER');
    // The month's forces are the ones the books were read for, give or take what the month itself delivered.
    expect(Math.abs(st.oil - Math.min(v.oil.cap, before + v.oil.net))).toBeLessThan(3);
  });

  it('opens the depots once, however often it is asked', () => {
    const g = grand('GER');
    const oil = g.nations.GER.res!.oil;
    openStores(g);
    expect(g.nations.GER.res!.oil).toBe(oil);
    expect(storesOf(g, 'GER')).toBe(g.nations.GER.res);
  });
});

describe('a saved campaign', () => {
  it('keeps its depots', () => {
    const g = grand('GER', 4);
    for (let k = 0; k < 6; k++) runTurnAI(g);
    g.nations.UK.res!.hunger = 1.5;
    const back = loadGame(saveGame(g))!;
    for (const nat of POWERS) expect(back.nations[nat].res, nat).toEqual(g.nations[nat].res);
    expect(willLost(back, 'UK')).toBe(willLost(g, 'UK'));
    expect(stores(back, 'GER')!.oil.made).toBeCloseTo(stores(g, 'GER')!.oil.made, 9);
  }, 120000);

  it('from before oil was counted is given depots the first time they are needed', () => {
    const g = grand('GER', 4);
    for (let k = 0; k < 6; k++) runTurnAI(g);
    const d = JSON.parse(saveGame(g));
    for (const n of Object.values(d.nations) as any[]) delete n.res;
    const old = loadGame(JSON.stringify(d))!;
    const v = stores(old, 'UK')!;
    expect(v.oil.stock).toBeGreaterThan(0);
    expect(v.oil.met).toBe(1);
    runTurnAI(old);
    expect(old.nations.GER.res).toBeTruthy();
  }, 120000);
});
