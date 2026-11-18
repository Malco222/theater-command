// The computer's air staff at sea and behind its army (ruling 52; the grand campaign). A computer staff searches the
// seas as a player does, and its search counts for its side as his does; it strikes at an enemy's shipping where his
// convoys run; its fighters come up behind its army whether or not an enemy air force is over it; and where a
// stretch of its front has stood beyond every airfield for months, it orders one from the arsenal.
import { beforeAll, describe, expect, it } from 'vitest';
import airwar from '../data/campaign/airwar.json';
import carriers from '../data/campaign/carriers.json';
import { AIR, AIRWAR, CARRIERS, airAI, airFleetsOf, airRange, cover, hasAirfield, km, whyNotBase } from '../src/campaign/air';
import { aiArsenal, bareFront } from '../src/campaign/arsenal';
import { searchOf, seaStaff } from '../src/campaign/carriers';
import { PRODUCTION } from '../src/campaign/economy';
import { allied, atWar, createGame, declareWar, removeCorps } from '../src/campaign/game';
import { NAVY, zoneOf } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { AirFleet, Game } from '../src/campaign/types';

const grand = (human: string | null = null, seed = 3): Game => startOpen(createGame(seed, human));
/** Makes two nations at peace, as a treaty would. */
const peace = (g: Game, a: string, b: string): void => { g.nations[a].atWar = g.nations[a].atWar.filter((x) => x !== b); g.nations[b].atWar = g.nations[b].atWar.filter((x) => x !== a); };
const landFighters = (g: Game, nat: string): AirFleet[] => airFleetsOf(g, nat).filter((f) => f.kind === 'ftr' && f.ftr > 0 && f.ship === undefined);
const fieldOrders = (g: Game, nat: string) => g.nations[nat].queue.filter((b) => b.what === 'work' && b.item === 'field');
/** The provinces where a nation's field corps stand beside ground an enemy holds. */
const frontOf = (g: Game, nat: string): number[] => [...new Set(g.corps.filter((c) => c.nat === nat && !c.dead && c.kind !== 'fort' && g.regions[c.reg].nb.some((j) => atWar(g, nat, g.regions[j].ctrl))).map((c) => c.reg))];

// One computer-run grand campaign played to the summer of 1941.
let war = '';
beforeAll(() => {
  const g = grand(null, 7);
  while (g.turn <= 22) runTurnAI(g);
  war = saveGame(g);
}, 120000);

describe('the data', () => {
  it('says how a computer staff searches, strikes at trade, follows its army and lays out fields', () => {
    expect(carriers.ai.trade).toBe(true);
    expect(carriers.ai.searches).toBeGreaterThan(0);
    expect(airwar.ai.basing.presence).toBeGreaterThan(0);
    expect(airwar.ai.basing.presence).toBeLessThan(1);
    expect(airwar.ai.basing.fields).toBeGreaterThanOrEqual(1);
    expect(airwar.ai.basing.idle).toBe(true);
    expect(CARRIERS.ai.searches).toBe(carriers.ai.searches);
    expect(AIRWAR.ai.basing.fields).toBe(airwar.ai.basing.fields);
  });
});

describe('a computer staff\'s search of the sea', () => {
  it('is flown as a player\'s is: so many seas a month, each by one group, at the same cost in readiness', () => {
    const g = grand();
    expect(g.nations.UK.atWar).toContain('GER');
    const ready = new Map(airFleetsOf(g, 'UK').map((f) => [f.id, f.ready]));
    seaStaff(g, 'UK');
    const by = g.intel.seaBy!.UK;
    const seas = Object.keys(by).map(Number);
    expect(seas.length).toBeGreaterThan(0);
    expect(seas.length).toBeLessThanOrEqual(carriers.ai.searches);
    for (const z of seas) expect(by[z]).toBe(g.turn);
    const flew = airFleetsOf(g, 'UK').filter((f) => f.scouted === g.turn);
    expect(flew.length).toBe(seas.length);
    for (const f of flew) expect(f.ready).toBeCloseTo(Math.max(AIR.ready.floor, ready.get(f.id)! - CARRIERS.search.ready), 9);
    // Asked again in the same month, the staff flies no more than its month's searches.
    seaStaff(g, 'UK');
    expect(Object.keys(g.intel.seaBy!.UK).length).toBeLessThanOrEqual(carriers.ai.searches);
  });

  it('counts for its own side as a player\'s does, once for a sea, and for nobody else', () => {
    const g = grand();
    const z = zoneOf(g, 'nsea');
    const before = { UK: searchOf(g, 'UK', z), FRA: searchOf(g, 'FRA', z), GER: searchOf(g, 'GER', z) };
    expect(allied(g, 'UK', 'FRA')).toBe(true);
    g.intel.seaBy = { UK: { [z]: g.turn } };
    expect(searchOf(g, 'UK', z)).toBeCloseTo(before.UK + CARRIERS.search.flight, 9);
    expect(searchOf(g, 'FRA', z)).toBeCloseTo(before.FRA + CARRIERS.search.flight, 9);
    expect(searchOf(g, 'GER', z)).toBeCloseTo(before.GER, 9);
    // Two staffs of one side over the same sea see no more than one.
    g.intel.seaBy.FRA = { [z]: g.turn };
    expect(searchOf(g, 'UK', z)).toBeCloseTo(before.UK + CARRIERS.search.flight, 9);
    // And last month's search is last month's.
    g.intel.seaBy = { UK: { [z]: g.turn - 1 } };
    expect(searchOf(g, 'UK', z)).toBeCloseTo(before.UK, 9);
  });

  it('is never flown for the player, whose reconnaissance is his own', () => {
    const g = grand('UK');
    seaStaff(g, 'UK', 'land');
    seaStaff(g, 'UK', 'decks');
    expect(g.intel.seaBy?.UK).toBeUndefined();
    expect(airFleetsOf(g, 'UK').some((f) => f.scouted === g.turn)).toBe(false);
  });

  it('is flown through a war, and kept in a saved game', () => {
    const g = loadGame(war)!;
    runTurnAI(g);
    const by = g.intel.seaBy ?? {};
    const month = g.turn - 1;
    const flown = Object.entries(by).filter(([, seas]) => Object.values(seas).some((t) => t === month));
    expect(flown.length).toBeGreaterThan(1);
    for (const [nat, seas] of flown) {
      expect(g.nations[nat].atWar.length + (g.nations[nat].alive ? 0 : 1)).toBeGreaterThan(0);
      expect(Object.values(seas).filter((t) => t === month).length).toBeLessThanOrEqual(carriers.ai.searches);
    }
    const again = loadGame(saveGame(g))!;
    expect(again.intel.seaBy).toEqual(g.intel.seaBy);
  }, 60000);
});

describe('a computer staff\'s bombers', () => {
  it('strike at shipping where an enemy\'s convoys run or his coasters work his own shore', () => {
    const g = loadGame(war)!;
    const strikes = g.airReports.filter((r) => r.mission === 'strike');
    expect(strikes.length).toBeGreaterThan(0);
    for (const r of strikes) {
      expect(r.zone).toBeGreaterThanOrEqual(0);
      expect(g.nations[r.attNat]).toBeTruthy();
      const sea = g.seas[r.zone];
      const lane = NAVY.convoys.some((c) => c.zones.includes(sea.key));
      const shore = sea.coast.some((id) => g.regions[id].port);
      expect(lane || shore).toBe(true);
    }
  });

  it('strike at no shipping in a history campaign, which keeps the air war it had', () => {
    const g = createGame(1, null);
    for (let k = 0; k < 6; k++) runTurnAI(g);
    expect(g.airReports.filter((r) => r.mission === 'strike')).toEqual([]);
    expect(g.intel?.seaBy).toBeUndefined();
  }, 60000);
});

describe('a computer staff\'s fighters', () => {
  it('come up behind its army though no enemy aircraft can reach it', () => {
    const g = grand();
    // Germany at war with Poland alone, and Poland with no air force left: nothing threatens Germany from the air.
    for (const e of [...g.nations.GER.atWar]) if (e !== 'POL') peace(g, 'GER', e);
    g.airFleets = g.airFleets.filter((f) => !atWar(g, 'GER', f.nat));
    const front = frontOf(g, 'GER');
    expect(front.length).toBeGreaterThan(3);
    const inReach = (f: AirFleet): boolean => front.some((id) => cover(km(g, f.base, id), airRange(f, g).ftr) > 0);
    const before = landFighters(g, 'GER').filter(inReach).length;
    expect(before).toBeLessThan(landFighters(g, 'GER').length);
    // The groups that can reach ground the enemy holds from where they stand.
    const foes = g.regions.filter((r) => atWar(g, 'GER', r.ctrl)).map((r) => r.id);
    const engaged = (f: AirFleet): boolean => foes.some((id) => km(g, f.base, id) <= airRange(f, g).ftr);
    // (One group stands on the Silesian border already.)
    landFighters(g, 'GER').find((f) => g.regions[f.base].name === 'Vienna')!.base = g.byName['Upper Silesia'].id;
    const was = landFighters(g, 'GER').filter(engaged).map((f) => f.id);
    expect(was.length).toBeGreaterThan(0);
    airAI(g, 'GER');
    expect(landFighters(g, 'GER').filter(inReach).length).toBe(landFighters(g, 'GER').length);
    expect(landFighters(g, 'GER').length).toBeGreaterThan(before);
    // No move behind the army has taken one of them out of reach of the enemy.
    for (const f of landFighters(g, 'GER')) if (was.includes(f.id)) expect(engaged(f), g.regions[f.base].name).toBe(true);
  });

  it('go by stages to a front beyond a month\'s flight', () => {
    const g = grand();
    declareWar(g, 'ITA', 'UK');
    // Nothing threatens Italy from the air, and a field has been laid out behind the Egyptian frontier.
    g.airFleets = g.airFleets.filter((f) => !atWar(g, 'ITA', f.nat));
    const bardia = g.byName['Bardia'];
    g.flags[`af:${bardia.id}`] = 2;
    expect(hasAirfield(g, bardia.id)).toBe(true);
    const far = landFighters(g, 'ITA').filter((f) => km(g, f.base, bardia.id) > AIR.ferry);
    expect(far.length).toBeGreaterThan(0);
    const was = new Map(landFighters(g, 'ITA').map((f) => [f.id, km(g, f.base, bardia.id)]));
    airAI(g, 'ITA');
    // The groups too far to fly there in a month have come a stage nearer; none has gone further off.
    for (const f of far) expect(km(g, f.base, bardia.id), g.regions[f.base].name).toBeLessThan(was.get(f.id)!);
    for (let m = 0; m < 3; m++) { g.turn++; airAI(g, 'ITA'); }
    const R = Math.min(...landFighters(g, 'ITA').map((f) => airRange(f, g).ftr));
    expect(landFighters(g, 'ITA').filter((f) => km(g, f.base, bardia.id) <= R).length).toBeGreaterThan(0);
  });

  it('do not stand idle where nothing within their reach asks for fighters, though every ask is met without them', () => {
    const g = grand();
    for (const e of [...g.nations.GER.atWar]) if (e !== 'POL') peace(g, 'GER', e);
    g.airFleets = g.airFleets.filter((f) => !atWar(g, 'GER', f.nat));
    airAI(g, 'GER');
    const front = frontOf(g, 'GER');
    const inReach = (h: Game, f: AirFleet): boolean => front.some((id) => cover(km(h, f.base, id), airRange(f, h).ftr) > 0);
    // A month on, one group has been left on the far side of Germany, at the open field furthest from the front.
    const last = landFighters(g, 'GER').length - 1;
    const f0 = landFighters(g, 'GER')[last];
    const field = g.regions.filter((r) => r.ctrl === 'GER' && hasAirfield(g, r.id) && whyNotBase(g, f0, r, true) === null)
      .map((r) => ({ id: r.id, d: Math.min(...front.map((id) => km(g, r.id, id))) })).sort((a, b) => b.d - a.d)[0];
    expect(field.d).toBeGreaterThan(2 * airRange(f0, g).ftr);
    const month = (idle: boolean): { h: Game; f: AirFleet } => {
      const h = loadGame(saveGame(g))!;
      h.turn++;
      const f = landFighters(h, 'GER')[last];
      f.base = field.id;
      AIRWAR.ai.basing.idle = idle;
      try { airAI(h, 'GER'); } finally { AIRWAR.ai.basing.idle = true; }
      return { h, f };
    };
    // The others meet all that the front asks, so the old rule leaves it where it is; the new one brings it up.
    const off = month(false);
    expect(off.f.base).toBe(field.id);
    expect(inReach(off.h, off.f)).toBe(false);
    const on = month(true);
    expect(on.f.base).not.toBe(field.id);
    expect(inReach(on.h, on.f)).toBe(true);
  });

  it('stay where they are in a nation at peace', () => {
    const g = grand();
    const bases = landFighters(g, 'SOV').map((f) => f.base);
    expect(g.nations.SOV.atWar).toEqual([]);
    airAI(g, 'SOV');
    expect(landFighters(g, 'SOV').map((f) => f.base)).toEqual(bases);
  });
});

describe('a computer staff\'s airfields', () => {
  it('are ordered behind a stretch of front no airfield reaches, once it has stood so for months, and one at a time', () => {
    const g = grand();
    g.nations.GER.pp = 500;
    const bare = bareFront(g, 'GER');
    // East Prussia begins the war with no airfield, and German divisions on its border.
    expect(bare.map(([id]) => g.regions[id].name)).toContain('Masuria');
    expect(bare.every(([, divs]) => divs > 0)).toBe(true);
    aiArsenal(g, 'GER');
    expect(fieldOrders(g, 'GER')).toEqual([]);
    g.turn += airwar.ai.basing.fields - 1;
    aiArsenal(g, 'GER');
    expect(fieldOrders(g, 'GER')).toEqual([]);
    g.turn += 1;
    aiArsenal(g, 'GER');
    const orders = fieldOrders(g, 'GER');
    expect(orders.length).toBe(1);
    const at = g.regions[orders[0].region];
    expect(at.ctrl).toBe('GER');
    expect(hasAirfield(g, at.id)).toBe(false);
    const R = Math.min(...landFighters(g, 'GER').map((f) => airRange(f, g).ftr));
    expect(bare.some(([id]) => km(g, at.id, id) <= R)).toBe(true);
    expect(g.nations.GER.pp).toBeLessThan(500);
    aiArsenal(g, 'GER');
    expect(fieldOrders(g, 'GER').length).toBe(1);
  });

  it('are not ordered from a treasury at its reserve, for a front its fields reach, or for the player', () => {
    const poor = grand();
    poor.nations.GER.pp = PRODUCTION.aiReserve;
    poor.turn += airwar.ai.basing.fields;
    aiArsenal(poor, 'GER');
    poor.turn += airwar.ai.basing.fields;
    aiArsenal(poor, 'GER');
    expect(fieldOrders(poor, 'GER')).toEqual([]);

    const g = grand();
    g.nations.GER.pp = 500;
    aiArsenal(g, 'GER');
    expect(g.flags['bf:GER']).toBeDefined();
    // The divisions leave the bare stretches: the staff forgets it was waiting.
    for (const [id] of bareFront(g, 'GER')) for (const c of [...g.regions[id].units]) if (c.nat === 'GER') removeCorps(g, c);
    expect(bareFront(g, 'GER')).toEqual([]);
    g.turn += airwar.ai.basing.fields;
    aiArsenal(g, 'GER');
    expect(g.flags['bf:GER']).toBeUndefined();
    expect(fieldOrders(g, 'GER')).toEqual([]);

    const h = grand('GER');
    h.nations.GER.pp = 500;
    h.turn += airwar.ai.basing.fields;
    aiArsenal(h, 'GER');
    h.turn += airwar.ai.basing.fields;
    aiArsenal(h, 'GER');
    expect(fieldOrders(h, 'GER')).toEqual([]);
  });

  it('are ordered in the first month of a war that begins with the front out of reach of every field', () => {
    const g = grand();
    expect(bareFront(g, 'ITA')).toEqual([]);
    declareWar(g, 'ITA', 'UK');
    g.nations.ITA.pp = 500;
    const bare = bareFront(g, 'ITA');
    // Libya has no airfield on the first day, and Italian divisions on the Egyptian frontier.
    expect(bare.length).toBeGreaterThan(0);
    aiArsenal(g, 'ITA');
    const orders = fieldOrders(g, 'ITA');
    expect(orders.length).toBe(1);
    const at = g.regions[orders[0].region];
    expect(at.ctrl).toBe('ITA');
    const R = Math.min(...landFighters(g, 'ITA').map((f) => airRange(f, g).ftr));
    expect(bare.some(([id]) => km(g, at.id, id) <= R)).toBe(true);
  });

  it('are nobody\'s at peace', () => {
    const g = grand();
    expect(bareFront(g, 'SOV')).toEqual([]);
    expect(bareFront(g, 'ITA')).toEqual([]);
  });
});
