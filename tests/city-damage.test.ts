// Damaged cities (Brian, 20 November 2026): "when you click the cities there should be a repair button to press to
// repair the damaged buildings from artillery, bomb runs, and attacks. the production output and money are diminished
// the more it is damaged so repairing is important but costs money to do so". The grand campaign only, behind its
// switch, which the interfaces turn on when a new grand campaign begins. The history campaigns have none of it.
import { describe, expect, it } from 'vitest';
import data from '../data/campaign/damage.json';
import { fly } from '../src/campaign/air';
import { townMoney } from '../src/campaign/buildings';
import { DAMAGE, battlePoints, damageCity, damageOf, damageRules, keepOf, lastHit, raidPoints, setDamage, siegeGunPoints } from '../src/campaign/citydamage';
import { income, provinceYield } from '../src/campaign/economy';
import { createGame } from '../src/campaign/game';
import { aiRepair, cityHit, damagedTowns, lostPay, repairAllPlan, repairCity, repairCost, repairGain, repairPays, repairView, townsDamaged, whyNotRepair } from '../src/campaign/repairs';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { townAt } from '../src/campaign/towns';
import {
  beginHuman, cityDamage, cityDamageOn, damagedCities, endHuman, humanUp, orderRepair, orderRepairAll, orderUndoRepair, repairAllOutlook, runTurnAI, setCityDamage, stepAI,
} from '../src/campaign/turn';
import type { Game, Region } from '../src/campaign/types';

/** A grand war nobody plays, with damaged cities on (or off). */
const grand = (seed = 3, on = true): Game => { const g = startOpen(createGame(seed, null)); if (on) setCityDamage(g, true); return g; };

/** A grand campaign with this country at the table, damaged cities on, at its first month of orders. */
function playing(nat: string, seed = 1): Game {
  const g = startOpen(createGame(seed, nat));
  setCityDamage(g, true);
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

const pay = (g: Game, r: Region): number => { const y = provinceYield(g, r); return y.fac + y.land + y.port + y.capital; };

describe('the data', () => {
  it('never wipes a town out, and prices a repair at a few months of what the damage costs', () => {
    expect(data.most).toBeGreaterThan(0);
    expect(data.most).toBeLessThan(100);
    expect(data.repair.months).toBeGreaterThan(1);
    expect(data.repair.least).toBeGreaterThan(0);
    expect(data.ai.share).toBeGreaterThan(0);
    expect(data.ai.share).toBeLessThan(1);
    // A storm does more than a fight thrown back, and a fight in the streets more than one in the fields round the town.
    expect(data.battle.storm).toBeGreaterThan(data.battle.repulsed);
    expect(data.battle.streets).toBeGreaterThan(data.battle.field);
  });
});

describe('the switch', () => {
  it('is off when a grand campaign is set up by the rules alone, and turned on by the interfaces', () => {
    const g = startOpen(createGame(3, null));
    expect(cityDamageOn(g)).toBe(false);
    expect(setCityDamage(g, true)).toBe(true);
    expect(cityDamageOn(g)).toBe(true);
    expect(setCityDamage(g, false)).toBe(true);
    expect(cityDamageOn(g)).toBe(false);
  });

  it('a historical scenario never has it: a flag left by mistake does nothing, and nothing can be damaged or repaired', () => {
    const g = createGame(4, 'GER');
    expect(setCityDamage(g, true)).toBe(false);
    expect(damageRules(g)).toBe(false);
    const berlin = g.byName['Berlin'];
    const before = provinceYield(g, berlin), earned = income(g, 'GER');
    g.flags.damage = 1; g.flags['dmg:any'] = 1; g.flags[`dmg:${berlin.id}`] = 50;
    expect(damageOf(g, berlin)).toBe(0);
    expect(provinceYield(g, berlin)).toEqual(before);
    expect(income(g, 'GER')).toBe(earned);
    expect(cityHit(g, berlin, 'raid', 600)).toBeNull();
    expect(cityDamage(g, berlin.id)).toBeNull();
    expect(damagedCities(g)).toEqual([]);
    expect(orderRepair(g, berlin.id)).toBe(false);
    expect(whyNotRepair(g, 'GER', berlin.id)).toMatch(/grand campaign/);
  });

  it('untouched means unchanged: switched on with no town hit, every nation earns exactly what it did', () => {
    const off = grand(3, false), on = grand(3, true);
    for (const nat of Object.keys(off.nations)) expect(income(on, nat), nat).toBe(income(off, nat));
    // And switched off, damage on the books neither costs nor shows anything.
    const r = off.byName['Berlin'];
    off.flags['dmg:any'] = 1; off.flags[`dmg:${r.id}`] = 40;
    expect(damageOf(off, r)).toBe(0);
    expect(income(off, 'GER')).toBe(income(on, 'GER'));
  });
});

describe('what damage costs', () => {
  it('a town in ruins pays that much less from its factories, land, harbour and seat of government; its oil is not the town', () => {
    const g = grand();
    const berlin = g.byName['Berlin'];
    const whole = provinceYield(g, berlin), earned = income(g, 'GER');
    setDamage(g, berlin, 40);
    expect(damageOf(g, berlin)).toBeCloseTo(0.4, 9);
    expect(keepOf(g, berlin)).toBeCloseTo(0.6, 9);
    const y = provinceYield(g, berlin);
    expect(y.damage).toBeCloseTo(0.4, 9);
    expect(y.fac).toBeCloseTo(whole.fac * 0.6, 9);
    expect(y.land).toBeCloseTo(whole.land * 0.6, 9);
    expect(y.port).toBeCloseTo(whole.port * 0.6, 9);
    expect(y.capital).toBeCloseTo(whole.capital * 0.6, 9);
    expect(y.oil).toBe(whole.oil);
    expect(income(g, 'GER')).toBeLessThan(earned);
    // What the damage costs at the holder's own rates is the share lost of what it pays standing whole.
    expect(lostPay(g, berlin)).toBeCloseTo((whole.fac + whole.land + whole.port + whole.capital) * 0.4 + townMoney(g, berlin) * 0.4, 9);
    // The town's worth on the map is what it pays when all is well; the damage is shown beside it.
    expect(townAt(g, berlin.id)!.damage).toBeCloseTo(0.4, 9);
    expect(townAt(g, g.byName['Paris'].id)!.damage).toBeUndefined();
  });

  it('the more of it lies in ruins the less it pays, and a fifth of it always works', () => {
    const g = grand();
    const r = g.byName['Berlin'];
    let last = pay(g, r);
    for (const pct of [10, 30, 60, 80]) { setDamage(g, r, pct); const now = pay(g, r); expect(now).toBeLessThan(last); last = now; }
    expect(damageCity(g, r, 50)).toBe(0);
    expect(damageOf(g, r)).toBeCloseTo(DAMAGE.most / 100, 9);
  });
});

describe('what does the damage', () => {
  it('bombs by day and by night, shells, a besieger\'s guns and battles, each by its weight', () => {
    expect(raidPoints(DAMAGE.raid.full, 0, false)).toBeCloseTo(DAMAGE.raid.points, 9);
    expect(raidPoints(DAMAGE.raid.full / 2, 0, false)).toBeCloseTo(DAMAGE.raid.points / 2, 9);
    expect(raidPoints(DAMAGE.raid.full / 2, 0, true)).toBeCloseTo(DAMAGE.raid.points, 9);
    expect(raidPoints(DAMAGE.raid.full, 0.5, false)).toBeCloseTo(DAMAGE.raid.points / 2, 9);
    expect(siegeGunPoints(100)).toBe(DAMAGE.guns.most);
    expect(battlePoints('storm', 40, 0)).toBeGreaterThan(battlePoints('field', 40, 0));
    expect(battlePoints('field', 1, 0)).toBeCloseTo(DAMAGE.battle.field * 0.25, 9);
    expect(battlePoints('streets', 40, 10)).toBeCloseTo(DAMAGE.battle.streets + DAMAGE.battle.gunsMost, 9);
  });

  it('a hit leaves its mark: how much, where it stands, the month, and a sentence for the report', () => {
    const g = grand();
    const r = g.byName['Ruhr'];
    const hit = cityHit(g, r, 'raid', DAMAGE.raid.full)!;
    expect(hit.added).toBeCloseTo(DAMAGE.raid.points, 9);
    expect(hit.now).toBeCloseTo(DAMAGE.raid.points, 9);
    // The air staffs weigh a raid as they always did (damage.json 'worth' is 0): the damage follows the raids they fly.
    expect(hit.worth).toBe(0);
    expect(hit.words).toMatch(/in ruins/);
    expect(hit.words).not.toMatch(/output/);
    expect(lastHit(g, r)).toBe(g.turn);
    expect(townsDamaged(g).map((t) => t.region)).toEqual([r.id]);
    // Off, nothing at all.
    const off = grand(3, false);
    expect(cityHit(off, off.byName['Ruhr'], 'raid', DAMAGE.raid.full)).toBeNull();
    expect(Object.keys(off.flags).some((k) => k.startsWith('dmg'))).toBe(false);
  });

  it('a real night raid on the Ruhr leaves its town damaged and says so', () => {
    const g = grand(7);
    g.turn = 12;
    const r = g.byName['Ruhr'];
    const fs = g.airFleets.filter((f) => f.nat === 'UK' && f.bmr > 0 && f.ship === undefined);
    const rep = fly(g, 'UK', 'night', r.id, fs)!;
    expect(rep).not.toBeNull();
    expect(damageOf(g, r)).toBeGreaterThan(0);
    expect(rep.city?.region).toBe(r.id);
    expect(rep.result).toMatch(/in ruins/);
    // The same raid with the switch off leaves no mark.
    const off = grand(7, false);
    off.turn = 12;
    const rep2 = fly(off, 'UK', 'night', r.id, off.airFleets.filter((f) => f.nat === 'UK' && f.bmr > 0 && f.ship === undefined))!;
    expect(rep2.city).toBeUndefined();
    expect(damageOf(off, off.byName['Ruhr'])).toBe(0);
  });

  it('a war fought with the switch on damages the towns it is fought over, and the computer repairs its own', () => {
    const g = grand(3);
    // Damage is cleared only by a repair, so a town damaged one month and whole the next was repaired.
    let hurt = 0, repaired = 0, was = new Set<number>();
    for (let k = 0; k < 8; k++) {
      runTurnAI(g);
      const now = new Set(townsDamaged(g).map((t) => t.region));
      hurt = Math.max(hurt, now.size);
      for (const id of was) if (!now.has(id)) repaired++;
      was = now;
    }
    expect(hurt).toBeGreaterThan(0);
    expect(repaired).toBeGreaterThan(0);
    // The same seed plays the same war, damage and repairs and all, and a save carries on as the live game does.
    const again = grand(3);
    for (let k = 0; k < 8; k++) runTurnAI(again);
    expect(saveGame(again)).toBe(saveGame(g));
    const loaded = loadGame(saveGame(g))!;
    runTurnAI(g); runTurnAI(loaded);
    expect(saveGame(loaded)).toBe(saveGame(g));
  }, 120_000);
});

describe('repairing', () => {
  it('shows the player what the damage costs, the price, and how soon it pays back, without writing a thing', () => {
    const g = playing('GER');
    const berlin = g.byName['Berlin'];
    damageCity(g, berlin, 30);
    const was = saveGame(g);
    const v = cityDamage(g, berlin.id)!;
    expect(v.town).toBe('Berlin');
    expect(v.pct).toBe(30);
    expect(v.lost).toBeGreaterThan(0);
    expect(v.cost).toBeCloseTo(repairCost(g, berlin), 9);
    expect(v.months).toBeCloseTo(Math.round((v.cost / v.lost) * 10) / 10, 9);
    expect(v.hit).toBe(true);
    expect(v.why).toBeNull();
    expect(damagedCities(g).map((t) => t.region)).toEqual([berlin.id]);
    expect(repairAllOutlook(g)!.pick.map((t) => t.region)).toEqual([berlin.id]);
    expect(saveGame(g)).toBe(was);
    // A town standing whole shows nothing.
    expect(cityDamage(g, g.byName['Hamburg'].id)).toBeNull();
  });

  it('a repair is paid at once and done at once: the town pays in full again, and can be taken back in the same month', () => {
    const g = playing('GER');
    const berlin = g.byName['Berlin'];
    const whole = income(g, 'GER');
    damageCity(g, berlin, 30);
    const hurt = income(g, 'GER');
    const v = cityDamage(g, berlin.id)!;
    expect(v.lost).toBeCloseTo(whole - hurt, 9);
    expect(repairGain(g, 'GER', [berlin.id])).toBeCloseTo(whole - hurt, 9);
    const pp = g.nations.GER.pp;
    expect(orderRepair(g, berlin.id)).toBe(true);
    expect(g.nations.GER.pp).toBeCloseTo(pp - v.cost, 9);
    expect(damageOf(g, berlin)).toBe(0);
    expect(income(g, 'GER')).toBeCloseTo(whole, 9);
    expect(cityDamage(g, berlin.id)!.undo).toBe(true);
    // Taken back: the money and the ruins as they were.
    expect(orderUndoRepair(g, berlin.id)).toBe(true);
    expect(g.nations.GER.pp).toBeCloseTo(pp, 9);
    expect(damageOf(g, berlin)).toBeCloseTo(0.3, 9);
    expect(lastHit(g, berlin)).toBe(g.turn);
    // Repaired again, the month ends and it can no longer be taken back.
    expect(orderRepair(g, berlin.id)).toBe(true);
    month(g);
    expect(cityDamage(g, berlin.id)).toBeNull();
    expect(orderUndoRepair(g, berlin.id)).toBe(false);
  }, 60_000);

  it('says why not: an empty treasury, a town that is not his, one standing whole', () => {
    const g = playing('GER');
    const berlin = g.byName['Berlin'], paris = g.byName['Paris'];
    damageCity(g, berlin, 30);
    damageCity(g, paris, 30);
    expect(cityDamage(g, paris.id)!.why).toMatch(/not yours/);
    expect(orderRepair(g, paris.id)).toBe(false);
    expect(whyNotRepair(g, 'GER', g.byName['Hamburg'].id)).toMatch(/undamaged/);
    g.nations.GER.pp = 0;
    const v = cityDamage(g, berlin.id)!;
    expect(v.poor).toBe(true);
    expect(v.why).toMatch(/treasury/);
    expect(orderRepair(g, berlin.id)).toBe(false);
    expect(damageOf(g, berlin)).toBeCloseTo(0.3, 9);
  }, 60_000);

  it('repair all: the towns whose damage costs most first, as many as the treasury covers', () => {
    const g = playing('GER');
    const towns = g.regions.filter((r) => r.ctrl === 'GER' && r.owner === 'GER' && r.fac > 0).slice(0, 4);
    towns.forEach((r, i) => damageCity(g, r, 20 + i * 10));
    const plan = repairAllOutlook(g)!;
    expect(plan.towns).toHaveLength(towns.length);
    for (let i = 1; i < plan.towns.length; i++) expect(plan.towns[i].lost).toBeLessThanOrEqual(plan.towns[i - 1].lost);
    // A repair is priced by what the damage costs, so each pays back in about the same months.
    for (const t of plan.towns) expect(t.months).toBeCloseTo(plan.towns[0].months, 0);
    // With money for only the first, only the first.
    const first = plan.towns[0];
    g.nations.GER.pp = first.cost + 0.01;
    const short = repairAllOutlook(g)!;
    expect(short.pick.map((t) => t.region)).toEqual([first.region]);
    expect(short.short).toBe(towns.length - 1);
    g.nations.GER.pp = 10_000;
    const all = repairAllOutlook(g)!;
    const before = income(g, 'GER');
    expect(orderRepairAll(g)).toHaveLength(towns.length);
    expect(g.nations.GER.pp).toBeCloseTo(10_000 - all.cost, 6);
    expect(income(g, 'GER') - before).toBeCloseTo(all.gain, 6);
    expect(damagedCities(g)).toEqual([]);
  }, 60_000);

  it('a computer nation repairs out of its purse, never on the front line or soon after a hit', () => {
    const g = grand();
    const n = g.nations.GER;
    const berlin = g.byName['Berlin'];
    expect(berlin.nb.some((j) => n.atWar.includes(g.regions[j].ctrl))).toBe(false);
    damageCity(g, berlin, 30);
    // Hit this month, or last: left alone.
    n.pp = 1000;
    expect(aiRepair(g, 'GER')).toEqual([]);
    g.turn++;
    expect(aiRepair(g, 'GER')).toEqual([]);
    expect(damageOf(g, berlin)).toBeCloseTo(0.3, 9);
    g.turn++;
    let done: string[] = [];
    for (let k = 0; k < 24 && !done.length; k++) done = aiRepair(g, 'GER');
    expect(done).toEqual(['Berlin']);
    expect(damageOf(g, berlin)).toBe(0);
    expect(n.pp).toBeLessThan(1000);
    // A town beside ground its enemies hold is on the front line: it waits for the front to move.
    const front = g.regions.find((r) => r.ctrl === 'GER' && r.fac > 0 && r.nb.some((j) => n.atWar.includes(g.regions[j].ctrl)))!;
    damageCity(g, front, 30);
    g.turn += 2;
    for (let k = 0; k < 12; k++) aiRepair(g, 'GER');
    expect(damageOf(g, front)).toBeCloseTo(0.3, 9);
    // A poor nation keeps its reserve.
    const p = grand();
    damageCity(p, p.byName['Berlin'], 30);
    p.turn += 2;
    p.nations.GER.pp = DAMAGE.ai.reserve / 2;
    for (let k = 0; k < 12; k++) aiRepair(p, 'GER');
    expect(damageOf(p, p.byName['Berlin'])).toBeCloseTo(0.3, 9);
  });

  it('a computer nation buys only a repair that pays for itself in the price\'s months, an occupier by its own share', () => {
    const g = grand();
    const n = g.nations.GER;
    n.pp = 1000;
    // A scratch on Berlin costs it so little that the floor sets the price: the repair would take twice the months to repay.
    const berlin = g.byName['Berlin'];
    damageCity(g, berlin, 1);
    expect(repairCost(g, berlin)).toBe(DAMAGE.repair.least);
    expect(repairCost(g, berlin)).toBeGreaterThan(lostPay(g, berlin) * DAMAGE.repair.months);
    expect(repairPays(repairCost(g, berlin), lostPay(g, berlin))).toBe(false);
    g.turn += 2;
    for (let k = 0; k < 12; k++) expect(aiRepair(g, 'GER')).toEqual([]);
    expect(damageOf(g, berlin)).toBeCloseTo(0.01, 9);
    // Amsterdam at 4% would repay its own country in time, but an occupier takes only its share of the town's pay.
    const holland = g.byName['Holland'];
    holland.ctrl = 'GER';
    damageCity(g, holland, 4);
    expect(repairPays(repairCost(g, holland, 'NED'), lostPay(g, holland, 'NED'))).toBe(true);
    expect(lostPay(g, holland, 'GER')).toBeLessThan(lostPay(g, holland, 'NED'));
    expect(repairPays(repairCost(g, holland, 'GER'), lostPay(g, holland, 'GER'))).toBe(false);
    g.turn += 2;
    for (let k = 0; k < 12; k++) expect(aiRepair(g, 'GER')).toEqual([]);
    // Worse damage pays its way, the occupier's share and all.
    damageCity(g, holland, 26);
    g.turn += 2;
    expect(repairPays(repairCost(g, holland, 'GER'), lostPay(g, holland, 'GER'))).toBe(true);
    let done: string[] = [];
    for (let k = 0; k < 24 && !done.length; k++) done = aiRepair(g, 'GER');
    expect(done).toEqual(['Amsterdam']);
    expect(damageOf(g, berlin)).toBeCloseTo(0.01, 9);
  });

  it('saves and loads with the damage, the repair and its taking back', () => {
    const g = playing('GER');
    const berlin = g.byName['Berlin'], ruhr = g.byName['Ruhr'];
    damageCity(g, berlin, 30);
    damageCity(g, ruhr, 25);
    expect(orderRepair(g, berlin.id)).toBe(true);
    const back = loadGame(saveGame(g))!;
    expect(damageOf(back, back.byName['Ruhr'])).toBeCloseTo(0.25, 9);
    expect(damagedTowns(back, 'GER').map((t) => t.region)).toEqual([ruhr.id]);
    expect(orderUndoRepair(back, berlin.id)).toBe(true);
    expect(damageOf(back, back.byName['Berlin'])).toBeCloseTo(0.3, 9);
    expect(repairAllPlan(back, 'GER').towns).toHaveLength(2);
    expect(repairCity(back, 'GER', ruhr.id)).toBe(true);
  }, 60_000);
});
