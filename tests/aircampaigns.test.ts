// Air campaigns (ruling 236.2; the design director's b545, b549, b550): a standing order to an air force, flown every
// month until it is called off, with memory. The battle for the sky over a province carries over (contested, held,
// lost), bombing damage carries over and comes back at the historian's rates, and at the stops that follow the book
// the historian's campaigns open and close on their months. The grand campaign only, behind a switch that is off.
import { afterEach, describe, expect, it } from 'vitest';
import { AIR, CLOSE, airAI, airBattles, airFleetsOf, airIncomeLoss, airOverBattle, airRange, fly, hasAirfield, kmTo, whyNotBase } from '../src/campaign/air';
import {
  AC, acOn, airWarMonth, bombLands, bombWorn, callOff, campaignView, chainAt, gunnerRatio, heldDown, lossBand, nightFighters, openCampaign, setAirCampaigns, skyFight, skyFought, stockLoss, whyNotCampaign,
} from '../src/campaign/aircampaigns';
import { createGame } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { fieldYield } from '../src/campaign/resources';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { AirFleet, AirReport, Game, Region } from '../src/campaign/types';

const was = { on: CLOSE.on, staffs: CLOSE.ai.on };
afterEach(() => { CLOSE.on = was.on; CLOSE.ai.on = was.staffs; });
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
    expect(c.name).toBe('The bombing of the Ruhr by night');
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
    CLOSE.ai.on = true;
    const g = grand();
    const { t, ger } = poland(g);
    const c = openCampaign(g, 'GER', 'support', [t.id], ger.map((f) => f.id), { name: 'Support for the drive on Warsaw' })!;
    expect(c).not.toBeNull();
    airBattles(g, 'GER');
    for (const f of ger) expect(f.close).toBe(true);
    callOff(g, c.id);
    for (const f of ger) expect(f.close).toBeUndefined();
  });

  it('as the data ships (the player\'s choice on, the staffs\' off): the player\'s own campaign holds its fighters for the armies, a staff\'s fights for the sky as before', () => {
    expect(CLOSE.on).toBe(true);
    expect(CLOSE.ai.on).toBe(false);
    // The player's own campaign: his fighters on call over the battles.
    const mine = startOpen(createGame(7, 'GER'));
    setAirCampaigns(mine, true);
    const a = poland(mine);
    expect(openCampaign(mine, 'GER', 'support', [a.t.id], a.ger.map((f) => f.id), { name: 'Support for the drive on Warsaw' })).not.toBeNull();
    airBattles(mine, 'GER');
    for (const f of a.ger) expect(f.close).toBe(true);
    // A staff's campaign: nobody on ground support, and its groups as they were with the switch off altogether.
    const staff = grand(), off = grand();
    const b = poland(staff), c = poland(off);
    openCampaign(staff, 'GER', 'support', [b.t.id], b.ger.map((f) => f.id), { name: 'Support for the drive on Warsaw' });
    airBattles(staff, 'GER');
    CLOSE.on = false;
    openCampaign(off, 'GER', 'support', [c.t.id], c.ger.map((f) => f.id), { name: 'Support for the drive on Warsaw' });
    airBattles(off, 'GER');
    expect(staff.airFleets.some((f) => f.close)).toBe(false);
    expect(JSON.stringify(staff.airFleets)).toBe(JSON.stringify(off.airFleets));
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

  it("reads in the UI director's words: the player is 'you', places take 'the' (21:06Z)", () => {
    const { g, ids, ruhr } = raf();
    const c = openCampaign(g, 'UK', 'bomb', [ruhr], ids, { night: true })!;
    expect(c.name).toBe('The bombing of the Ruhr by night');
    airBattles(g, 'UK');
    // Germany has already moved this month, so the raid's harm falls on its next month's output.
    g.turn++;
    const [row] = campaignView(g, 'UK');
    expect(row.damage[0].words).toMatch(/^\d+(\.\d)?% of the Ruhr's output is down: \d+(\.\d)?% from this month's raids, the rest from earlier raids not yet repaired\. \d+(\.\d)?% has come back since last month\. Only raids repeated before the repairs are done keep it down\.$/);
    expect(row.replace.words).toMatch(/This month the campaign lost \d+ aircraft\.$/);
    g.human = 'UK';
    callOff(g, c.id);
    expect(g.headlines[g.headlines.length - 1].text).toMatch(/^The bombing of the Ruhr by night has ended after a month: you called it off\.$/);
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
  it("takes its share of the air force and moves groups out of reach up to a field near the record's base (b569(3), (4))", () => {
    const g = grand();
    setStop(g, 1, true);
    g.turn = 17;
    const sicily = g.byName.Sicily.id, malta = g.byName.Malta.id;
    // Before the month no German bomber is in reach of Malta.
    const bombers = (): AirFleet[] => airFleetsOf(g, 'GER').filter((f) => f.bmr > 0 && f.kind !== 'ftr' && f.ship === undefined);
    expect(bombers().some((f) => kmTo(g, f, malta) <= airRange(f, g).bmr)).toBe(false);
    airWarMonth(g);
    const c = g.ac!.campaigns.find((x) => x.book === 6 && x.off === undefined)!;
    expect(c).toBeDefined();
    const fs = c.groups.map((id) => g.airFleets.find((f) => f.id === id)!);
    for (const f of fs) {
      expect(kmTo(g, f, malta)).toBeLessThanOrEqual(airRange(f, g).bmr);
      expect(f.moved).toBe(17);
    }
    expect(fs.some((f) => f.base === sicily)).toBe(true);
    // Its share of Germany's bombers: 0.7 of the Mediterranean's, which is 0.3 of the Luftwaffe in 1941.
    const all = bombers().reduce((s, f) => s + f.bmr, 0), took = fs.reduce((s, f) => s + f.bmr, 0);
    const med = AC.split.GER.filter(([from]) => from <= 17).pop()![1].med;
    expect(med).toBe(0.3);
    // Whole groups, the nearest to its share: one group, or never more than the share and half the last group over.
    expect(took).toBeGreaterThan(0);
    const big = Math.max(...fs.map((f) => f.bmr));
    expect(took).toBeLessThanOrEqual(Math.max(big, 0.7 * med * all + big / 2));
  });

  it("gives Rhodes, Sardinia and the Libyan fields their airfields with the switch on, and only then (b574)", () => {
    const g = grand();
    const ita = airFleetsOf(g, 'ITA').find((f) => f.bmr > 0 && f.ship === undefined)!;
    const rhodes = g.byName.Rhodes, tobruk = g.byName.Tobruk;
    setAirCampaigns(g, false);
    expect(hasAirfield(g, rhodes.id)).toBe(false);
    expect(whyNotBase(g, ita, rhodes, true)).not.toBeNull();
    setAirCampaigns(g, true);
    for (const nm of ['Rhodes', 'Sardinia', 'Tripoli', 'Benghazi', 'Derna', 'Tobruk', 'Crete', 'Tunis', 'El Alamein']) expect(hasAirfield(g, g.byName[nm].id)).toBe(true);
    expect(whyNotBase(g, ita, rhodes, true)).toBeNull();
    // Whoever holds the field uses it.
    tobruk.ctrl = 'UK';
    const raf = airFleetsOf(g, 'UK').find((f) => f.ship === undefined)!;
    expect(whyNotBase(g, raf, tobruk, true)).toBeNull();
    expect(whyNotBase(g, ita, tobruk, true)).not.toBeNull();
  });

  it("holds the record's share of the RAF at home in August 1941, not every group in reach (b569(3))", () => {
    const g = grand();
    setStop(g, 1, true);
    g.turn = 23;
    // France has fallen: the sweeps' ground is Germany's.
    for (const nm of ['Calais', 'Somme']) g.byName[nm].ctrl = 'GER';
    airWarMonth(g);
    const home = (f: AirFleet): boolean => { const r = g.regions[f.base]; return (r.first ?? r.owner) === 'UK' && !r.overseas; };
    const raf = airFleetsOf(g, 'UK').filter((f) => f.ship === undefined && home(f));
    const bmr = raf.filter((f) => f.bmr > 0 && f.kind !== 'ftr').reduce((s, f) => s + f.bmr, 0);
    const ftr = raf.filter((f) => f.ftr > 0 && f.kind !== 'bmr').reduce((s, f) => s + f.ftr, 0);
    const held = (i: number, k: 'bmr' | 'ftr'): number => {
      const c = g.ac!.campaigns.find((x) => x.book === i && x.off === undefined);
      return (c?.groups ?? []).reduce((s, id) => s + (g.airFleets.find((f) => f.id === id)?.[k] ?? 0), 0);
    };
    const big = (k: 'bmr' | 'ftr'): number => Math.max(...raf.map((f) => f[k]));
    // The historian's shares are of the whole RAF at home: the night bombing 0.3 of it, in bombers; the sweeps 0.2,
    // in fighters. Each is flown, in whole groups: one, or never more than the share and half the last group over.
    const all = bmr + ftr;
    expect(held(17, 'bmr')).toBeGreaterThan(0);
    expect(held(17, 'bmr')).toBeLessThanOrEqual(Math.max(big('bmr'), 0.3 * all + big('bmr') / 2));
    expect(held(18, 'ftr')).toBeGreaterThan(0);
    expect(held(18, 'ftr')).toBeLessThanOrEqual(Math.max(big('ftr'), 0.2 * all + big('ftr') / 2));
    // Together about half the RAF at home, as the record had it, not nine tenths.
    expect((held(17, 'bmr') + held(18, 'ftr')) / all).toBeLessThan(0.6);
  });

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

describe("radar by the tree and the record (riders b553 and b558, Brian's \"if you had radar then your fighters are much better at intercepting bombing raids\")", () => {
  const raid = (g: Game, att: string, def: string, target: string, m: 'bomb' | 'night' = 'bomb'): ReturnType<typeof fly> => {
    if (!g.nations[att].atWar.includes(def)) { g.nations[att].atWar.push(def); g.nations[def].atWar.push(att); }
    const fs = g.airFleets.filter((f) => f.nat === att && f.bmr > 0 && f.kind !== 'ftr' && f.ship === undefined);
    return fly(g, att, m, g.byName[target].id, fs);
  };
  const learn = (g: Game, nat: string, ...ids: string[]): void => { const t = (g.nations[nat].tech ??= { done: [], work: [] } as never); for (const id of ids) if (!t.done.includes(id)) t.done.push(id); };
  const warned = (r: AirReport | null): string | undefined => r?.lines.find((l) => l.label === 'Radar')?.value;
  const chain = (g: Game, place: string): string | undefined => chainAt(g, g.byName[place], AIR.radar.chains)?.name;

  it('at stops 3 to 5 the warning comes with Early-warning radar, for any nation; without it 7 in 10 meet the raid', () => {
    const g = grand();
    g.turn = 12;
    expect(warned(raid(g, 'GER', 'FRA', 'Paris'))).toBe('No radar warning');
    const h = grand();
    h.turn = 12;
    learn(h, 'FRA', 'earlywarning');
    expect(warned(raid(h, 'GER', 'FRA', 'Paris'))).toBe('The French radar chain gives warning');
  });

  it("at the stops that follow the book each great power's chain comes on the record's month, over the record's ground, whatever its research", () => {
    const g = grand();
    setStop(g, 1, true);
    learn(g, 'SOV', 'earlywarning');
    g.turn = 22;
    expect(chain(g, 'Moscow')).toBeUndefined();
    g.turn = 23;
    expect(chain(g, 'Moscow')).toBe('the Redut stations');
    expect(chain(g, 'Leningrad')).toBe('the Redut stations');
    expect(chain(g, 'Kiev')).toBeUndefined();
    g.turn = 41;
    expect(chain(g, 'Kiev')).toBe('the Redut stations');
    // Germany's North Sea coast has the Freya stations from the first month; inland waits for the Kammhuber Line.
    g.turn = 1;
    expect(chain(g, 'Bremen')).toBe('the Freya stations');
    expect(chain(g, 'Ruhr')).toBeUndefined();
    g.turn = 24;
    expect(chain(g, 'Bremen')).toBe('the Kammhuber Line');
    expect(chain(g, 'Ruhr')).toBe('the Kammhuber Line');
    g.turn = 40;
    expect(chain(g, 'Rome')).toBeUndefined();
    expect(chain(g, 'Paris')).toBeUndefined();
    g.turn = 41;
    expect(chain(g, 'Rome')).toBe('the German radar stations');
    // France has none at the book, whatever it researches.
    learn(g, 'FRA', 'earlywarning');
    expect(chain(g, 'Paris')).toBeUndefined();
    // At stops 3 to 5 the tree rules: the record's part-ground rows give way to it.
    setStop(g, 3, true);
    g.turn = 23;
    expect(chain(g, 'Moscow')).toBe('the Soviet radar chain');
    expect(chain(g, 'Kiev')).toBe('the Soviet radar chain');
    expect(chain(g, 'Paris')).toBe('the French radar chain');
    g.turn = 1;
    expect(chain(g, 'Bremen')).toBeUndefined();
  });

  it('the raid report agrees the verb with the chain', () => {
    const g = grand();
    setStop(g, 1, true);
    g.turn = 3;
    expect(warned(raid(g, 'UK', 'GER', 'Bremen'))).toBe('The Freya stations give warning');
    g.turn = 12;
    expect(warned(raid(g, 'GER', 'UK', 'London'))).toBe('Chain Home gives warning');
  });

  it("keeps the record's two chains on their months at every stop", () => {
    for (const stop of [1, 3, 5]) {
      const g = grand();
      setStop(g, stop, true);
      g.turn = 12;
      expect(warned(raid(g, 'GER', 'UK', 'London'))).toBe('Chain Home gives warning');
      g.turn = 24;
      expect(chain(g, 'Ruhr')).toBe('the Kammhuber Line');
    }
  });

  it("the night figure is the historian's dated row for the nation; at stops 3 to 5 Fighter control is the gate", () => {
    const g = grand();
    setStop(g, 1, true);
    g.turn = 12;
    expect(nightFighters(g, 'UK')).toBe(0.001);
    expect(nightFighters(g, 'GER')).toBe(0.001);
    g.turn = 14;
    expect(nightFighters(g, 'GER')).toBe(0.004);
    g.turn = 19;
    expect(nightFighters(g, 'UK')).toBe(0.008);
    g.turn = 47;
    expect(nightFighters(g, 'GER')).toBe(0.01);
    g.turn = 50;
    expect(nightFighters(g, 'GER')).toBe(0.033);
    expect(nightFighters(g, 'FRA')).toBe(0.001);
    setStop(g, 3, true);
    expect(nightFighters(g, 'GER')).toBe(0.001);
    learn(g, 'GER', 'gci');
    expect(nightFighters(g, 'GER')).toBe(0.033);
    g.turn = 20;
    expect(nightFighters(g, 'GER')).toBe(0.004);
  });

  it('Fighter control steers the defending fighters by night: more bombers fall to them', () => {
    const night = (ctl: boolean): number => {
      let lost = 0;
      for (let t = 45; t < 51; t++) lost += nightRaid(ctl, t);
      return lost;
    };
    const nightRaid = (ctl: boolean, t: number): number => {
      const g = grand();
      g.turn = t;
      learn(g, 'GER', 'earlywarning', ...(ctl ? ['gci'] : []));
      // Germany's fighters stand over the Ruhr, so they come up against the raid.
      for (const f of g.airFleets) if (f.nat === 'GER' && f.ftr > 0 && f.ship === undefined) { f.base = g.byName.Ruhr.id; f.mission = 'support'; }
      const r = raid(g, 'UK', 'GER', 'Ruhr', 'night')!;
      expect(r.def.length).toBeGreaterThan(0);
      return r.att.reduce((s, x) => s + x.lostBmr, 0);
    };
    expect(night(true)).toBeGreaterThan(night(false));
  });

  it("the bombers' gunners bring down a few night fighters with the switch on, none with it off (b557)", () => {
    const shot = (on: boolean): number => {
      let down = 0;
      for (let t = 49; t < 55; t++) {
        const g = grand(on);
        setStop(g, 1, true);
        g.turn = t;
        for (const f of g.airFleets) if (f.nat === 'GER' && f.ftr > 0 && f.ship === undefined) { f.base = g.byName.Ruhr.id; f.mission = 'support'; }
        const r = raid(g, 'UK', 'GER', 'Ruhr', 'night')!;
        down += r.def.reduce((s, x) => s + x.lostFtr, 0);
      }
      return down;
    };
    expect(shot(false)).toBe(0);
    expect(shot(true)).toBeGreaterThan(0);
  });

  it('a staff bombed by night takes Fighter control first at stops 3 to 5, once it holds Early-warning radar (b569(2))', async () => {
    const { staffPick } = await import('../src/campaign/tech');
    const g = grand();
    g.turn = 20;
    learn(g, 'GER', 'earlywarning');
    const before = staffPick(g, 'GER');
    expect(before).not.toBe('gci');
    for (const f of g.airFleets) if (f.nat === 'GER' && f.ftr > 0 && f.ship === undefined) { f.base = g.byName.Ruhr.id; f.mission = 'support'; }
    raid(g, 'UK', 'GER', 'Ruhr', 'night');
    expect(g.ac?.night?.GER).toBe(20);
    expect(staffPick(g, 'GER')).toBe('gci');
    // Not without Early-warning radar, not months after the last raid, not at the book, and not with the switch off.
    const h = structuredClone(g);
    h.nations.GER.tech!.done = h.nations.GER.tech!.done.filter((x) => x !== 'earlywarning');
    expect(staffPick(h, 'GER')).not.toBe('gci');
    g.turn = 20 + AC.radar.need + 1;
    expect(staffPick(g, 'GER')).not.toBe('gci');
    g.turn = 21;
    setStop(g, 1, true);
    expect(staffPick(g, 'GER')).not.toBe('gci');
    setStop(g, 3, true);
    expect(staffPick(g, 'GER')).toBe('gci');
    setAirCampaigns(g, false);
    expect(staffPick(g, 'GER')).not.toBe('gci');
  });

  it("the gunners' ratio reads the historian's kinds from what the game knows (b559)", () => {
    const g = grand();
    const at = (nat: string, place: number, t: number, o: { m?: 'bomb' | 'ground' | 'line'; night?: boolean; escort?: boolean } = {}): number => {
      g.turn = t;
      return gunnerRatio(g, nat, o.m ?? 'bomb', place, !!o.night, !!o.escort);
    };
    const london = g.byName.London.id, ruhr = g.byName.Ruhr.id, japan = g.byName.Rome.id;
    // Ground Japan holds (the Europe chart has none, so a stand-in province changes hands): first an outer island, then a home island.
    g.regions[japan].ctrl = 'JAP';
    g.regions[japan].name = 'Formosa';
    expect(at('USA', japan, 60)).toBe(0.4);
    g.regions[japan].name = 'Tokyo';
    expect(at('GER', london, 12)).toBe(0.07);
    expect(at('UK', ruhr, 4)).toBe(0.15);
    expect(at('UK', ruhr, 20)).toBe(0.07);
    expect(at('UK', ruhr, 30)).toBe(0.15);
    expect(at('GER', london, 30)).toBe(0.15);
    expect(at('USA', ruhr, 47)).toBe(0.4);
    expect(at('USA', ruhr, 47, { escort: true })).toBe(0.2);
    expect(at('USA', japan, 57)).toBe(0.4);
    expect(at('USA', japan, 58)).toBe(0.5);
    expect(at('UK', ruhr, 50, { night: true })).toBe(0.05);
    expect(at('SOV', ruhr, 50, { m: 'ground' })).toBe(0.07);
  });

  it('the tree cards say so, from the rows, only with the switch on', async () => {
    const { techRule, TECH } = await import('../src/campaign/tech');
    const ew = TECH.techs.find((t) => t.id === 'earlywarning')!, gci = TECH.techs.find((t) => t.id === 'gci')!;
    const off = grand(false);
    expect(techRule(ew, off, 'FRA')).toBe(techRule(ew));
    const g = grand();
    expect(techRule(ew, g, 'FRA')).toMatch(/only 7 in 10 do/);
    expect(techRule(gci, g, 'FRA')).toMatch(/as well as each year's sets allow\. Without this, its night defence stays as it was at the start of the war\.$/);
    setStop(g, 1, true);
    expect(techRule(ew, g, 'UK')).toMatch(/Its warning chain follows history's, with or without this research: over all its ground from September 1939\.$/);
    expect(techRule(ew, g, 'GER')).toMatch(/: over its North Sea coast from September 1939, over all its ground from August 1941\.$/);
    expect(techRule(ew, g, 'SOV')).toMatch(/: over Moscow and Leningrad from July 1941, over all its ground from January 1943\.$/);
    expect(techRule(ew, g, 'USA')).toMatch(/: over all its ground from January 1942\.$/);
    expect(techRule(ew, g, 'FRA')).not.toMatch(/warning chain/);
    expect(techRule(gci, g, 'FRA')).toMatch(/Its night defence grows with the years as history's did, with or without this research\.$/);
  });
});
