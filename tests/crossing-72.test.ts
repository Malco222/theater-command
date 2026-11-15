// Crossing a defended sea (Version 72), the grand campaign only. One rule for every opposed crossing, for the player
// and the computer alike: the danger to the troopships is reckoned line by line and shown before the order, with
// what would bring it down (the sky over the beach, a fleet off it, the enemy's fleets beaten) and the danger as it
// would then be; a nation's sea lift is a number it can read; a crossing can be named as the plan, and staff that
// run the air war or the navy prepare it; and the coast watch reports troops and shipping gathering on a quay a
// month before they can sail. History's scenarios use none of it.
import { describe, expect, it } from 'vitest';
import { liftView } from '../src/campaign/arsenal';
import { AIR, airAI, skyDenies, skyOver, whyNotMission } from '../src/campaign/air';
import { setDesk } from '../src/campaign/depth';
import { addCorps, allied, atWar, createGame, endPhase } from '../src/campaign/game';
import {
  NAVY, closedTo, coastReport, crossingAccount, crossingsOf, fleetPower, fleetReach, landCorps, landingRisk, landingTargets, liftLeft, navalAI, planCrossing, planLanding,
  plannedCrossing, sail, seaPath, sortieAgainst, surfaceShips, whyNotLand, type CrossingOutlook,
} from '../src/campaign/navy';
import { coastWatch, startOpen } from '../src/campaign/staff';
import { crossings, liftOf, orderCrossingPlan, runTurnAI } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';

/** A war the computer plays on every side (or with one seat a player's), as it stands once Poland has fallen. */
const opened = (human: string | null = null): Game => {
  const g = startOpen(createGame(3, human));
  for (let k = 0; k < 8 && (k < 2 || g.nations.POL.alive); k++) runTurnAI(g);
  return g;
};
const corps = (g: Game, nat: string, at: Region, divs = 3): Corps => addCorps(g, nat, `Test Corps ${g.nextCorps}`, 'inf', divs, at.id, {});
const clear = (r: Region): Region => { for (const c of r.units) c.dead = true; r.units = []; return r; };
/** Every fleet at war with a nation goes back to its harbour: none at sea, all of them ready to come out. */
const harbours = (g: Game, nat: string): void => { for (const f of g.fleets) if (atWar(g, f.nat, nat)) sail(g, f, f.home, 'port'); };
/** Germany's way to Britain's own shore, as the list of crossings gives it. */
const toBritain = (g: Game): CrossingOutlook => crossingsOf(g, 'GER').find((x) => x.owner === 'UK' && x.home)!;
const warnings = (g: Game): Game['headlines'] => g.headlines.filter((h) => h.warn === 'Coast watch');

describe('the reckoning of a crossing', () => {
  it('is the same danger the landing runs, set out line by line', () => {
    const g = opened();
    harbours(g, 'GER');
    const x = toBritain(g);
    expect(x).toBeTruthy();
    expect(x.from.port).toBeTruthy();
    expect(x.to.owner).toBe('UK');
    expect(x.path.length).toBeGreaterThan(0);
    expect(x.path.length).toBeLessThanOrEqual(NAVY.landing.range);
    const acc = x.account;
    expect(acc.risk).toBeCloseTo(landingRisk(g, 'GER', x.path, x.to), 12);
    expect(acc.loss).toBeCloseTo(acc.risk * NAVY.intercept.loss, 12);
    expect(acc.risk).toBeGreaterThan(0);
    expect(acc.lines[0].label).toBe('The way');
    expect(acc.lines[0].value).toContain(g.seas[x.path[0]].name);
    const last = acc.lines[acc.lines.length - 1];
    expect(last.label).toBe('Caught at sea');
    expect(last.value).toBe(`${Math.round(acc.risk * 100)} in 100`);
    expect(last.effect).toContain(`${Math.round(acc.loss * 100)}%`);
    // The fleets that would come out are named with their strength, and the sky over the beach with who holds it.
    const sortie = sortieAgainst(g, 'GER', x.path);
    expect(sortie).toBeGreaterThan(0);
    expect(acc.lines.find((l) => l.label === 'Enemy fleets that will come out')!.value).toBe(Math.round(sortie).toLocaleString('en-US'));
    expect(acc.lines.some((l) => l.label === `The sky over ${x.to.name}`)).toBe(true);
    expect(x.path).toContain(acc.worst);
    // Nothing reads as a blank or a broken number.
    for (const l of acc.lines) expect(`${l.label} ${l.value} ${l.effect ?? ''}`).not.toMatch(/undefined|NaN|null/);
    for (const h of acc.helps) { expect(h.text).not.toMatch(/undefined|NaN|null/); expect(h.risk).toBeGreaterThanOrEqual(0); expect(h.risk).toBeLessThan(acc.risk); }
  });

  it('says what winning the sky over the beach would do, and winning it does exactly that', () => {
    const g = opened();
    harbours(g, 'GER');
    const x = toBritain(g);
    const sky = x.account.helps.find((h) => h.id === 'sky')!;
    expect(sky).toBeTruthy();
    expect(sky.text).toContain(x.to.name);
    const before = JSON.stringify(g.sky);
    crossingAccount(g, 'GER', x.path, x.to);
    // Asking changes nothing.
    expect(JSON.stringify(g.sky)).toBe(before);
    g.sky[x.to.id] = { nat: 'GER', turn: g.turn };
    expect(landingRisk(g, 'GER', x.path, x.to)).toBeCloseTo(sky.risk, 12);
    const after = crossingAccount(g, 'GER', x.path, x.to);
    expect(after.sky).toBe('ours');
    expect(after.helps.some((h) => h.id === 'sky')).toBe(false);
    expect(after.lines.find((l) => l.label === `The sky over ${x.to.name}`)!.value).toBe('yours');
    expect(after.lines.find((l) => l.label === `The sky over ${x.to.name}`)!.effect).toContain(`${Math.round((1 - AIR.sky.seaFoe) * 100)}%`);
  });

  it('says what a fleet off the beach would do, and sending it does exactly that', () => {
    const g = opened();
    harbours(g, 'GER');
    for (const f of g.fleets) if (f.nat === 'GER') sail(g, f, f.home, 'port');
    const x = toBritain(g);
    const cover = g.fleets.filter((f) => f.nat === 'GER' && surfaceShips(f) > 0 && fleetReach(g, f).includes(x.account.worst));
    expect(cover.length).toBeGreaterThan(0);
    const sea = x.account.helps.find((h) => h.id === 'sea')!;
    expect(sea).toBeTruthy();
    expect(sea.text).toContain(g.seas[x.account.worst].name);
    const power = cover.reduce((s, f) => s + fleetPower(g, f), 0);
    for (const f of cover) expect(sail(g, f, x.account.worst, 'patrol')).toBe(true);
    // (A fleet at sea is as strong as it was in harbour: the reckoning counted it at that strength.)
    expect(cover.reduce((s, f) => s + fleetPower(g, f), 0)).toBeCloseTo(power, 9);
    expect(landingRisk(g, 'GER', x.path, x.to)).toBeCloseTo(sea.risk, 9);
    expect(crossingAccount(g, 'GER', x.path, x.to).helps.some((h) => h.id === 'sea')).toBe(false);
  });

  it('says what beating the enemy fleets would do, and all of it together is the safest', () => {
    const g = opened();
    harbours(g, 'GER');
    const x = toBritain(g);
    const fleets = x.account.helps.find((h) => h.id === 'fleets')!;
    const all = x.account.helps.find((h) => h.id === 'all')!;
    expect(fleets).toBeTruthy();
    expect(all).toBeTruthy();
    for (const h of x.account.helps) expect(all.risk).toBeLessThanOrEqual(h.risk + 1e-12);
    g.fleets = g.fleets.filter((f) => !atWar(g, f.nat, 'GER'));
    expect(landingRisk(g, 'GER', x.path, x.to)).toBeCloseTo(fleets.risk, 12);
    expect(crossingAccount(g, 'GER', x.path, x.to).helps.some((h) => h.id === 'fleets')).toBe(false);
  });

  it('goes with the plan of a landing, and a landing in the month war is declared has none', () => {
    const g = opened();
    harbours(g, 'GER');
    g.airFleets = g.airFleets.filter((f) => f.nat !== 'UK');
    const x = toBritain(g);
    expect(x.bar).toBeNull();
    clear(x.from);
    const force = [corps(g, 'GER', x.from), corps(g, 'GER', x.from)];
    const plan = planLanding(g, force, x.to);
    expect(plan.why).toBeNull();
    expect(plan.account).toBeTruthy();
    expect(plan.account!.risk).toBeCloseTo(plan.risk, 12);
    // Surprise: no fleet can stop it, and there is nothing to reckon.
    g.flags[`war:GER:${x.to.ctrl}`] = g.turn;
    const sudden = planLanding(g, force, x.to);
    expect(sudden.surprise).toBe(true);
    expect(sudden.risk).toBe(0);
    expect(sudden.account).toBeUndefined();
  });
});

describe('the crossings a nation could make', () => {
  it('lists the shores its army cannot march to, the safest way to each, and never one it can reach by land', () => {
    const g = opened();
    const rows = crossingsOf(g, 'GER');
    expect(rows.length).toBeGreaterThan(0);
    // France has a land frontier with Germany: her mainland is no crossing (an island of hers is). Britain's own shore is one.
    const paris = g.regions[g.nations.FRA.capital];
    const mainland = new Set<number>([paris.id]);
    for (const id of mainland) for (const j of g.regions[id].nb) if (g.regions[j].ctrl === 'FRA') mainland.add(j);
    expect(mainland.size).toBeGreaterThan(5);
    expect(rows.some((x) => mainland.has(x.to.id))).toBe(false);
    expect(rows.every((x) => !x.to.nb.some((j) => allied(g, 'GER', g.regions[j].ctrl)))).toBe(true);
    expect(rows.some((x) => x.owner === 'UK' && x.home)).toBe(true);
    for (const x of rows) {
      expect(atWar(g, 'GER', x.to.ctrl)).toBe(true);
      expect(x.foe).toBe(x.to.ctrl);
      expect(x.owner).toBe(x.to.owner);
      expect(x.home).toBe(x.to.owner === x.to.ctrl && !x.to.overseas);
      expect(x.from.port).toBeTruthy();
      expect(seaPath(g, 'GER', x.from, x.to, { max: NAVY.landing.range, surprise: true })).toBeTruthy();
      expect(x.account.sky === 'denied').toBe(skyDenies(g, 'GER', x.to.id));
    }
    // The enemy's own country comes before his colonies and the countries he holds.
    const firstAway = rows.findIndex((x) => !x.home);
    if (firstAway >= 0) expect(rows.slice(firstAway).every((x) => !x.home)).toBe(true);
    // A nation at peace has none, and neither has anyone in one of history's scenarios.
    expect(crossingsOf(g, 'SWE')).toEqual([]);
    const hist = createGame(3, null);
    for (let k = 0; k < 3; k++) runTurnAI(hist);
    expect(crossingsOf(hist, 'GER')).toEqual([]);
  });

  it('says what bars a crossing: no shipping, a sea the enemy has closed, a sky his fighters hold', () => {
    const g = opened();
    harbours(g, 'GER');
    // Whatever is offered to a force on the quay is not barred, and whatever is barred is not offered.
    for (const x of crossingsOf(g, 'GER')) {
      if (x.from.ctrl !== 'GER') continue;
      const h = startOpenCopy(g);
      const from = h.regions[x.from.id], to = h.regions[x.to.id];
      clear(from);
      const force = [corps(h, 'GER', from), corps(h, 'GER', from)];
      expect(landingTargets(h, force).includes(to)).toBe(x.bar === null);
      if (x.bar !== null) expect(whyNotLand(h, force, to)).not.toBeNull();
    }
    // A sky the enemy's fighters hold: the list says so, where no landing force is offered that coast at all.
    const held = g.regions.find((r) => r.owner === 'UK' && r.ctrl === 'UK' && !r.overseas && r.seas.length > 0 && skyDenies(g, 'GER', r.id))!;
    expect(held).toBeTruthy();
    const port = crossingsOf(g, 'GER').find((x) => x.owner === 'UK' && x.home)!.from;
    const path = seaPath(g, 'GER', port, held, { max: NAVY.landing.range, surprise: true });
    if (path) {
      const acc = crossingAccount(g, 'GER', path, held);
      expect(acc.sky).toBe('denied');
      expect(acc.lines.find((l) => l.label === `The sky over ${held.name}`)!.effect).toBe('no invasion fleet can cross under them');
      expect(acc.helps.some((h) => h.id === 'sky')).toBe(true);
    }
    // A sea the enemy has closed: the Home Fleet at sea in strength on the way.
    const x = toBritain(g);
    const fleet = g.fleets.filter((f) => f.nat === 'UK' && surfaceShips(f) > 0).sort((a, b) => fleetPower(g, b) - fleetPower(g, a))[0];
    fleet.ships.bb += 40;
    fleet.base = -1;
    fleet.home = x.path[0];
    fleet.zone = x.path[0];
    fleet.stance = 'patrol';
    expect(closedTo(g, x.path[0], 'GER')).toBe(true);
    const shut = crossingsOf(g, 'GER').find((y) => y.owner === 'UK' && y.home);
    if (shut && shut.path.some((z) => closedTo(g, z, 'GER'))) expect(shut.bar).toMatch(/is closed to your troopships/);
    // No shipping at all.
    g.nations.GER.sealift = 0;
    for (const y of crossingsOf(g, 'GER')) expect(y.bar).toMatch(/has no shipping for a landing/);
  });

  it('a crossing can be named as the plan, comes first on the list, and can be dropped', () => {
    const g = opened('GER');
    harbours(g, 'GER');
    const x = toBritain(g);
    expect(plannedCrossing(g, 'GER')).toBeNull();
    // A friendly coast, a coast out of reach and a quay that is no port are refused.
    expect(planCrossing(g, 'GER', x.from, x.from)).toBe(false);
    const inland = g.regions.find((r) => r.ctrl === 'GER' && !r.port)!;
    expect(planCrossing(g, 'GER', inland, x.to)).toBe(false);
    expect(plannedCrossing(g, 'GER')).toBeNull();
    // Another beach than the safest: the plan is the player's choice, and the list shows it in place of its own.
    const other = g.regions.find((r) => r.owner === 'UK' && r.ctrl === 'UK' && !r.overseas && r.seas.length > 0 && r.id !== x.to.id && !!seaPath(g, 'GER', x.from, r, { max: NAVY.landing.range, surprise: true }))!;
    expect(other).toBeTruthy();
    g.phase = g.order.indexOf('GER');
    expect(orderCrossingPlan(g, x.from.id, other.id)).toBe(true);
    expect(plannedCrossing(g, 'GER')).toEqual({ from: x.from, to: other });
    const rows = crossings(g);
    expect(rows[0].planned).toBe(true);
    expect(rows[0].to).toBe(other);
    expect(rows[0].from).toBe(x.from);
    expect(rows.filter((y) => y.owner === 'UK' && y.home).length).toBe(1);
    expect(rows.filter((y) => y.planned).length).toBe(1);
    // Not in another nation's month.
    g.phase = g.order.indexOf('UK');
    expect(orderCrossingPlan(g, x.from.id, x.to.id)).toBe(false);
    g.phase = g.order.indexOf('GER');
    expect(orderCrossingPlan(g, null, null)).toBe(true);
    expect(plannedCrossing(g, 'GER')).toBeNull();
    expect(crossings(g).some((y) => y.planned)).toBe(false);
    // A beach that is no longer the enemy's is no plan.
    planCrossing(g, 'GER', x.from, x.to);
    x.to.ctrl = 'GER';
    expect(plannedCrossing(g, 'GER')).toBeNull();
  });
});

/** A copy of a game to try something on. */
function startOpenCopy(g: Game): Game {
  return structuredClone(g);
}

describe('staff who prepare the plan', () => {
  /** Britain with a stretch of the Low Countries' coast in German hands in reach of her fighters, and no German fighter left to hold its sky. */
  const beachhead = (human: string | null): { g: Game; beach: Region; quay: Region; mine: Game['airFleets'] } => {
    const g = opened(human);
    g.turn += 13;
    for (const c of g.corps) c.ap = c.apMax;
    const mine = g.airFleets.filter((f) => f.nat === 'UK' && f.kind === 'ftr' && f.ftr >= 100);
    let beach: Region | null = null;
    for (const r of g.regions) {
      if (beach || (r.owner !== 'BEL' && r.owner !== 'NED') || !r.seas.length) continue;
      const was = r.ctrl;
      r.ctrl = 'GER';
      if (mine.some((f) => whyNotMission(g, f, 'superiority', r.id) === null)) beach = r; else r.ctrl = was;
    }
    for (const f of g.airFleets) if (f.nat === 'GER') f.ftr = f.bmr = 0;
    const quay = g.regions.find((r) => r.owner === 'UK' && r.ctrl === 'UK' && r.port && !r.overseas && !!seaPath(g, 'UK', r, beach!, { max: NAVY.landing.range, surprise: true }))!;
    return { g, beach: beach!, quay, mine };
  };
  const over = (x: { g: Game; beach: Region; mine: Game['airFleets'] }): number => x.mine.filter((f) => f.mission === 'superiority' && f.target === x.beach.id && f.flew === x.g.turn).length;

  it('a player who has handed over the air war has the sky over his beach fought for, even where nobody holds it', () => {
    const a = beachhead('UK');
    expect(skyDenies(a.g, 'UK', a.beach.id)).toBe(false);
    expect(skyOver(a.g, a.beach.id)).toBeNull();
    setDesk(a.g, 'air', true);
    // With no plan named the fighters stay over their own country.
    airAI(a.g, 'UK');
    expect(over(a)).toBe(0);
    expect(planCrossing(a.g, 'UK', a.quay, a.beach)).toBe(true);
    const path = seaPath(a.g, 'UK', a.quay, a.beach, { max: NAVY.landing.range, surprise: true })!;
    const before = landingRisk(a.g, 'UK', path, a.beach);
    airAI(a.g, 'UK');
    expect(over(a)).toBeGreaterThan(0);
    expect(skyOver(a.g, a.beach.id)).toBe('UK');
    expect(landingRisk(a.g, 'UK', path, a.beach)).toBeLessThanOrEqual(before);
    // The air war in his own hands: nothing is flown for him.
    const b = beachhead('UK');
    planCrossing(b.g, 'UK', b.quay, b.beach);
    airAI(b.g, 'UK');
    expect(over(b)).toBe(0);
    // A computer staff with its eye on the same beach flies only against a sky the enemy holds, as before.
    const c = beachhead(null);
    c.g.flags['st:aim:UK'] = c.beach.id + 1;
    airAI(c.g, 'UK');
    expect(over(c)).toBe(0);
  });

  it('a player who has handed over the navy has a fleet stand off his beach', () => {
    const { g, beach, quay } = beachhead('UK');
    g.fleets = g.fleets.filter((f) => f.nat !== 'GER');
    const path = seaPath(g, 'UK', quay, beach, { max: NAVY.landing.range, surprise: true })!;
    const off = path[path.length - 1];
    const fleet = g.fleets.find((f) => f.nat === 'UK' && f.duty === 'patrol' && !f.tied && fleetReach(g, f).includes(off))!;
    expect(fleet).toBeTruthy();
    fleet.station = fleetReach(g, fleet).find((z) => z !== off)!;
    setDesk(g, 'navy', true);
    navalAI(g, 'UK');
    expect(fleet.zone).toBe(fleet.station);
    planCrossing(g, 'UK', quay, beach);
    navalAI(g, 'UK');
    expect(fleet.zone).toBe(off);
    expect(fleet.stance).toBe('patrol');
    // In his own hands the fleets are left where he put them.
    setDesk(g, 'navy', false);
    sail(g, fleet, fleet.station, 'patrol');
    navalAI(g, 'UK');
    expect(fleet.zone).toBe(fleet.station);
  });
});

describe('sea lift on the fleet card', () => {
  it('tells what the shipping carries, what is left this month, and what is on the slips', () => {
    const g = opened('GER');
    harbours(g, 'GER');
    g.airFleets = g.airFleets.filter((f) => f.nat !== 'UK');
    const n = g.nations.GER;
    const l = liftView(g, 'GER');
    expect(l.lift).toBe(n.sealift);
    expect(l.left).toBe(liftLeft(g, 'GER'));
    expect(l.left).toBe(l.lift);
    expect(l.building).toEqual([]);
    expect(l.room).toBeGreaterThan(0);
    expect(liftOf(g)).toEqual(l);
    // Troopships on the slips: the month each lot is ready, soonest first, and one lot less room in the ports.
    n.queue.push({ id: 9001, what: 'ship', item: 'tp', count: 1, name: 'Troopships', region: n.capital, ordered: g.turn, eta: g.turn + 6, cost: 14 });
    n.queue.push({ id: 9002, what: 'ship', item: 'tp', count: 1, name: 'Troopships', region: n.capital, ordered: g.turn, eta: g.turn + 3, cost: 14 });
    n.queue.push({ id: 9003, what: 'ship', item: 'dd', count: 4, name: 'Destroyers', region: n.capital, ordered: g.turn, eta: g.turn + 2, cost: 10 });
    const more = liftView(g, 'GER');
    expect(more.building).toEqual([g.turn + 3, g.turn + 6]);
    expect(more.room).toBe(l.room - 2);
    // A landing uses the lift of the month it sails in.
    const x = toBritain(g);
    clear(x.from);
    const force = [corps(g, 'GER', x.from), corps(g, 'GER', x.from)];
    expect(landCorps(g, force, x.to).ok).toBe(true);
    expect(liftView(g, 'GER').left).toBe(l.lift - 2);
    expect(liftView(g, 'GER').lift).toBe(l.lift);
  });
});

describe('the coast watch reports shipping gathering', () => {
  /** Germany with two corps on the quay nearest Britain, every British ship in harbour. */
  const gathering = (human: string | null, count = 2): { g: Game; quay: Region; force: Corps[] } => {
    const g = opened(human);
    harbours(g, 'GER');
    const quay = toBritain(g).from;
    clear(quay);
    // (Nobody else's troops stand on a quay in this picture.)
    for (const r of g.regions) if (r.port && r.id !== quay.id && r.units.some((u) => u.nat === 'GER')) clear(r);
    const force = Array.from({ length: count }, () => corps(g, 'GER', quay));
    return { g, quay, force };
  };

  it('warns a player a month before the troops can sail, and his staff would man the harbours', () => {
    const { g, quay } = gathering('UK');
    expect(warnings(g)).toEqual([]);
    endPhase(g, 'GER');
    const w = warnings(g);
    expect(w.length).toBe(1);
    expect(w[0].nat).toBe('UK');
    expect(w[0].kind).toBe('event');
    expect(w[0].text).toContain(`German shipping gathering at ${quay.name}`);
    expect(w[0].text).toContain('enough to carry 2 corps');
    expect(w[0].text).toContain('as soon as next month');
    const coast = g.regions[w[0].region!];
    expect(coast.owner).toBe('UK');
    expect(coast.ctrl).toBe('UK');
    expect(w[0].text).toContain(coast.name);
    expect(g.flags['st:watch:UK']).toBe(g.turn);
    // The same quay is not cried again every month, but it is after a while, and a new quay is reported at once.
    coastReport(g, 'GER');
    g.turn += 1;
    coastReport(g, 'GER');
    expect(warnings(g).length).toBe(1);
    g.turn += NAVY.watch!.every;
    coastReport(g, 'GER');
    expect(warnings(g).length).toBe(2);
  });

  it('a corps that marches onto the quay cannot sail that month, so the report is always a month ahead', () => {
    const { g, quay, force } = gathering('UK');
    const x = toBritain(g);
    expect(x.from).toBe(quay);
    force[0].ap = force[0].apMax - 1;
    expect(whyNotLand(g, force, x.to)).toBe('Only corps that have not moved this month can embark.');
  });

  it('says nothing of one corps, of a nation with no ships, or of a quay with no open sea to the coast', () => {
    const one = gathering('UK', 1);
    coastReport(one.g, 'GER');
    expect(warnings(one.g)).toEqual([]);
    const none = gathering('UK');
    none.g.nations.GER.sealift = 0;
    coastReport(none.g, 'GER');
    expect(warnings(none.g)).toEqual([]);
    // The Home Fleet at sea in strength across the way: nothing can sail, and nothing is reported.
    const shut = gathering('UK');
    const way = toBritain(shut.g).path;
    for (const z of new Set(shut.g.seas.map((s) => s.id))) {
      const fleet = shut.g.fleets.find((f) => f.nat === 'UK' && surfaceShips(f) > 0)!;
      const copy = { ...fleet, id: 9000 + z, ships: { ...fleet.ships, bb: 60 }, base: -1, home: z, zone: z, stance: 'patrol' as const };
      if (way.includes(z) || shut.g.seas[z].adj.some((j) => way.includes(j))) shut.g.fleets.push(copy);
    }
    expect(way.every((z) => closedTo(shut.g, z, 'GER'))).toBe(true);
    coastReport(shut.g, 'GER');
    expect(warnings(shut.g)).toEqual([]);
    // History's scenarios have no coast watch.
    const hist = createGame(3, 'UK');
    for (let k = 0; k < 3; k++) runTurnAI(hist);
    const port = hist.regions.find((r) => r.ctrl === 'GER' && r.port && r.seas.length > 0)!;
    corps(hist, 'GER', port); corps(hist, 'GER', port);
    endPhase(hist, 'GER');
    expect(warnings(hist)).toEqual([]);
  });

  it('a player who gathers is seen too: he is told, and the computer across the water mans its harbours', () => {
    const { g, quay } = gathering('GER');
    delete g.flags['st:watch:UK'];
    endPhase(g, 'GER');
    expect(warnings(g)).toEqual([]);
    const told = g.headlines.filter((h) => h.nat === 'GER' && h.text.includes('coast watchers have seen your shipping'));
    expect(told.length).toBe(1);
    expect(told[0].text).toContain(quay.name);
    expect(told[0].region).toBe(quay.id);
    expect(g.flags['st:watch:UK']).toBe(g.turn);
    expect(Object.keys(coastWatch(g, 'UK')).length).toBeGreaterThan(0);
  });

  it('a war nobody is playing raises no news, and the staffs still take the warning', () => {
    const { g } = gathering(null);
    const news = g.headlines.length;
    coastReport(g, 'GER');
    expect(g.headlines.length).toBe(news);
    expect(g.flags['st:watch:UK']).toBe(g.turn);
  });
});
