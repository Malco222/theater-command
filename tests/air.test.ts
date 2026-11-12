// The war in the air: air fleets and their bases, range and radar, the missions, the fight for
// the sky, bombing, strikes on shipping, replacements, the player's orders and saved games.
import { describe, expect, it } from 'vitest';
import { addCorps, assess, capture, createGame, declareWar } from '../src/campaign/game';
import { income } from '../src/campaign/economy';
import {
  AIR, airBattles, airFleetsOf, airIncomeLoss, airPlanName, airPool, airTargets, airUpkeep, assessAir, cover, crowding, km, previewAir,
  rebaseAir, setMission, skyOver, whyNotBase, whyNotMission,
} from '../src/campaign/air';
import { airOver, shipCount, zoneOf } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { airOptions, beginHuman, endHuman, humanUp, orderAir, orderAirBase, previewAirOrder, runTurnAI, stepAI } from '../src/campaign/turn';
import type { AirFleet, AirReport, Game } from '../src/campaign/types';

const air = (g: Game, name: string): AirFleet => g.airFleets.find((f) => f.name === name)!;
const at = (g: Game, name: string): number => g.byName[name].id;
const last = (g: Game): AirReport => g.airReports[g.airReports.length - 1];
const line = (lines: { label: string; value: string; effect?: string }[], label: string) => lines.find((l) => l.label === label);

function asPlayer(nat: string, seed = 7): Game {
  const g = createGame(seed, nat);
  let guard = 0;
  while (!humanUp(g) && guard++ < 60) stepAI(g);
  beginHuman(g);
  return g;
}

/** History to the end of June 1940, then the player takes Germany into July with France beaten. */
function july1940(seed = 3): Game {
  const g = createGame(seed, null);
  for (let t = 1; t <= 10; t++) runTurnAI(g);
  g.human = 'GER';
  beginHuman(g);
  return g;
}

/** Ends the player's month and runs the computer's nations until the player is up again. */
function nextMonth(g: Game): void {
  endHuman(g);
  let guard = 0;
  while (!humanUp(g) && guard++ < 60) stepAI(g);
  beginHuman(g);
}

describe('air fleets', () => {
  it('every air fleet starts at its base under its commander', () => {
    const g = createGame(1, null);
    const names = g.airFleets.map((f) => f.name);
    expect(names).toContain('Luftflotte 2');
    expect(names).toContain('Bomber Command');
    expect(names).toContain('Western District Air Force');
    expect(names).not.toContain('Luftflotte 5');
    expect(names).not.toContain('No. 10 Group');
    expect(new Set(g.airFleets.map((f) => f.id)).size).toBe(g.airFleets.length);
    for (const f of g.airFleets) {
      expect(g.regions[f.base].ctrl, f.name).toBe(f.nat);
      expect(f.mission).toBe('support');
      expect(f.ready).toBe(1);
      expect(f.ftr).toBeLessThanOrEqual(f.est.ftr);
      expect(f.bmr).toBeLessThanOrEqual(f.est.bmr);
    }
    const park = air(g, 'No. 11 Group');
    expect(g.regions[park.base].name).toBe('Kent');
    expect(park.leader?.name).toBe('Keith Park');
    expect([park.ftr, park.est.ftr, park.bmr]).toEqual([180, 220, 0]);
    expect(air(g, 'Luftflotte 2').leader?.name).toBe('Albert Kesselring');
    expect([air(g, 'Bomber Command').ftr, air(g, 'Bomber Command').bmr]).toEqual([0, 280]);
  });

  it('fighters lose their grip toward the limit of their range and have none beyond it', () => {
    expect(cover(0, 200)).toBe(1);
    expect(cover(120, 200)).toBe(1);
    expect(cover(160, 200)).toBeCloseTo(0.75, 5);
    expect(cover(200, 200)).toBeCloseTo(0.5, 5);
    expect(cover(201, 200)).toBe(0);
  });

  it('the map puts Calais close to Kent and the Rhineland far from it', () => {
    const g = createGame(1, null);
    expect(km(g, at(g, 'Calais'), at(g, 'Kent'))).toBeLessThan(120);
    expect(km(g, at(g, 'Calais'), at(g, 'London'))).toBeGreaterThan(120);
    expect(km(g, at(g, 'Calais'), at(g, 'London'))).toBeLessThan(200);
    expect(km(g, at(g, 'Westphalia'), at(g, 'Kent'))).toBeGreaterThan(300);
    expect(km(g, at(g, 'Cotentin'), at(g, 'Hampshire'))).toBeLessThan(200);
  });

  it('by the summer of 1940 the staff has built up the western air fleets, and new formations have appeared', () => {
    const g = createGame(4, null);
    for (let t = 1; t <= 10; t++) runTurnAI(g);
    expect(air(g, 'Luftflotte 2').est).toEqual({ ftr: 650, bmr: 750 });
    expect(air(g, 'Luftflotte 1').est).toEqual({ ftr: 80, bmr: 150 });
    expect(air(g, 'No. 11 Group').est.ftr).toBe(330);
    expect(air(g, 'Luftflotte 5')).toBeDefined();
    expect(g.regions[air(g, 'Luftflotte 5').base].name).toBe('Oslo');
    expect(g.regions[air(g, 'No. 10 Group').base].name).toBe('Hampshire');
    expect(g.headlines.some((h) => h.text === 'No. 10 Group forms in Hampshire.')).toBe(true);
    // Poland and France are out of the war, and their air forces with them.
    expect(airFleetsOf(g, 'POL')).toEqual([]);
    expect(airFleetsOf(g, 'FRA')).toEqual([]);
  });

  it('new aircraft need pilots: fighters without them stay in the reserve', () => {
    const g = asPlayer('UK');
    const f = air(g, 'No. 11 Group'), p = airPool(g, 'UK');
    f.ftr = 100;
    p.ftr = 300; p.pilots = 0;
    g.turn++;
    airUpkeep(g, 'UK');
    const trained = 80;
    // Only the month's newly trained pilots can take fighters up, and No. 11 Group is first in line.
    expect(f.ftr).toBe(100 + trained);
    expect(p.pilots).toBe(0);
    expect(p.ftr).toBeGreaterThan(0);
  });

  it('a resting fleet is first in line for replacements and recovers fastest', () => {
    const g = asPlayer('UK');
    const a = air(g, 'No. 11 Group'), b = air(g, 'No. 12 Group'), p = airPool(g, 'UK');
    a.ftr = 100; b.ftr = 60; a.ready = b.ready = 0.4;
    b.mission = 'rest';
    p.ftr = 300; p.pilots = 0;
    g.turn++;
    airUpkeep(g, 'UK');
    expect(b.ftr).toBe(140);
    expect(a.ftr).toBe(100);
    expect(b.ready).toBeCloseTo(0.4 + AIR.ready.rest, 5);
    expect(a.ready).toBeCloseTo(0.4 + AIR.ready.support, 5);
  });

  it('a fleet whose base falls flies out to the nearest ground still held, leaving aircraft behind', () => {
    const g = asPlayer('UK');
    const f = air(g, 'No. 11 Group');
    const had = f.ftr;
    declareWar(g, 'GER', 'UK');
    capture(g, g.byName['Kent'], 'GER');
    g.turn++;
    airUpkeep(g, 'UK');
    expect(g.regions[f.base].name).not.toBe('Kent');
    expect(g.regions[f.base].ctrl).toBe('UK');
    expect(f.lost.ftr).toBe(Math.round(had * 0.1));
    expect(g.headlines.some((h) => h.text.startsWith('With Kent lost, No. 11 Group falls back'))).toBe(true);
  });
});

describe('bases and missions', () => {
  it('a nation at peace flies no missions', () => {
    const g = asPlayer('ITA');
    const f = air(g, '1st Air Squadron');
    expect(g.nations.ITA.atWar).toEqual([]);
    expect(whyNotMission(g, f, 'bomb', at(g, 'Marseille'))).toBe('The nation is at peace.');
    expect(airTargets(g, f, 'superiority')).toEqual([]);
    expect(orderAir(g, f.id, 'superiority', at(g, 'Marseille'))).toBe(false);
    expect(orderAir(g, f.id, 'rest')).toBe(true);
    expect(f.mission).toBe('rest');
  });

  it('a mission needs an enemy target the fleet can reach with the right aircraft', () => {
    const g = asPlayer('GER');
    const lf1 = air(g, 'Luftflotte 1');
    // Warsaw is beyond the fighters from Berlin, but the bombers can go alone.
    expect(km(g, lf1.base, at(g, 'Warsaw'))).toBeGreaterThan(AIR.nations.GER.ftrRange);
    expect(whyNotMission(g, lf1, 'superiority', at(g, 'Warsaw'))).toBeNull();
    expect(whyNotMission(g, lf1, 'bomb', at(g, 'Warsaw'))).toBeNull();
    expect(whyNotMission(g, lf1, 'bomb', at(g, 'Brussels'))).toBe('That region is not in enemy hands.');
    const bare = g.regions.find((r) => r.ctrl === 'POL' && r.fac === 0)!;
    expect(whyNotMission(g, lf1, 'bomb', bare.id)).toBe('There is no industry there to bomb.');
    expect(whyNotMission(g, lf1, 'strike', zoneOf(g, 'baltic'))).toBeNull();
    expect(whyNotMission(g, lf1, 'strike', zoneOf(g, 'biscay'))).not.toBeNull();
    const targets = airTargets(g, lf1, 'bomb');
    expect(targets).toContain(at(g, 'Warsaw'));
    for (const t of targets) { expect(g.nations.GER.atWar).toContain(g.regions[t].ctrl); expect(g.regions[t].fac).toBeGreaterThan(0); }
    // Fighters cannot bomb.
    const pursuit = air(g, 'Pursuit Brigade');
    expect(whyNotMission(g, pursuit, 'bomb', at(g, 'Berlin'))).toBe('It has no bombers.');
    expect(whyNotMission(g, pursuit, 'strike', zoneOf(g, 'baltic'))).toBe('It has no bombers.');
    expect(setMission(g, lf1, 'bomb', at(g, 'Brussels'))).toBe(false);
    expect(lf1.mission).toBe('support');
  });

  it('a fleet can move to friendly airfields once a month, and settling in costs readiness', () => {
    const g = asPlayer('GER');
    const f = air(g, 'Luftflotte 2');
    expect(whyNotBase(g, f, g.byName['Westphalia'])).toBe('It is already based there.');
    expect(whyNotBase(g, f, g.byName['Brussels'])).toBe('That ground is not in friendly hands.');
    const opts = airOptions(g, f.id, 'support');
    expect(opts.targets).toEqual([]);
    expect(opts.bases).toContain(at(g, 'Berlin'));
    expect(opts.bases).not.toContain(at(g, 'Westphalia'));
    expect(orderAirBase(g, f.id, at(g, 'Berlin'))).toBe(true);
    expect(g.regions[f.base].name).toBe('Berlin');
    expect(f.ready).toBeCloseTo(1 - AIR.ready.move, 5);
    expect(whyNotBase(g, f, g.byName['Frankfurt'])).toBe('It has already changed base this month.');
    expect(orderAirBase(g, f.id, at(g, 'Frankfurt'))).toBe(false);
  });

  it('airfields taken this month are not yet in use', () => {
    const g = asPlayer('GER');
    const f = air(g, 'Luftflotte 1');
    const poznan = g.regions.find((r) => r.ctrl === 'POL' && r.nb.some((j) => g.regions[j].ctrl === 'GER'))!;
    capture(g, poznan, 'GER');
    expect(whyNotBase(g, f, poznan)).toBe('Airfields taken this month are not yet in use.');
    g.turn++;
    expect(whyNotBase(g, f, poznan)).toBeNull();
  });

  it('moving out of reach of a target cancels the orders against it', () => {
    const g = july1940();
    const f = air(g, 'Luftflotte 2');
    expect(orderAirBase(g, f.id, at(g, 'Calais'))).toBe(true);
    expect(orderAir(g, f.id, 'superiority', at(g, 'Kent'))).toBe(true);
    // Flanders is still within reach of Kent, so the orders stand.
    g.turn++;
    expect(rebaseAir(g, f, g.byName['Flanders'])).toBe(true);
    expect(f.mission).toBe('superiority');
    // Vienna is not.
    g.turn++;
    expect(rebaseAir(g, f, g.byName['Vienna'])).toBe(true);
    expect(f.mission).toBe('support');
    expect(f.target).toBe(-1);
  });

  it('the player gives orders only to his own air fleets, in his own month', () => {
    const g = july1940();
    const lf2 = air(g, 'Luftflotte 2'), park = air(g, 'No. 11 Group');
    expect(orderAir(g, park.id, 'rest')).toBe(false);
    expect(orderAirBase(g, park.id, at(g, 'London'))).toBe(false);
    expect(orderAir(g, lf2.id, 'night', at(g, 'London'))).toBe(true);
    endHuman(g);
    expect(orderAir(g, lf2.id, 'rest')).toBe(false);
    expect(airOptions(g, lf2.id, 'bomb')).toEqual({ targets: [], bases: [] });
  });

  it('too many aircraft on one region\'s airfields cannot all fly', () => {
    const g = july1940();
    const lf2 = air(g, 'Luftflotte 2'), lf3 = air(g, 'Luftflotte 3');
    orderAirBase(g, lf2.id, at(g, 'Calais'));
    expect(crowding(g, lf2)).toBe(1);
    const alone = previewAirOrder(g, lf2.id, 'superiority', at(g, 'Kent'))!;
    expect(line(alone.lines, 'Airfields')).toBeUndefined();
    orderAirBase(g, lf3.id, at(g, 'Calais'));
    const all = lf2.ftr + lf2.bmr + lf3.ftr + lf3.bmr;
    expect(all).toBeGreaterThan(AIR.airfields.holds);
    expect(crowding(g, lf2)).toBeCloseTo(AIR.airfields.holds / all, 5);
    expect(crowding(g, lf3)).toBe(crowding(g, lf2));
    orderAir(g, lf3.id, 'superiority', at(g, 'Kent'));
    const both = previewAirOrder(g, lf2.id, 'superiority', at(g, 'Kent'))!;
    expect(line(both.lines, 'Airfields')?.value).toBe('The airfields of Calais are crowded');
    // Two fleets packed into Calais put fewer fighters over Kent than one of them with room to work.
    expect(both.E).toBeLessThan(lf2.ftr + lf3.ftr);
    expect(both.E).toBeCloseTo((lf2.ftr + lf3.ftr) * crowding(g, lf2), 3);
  });
});

describe('an air campaign', () => {
  it('the ticket shows range, radar and the commanders', () => {
    const g = july1940();
    const f = air(g, 'Luftflotte 2');
    orderAirBase(g, f.id, at(g, 'Calais'));
    const kent = previewAirOrder(g, f.id, 'superiority', at(g, 'Kent'))!;
    expect(kent.why).toBeNull();
    expect(kent.place).toBe('Kent');
    expect(line(kent.lines, 'Forces')?.value).toMatch(/^650 fighters and 750 bombers against \d+ fighters$/);
    expect(line(kent.lines, 'Range')?.value).toBe('The target is well inside fighter range');
    expect(line(kent.lines, 'Radar')?.value).toBe('Chain Home gives warning');
    expect(line(kent.lines, 'Commanders')?.value).toMatch(/^Albert Kesselring against .*Keith Park/);
    expect(line(kent.lines, 'Readiness')?.value).toContain('Luftflotte 2');
    expect(kent.radar?.name).toBe('Chain Home');
    expect(kent.E).toBe(650);
    expect(kent.B).toBe(750);
    // London is near the limit of the fighters from Calais.
    const london = previewAirOrder(g, f.id, 'bomb', at(g, 'London'))!;
    expect(line(london.lines, 'Range')?.value).toMatch(/^The fighters are at \d\d% effect over the target$/);
    expect(london.E).toBeLessThan(650);
    expect(london.E).toBeGreaterThan(325);
    // The Midlands are beyond them.
    const midlands = previewAirOrder(g, f.id, 'bomb', at(g, 'Midlands'))!;
    expect(line(midlands.lines, 'Range')).toEqual({ label: 'Range', value: 'The target is beyond fighter range', effect: 'The bombers go alone' });
    expect(midlands.E).toBe(0);
    const night = previewAirOrder(g, f.id, 'night', at(g, 'London'))!;
    expect(line(night.lines, 'Night')).toBeDefined();
    expect(line(night.lines, 'Range')).toBeUndefined();
  });

  it('without radar only part of the defence gets up in time', () => {
    const g = asPlayer('GER');
    expect(g.nations.GER.atWar).toEqual(expect.arrayContaining(['FRA', 'UK']));
    const france = assessAir(g, [air(g, 'Luftflotte 3')], 'superiority', at(g, 'Lorraine'));
    expect(france.radar).toBeNull();
    expect(line(france.lines, 'Radar')?.value).toBe('No radar warning');
    const tetu = france.def.find((x) => x.f.name === 'Eastern Air Zone')!;
    expect(tetu.w).toBeCloseTo(AIR.radar.without, 5);
    const britain = assessAir(g, [air(g, 'Luftflotte 2')], 'superiority', at(g, 'Kent'));
    expect(britain.def.find((x) => x.f.name === 'No. 11 Group')!.w).toBe(1);
    // Fighters on other duties do not come up at all.
    air(g, 'No. 11 Group').mission = 'rest';
    expect(assessAir(g, [air(g, 'Luftflotte 2')], 'superiority', at(g, 'Kent')).def.some((x) => x.f.name === 'No. 11 Group')).toBe(false);
  });

  it('beyond fighter range the bombers go alone and suffer for it', () => {
    const g = july1940();
    const f = air(g, 'Luftflotte 2');
    const alone = previewAir(g, f, 'bomb', at(g, 'London'));
    expect(alone.E).toBe(0);
    orderAirBase(g, f.id, at(g, 'Calais'));
    const escorted = previewAir(g, f, 'bomb', at(g, 'London'));
    expect(escorted.E).toBeGreaterThan(0);
    expect(escorted.D).toBe(alone.D);
    expect(alone.bomberLoss).toBeGreaterThan(escorted.bomberLoss * 1.1);
    expect(line(alone.lines, 'Range')?.effect).toBe('The bombers go alone');
  });

  it('the same seed gives the same battle, and the air war never touches the land war\'s dice', () => {
    const play = (): Game => {
      const g = july1940(5);
      orderAirBase(g, air(g, 'Luftflotte 2').id, at(g, 'Calais'));
      orderAir(g, air(g, 'Luftflotte 2').id, 'superiority', at(g, 'Kent'));
      const dice = g.rng.state;
      const reps = airBattles(g, 'GER');
      expect(reps.length).toBe(1);
      expect(g.rng.state).toBe(dice);
      return g;
    };
    const a = play(), b = play();
    expect(last(a)).toEqual(last(b));
    expect(a.airFleets).toEqual(b.airFleets);
    expect(a.airPool).toEqual(b.airPool);
  });

  it('a month over Kent costs both sides heavily, and the defender\'s pilots mostly fly again', () => {
    const g = july1940();
    const f = air(g, 'Luftflotte 2');
    orderAirBase(g, f.id, at(g, 'Calais'));
    orderAir(g, f.id, 'superiority', at(g, 'Kent'));
    const uk0 = { ...airPool(g, 'UK') }, ge0 = { ...airPool(g, 'GER') };
    const park0 = air(g, 'No. 11 Group').ready;
    endHuman(g);
    const r = last(g);
    expect(r.mission).toBe('superiority');
    expect(r.place).toBe('Kent');
    expect(r.attNat).toBe('GER');
    expect(r.defNat).toBe('UK');
    expect(r.sky).toBe(false);
    expect(r.hold).toBeGreaterThan(0.4);
    expect(r.hold).toBeLessThan(AIR.sky.win);
    const uk = airPool(g, 'UK'), ge = airPool(g, 'GER');
    const ukLost = uk.lostFtr - uk0.lostFtr, geLost = ge.lostFtr - ge0.lostFtr;
    expect(ukLost).toBeGreaterThan(100);
    expect(geLost).toBeGreaterThan(50);
    expect(ge.lostBmr - ge0.lostBmr).toBeGreaterThan(100);
    expect(r.att[0].lostFtr).toBe(geLost);
    expect(r.def.reduce((s, x) => s + x.lostFtr, 0)).toBe(ukLost);
    // About half the British pilots come down over their own fields and fly again; few Germans get home.
    const ukDead = (uk.lostPilots - uk0.lostPilots) / ukLost, geDead = (ge.lostPilots - ge0.lostPilots) / geLost;
    expect(ukDead).toBeGreaterThan(0.35);
    expect(ukDead).toBeLessThan(0.6);
    expect(geDead).toBeGreaterThan(0.8);
    // Bombed airfields tell on the group based there.
    expect(air(g, 'No. 11 Group').ready).toBeLessThan(park0 - 0.1);
    expect(r.result).toContain('The airfields of No. 11 Group were bombed');
    expect(g.headlines.some((h) => h.kind === 'battle' && h.text.startsWith('Luftflotte 2 fights for the sky over Kent'))).toBe(true);
  });

  it('bombing by night costs little and does little', () => {
    const fly = (m: 'bomb' | 'night'): { g: Game; r: AirReport } => {
      const g = july1940();
      const f = air(g, 'Luftflotte 2');
      orderAirBase(g, f.id, at(g, 'Calais'));
      orderAir(g, f.id, m, at(g, 'London'));
      endHuman(g);
      return { g, r: last(g) };
    };
    const day = fly('bomb'), night = fly('night');
    expect(night.r.att[0].lostBmr).toBeLessThan(day.r.att[0].lostBmr / 3);
    expect(night.r.att[0].lostFtr).toBe(0);
    expect(night.r.def.reduce((s, x) => s + x.lostFtr, 0)).toBe(0);
    expect(day.r.def.reduce((s, x) => s + x.lostFtr, 0)).toBeGreaterThan(0);
    expect(night.g.raids.UK.share).toBeGreaterThan(0);
    expect(night.g.raids.UK.share).toBeLessThan(day.g.raids.UK.share);
    expect(night.r.result).toMatch(/^The bombing stopped \d+% of London's output for a month/);
  });

  it('bombing cuts the victim\'s next month of production', () => {
    const g = july1940();
    const f = air(g, 'Luftflotte 2');
    orderAirBase(g, f.id, at(g, 'Calais'));
    orderAir(g, f.id, 'bomb', at(g, 'London'));
    const before = income(g, 'UK');
    expect(airIncomeLoss(g, 'UK')).toBe(0);
    endHuman(g);
    // Germany moves before Britain, so the loss falls on Britain's production this same month.
    expect(g.raids.UK.turn).toBe(g.turn);
    const share = airIncomeLoss(g, 'UK');
    expect(share).toBeGreaterThan(0.005);
    expect(share).toBeLessThanOrEqual(AIR.damage.industry.maxShare);
    expect(income(g, 'UK')).toBeCloseTo(before * (1 - share), 5);
    g.turn++;
    expect(airIncomeLoss(g, 'UK')).toBe(0);
  });

  it('a strike in the Channel hurts the coastal trade and any warships caught there', () => {
    const g = july1940();
    const f = air(g, 'Luftflotte 2');
    const channel = zoneOf(g, 'channel');
    orderAirBase(g, f.id, at(g, 'Calais'));
    expect(airOptions(g, f.id, 'strike').targets).toContain(channel);
    const ships = (): number => g.fleets.filter((x) => x.nat === 'UK').reduce((s, x) => s + shipCount(x.ships), 0);
    const had = ships();
    expect(orderAir(g, f.id, 'strike', channel)).toBe(true);
    endHuman(g);
    const r = last(g);
    expect(r.zone).toBe(channel);
    expect(r.region).toBe(-1);
    expect(r.place).toBe('the English Channel');
    expect(r.result).toContain('Merchant shipping losses cost United Kingdom');
    expect(g.raids.UK.share).toBeGreaterThan(0);
    expect(ships()).toBeLessThanOrEqual(had);
    // Fighters cover ships at sea less well than their own airfields.
    expect(r.def.length).toBeGreaterThan(0);
  });
});

describe('the sky', () => {
  it('is won when the defender has nothing left to put up, and tells at sea and on land', () => {
    const g = july1940();
    const f = air(g, 'Luftflotte 2');
    const kent = at(g, 'Kent'), channel = zoneOf(g, 'channel');
    for (const x of airFleetsOf(g, 'UK')) x.ftr = 0;
    airPool(g, 'UK').ftr = 0;
    const ukAir = airOver(g, channel, 'UK'), geAir = airOver(g, channel, 'GER');
    orderAirBase(g, f.id, at(g, 'Calais'));
    orderAir(g, f.id, 'superiority', kent);
    expect(skyOver(g, kent)).toBeNull();
    airBattles(g, 'GER');
    const r = last(g);
    expect(r.sky).toBe(true);
    expect(r.level).toBe('Decisive victory');
    expect(g.sky[kent]).toEqual({ nat: 'GER', turn: g.turn });
    expect(skyOver(g, kent)).toBe('GER');
    expect(g.headlines.some((h) => h.text.includes('The German air force now holds the sky there.'))).toBe(true);
    // Over the Channel the Luftwaffe now flies from its own coast and over Kent as well; the RAF is driven off it.
    expect(airOver(g, channel, 'GER')).toBeGreaterThan(geAir);
    expect(airOver(g, channel, 'UK')).toBeLessThan(ukAir);
    // A landing in Kent would have the sky.
    const corps = addCorps(g, 'GER', 'Test Corps', 'inf', 3, 'London', {});
    g.byName['London'].ctrl = 'GER';
    const t = assess(g, [corps], g.byName['Kent']);
    expect(line(t.lines, 'Air')?.value).toBe('The German air force has won the sky here');
    g.byName['London'].ctrl = 'UK';
    // It lasts through the next month, and then has to be won again.
    g.turn++;
    expect(skyOver(g, kent)).toBe('GER');
    g.turn++;
    expect(skyOver(g, kent)).toBeNull();
  });

  it('means nothing once the ground itself has changed hands', () => {
    const g = july1940();
    const kent = at(g, 'Kent');
    g.sky[kent] = { nat: 'GER', turn: g.turn };
    expect(skyOver(g, kent)).toBe('GER');
    capture(g, g.byName['Kent'], 'GER');
    expect(skyOver(g, kent)).toBeNull();
  });
});

describe('the computer\'s air staff', () => {
  it('flies history\'s plan against Britain in the summer of 1940', () => {
    const g = createGame(2, null);
    for (let t = 1; t <= 10; t++) runTurnAI(g);
    expect(g.airReports).toEqual([]);
    expect(airPlanName(g, 'GER')).toBe('The Channel battles');
    runTurnAI(g);
    expect(['Calais', 'Flanders', 'Somme']).toContain(g.regions[air(g, 'Luftflotte 2').base].name);
    expect(['Cotentin', 'Normandy', 'Rouen']).toContain(g.regions[air(g, 'Luftflotte 3').base].name);
    expect(g.airReports.map((r) => [r.turn, r.mission, r.place])).toEqual([[11, 'strike', 'the English Channel']]);
    expect(airPlanName(g, 'GER')).toBe('Eagle Attack');
    runTurnAI(g);
    const august = g.airReports.filter((r) => r.turn === 12).map((r) => `${r.mission} ${r.place}`);
    expect(august).toContain('superiority Kent');
    expect(august).toContain('superiority Hampshire');
    expect(airPlanName(g, 'UK')).toBeNull();
  });

  it('leaves the player\'s air fleets alone', () => {
    const g = july1940();
    const f = air(g, 'Luftflotte 2');
    orderAir(g, f.id, 'rest');
    nextMonth(g);
    expect(f.mission).toBe('rest');
    expect(g.regions[f.base].name).toBe('Westphalia');
    expect(g.airReports).toEqual([]);
  });
});

describe('saved games', () => {
  it('keep the air war and play on the same', () => {
    const a = createGame(9, null);
    for (let t = 1; t <= 12; t++) runTurnAI(a);
    expect(a.airReports.length).toBeGreaterThan(1);
    const b = loadGame(saveGame(a))!;
    expect(b.airFleets).toEqual(a.airFleets);
    expect(b.airPool).toEqual(a.airPool);
    expect(b.airReports).toEqual(a.airReports);
    expect(b.sky).toEqual(a.sky);
    expect(b.raids).toEqual(a.raids);
    expect(b.nextAir).toBe(a.nextAir);
    for (let t = 0; t < 3; t++) { runTurnAI(a); runTurnAI(b); }
    expect(saveGame(b)).toBe(saveGame(a));
  });
});
