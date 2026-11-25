// Crossing a defended sea (Version 72), the grand campaign only. One rule for every opposed crossing, for the player
// and the computer alike: the danger to the troopships is reckoned line by line and shown before the order, with
// what would bring it down (the sky over the beach, a fleet off it, the enemy's fleets beaten) and the danger as it
// would then be; a nation's sea lift is a number it can read; a crossing can be named as the plan, and staff that
// run the air war or the navy prepare it; and the coast watch reports troops and shipping gathering on a quay a
// month before they can sail. History's scenarios use none of it.
import { afterAll, beforeAll, describe, expect, it } from 'vitest';
import openData from '../data/campaign/open.json';
import { runAI } from '../src/campaign/ai';
import { STRENGTH } from '../src/campaign/ashore';
import { MIDDLE_EAST } from '../src/campaign/middleeast';
import { liftView } from '../src/campaign/arsenal';
import { AIR, AIRWAR, airAI, airBases, airRange, airfieldsHold, km, rebaseAir, skyDenies, skyOver, skyReach, whyNotMission } from '../src/campaign/air';
import { setDesk } from '../src/campaign/depth';
import { addCorps, atWar, computeSupply, createGame, endPhase } from '../src/campaign/game';
import { onBook, setStop, shoreOpen } from '../src/campaign/history';
import {
  NAVY, SeaPicture, closedTo, coastReport, crossingAccount, crossingsOf, fleetPower, fleetReach, landCorps, landingRisk, landingTargets, liftLeft, navalAI, openSeas, planCrossing, planLanding,
  plannedCrossing, sail, seaPath, sortieAgainst, surfaceShips, whyNotLand, type CrossingOutlook,
} from '../src/campaign/navy';
import { coastWatch, seaBase, startOpen } from '../src/campaign/staff';
import { crossings, liftOf, orderCrossingPlan, runTurnAI } from '../src/campaign/turn';
import type { AirFleet, Corps, Game, Region } from '../src/campaign/types';

/** A war the computer plays on every side (or with one seat a player's), as it stands once Poland has fallen. */
// The crossings and the beach rule are tried with the fighters where the opening months and each scenario put them: the
// basing of fighters by the threat (ruling 227, b428) is held off in this file.
const threatBasing = AIRWAR.ai.basing.better;
beforeAll(() => { AIRWAR.ai.basing.better = Infinity; });
afterAll(() => { AIRWAR.ai.basing.better = threatBasing; });
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
const fighters = (g: Game, nat: string): AirFleet[] => g.airFleets.filter((f) => f.nat === nat && f.kind === 'ftr' && f.ftr > 0 && f.ship === undefined);
/**
 * Germany given a port across the Channel with an airfield her fighters can use, a British beach within their reach
 * of it, and (if asked) a fleet of hers in the water between strong enough that the sea is not closed to her.
 */
const acrossTheChannel = (g: Game, fleet = false): { field: Region; beach: Region; way: number[] } => {
  const group = fighters(g, 'GER')[0];
  const coasts = g.regions.filter((r) => r.owner === 'UK' && r.ctrl === 'UK' && !r.overseas && r.seas.length > 0);
  let field: Region | null = null, beach: Region | null = null;
  for (const r of g.regions) {
    if (field || r.ctrl === 'GER' || !r.port || !['FRA', 'BEL', 'NED'].includes(r.owner)) continue;
    const c = coasts.find((x) => km(g, r.id, x.id) <= airRange(group).ftr);
    if (!c) continue;
    const was = r.ctrl, since = r.since;
    r.ctrl = 'GER';
    r.since = g.turn - 3;
    if (airBases(g, group).includes(r) && seaPath(g, 'GER', r, c, { max: NAVY.landing.range, surprise: true })) { field = r; beach = c; } else { r.ctrl = was; r.since = since; }
  }
  const way = seaPath(g, 'GER', field!, beach!, { max: NAVY.landing.range, surprise: true })!;
  if (fleet) {
    const f = g.fleets.find((x) => x.nat === 'GER' && surfaceShips(x) > 0)!;
    f.ships.bb += 60;
    f.base = -1;
    f.home = way[way.length - 1];
    f.zone = way[way.length - 1];
    f.stance = 'patrol';
  }
  return { field: field!, beach: beach!, way };
};

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
    // A share of the men that rounds to nothing is said in words, never "about 0%" (the UI director, build 1638).
    const loss = NAVY.intercept.loss;
    NAVY.intercept.loss = 0.001 / acc.risk;
    try {
      const few = crossingAccount(g, 'GER', x.path, x.to);
      expect(few.lines[few.lines.length - 1].effect).toBe('almost none of the men lost');
    } finally { NAVY.intercept.loss = loss; }
    // The fleets that would come out are named with their strength, and the sky over the beach with who holds it.
    const sortie = sortieAgainst(g, 'GER', x.path);
    expect(sortie).toBeGreaterThan(0);
    expect(acc.lines.find((l) => l.label === 'Enemy ships that will come out')!.value).toBe(Math.round(sortie).toLocaleString('en-US'));
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
    // (Reckoned for fighters that can reach the beach: where they cannot is the next test.)
    const inReach = (): { how: 'now'; base: number } => ({ how: 'now', base: x.from.id });
    const acc = crossingAccount(g, 'GER', x.path, x.to, inReach);
    const sky = acc.helps.find((h) => h.id === 'sky')!;
    expect(sky).toBeTruthy();
    expect(sky.text).toBe(`Win the sky over ${x.to.name} with a fight for the sky flown by your fighters`);
    expect(acc.skyWon).toBeCloseTo(sky.risk, 12);
    expect(acc.risk).toBeCloseTo(x.account.risk, 12);
    const before = JSON.stringify(g.sky);
    crossingAccount(g, 'GER', x.path, x.to, inReach);
    // Asking changes nothing.
    expect(JSON.stringify(g.sky)).toBe(before);
    g.sky[x.to.id] = { nat: 'GER', turn: g.turn };
    expect(landingRisk(g, 'GER', x.path, x.to)).toBeCloseTo(sky.risk, 12);
    const after = crossingAccount(g, 'GER', x.path, x.to, inReach);
    expect(after.sky).toBe('ours');
    expect(after.helps.some((h) => h.id === 'sky')).toBe(false);
    expect(after.lines.find((l) => l.label === `The sky over ${x.to.name}`)!.value).toBe('yours');
    expect(after.lines.find((l) => l.label === `The sky over ${x.to.name}`)!.effect).toContain(`${Math.round((1 - AIR.sky.seaFoe) * 100)}%`);
    expect(after.skyWon).toBeNull();
  });

  it('offers the sky only where fighters can reach it: from the fields they are on, or from one they could be brought up to', () => {
    const g = opened();
    harbours(g, 'GER');
    const home = g.regions.filter((r) => r.owner === 'UK' && r.ctrl === 'UK' && !r.overseas && r.seas.length > 0);
    // From Germany's own airfields no fighter reaches any beach in Britain: the sky is no advice, and the line says why.
    const far = skyReach(g, 'GER');
    expect(home.every((r) => far(r.id) === null)).toBe(true);
    const x = toBritain(g);
    expect(x.account.helps.some((h) => h.id === 'sky')).toBe(false);
    expect(x.account.skyWon).toBeNull();
    const open = home.find((r) => !skyDenies(g, 'GER', r.id) && !!seaPath(g, 'GER', x.from, r, { max: NAVY.landing.range, surprise: true }))!;
    expect(open).toBeTruthy();
    const there = crossingAccount(g, 'GER', seaPath(g, 'GER', x.from, open, { max: NAVY.landing.range, surprise: true })!, open);
    expect(there.lines.find((l) => l.label === `The sky over ${open.name}`)!.effect).toBe('out of reach of your fighters from any airfield you hold');
    // Britain's own fighters reach a beach just across the water from the fields they stand on.
    const uk = skyReach(g, 'UK');
    const near = g.regions.find((r) => fighters(g, 'UK').some((f) => r.id !== f.base && km(g, f.base, r.id) <= airRange(f).ftr))!;
    expect(uk(near.id)!.how).toBe('now');
    // A field across the Channel in German hands: the beach opposite can be reached once fighters are brought up to it.
    const { field, beach, way } = acrossTheChannel(g);
    const fwd = skyReach(g, 'GER');
    expect(fwd(beach.id)).toEqual({ how: 'forward', base: field.id });
    expect(km(g, field.id, beach.id)).toBeLessThanOrEqual(airRange(fighters(g, 'GER')[0]).ftr);
    expect(home.filter((r) => fwd(r.id) !== null).every((r) => km(g, field.id, r.id) <= airRange(fighters(g, 'GER')[0]).ftr)).toBe(true);
    const acc = crossingAccount(g, 'GER', way, beach);
    const sky = acc.helps.find((h) => h.id === 'sky')!;
    expect(sky.text).toBe(`Bring fighters up to the airfields of ${field.name} and win the sky over ${beach.name}`);
    expect(acc.skyWon).toBeCloseTo(sky.risk, 12);
    // With a group on that field the advice is to fly, and the danger named is the danger a won sky leaves.
    expect(rebaseAir(g, fighters(g, 'GER')[0], field)).toBe(true);
    expect(skyReach(g, 'GER')(beach.id)).toEqual({ how: 'now', base: field.id });
    const now = crossingAccount(g, 'GER', way, beach);
    expect(now.helps.find((h) => h.id === 'sky')!.text).toBe(`Win the sky over ${beach.name} with a fight for the sky flown by your fighters`);
    g.sky[beach.id] = { nat: 'GER', turn: g.turn };
    expect(landingRisk(g, 'GER', way, beach)).toBeCloseTo(now.skyWon!, 12);
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
    // Since Version 76 the march is the nation's own: ground beside Germany's own is no crossing, but Egypt is one
    // until a German corps stands in Italy's Libya (an ally's front is not hers).
    expect(rows.every((x) => !x.to.nb.some((j) => g.regions[j].ctrl === 'GER'))).toBe(true);
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
      // Winning it is the advice only where fighters could reach it.
      expect(acc.helps.some((h) => h.id === 'sky')).toBe(skyReach(g, 'GER')(held.id) !== null);
      expect(crossingAccount(g, 'GER', path, held, () => ({ how: 'now', base: port.id })).helps.some((h) => h.id === 'sky')).toBe(true);
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

  it('shows the beach whose sky could be won beside the safest way as things stand', () => {
    const g = opened();
    harbours(g, 'GER');
    expect(crossingsOf(g, 'GER').some((x) => x.hope)).toBe(false);
    // A field across the Channel, the water between held by a German fleet, British fighters over the beach opposite.
    const { field, beach } = acrossTheChannel(g, true);
    expect(skyDenies(g, 'GER', beach.id)).toBe(true);
    const rows = crossingsOf(g, 'GER').filter((x) => x.owner === 'UK' && x.home);
    const hope = rows.find((x) => x.hope)!;
    expect(hope).toBeTruthy();
    expect(rows.filter((x) => x.hope).length).toBe(1);
    expect(hope.to).toBe(beach);
    expect(hope.from).toBe(field);
    // It is barred today by the sky alone, and with the sky won it is clearly safer than anything open today.
    expect(hope.barred).toBe('sky');
    expect(hope.bar).toMatch(/hold the sky over/);
    expect(hope.account.skyWon).not.toBeNull();
    const open = rows.filter((x) => !x.hope);
    expect(open.length).toBeGreaterThan(0);
    expect(open.some((x) => x.bar === null)).toBe(true);
    for (const x of open) if (x.bar === null) expect(hope.account.skyWon!).toBeLessThan(x.account.risk - 0.05);
    // Once the sky there is won it is simply the safest way, and no second row is needed.
    g.sky[beach.id] = { nat: 'GER', turn: g.turn };
    const won = crossingsOf(g, 'GER').filter((x) => x.owner === 'UK' && x.home);
    expect(won.some((x) => x.hope)).toBe(false);
    expect(won[0].to).toBe(beach);
    expect(won[0].bar).toBeNull();
    expect(won[0].account.risk).toBeCloseTo(hope.account.skyWon!, 12);
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
    expect(rows.filter((y) => y.planned).length).toBe(1);
    // A safer way to the same shore is still shown beside the plan, and only if it is safer.
    const beside = rows.filter((y) => y.owner === 'UK' && y.home && !y.planned);
    expect(beside.length).toBeLessThanOrEqual(1);
    if (beside.length) {
      expect(beside[0].to).toBe(x.to);
      expect(rows[0].bar !== null || beside[0].account.risk < rows[0].account.risk).toBe(true);
    }
    // The safest way named as the plan stands alone on the list.
    expect(orderCrossingPlan(g, x.from.id, x.to.id)).toBe(true);
    expect(crossings(g).filter((y) => y.owner === 'UK' && y.home).length).toBe(1);
    expect(orderCrossingPlan(g, x.from.id, other.id)).toBe(true);
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

  it('a player who has handed over the air war has fighters brought up to the fields in reach of his beach, a few a month, and none onto a full field', () => {
    const picture = (human: string | null): { g: Game; field: Region; beach: Region; groups: AirFleet[] } => {
      const g = opened(human);
      harbours(g, 'GER');
      const x = acrossTheChannel(g);
      return { g, ...x, groups: fighters(g, 'GER') };
    };
    const B = AIRWAR.ai.raids.beach!;
    const inReach = (x: { g: Game; beach: Region; groups: AirFleet[] }): AirFleet[] => x.groups.filter((f) => whyNotMission(x.g, f, 'superiority', x.beach.id) === null || (f.moved === x.g.turn && km(x.g, f.base, x.beach.id) <= airRange(f).ftr));
    const atHome = (x: { g: Game; groups: AirFleet[] }): number => x.groups.filter((f) => x.g.regions[f.base].owner === 'GER' && !x.g.regions[f.base].overseas).length;
    const a = picture('GER');
    const cap = a.g.nations.GER.capital;
    expect(a.groups.length).toBeGreaterThan(B.home + 1);
    expect(inReach(a).length).toBe(0);
    setDesk(a.g, 'air', true);
    // No plan named: nothing moves toward the Channel.
    airAI(a.g, 'GER');
    expect(a.groups.filter((f) => f.base === a.field.id).length).toBe(0);
    expect(planCrossing(a.g, 'GER', a.field, a.beach)).toBe(true);
    const overCapital = a.groups.filter((f) => f.base === cap).length;
    airAI(a.g, 'GER');
    // No more than the month's number go, each to a field within reach of the beach.
    const first = a.groups.filter((f) => f.moved === a.g.turn);
    expect(first.length).toBeGreaterThan(0);
    expect(first.length).toBeLessThanOrEqual(B.monthly);
    for (const f of first) expect(km(a.g, f.base, a.beach.id)).toBeLessThanOrEqual(airRange(f).ftr);
    // The capital keeps its fighters while another group can go.
    if (a.groups.length - overCapital >= first.length) expect(a.groups.filter((f) => f.base === cap).length).toBe(overCapital);
    // Month after month, until enough are in reach or only the homeland's guard is left; never fewer than that guard.
    for (let k = 0; k < 6; k++) { a.g.turn += 1; airAI(a.g, 'GER'); expect(atHome(a)).toBeGreaterThanOrEqual(B.home); }
    // No field holds more aircraft than it can serve, and whoever stayed behind stayed because there was no room left.
    const load = (g: Game, base: number): number => g.airFleets.reduce((s, f) => s + (f.base === base && f.ship === undefined ? f.ftr + f.bmr : 0), 0);
    const holds = (g: Game, base: number): number => Math.max(AIRWAR.fields.least, airfieldsHold(g, base));
    const up = inReach(a);
    expect(up.length).toBeGreaterThanOrEqual(first.length);
    for (const f of up) expect(load(a.g, f.base)).toBeLessThanOrEqual(holds(a.g, f.base));
    const fields = a.g.regions.filter((r) => r.ctrl === 'GER' && km(a.g, r.id, a.beach.id) <= airRange(a.groups[0]).ftr && airfieldsHold(a.g, r.id) > 0);
    const noRoom = a.groups.filter((f) => !up.includes(f)).every((f) => fields.every((r) => load(a.g, r.id) + f.ftr + f.bmr > holds(a.g, r.id)));
    expect(up.length === Math.min(B.groups, a.groups.length - B.home) || noRoom).toBe(true);
    // A field with no room is passed over: with the one field in reach full, nobody is sent to it.
    const b = picture('GER');
    setDesk(b.g, 'air', true);
    planCrossing(b.g, 'GER', b.field, b.beach);
    const inRange = b.g.regions.filter((r) => airBases(b.g, b.groups[0]).includes(r) && km(b.g, r.id, b.beach.id) <= airRange(b.groups[0]).ftr);
    const filler = b.g.airFleets.find((f) => f.nat === 'GER' && f.kind === 'bmr')!;
    for (const r of inRange) b.g.airFleets.push({ ...filler, id: 9000 + r.id, name: `Filler ${r.id}`, base: r.id, bmr: Math.max(AIRWAR.fields.least, airfieldsHold(b.g, r.id)), ftr: 0, moved: b.g.turn, flew: b.g.turn });
    airAI(b.g, 'GER');
    expect(b.groups.filter((f) => inRange.some((r) => r.id === f.base)).length).toBe(0);
    // The air war in his own hands: nothing is moved for him.
    const c = picture('GER');
    planCrossing(c.g, 'GER', c.field, c.beach);
    airAI(c.g, 'GER');
    expect(c.groups.some((f) => f.moved === c.g.turn)).toBe(false);
    // A computer Germany with its eye on the same beach leaves the homeland's fighters over the homeland, as before.
    const d = picture(null);
    d.g.flags['st:aim:GER'] = d.beach.id + 1;
    const was = d.groups.map((f) => f.base);
    airAI(d.g, 'GER');
    expect(d.groups.map((f) => f.base)).toEqual(was);
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

describe('the computer under the same rule', () => {
  const D = (openData as unknown as { descent: { after: number; risk: number; chance: number } }).descent;
  // (These are tests of the crossing. The test of strength of Version 83 would keep three corps off Germany's own
  // shore whatever the sea, so it is set aside here: strength-83.test.ts is its own. And the floor Britain keeps at
  // home in 1940 to 1942, ruling 229 part 1b, likewise: egypt-229.test.ts is its own.)
  const asked = { ...STRENGTH! }, floor = MIDDLE_EAST.floor.UK;
  beforeAll(() => { Object.assign(STRENGTH!, { sure: 0, bold: 0, even: 0 }); MIDDLE_EAST.floor.UK = []; });
  afterAll(() => { Object.assign(STRENGTH!, asked); MIDDLE_EAST.floor.UK = floor; });
  /**
   * Britain's staff a quiet year into its war with a landing force by its quay, every German harbour in reach emptied
   * with the country behind it, no German aircraft, and the German fleet in harbour.
   */
  const descent = (): { g: Game; quay: Region; force: Corps[]; shores: Region[] } => {
    const g = opened();
    g.turn += D.after + 1;
    // (Since Version 79 Britain's staff keeps history's dates for its great landings where they hold, and Germany's own
    // shore is not on its list: the rule is weighed at the stop of the history dial where it has no list.)
    expect(setStop(g, 4)).toBe(true);
    for (const c of g.corps) c.ap = c.apMax;
    g.airFleets = g.airFleets.filter((f) => f.nat !== 'GER');
    for (const f of g.fleets) if (f.nat === 'GER') sail(g, f, f.home, 'port');
    const quay = seaBase(g, 'UK')!;
    const force = [corps(g, 'UK', quay), corps(g, 'UK', quay), corps(g, 'UK', quay)];
    const shores = landingTargets(g, force).filter((r) => r.ctrl === 'GER' && r.port);
    for (const r of shores) { clear(r); for (const j of r.nb) if (g.regions[j].ctrl === 'GER') clear(g.regions[j]); }
    return { g, quay, force, shores };
  };
  /** The staff's month, with its coin for a descent falling the right way. */
  const month = (g: Game, nat: string): void => { const was = D.chance; D.chance = 1; try { runAI(g, nat); } finally { D.chance = was; } };
  /** The German fleet in harbour made strong enough that every crossing is over the staff's limit. */
  const armed = (x: { g: Game; force: Corps[]; shores: Region[] }): void => {
    const fleet = x.g.fleets.find((f) => f.nat === 'GER' && surfaceShips(f) > 0)!;
    for (let k = 0; k < 400 && Math.min(...x.shores.map((r) => planLanding(x.g, x.force, r).risk)) <= D.risk + 0.05; k++) fleet.ships.bb += 1;
  };

  it('does not cross while the danger is over its limit, and crosses once the sky is won and the danger is within it', () => {
    const a = descent();
    expect(a.shores.length).toBeGreaterThan(0);
    armed(a);
    // The danger the staff weighs is the danger the player is shown.
    for (const r of a.shores) {
      const p = planLanding(a.g, a.force, r);
      expect(p.why).toBeNull();
      expect(p.risk).toBeGreaterThan(D.risk);
      expect(p.account!.risk).toBeCloseTo(p.risk, 12);
      // (Winning the sky is advice only for a beach Britain's fighters can reach.)
      expect(p.account!.helps.some((h) => h.id === 'sky' && h.risk < p.risk)).toBe(skyReach(a.g, 'UK')(r.id) !== null);
    }
    month(a.g, 'UK');
    expect(a.force.every((c) => !c.dead && c.reg === a.quay.id)).toBe(true);
    // The same war with the sky over those beaches won: the fleets that come out are kept off, and the staff goes.
    const b = descent();
    armed(b);
    for (const r of b.shores) b.g.sky[r.id] = { nat: 'UK', turn: b.g.turn };
    const within = b.shores.filter((r) => planLanding(b.g, b.force, r).risk <= D.risk);
    expect(within.length).toBeGreaterThan(0);
    month(b.g, 'UK');
    const ashore = b.force.filter((c) => !c.dead && c.reg !== b.quay.id);
    expect(ashore.length).toBeGreaterThan(0);
    expect(within).toContain(b.g.regions[ashore[0].reg]);
  });

  it('opens no second front before history did at the two stops that keep to the book, and does at the others', () => {
    for (const stop of [1, 2]) {
      const { g, quay, force, shores } = descent();
      g.fleets = g.fleets.filter((f) => f.nat !== 'GER');
      expect(setStop(g, stop)).toBe(true);
      expect(onBook(g, 'second')).toBe(true);
      for (const r of shores) {
        expect(planLanding(g, force, r).risk).toBeLessThanOrEqual(D.risk);
        expect(shoreOpen(g, r.owner, !!r.overseas)).toBe(false);
      }
      month(g, 'UK');
      expect(force.every((c) => !c.dead && c.reg === quay.id)).toBe(true);
      expect(g.flags['st:aim:UK']).toBeUndefined();
      // Britain's own shore is never opened by the book: no computer Germany crosses to it while the book stands.
      g.turn = 90;
      expect(shoreOpen(g, 'UK', false)).toBe(false);
      g.flags['hist:off:second'] = 40;
      expect(shoreOpen(g, 'UK', false)).toBe(true);
    }
    // At "History's shape" the book is shut, and Britain's staff still keeps history's order for its great landings
    // (Version 79): Germany's own shore is not on its list, and it names no beach there.
    const shape = descent();
    shape.g.fleets = shape.g.fleets.filter((f) => f.nat !== 'GER');
    expect(setStop(shape.g, 3)).toBe(true);
    expect(onBook(shape.g, 'second')).toBe(false);
    month(shape.g, 'UK');
    expect(shape.shores.every((r) => r.ctrl === 'GER' && !r.units.some((u) => u.nat === 'UK'))).toBe(true);
    expect(shape.g.flags['st:aim:UK']).toBeUndefined();
    // Where neutrals have ambitions, and where nobody has read the books, it goes where it judges best.
    for (const stop of [4, 5]) {
      const free = descent();
      free.g.fleets = free.g.fleets.filter((f) => f.nat !== 'GER');
      expect(setStop(free.g, stop)).toBe(true);
      month(free.g, 'UK');
      expect(free.force.every((c) => !c.dead && free.shores.includes(free.g.regions[c.reg])), `stop ${stop}`).toBe(true);
    }
  });
});

describe('an army across a narrow sea is fed from the conquered coast it sailed from', () => {
  /**
   * Germany with a road of her own from Berlin to a harbour on the Channel, a British harbour across it in her
   * hands with a corps in it, and the Home Fleet in strength in every sea her own harbours stand on.
   */
  const lodged = (g: Game): { quay: Region; beach: Region; road: Region[]; c: Corps; channel: number } => {
    harbours(g, 'GER');
    const home = new Set(g.regions.filter((r) => r.port && r.owner === 'GER' && r.ctrl === 'GER').flatMap((r) => r.seas));
    const coasts = g.regions.filter((r) => r.owner === 'UK' && r.ctrl === 'UK' && !r.overseas && r.port && r.seas.length > 0);
    // (A stretch of water none of Germany's own harbours stands on.)
    const shared = (a: Region, b: Region): number | undefined => a.seas.find((z) => b.seas.includes(z) && !home.has(z));
    const quay = g.regions.find((r) => r.port && ['FRA', 'BEL', 'NED'].includes(r.owner) && coasts.some((c) => shared(r, c) !== undefined))!;
    const beach = coasts.find((c) => shared(quay, c) !== undefined)!;
    const channel = shared(quay, beach)!;
    // The road: the shortest way overland from Berlin to the quay, all of it put in German hands.
    const from = new Int32Array(g.regions.length).fill(-1);
    const q = [g.nations.GER.capital];
    from[q[0]] = q[0];
    for (let i = 0; i < q.length; i++) for (const j of g.regions[q[i]].nb) if (from[j] < 0) { from[j] = q[i]; q.push(j); }
    const road: Region[] = [];
    for (let at = quay.id; at !== g.nations.GER.capital; at = from[at]) road.push(g.regions[at]);
    for (const r of [...road, beach]) { clear(r); r.ctrl = 'GER'; r.since = g.turn - 3; }
    const c = corps(g, 'GER', beach);
    const fleet = g.fleets.find((f) => f.nat === 'UK' && surfaceShips(f) > 0)!;
    for (const z of home) g.fleets.push({ ...fleet, id: 9000 + z, ships: { ...fleet.ships, bb: 60 }, base: -1, home: z, zone: z, stance: 'patrol' as const });
    for (const f of g.fleets) if (f.nat === 'GER') sail(g, f, f.home, 'port');
    g.airFleets = g.airFleets.filter((f) => f.nat !== 'UK' && f.nat !== 'FRA');
    return { quay, beach, road, c, channel };
  };

  it('a harbour taken and reached by road serves the supply ships as a home port does', () => {
    const g = opened();
    const { quay, beach, road, c, channel } = lodged(g);
    // Every sea Germany's own harbours stand on is closed to her, and the Channel is not.
    for (const r of g.regions) if (r.port && r.owner === 'GER' && r.ctrl === 'GER') for (const z of r.seas) expect(closedTo(g, z, 'GER')).toBe(true);
    expect(closedTo(g, channel, 'GER')).toBe(false);
    expect(openSeas(g, 'GER')[channel]).toBe(1);
    g.supplyDirty = true;
    computeSupply(g);
    expect(c.supplied).toBe(true);
    // The road cut behind the quay: no stores come down to it, and the army across the water goes hungry.
    const behind = road.filter((r) => r !== quay && r.owner !== 'GER');
    expect(behind.length).toBeGreaterThan(0);
    for (const r of behind) r.ctrl = r.owner;
    expect(openSeas(g, 'GER')[channel]).toBe(0);
    g.supplyDirty = true;
    computeSupply(g);
    expect(c.supplied).toBe(false);
    for (const r of behind) r.ctrl = 'GER';
    expect(openSeas(g, 'GER')[channel]).toBe(1);
    // The Channel itself closed by the enemy's fleet: the quay is there, and nothing can sail from it.
    const fleet = g.fleets.find((f) => f.nat === 'UK' && surfaceShips(f) > 0)!;
    g.fleets.push({ ...fleet, id: 9500, ships: { ...fleet.ships, bb: 60 }, base: -1, home: channel, zone: channel, stance: 'patrol' as const });
    expect(closedTo(g, channel, 'GER')).toBe(true);
    expect(openSeas(g, 'GER')[channel]).toBe(0);
    g.fleets.pop();
    // An ally's harbour or a harbour held with the capital lost is no home port of this kind.
    const taken = road.filter((r) => r.owner !== 'GER');
    for (const r of taken) r.ctrl = 'ITA';
    expect(openSeas(g, 'GER')[channel]).toBe(0);
    for (const r of taken) r.ctrl = 'GER';
    expect(openSeas(g, 'GER')[channel]).toBe(1);
    g.regions[g.nations.GER.capital].ctrl = 'SOV';
    expect(openSeas(g, 'GER')[channel]).toBe(0);
    expect(beach.ctrl).toBe('GER');
  });

  it('is the grand campaign\'s rule alone: in history\'s scenarios supply ships sail from the homeland as before', () => {
    const hist = createGame(3, null);
    for (let k = 0; k < 3; k++) runTurnAI(hist);
    const { channel } = lodged(hist);
    expect(closedTo(hist, channel, 'GER')).toBe(false);
    expect(openSeas(hist, 'GER')[channel]).toBe(0);
  });

  it('the far side\'s home-port form is not this chart\'s: a home port across the water still sends out its own supply ships', () => {
    // (The director's ruling of 03:01Z, 22 November 2026; tests/far-home-ports.test.ts. With the far side off the
    // old form stands; the form for this chart is after release.)
    const g = opened();
    const rhodes = g.byName['Rhodes'];
    expect([rhodes.owner, rhodes.ctrl, rhodes.port]).toEqual(['ITA', 'ITA', true]);
    // No other harbour of Italy's stands on its seas, and every other sea is closed to Italy by hand: no ship from
    // Italy reaches it.
    for (const r of g.regions) if (r !== rhodes && r.port && r.owner === 'ITA') expect(r.seas.some((z) => rhodes.seas.includes(z)), r.name).toBe(false);
    const pic = new SeaPicture(g);
    pic.closed = (z: number): boolean => !rhodes.seas.includes(z);
    for (const z of rhodes.seas) expect(openSeas(g, 'ITA', pic)[z]).toBe(1);
    // The form itself, asked for by hand, would leave them shut: the switch is what keeps it off here.
    for (const z of rhodes.seas) expect(openSeas(g, 'ITA', pic, true)[z]).toBe(0);
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
    // (The ground landed on is written down, for the coast watch: ruling 229(e).)
    expect(g.flags[`landedAt:${x.to.id}:GER`]).toBe(g.turn);
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
