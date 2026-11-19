// America's answer in the South Pacific with the far side's switch on (rider b585 to ruling 234, items 1(a) to 1(c);
// ruling 190 and the Pacific planner's papers 05 and 14). (a) The run south: a share of America's sea lift works the
// Pacific from the month it is at war with Japan, on two runs parted by the equator, from the west coast's own quays,
// onto its lane. (c) Whose road each of the Allies' hop rows is. (b) The fleet goes to the beach it covers. The
// states are set by hand on the first day's chart; no war is played here.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { atWar, createGame, declareWar } from '../src/campaign/game';
import { HISTORY, setStop } from '../src/campaign/history';
import { NAVY, fleetReach, navalAI, seaPath } from '../src/campaign/navy';
import { PACIFIC, coversBeaches, firstOnRun, hopShores, laneOn, onRun, pacificFreed, pacificRuns, runOf, runQuays } from '../src/campaign/pacific';
import { startOpen } from '../src/campaign/staff';
import { acrossOcean, oceanShares, overseasOf, sailings } from '../src/campaign/world';
import type { Fleet, Game, Region } from '../src/campaign/types';

/** A grand campaign on the whole world at a stop of the dial, moved by hand to a month of the war. */
const war = (stop: number, turn: number, human: string | null = null, seed = 3): Game => {
  const g = startOpen(createGame(seed, human));
  if (stop !== 3) setStop(g, stop, true);
  g.turn = turn;
  return g;
};
/** The same, with Japan at war with America, Britain, the Dutch and the dominions as the strike south leaves it. */
const pacificWar = (stop: number, turn: number, human: string | null = null, seed = 3): Game => {
  const g = war(stop, turn, human, seed);
  for (const e of ['USA', 'UK', 'NED', 'AUS', 'NZL']) if (!atWar(g, 'JAP', e)) declareWar(g, 'JAP', e);
  g.turn = turn;
  return g;
};
const names = (rs: Region[]): string[] => rs.map((r) => r.name);
const fleet = (g: Game, nat: string, name: string): Fleet => g.fleets.find((f) => f.nat === nat && f.name === name)!;
/** America's two Pacific fleets put at Pearl Harbor, where the book's move of May 1940 leaves them. */
const atPearl = (g: Game): void => {
  const sea = g.seas.findIndex((z) => z.key === 'hawaii');
  for (const nm of ['Pacific Fleet', 'Pacific Carrier Fleet']) {
    const f = fleet(g, 'USA', nm);
    f.base = g.byName['Pearl Harbor'].id; f.port = 'Pearl Harbor'; f.home = sea; f.zone = sea; f.station = sea; f.stance = 'port';
  }
};
type Hops = { allies: [string, number][]; lead?: string; roads?: Record<string, string[]>; behind?: Record<string, string[]> };
const HOPS = (): Hops => (HISTORY as unknown as { hops: Hops }).hops;

describe("America's run south (ruling 190; rider b585 (a))", () => {
  it('lays the rows: America, a twentieth of its lift, two runs parted by the equator, the west coast quays, the lane and the Marines first', () => {
    const R = PACIFIC!.runs!;
    expect(R.of).toBe('USA');
    expect(R.share).toBe(0.05);
    expect(R.idle).toBe('south');
    expect(R.first).toBe('marines');
    expect(R.from).toEqual(['San Francisco', 'Los Angeles', 'San Diego', 'Puget Sound']);
    expect(R.lane).toEqual(['AUS', 'NZL']);
    expect(R.havens).toEqual(['New Caledonia', 'Fiji', 'Samoa', 'Espiritu Santo', 'Port Moresby']);
    expect(Object.keys(R.roads)).toEqual(['south', 'central']);
    const g = war(1, 30);
    for (const nm of [...R.from, ...R.havens]) expect(g.byName[nm], nm).toBeDefined();
    // The run's own quays are at home on the west coast.
    expect(names(runQuays(g, 'south')!)).toEqual(R.from);
    expect(runQuays(g, 'atlantic')).toBeNull();
    expect(firstOnRun('south')).toBe('marines');
    expect(firstOnRun('atlantic')).toBeNull();
  });

  it('has no run before America is at war with Japan, and the southern run the whole share while the book stands to June 1943', () => {
    const g = war(1, 30);
    expect(atWar(g, 'USA', 'JAP')).toBe(false);
    expect(pacificRuns(g, 'USA')).toEqual([]);
    const p = pacificWar(1, 30);
    const O = overseasOf(p, 'USA')!;
    expect(oceanShares(g, 'USA', O)).toEqual(Object.entries(O.oceans));
    expect(pacificRuns(p, 'USA')).toEqual([['south', 0.05]]);
    // From June 1943 three parts in five go south and two to the central run.
    p.turn = 46;
    expect(pacificRuns(p, 'USA')).toEqual([['south', 0.03], ['central', 0.02]]);
    // Nobody else's shipping works the Pacific's runs.
    expect(pacificRuns(p, 'UK')).toEqual([]);
    expect(pacificRuns(p, 'AUS')).toEqual([]);
  });

  it("adds the Pacific's share on top of the Atlantic's while an enemy stands across the Atlantic, and the Atlantic's to the Pacific's when none does", () => {
    const g = pacificWar(1, 30);
    const O = overseasOf(g, 'USA')!;
    // At war with Japan alone: the whole of the shipping that would cross the Atlantic works the Pacific.
    const alone = oceanShares(g, 'USA', O);
    expect(alone.map(([o]) => o)).toEqual(['south']);
    expect(alone[0][1]).toBeCloseTo(0.15 + 0.05, 9);
    // At war with Germany too: both, side by side.
    declareWar(g, 'GER', 'USA');
    g.turn = 30;
    expect(oceanShares(g, 'USA', O)).toEqual([['atlantic', 0.15], ['south', 0.05]]);
    expect(sailings(g, 'USA', O)).toBeCloseTo(0.2 * g.nations.USA.sealift, 9);
    // Japan beaten first: the Pacific's share joins the Atlantic's.
    expect(pacificFreed(g, 'USA')).toBe(0);
    g.nations.JAP.alive = false;
    expect(pacificRuns(g, 'USA')).toEqual([]);
    expect(pacificFreed(g, 'USA')).toBe(0.05);
    expect(oceanShares(g, 'USA', O)).toEqual([['atlantic', 0.2]]);
  });

  it("puts an army down only on the lane: the run's own side of the equator, America's own line, Australia and New Zealand", () => {
    const g = pacificWar(1, 30);
    const at = (nm: string): Region => g.byName[nm];
    expect(runOf(g, at('Brisbane'))).toBe('south');
    expect(runOf(g, at('Pearl Harbor'))).toBe('central');
    expect(runOf(g, at('Lancashire'))).toBeNull();
    for (const nm of ['Brisbane', 'Darwin', 'Wellington', 'New Caledonia', 'Espiritu Santo', 'Fiji', 'Samoa', 'Port Moresby']) expect(acrossOcean(g, 'south', at(nm)), nm).toBe(true);
    // The Dutch Indies and the central run's harbours are not the southern run's.
    for (const nm of ['Batavia', 'Pearl Harbor']) expect(acrossOcean(g, 'south', at(nm)), nm).toBe(false);
    expect(acrossOcean(g, 'central', at('Pearl Harbor'))).toBe(true);
    expect(onRun(g, 'atlantic', at('Brisbane'))).toBeNull();
    // The Atlantic run is as it was: the old chart only.
    expect(acrossOcean(g, 'atlantic', at('Lancashire'))).toBe(true);
    expect(acrossOcean(g, 'atlantic', at('Brisbane'))).toBe(false);
  });

  it('sends a corps across the Pacific on only along the lane, and none on to the Pacific from anywhere else', () => {
    const g = pacificWar(1, 30);
    const at = (nm: string): Region => g.byName[nm];
    expect(laneOn(g, 'USA', at('Brisbane'), at('Townsville'))).toBe(true);
    expect(laneOn(g, 'USA', at('Brisbane'), at('New Caledonia'))).toBe(true);
    expect(laneOn(g, 'USA', at('Brisbane'), at('Batavia'))).toBe(false);
    expect(laneOn(g, 'USA', at('Lancashire'), at('Brisbane'))).toBe(false);
    expect(laneOn(g, 'USA', at('Lancashire'), at('Brittany'))).toBe(true);
    // Another country's shipping, and a player's America, are not bound by the runs.
    expect(laneOn(g, 'UK', at('Lancashire'), at('Brisbane'))).toBe(true);
    const h = pacificWar(1, 30, 'USA');
    expect(pacificRuns(h, 'USA')).toEqual([]);
    expect(laneOn(h, 'USA', h.byName['Brisbane'], h.byName['Batavia'])).toBe(true);
  });
});

describe("whose road each of the Allies' hop rows is (rider b585 (c); the planner's paper 14, section 4)", () => {
  it('names America the lead, Britain the road back to Burma and Malaya, and Australia behind America (Buna, then Borneo)', () => {
    const H = HOPS();
    expect(H.lead).toBe('USA');
    expect(H.roads!.UK).toEqual(['Akyab', 'Myitkyina', 'Kalewa', 'Mandalay', 'Lashio', 'Yenangyaung', 'Toungoo', 'Prome', 'Rangoon', 'Kuala Lumpur', 'Penang', 'Johore', 'Singapore']);
    expect(H.behind!.AUS).toEqual(['Buna', 'Tarakan', 'Brunei', 'Balikpapan']);
    expect(H.behind!.NZL).toEqual([]);
    const listed = new Set(H.allies.map(([nm]) => nm));
    for (const nm of [...H.roads!.UK, ...H.behind!.AUS]) expect(listed.has(nm), nm).toBe(true);
  });

  it('opens each shore to its own staff only: Guadalcanal to America, Akyab to Britain, Buna to Australia and America, nothing to New Zealand or the Dutch', () => {
    const g = pacificWar(1, 36);
    for (const nm of ['Guadalcanal', 'Akyab', 'Buna']) g.byName[nm].ctrl = 'JAP';
    expect(names(hopShores(g, 'USA')!.open)).toEqual(['Guadalcanal']);
    for (const nat of ['UK', 'AUS', 'NZL']) expect(hopShores(g, nat)!.open, nat).toEqual([]);
    g.turn = 40;
    const usa = names(hopShores(g, 'USA')!.open);
    expect(usa).toContain('Guadalcanal');
    expect(usa).toContain('Buna');
    expect(usa).not.toContain('Akyab');
    expect(names(hopShores(g, 'UK')!.open)).toEqual(['Akyab']);
    expect(names(hopShores(g, 'AUS')!.open)).toEqual(['Buna']);
    expect(hopShores(g, 'NZL')!.open).toEqual([]);
    expect(hopShores(g, 'NED')!.open).toEqual([]);
  });

  it("leaves ground Japan's side took beyond the record open to every Allied staff", () => {
    const g = pacificWar(1, 34);
    g.byName['Fiji'].ctrl = 'JAP';
    for (const nat of ['USA', 'UK', 'AUS', 'NZL']) expect(names(hopShores(g, nat)!.open), nat).toContain('Fiji');
  });
});

describe('the fleet goes to the beach it covers (ruling 200(j); rider b585 (b))', () => {
  /** August 1942 by hand: Guadalcanal Japan's, named by America's staff for a landing from Samoa. */
  const august = (human: string | null = null): { g: Game; beach: number } => {
    const g = pacificWar(1, 36, human);
    atPearl(g);
    const gu = g.byName['Guadalcanal'], samoa = g.byName['Samoa'];
    gu.ctrl = 'JAP';
    g.flags['st:hop:USA:0'] = gu.id + 1;
    g.flags['st:hopQ:USA:0'] = samoa.id + 1;
    const path = seaPath(g, 'USA', samoa, gu, { max: NAVY.landing.range, surprise: true })!;
    return { g, beach: path[path.length - 1] };
  };

  it("is America's rule and the Allies', never Japan's or a player's", () => {
    const g = pacificWar(1, 36);
    expect(coversBeaches(g, 'USA')).toBe(true);
    expect(coversBeaches(g, 'AUS')).toBe(true);
    expect(coversBeaches(g, 'JAP')).toBe(false);
    expect(coversBeaches(war(1, 36), 'USA')).toBe(false);
    expect(coversBeaches(pacificWar(1, 36, 'USA'), 'USA')).toBe(false);
  });

  it('moves the strongest fleet that could cover to the nearest harbour of its side within three seas of the beach; it spends the month in harbour there', () => {
    const { g, beach } = august();
    const pf = fleet(g, 'USA', 'Pacific Fleet');
    expect(fleetReach(g, pf)).not.toContain(beach);
    navalAI(g, 'USA');
    expect(g.regions[pf.base].name).toBe('Samoa');
    expect(pf.stance).toBe('port');
    expect(fleetReach(g, pf)).toContain(beach);
    expect(g.headlines.some((h) => h.turn === 36 && h.text === 'The Pacific Fleet moves its base to Samoa to cover the landing at Guadalcanal.')).toBe(true);
    // One fleet is enough: the carriers stay where they were.
    expect(g.regions[fleet(g, 'USA', 'Pacific Carrier Fleet').base].name).toBe('Pearl Harbor');
    // The next month it covers the crossing from there: it stands off the beach.
    g.turn = 37;
    navalAI(g, 'USA');
    expect(g.regions[pf.base].name).toBe('Samoa');
    expect(pf.zone).toBe(beach);
    expect(pf.stance).toBe('patrol');
  });

  it('goes back to the base it had between landings, and a dated move of the book meanwhile stands', () => {
    const { g } = august();
    const pf = fleet(g, 'USA', 'Pacific Fleet');
    navalAI(g, 'USA');
    expect(g.regions[pf.base].name).toBe('Samoa');
    delete g.flags['st:hop:USA:0'];
    delete g.flags['st:hopQ:USA:0'];
    g.turn = 37;
    navalAI(g, 'USA');
    expect(g.regions[pf.base].name).toBe('Pearl Harbor');
    expect(g.headlines.some((h) => h.turn === 37 && h.text === 'The Pacific Fleet returns to Pearl Harbor.')).toBe(true);
    expect(Object.keys(g.flags).filter((k) => k.startsWith('fl:cover'))).toEqual([]);
    // Away again; then the book moves it to Espiritu Santo: that is its base now, and it is not sent back.
    const { g: h } = august();
    const hf = fleet(h, 'USA', 'Pacific Fleet');
    navalAI(h, 'USA');
    const es = h.byName['Espiritu Santo'];
    hf.base = es.id; hf.port = es.name;
    delete h.flags['st:hop:USA:0'];
    h.turn = 37;
    navalAI(h, 'USA');
    expect(h.regions[hf.base].name).toBe('Espiritu Santo');
    expect(Object.keys(h.flags).filter((k) => k.startsWith('fl:cover'))).toEqual([]);
  });

  it('moves nothing where a fleet of its own already reaches the beach, nor for a player, nor with no beach named', () => {
    const { g, beach } = august();
    const cf = fleet(g, 'USA', 'Pacific Carrier Fleet');
    const samoa = g.byName['Samoa'];
    cf.base = samoa.id; cf.port = samoa.name; cf.home = samoa.seas[0]; cf.zone = cf.home; cf.station = cf.home;
    expect(fleetReach(g, cf)).toContain(beach);
    navalAI(g, 'USA');
    expect(g.regions[fleet(g, 'USA', 'Pacific Fleet').base].name).toBe('Pearl Harbor');
    const { g: p } = august('USA');
    navalAI(p, 'USA');
    expect(p.regions[fleet(p, 'USA', 'Pacific Fleet').base].name).toBe('Pearl Harbor');
    const q = pacificWar(1, 36);
    atPearl(q);
    navalAI(q, 'USA');
    expect(q.regions[fleet(q, 'USA', 'Pacific Fleet').base].name).toBe('Pearl Harbor');
  });
});
