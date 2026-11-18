// Air campaigns (ruling 236.2; the design director's b545, b549, b550): a standing order to an air force, flown every
// month until it is called off, with memory. The battle for the sky over a province carries over (contested, held,
// lost), bombing damage carries over and comes back at the historian's rates, and at the stops that follow the book
// the historian's campaigns open and close on their months. The grand campaign only, behind a switch that is off.
import { afterEach, describe, expect, it } from 'vitest';
import { AIR, CLOSE, airAI, airBattles, airFleetsOf, airIncomeLoss, airOverBattle, fly } from '../src/campaign/air';
import {
  AC, acOn, airWarMonth, bombLands, bombWorn, callOff, campaignView, heldDown, lossBand, openCampaign, setAirCampaigns, skyFight, skyFought, stockLoss, whyNotCampaign,
} from '../src/campaign/aircampaigns';
import { createGame } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { fieldYield } from '../src/campaign/resources';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { AirFleet, AirReport, Game, Region } from '../src/campaign/types';

const was = { on: CLOSE.on };
afterEach(() => { CLOSE.on = was.on; });
const grand = (on = true): Game => { const g = startOpen(createGame(7, null)); if (on) setAirCampaigns(g, true); return g; };

/** A Polish province beside the German border, German fighters based next to it, and the Polish air force in reach. */
function poland(g: Game): { t: Region; ger: AirFleet[]; pol: AirFleet[] } {
  for (const r of g.regions) {
    if (r.ctrl !== 'POL') continue;
    const by = r.nb.map((j) => g.regions[j]).find((x) => x.ctrl === 'GER' && !x.overseas);
    if (!by) continue;
    const ger = airFleetsOf(g, 'GER').filter((f) => f.ftr > 0 && f.kind === 'ftr' && f.ship === undefined).slice(0, 3);
    for (const f of ger) f.base = by.id;
    const pol = airFleetsOf(g, 'POL').filter((f) => f.ftr + f.bmr > 0);
    for (const f of pol) f.base = r.id;
    return { t: r, ger, pol };
  }
  throw new Error('no border');
}
/** Sets the Polish air force over a province so that the Germans have `ratio` times its numbers there (b446's count). */
function ratio(g: Game, t: number, pol: AirFleet[], k: number): void {
  const a = airOverBattle(g, new Set(['GER']), new Set(['POL']), t);
  for (let i = 0; i < 4; i++) {
    const d = airOverBattle(g, new Set(['POL']), new Set(['GER']), t);
    if (d <= 0) break;
    const want = a / k;
    for (const f of pol) { f.ftr *= want / d; f.bmr *= want / d; }
  }
}
const next = (g: Game): void => { g.turn++; airWarMonth(g); };

describe('the switch (ruling 236.2)', () => {
  it('is off by default, and off nothing is written: no memory, no campaign, the raids as they were', () => {
    const g = grand(false);
    expect(acOn(g)).toBe(false);
    for (let k = 0; k < 3; k++) runTurnAI(g);
    expect(g.ac).toBeUndefined();
    expect(g.airFleets.every((f) => f.camp === undefined)).toBe(true);
    expect(whyNotCampaign(g, 'GER', 'sky', [0], [1])).toBe('Air campaigns are flown in the grand campaign.');
    const r = g.raids.GER;
    expect(airIncomeLoss(g, 'GER')).toBe(r && r.turn === g.turn ? r.share : 0);
  });

  it('is the grand campaign only', () => {
    const g = createGame(7, null);
    g.flags.ac = 1;
    expect(acOn(g)).toBe(false);
  });
});

describe('the battle for the sky over a province', () => {
  it('is held after two months running at 1.5 to 1, and not after one', () => {
    const g = grand();
    const { t, pol } = poland(g);
    ratio(g, t.id, pol, 2);
    expect(skyFought(g, 'GER', t.id, false)).toBe('contested');
    expect(skyFight(g, t.id)!.run).toBe(1);
    next(g);
    ratio(g, t.id, pol, 2);
    expect(skyFought(g, 'GER', t.id, false)).toBe('held');
    // Held, the sky is Germany's for the battles, landings and raids below it.
    expect(g.sky[t.id]).toEqual({ nat: 'GER', turn: g.turn });
  });

  it('counts a month once, at its best fight, and a month without a fight breaks the run', () => {
    const g = grand();
    const { t, pol } = poland(g);
    ratio(g, t.id, pol, 2);
    skyFought(g, 'GER', t.id, false);
    skyFought(g, 'GER', t.id, false);
    expect(skyFight(g, t.id)!.run).toBe(1);
    next(g);
    next(g);
    ratio(g, t.id, pol, 2);
    expect(skyFought(g, 'GER', t.id, false)).toBe('contested');
    expect(skyFight(g, t.id)!.run).toBe(1);
  });

  it('is lost after two months outnumbered half again', () => {
    const g = grand();
    const { t, pol } = poland(g);
    ratio(g, t.id, pol, 0.5);
    expect(skyFought(g, 'GER', t.id, false)).toBe('contested');
    next(g);
    ratio(g, t.id, pol, 0.5);
    expect(skyFought(g, 'GER', t.id, false)).toBe('lost');
  });

  it("keeps b446's radar guard: over a defender's own ground with radar, numbers alone never win it, a fight won does", () => {
    const g = grand();
    const { t, pol } = poland(g);
    g.nations.POL.tech = { ...(g.nations.POL.tech ?? { done: [] }), done: [...(g.nations.POL.tech?.done ?? []), 'earlywarning'] } as never;
    ratio(g, t.id, pol, 3);
    skyFought(g, 'GER', t.id, false);
    next(g);
    ratio(g, t.id, pol, 3);
    expect(skyFought(g, 'GER', t.id, false)).toBe('contested');
    expect(skyFight(g, t.id)!.run).toBe(0);
    skyFought(g, 'GER', t.id, true);
    next(g);
    expect(skyFought(g, 'GER', t.id, true)).toBe('held');
  });

  it("is held at once when the defender's fields are knocked out: its fighters worn to the floor on fields that were bombed", () => {
    const g = grand();
    const { t, pol } = poland(g);
    ratio(g, t.id, pol, 2);
    for (const f of pol) f.ready = AIR.ready.floor;
    // Not until their fields have been bombed: fighters worn out on fields nobody hit still come up.
    expect(skyFought(g, 'GER', t.id, false)).toBe('contested');
    g.airReports.push({ id: 999, turn: g.turn, mission: 'superiority', region: t.id, zone: -1, place: t.name, attNat: 'GER', defNat: 'POL', att: [], def: [], lines: [], odds: 1, level: 'Close victory', won: true, sky: false, hold: 1, result: '' } as AirReport);
    expect(skyFought(g, 'GER', t.id, false)).toBe('held');
    expect(skyFight(g, t.id)!.won).toBe(g.turn);
  });

  it('stays held from month to month while the holder keeps the numbers, and is contested again when it does not', () => {
    const g = grand();
    const { t, pol } = poland(g);
    ratio(g, t.id, pol, 2);
    skyFought(g, 'GER', t.id, false);
    next(g);
    ratio(g, t.id, pol, 2);
    skyFought(g, 'GER', t.id, false);
    next(g);
    expect(skyFight(g, t.id)!.now).toBe('held');
    expect(g.sky[t.id].turn).toBe(g.turn);
    ratio(g, t.id, pol, 0.8);
    next(g);
    expect(skyFight(g, t.id)!.now).toBe('contested');
  });
});

describe('bombing with memory (b550: only a campaign that comes back before the repair is done keeps output down)', () => {
  /** The RAF by night on the Ruhr in August 1940: Germany moves first, so a raid lands on next month's production. */
  const ruhr = (): { g: Game; r: Region } => { const g = grand(); g.turn = 12; return { g, r: g.byName.Ruhr }; };

  it('one raid comes back at the city rate, month by month', () => {
    const { g, r } = ruhr();
    expect(bombLands(g, 'UK', r, 0.2, true, 'works')).toBe(0.2);
    const rate = AC.repair.city.perMonth;
    next(g);
    expect(heldDown(g, r, 'works')).toBeCloseTo(0.2, 9);
    next(g);
    expect(heldDown(g, r, 'works')).toBeCloseTo(0.2 * (1 - rate), 9);
    next(g);
    expect(heldDown(g, r, 'works')).toBeCloseTo(0.2 * (1 - rate) ** 2, 9);
    // What is still down from earlier raids is Germany's production lost, on top of the month's own raids.
    const total = g.regions.reduce((s, x) => s + (x.ctrl === 'GER' ? x.fac : 0), 0);
    expect(stockLoss(g, 'GER')).toBeCloseTo((0.2 * (1 - rate) ** 2 * r.fac) / total, 9);
    expect(airIncomeLoss(g, 'GER')).toBeCloseTo(stockLoss(g, 'GER'), 9);
  });

  it('a campaign that comes back every month keeps more of it down than one raid, and less when it comes back seldom', () => {
    const run = (every: number): number => {
      const { g, r } = ruhr();
      for (let k = 0; k < 6; k++) { if (k % every === 0) bombLands(g, 'UK', r, 0.1, true, 'works'); next(g); }
      return heldDown(g, r, 'works');
    };
    const once = (() => { const { g, r } = ruhr(); bombLands(g, 'UK', r, 0.1, true, 'works'); for (let k = 0; k < 6; k++) next(g); return heldDown(g, r, 'works'); })();
    expect(run(1)).toBeGreaterThan(run(3));
    expect(run(3)).toBeGreaterThan(once);
    // Never more than the stock's ceiling.
    const { g, r } = ruhr();
    for (let k = 0; k < 24; k++) { bombLands(g, 'UK', r, 0.3, true, 'works'); next(g); }
    expect(heldDown(g, r, 'works')).toBeLessThanOrEqual(AC.stock.most + 1e-9);
  });

  it('a real raid feeds it: the RAF flies, and the Ruhr stays down after the month of the raid', () => {
    const { g, r } = ruhr();
    const fs = g.airFleets.filter((f) => f.nat === 'UK' && f.bmr > 0 && f.ship === undefined);
    fly(g, 'UK', 'night', r.id, fs);
    next(g);
    const first = heldDown(g, r, 'works');
    expect(first).toBeGreaterThan(0);
    next(g);
    expect(heldDown(g, r, 'works')).toBeCloseTo(first * (1 - AC.repair.city.perMonth), 9);
  });

  it("Germany's dispersal from March 1944: a day raid on its works does less, and they come back faster", () => {
    const { g, r } = ruhr();
    g.turn = AC.dispersal.from;
    expect(bombLands(g, 'USA', r, 0.2, false, 'works')).toBeCloseTo(0.2 * AC.dispersal.cut, 9);
    expect(g.ac!.stock[`${r.id}:works`].rate).toBeCloseTo(AC.repair.aircraft.dispersed!, 12);
    g.turn = AC.dispersal.from - 1;
    const h = ruhr();
    h.g.turn = AC.dispersal.from - 1;
    expect(bombLands(h.g, 'USA', h.r, 0.2, false, 'works')).toBe(0.2);
    expect(h.g.ac!.stock[`${h.r.id}:works`].rate).toBeCloseTo(AC.repair.aircraft.perMonth, 12);
  });

  it('an oilfield bombed gives less oil, and comes back at the refinery rate', () => {
    const g = grand();
    const p = g.byName.Ploiesti;
    const full = fieldYield(g, p);
    bombLands(g, 'UK', p, 0.4, false, 'oil');
    expect(fieldYield(g, p)).toBeCloseTo(full * 0.6, 9);
    next(g);
    expect(fieldYield(g, p)).toBeCloseTo(full * (1 - 0.4 * (1 - AC.repair.refinery.perMonth)), 9);
  });

  it("a city's works held down wear its country's will to fight a little, never past the ceiling", () => {
    const { g, r } = ruhr();
    expect(bombWorn(g, 'GER')).toBe(0);
    bombLands(g, 'UK', r, 0.3, true, 'works');
    next(g);
    expect(bombWorn(g, 'GER')).toBeCloseTo(AC.will.per * 0.3, 9);
    for (const nm of ['Hamburg', 'Berlin', 'Rhineland', 'Saxony', 'Bavaria', 'Frankfurt', 'Vienna', 'Bohemia']) bombLands(g, 'UK', g.byName[nm], 0.6, true, 'works');
    next(g);
    expect(bombWorn(g, 'GER')).toBe(AC.will.most);
    expect(AC.will.most).toBeLessThanOrEqual(0.05);
  });

  it('ground that changes hands takes its damage with it', () => {
    const { g, r } = ruhr();
    bombLands(g, 'UK', r, 0.2, true, 'works');
    next(g);
    r.ctrl = 'FRA';
    next(g);
    expect(g.ac!.stock[`${r.id}:works`]).toBeUndefined();
  });
});

describe('a campaign is a standing order', () => {
  /** The RAF's bombers, unescorted by night, on the Ruhr in August 1940. */
  const raf = (): { g: Game; ids: number[]; ruhr: number } => {
    const g = grand();
    g.turn = 12;
    const ids = g.airFleets.filter((f) => f.nat === 'UK' && f.bmr > 0 && f.kind !== 'ftr' && f.ship === undefined).map((f) => f.id);
    return { g, ids, ruhr: g.byName.Ruhr.id };
  };

  it('is flown each month down the same road as any raid, and remembers what it did', () => {
    const { g, ids, ruhr } = raf();
    expect(whyNotCampaign(g, 'UK', 'bomb', [ruhr], ids, { night: true })).toBeNull();
    const c = openCampaign(g, 'UK', 'bomb', [ruhr], ids, { night: true })!;
    expect(c.name).toBe('The bombing of Ruhr by night');
    const reps = airBattles(g, 'UK');
    expect(reps.some((r) => r.mission === 'night' && r.region === ruhr)).toBe(true);
    expect(c.log).toHaveLength(1);
    expect(c.log[0].sent).toBeGreaterThan(0);
    expect(c.log[0].done).toBeGreaterThan(0);
    // The next month it flies again by itself.
    next(g);
    for (const f of g.airFleets) if (ids.includes(f.id)) f.mission = 'support';
    airBattles(g, 'UK');
    expect(c.log).toHaveLength(2);
    expect(c.log[0].lost).toBeGreaterThanOrEqual(0);
  });

  it("is left alone by its staff: a computer nation's groups in a campaign keep its orders", () => {
    const { g, ids, ruhr } = raf();
    expect(openCampaign(g, 'UK', 'bomb', [ruhr], ids, { night: true })).not.toBeNull();
    for (const id of ids) { const f = g.airFleets.find((x) => x.id === id)!; f.mission = 'night'; f.target = ruhr; }
    airAI(g, 'UK');
    for (const id of ids) expect(g.airFleets.find((x) => x.id === id)!.mission).toBe('night');
    // Without the campaign, the staff takes them back each month.
    const h = raf();
    for (const id of h.ids) { const f = h.g.airFleets.find((x) => x.id === id)!; f.mission = 'night'; f.target = h.ruhr; }
    airAI(h.g, 'UK');
    expect(h.ids.some((id) => h.g.airFleets.find((x) => x.id === id)!.mission !== 'night' || h.g.airFleets.find((x) => x.id === id)!.flew === h.g.turn)).toBe(true);
  });

  it('can be called off, and its groups go back on patrol', () => {
    const { g, ids, ruhr } = raf();
    const c = openCampaign(g, 'UK', 'bomb', [ruhr], ids, { night: true })!;
    expect(callOff(g, c.id)).toBe(true);
    expect(c.off).toBe(g.turn);
    for (const id of ids) { const f = g.airFleets.find((x) => x.id === id)!; expect(f.camp).toBeUndefined(); expect(f.mission).toBe('support'); }
    expect(callOff(g, c.id)).toBe(false);
  });

  it('says why it cannot be opened', () => {
    const { g, ids, ruhr } = raf();
    expect(whyNotCampaign(g, 'UK', 'bomb', [g.byName.London.id], ids)).toBe('London is not in enemy hands.');
    expect(whyNotCampaign(g, 'UK', 'bomb', [ruhr], [])).toBe('Pick at least one air group.');
    const ftr = g.airFleets.filter((f) => f.nat === 'UK' && f.kind === 'ftr' && f.ship === undefined).map((f) => f.id);
    expect(whyNotCampaign(g, 'UK', 'bomb', [ruhr], ftr)).toBe('A bombing campaign needs bombers.');
    expect(whyNotCampaign(g, 'UK', 'bomb', [ruhr], ids, { night: true, escorts: ftr })).toBe('Fighters do not fly with the night bombers.');
    openCampaign(g, 'UK', 'bomb', [ruhr], ids, { night: true });
    expect(whyNotCampaign(g, 'UK', 'bomb', [ruhr], ids)).toMatch(/already flies in a campaign/);
  });

  it('behind a named offensive puts its fighters on call over the battles (close air support)', () => {
    CLOSE.on = true;
    const g = grand();
    const { t, ger } = poland(g);
    const c = openCampaign(g, 'GER', 'support', [t.id], ger.map((f) => f.id), { name: 'Support for the drive on Warsaw' })!;
    expect(c).not.toBeNull();
    airBattles(g, 'GER');
    for (const f of ger) expect(f.close).toBe(true);
    callOff(g, c.id);
    for (const f of ger) expect(f.close).toBeUndefined();
  });

  it("shows on the planner: its month, the sky's state, its losses and the record's band for them", () => {
    const g = grand();
    const { t, ger, pol } = poland(g);
    ratio(g, t.id, pol, 2);
    const c = openCampaign(g, 'GER', 'sky', [t.id], ger.map((f) => f.id))!;
    airBattles(g, 'GER');
    const [row] = campaignView(g, 'GER');
    expect(row.id).toBe(c.id);
    expect(row.month).toBe(1);
    expect(row.kind).toBe('sky');
    expect(row.skies[0].place).toBe(t.name);
    expect(row.losses.sent).toBeGreaterThan(0);
    expect(row.losses.band).toBe(lossBand(row.losses.rate));
    expect(row.replace.words).toMatch(/The works build about/);
    expect(lossBand(0.2)).toMatch(/every one stopped/);
    expect(lossBand(0.05)).toMatch(/5% a raid/);
    expect(lossBand(0.01)).toMatch(/Light/);
  });

  it('is kept in a save', () => {
    const { g, ids, ruhr } = raf();
    openCampaign(g, 'UK', 'bomb', [ruhr], ids, { night: true });
    airBattles(g, 'UK');
    const h = loadGame(saveGame(g))!;
    expect(h.ac).toEqual(g.ac);
    expect(h.airFleets.filter((f) => f.camp !== undefined).length).toBe(ids.length);
  });
});

describe("the book: history's campaigns on their months (b549)", () => {
  it("opens the battle for the sky over southern England in August 1940 and closes it when its months are over", () => {
    const g = grand();
    setStop(g, 1, true);
    g.turn = 12;
    airWarMonth(g);
    const bob = g.ac!.campaigns.find((c) => c.book === 3)!;
    expect(bob.nat).toBe('GER');
    expect(bob.kind).toBe('sky');
    expect(bob.targets.map((t) => g.regions[t].name)).toEqual(['Kent', 'Hampshire', 'London']);
    expect(bob.groups.length).toBeGreaterThan(0);
    g.turn = 14;
    airWarMonth(g);
    expect(bob.off).toBe(14);
    expect(bob.why).toBe('Its months are over.');
  });

  it('does not fly the player\'s air force, nor at the stops that do not follow the book', () => {
    const g = startOpen(createGame(7, 'GER'));
    setAirCampaigns(g, true);
    setStop(g, 1, true);
    g.turn = 12;
    airWarMonth(g);
    expect(g.ac!.campaigns.some((c) => c.nat === 'GER')).toBe(false);
    const h = grand();
    setStop(h, 3, true);
    h.turn = 12;
    airWarMonth(h);
    expect(h.ac?.campaigns.length ?? 0).toBe(0);
  });

  it("carries all the historian's 54 rows, and says for each it does not fly why", () => {
    expect(AC.book).toHaveLength(54);
    for (const r of AC.book) expect(!!r.fly || !!r.book).toBe(true);
    for (const k of ['airfield', 'aircraft', 'synthetic', 'refinery', 'city']) expect(AC.repair[k].perMonth).toBeGreaterThan(0);
  });
});
