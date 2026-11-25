// America's answer in the South Pacific with the far side's switch on (rider b585 to ruling 234, items 1(a) to 1(c);
// ruling 190 and the Pacific planner's papers 05 and 14). (a) The run south: a share of America's sea lift works the
// Pacific from the month it is at war with Japan, on two runs parted by the equator, from the west coast's own quays,
// onto its lane. (c) Whose road each of the Allies' hop rows is. (b) The fleet goes to the beach it covers. The
// states are set by hand on the first day's chart; no war is played here.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { endStaffMonth, ferry } from '../src/campaign/ai';
import { atWar, createGame, declareWar, relocate } from '../src/campaign/game';
import { HISTORY, setStop } from '../src/campaign/history';
import { NAVY, fleetReach, navalAI, navalUpkeep, seaPath, setBeatReach, voyageMonths } from '../src/campaign/navy';
import { PACIFIC, coversBeaches, firstOnRun, hopShores, hopsBind, laneOn, onRun, pacificFreed, pacificRuns, ringDrawn, runOf, runQuays } from '../src/campaign/pacific';
import { oceanRuns, startOpen } from '../src/campaign/staff';
import { acrossOcean, oceanShares, overseasOf, sailings } from '../src/campaign/world';
import type { Corps, Fleet, Game, Region } from '../src/campaign/types';

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

  it('splits the share by the shores its staff names once the book no longer binds: all to the run named, each its part when both are', () => {
    const g = pacificWar(3, 36);
    const at = (nm: string): Region => g.byName[nm];
    expect(hopsBind(g)).toBe(false);
    // No shore named: the whole share works the idle run.
    expect(pacificRuns(g, 'USA')).toEqual([['south', 0.05]]);
    // A shore north of the equator named alone: the whole share goes to the central run.
    at('Wake').ctrl = 'JAP';
    g.flags['st:hop:USA:0'] = at('Wake').id + 1;
    g.flags['st:hopQ:USA:0'] = at('Pearl Harbor').id + 1;
    expect(runOf(g, at('Wake'))).toBe('central');
    expect(pacificRuns(g, 'USA')).toEqual([['central', 0.05]]);
    // A shore on each road: each run its own part.
    at('Guadalcanal').ctrl = 'JAP';
    g.flags['st:hop:USA:1'] = at('Guadalcanal').id + 1;
    g.flags['st:hopQ:USA:1'] = at('Samoa').id + 1;
    expect(pacificRuns(g, 'USA')).toEqual([['south', 0.03], ['central', 0.02]]);
    // While the book binds, the named shores do not move it: the book's months alone.
    const b = pacificWar(1, 36);
    b.flags['st:hop:USA:0'] = b.byName['Wake'].id + 1;
    b.flags['st:hopQ:USA:0'] = b.byName['Pearl Harbor'].id + 1;
    b.byName['Wake'].ctrl = 'JAP';
    expect(hopsBind(b)).toBe(true);
    expect(pacificRuns(b, 'USA')).toEqual([['south', 0.05]]);
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
  /**
   * August 1942 by hand: Guadalcanal Japan's, named by America's staff for a landing from Samoa. Guam is Japan's too,
   * as it was from December 1941: left America's, as the first day's chart has it, it is the nearer harbour of the
   * two on the globe (2,886 km from the middle of the Eastern Solomons to Samoa's 3,158; the map's straight line read
   * 2,520 and 2,168), each three seas from the beach.
   */
  const august = (human: string | null = null): { g: Game; beach: number } => {
    const g = pacificWar(1, 36, human);
    atPearl(g);
    const gu = g.byName['Guadalcanal'], samoa = g.byName['Samoa'];
    gu.ctrl = 'JAP';
    g.byName['Guam'].ctrl = 'JAP';
    g.flags['st:hop:USA:0'] = gu.id + 1;
    g.flags['st:hopQ:USA:0'] = samoa.id + 1;
    const path = seaPath(g, 'USA', samoa, gu, { max: NAVY.landing.range, surprise: true })!;
    return { g, beach: path[path.length - 1] };
  };

  it("is every computer staff's in the Pacific's war, Japan's included (rider b599 (iii)), never a player's", () => {
    const g = pacificWar(1, 36);
    expect(coversBeaches(g, 'USA')).toBe(true);
    expect(coversBeaches(g, 'AUS')).toBe(true);
    expect(coversBeaches(g, 'JAP')).toBe(true);
    expect(coversBeaches(war(1, 36), 'USA')).toBe(false);
    expect(coversBeaches(pacificWar(1, 36, 'USA'), 'USA')).toBe(false);
    expect(coversBeaches(pacificWar(1, 36, 'JAP'), 'JAP')).toBe(false);
  });

  /**
   * August 1942 by hand at a stop with no book: Koepang Japan's, named by Japan's staff for a landing at Darwin. With
   * `ring`, Japan has drawn its ring (paper 14, section 3): its war on America six months old, no ground gained since.
   */
  const darwin = (human: string | null = null, ring = false): { g: Game; beach: number } => {
    const g = pacificWar(3, 36, human);
    if (ring) { g.flags['war:JAP:USA'] = 30; g.flags['st:gain:JAP'] = 30; }
    const kp = g.byName['Koepang'], dw = g.byName['Darwin'];
    kp.ctrl = 'JAP';
    g.flags['st:hop:JAP:0'] = dw.id + 1;
    g.flags['st:hopQ:JAP:0'] = kp.id + 1;
    const path = seaPath(g, 'JAP', kp, dw, { max: NAVY.landing.range, surprise: true })!;
    return { g, beach: path[path.length - 1] };
  };
  const covering = (g: Game): string[] => g.fleets.filter((f) => f.nat === 'JAP' && g.flags[`fl:cover:${f.id}`] !== undefined).map((f) => f.name);

  it("draws Japan's ring by the book from the outer line's month, slid with the strike; elsewhere the first month its offensive has stalled since its war on America began, and for good", () => {
    expect(PACIFIC!.ring).toEqual({ from: 33 });
    const b = pacificWar(1, 32);
    expect(hopsBind(b)).toBe(true);
    expect(ringDrawn(b)).toBe(false);
    b.turn = 33;
    expect(ringDrawn(b)).toBe(true);
    const g = pacificWar(3, 36);
    expect(ringDrawn(g)).toBe(false);
    g.flags['war:JAP:USA'] = 30;
    g.flags['st:gain:JAP'] = 34;
    expect(ringDrawn(g)).toBe(false);
    g.flags['st:gain:JAP'] = 33;
    expect(ringDrawn(g)).toBe(true);
    // Once drawn it stays drawn, whatever is gained after.
    g.flags['st:gain:JAP'] = 36;
    expect(ringDrawn(g)).toBe(true);
    // Not before the war on America, however long the offensive in China has stood still.
    const c = war(3, 26);
    c.flags['st:gain:JAP'] = 10;
    expect(ringDrawn(c)).toBe(false);
  });

  it("moves Japan's fleets to the beach they cover in the opening as the Allies' do, its carrier striking force too; its battle line stays in home waters (paper 14, section 2)", () => {
    const { g, beach } = darwin();
    expect(ringDrawn(g)).toBe(false);
    navalAI(g, 'JAP');
    expect(covering(g)).toEqual(['Mobile Fleet']);
    const f = fleet(g, 'JAP', 'Mobile Fleet');
    expect(g.regions[f.base].name).toBe('Koepang');
    expect(fleetReach(g, f)).toContain(beach);
    expect(g.headlines.some((h) => h.turn === 36 && h.text === 'The Mobile Fleet moves its base to Koepang to cover the landing at Darwin.')).toBe(true);
    // The battle line, the fleet with the most battleships, never covers: not even when no other fleet is fit to go.
    const { g: h } = darwin();
    for (const x of h.fleets) if (x.nat === 'JAP' && x.name !== 'Combined Fleet') x.ready = 0.3;
    navalAI(h, 'JAP');
    expect(covering(h)).toEqual([]);
    expect(h.regions[fleet(h, 'JAP', 'Combined Fleet').base].name).toBe('Hiroshima');
  });

  it("once the ring is drawn keeps its carrier striking force and battle line together at their base: another fleet goes (paper 14, section 3)", () => {
    const { g, beach } = darwin(null, true);
    expect(ringDrawn(g)).toBe(true);
    const second = fleet(g, 'JAP', '2nd Fleet');
    expect(fleetReach(g, second)).not.toContain(beach);
    expect(voyageMonths(g, second, g.byName['Koepang'])).toBe(1);
    navalAI(g, 'JAP');
    expect(g.regions[second.base].name).toBe('Koepang');
    // (Release 2: beyond a month's steaming the move is a voyage, in the month after.)
    expect(second.stance).toBe('passage');
    expect(second.passage?.due).toBe(37);
    expect(g.headlines.some((h) => h.turn === 36 && h.text === 'The 2nd Fleet moves its base to Koepang to cover the landing at Darwin.')).toBe(true);
    expect(covering(g)).toEqual(['2nd Fleet']);
    for (const nm of ['Combined Fleet', 'Mobile Fleet']) expect(g.regions[fleet(g, 'JAP', nm).base].name, nm).toBe('Hiroshima');
    // In the next it comes in, and lies that month in harbour there; from the month after it covers the crossing.
    g.turn = 37;
    navalUpkeep(g, 'JAP');
    navalAI(g, 'JAP');
    expect([second.stance, second.zone]).toEqual(['port', second.home]);
    g.turn = 38;
    navalUpkeep(g, 'JAP');
    navalAI(g, 'JAP');
    expect(second.zone).toBe(beach);
    expect(second.stance).toBe('patrol');
    for (const nm of ['Combined Fleet', 'Mobile Fleet']) expect(g.regions[fleet(g, 'JAP', nm).base].name, nm).toBe('Hiroshima');
  });

  it("then never sends them to a beach, though no other fleet is fit to go, nor when the striking force is down to the line's count of carriers; nothing for a player's Japan", () => {
    const { g } = darwin(null, true);
    for (const f of g.fleets) if (f.nat === 'JAP' && f.name !== 'Combined Fleet' && f.name !== 'Mobile Fleet') f.ready = 0.3;
    // Three of the striking force's carriers lost: it has as many as the battle line, and is still the striking force.
    const mobile = fleet(g, 'JAP', 'Mobile Fleet');
    mobile.ships.cv = fleet(g, 'JAP', 'Combined Fleet').ships.cv;
    navalAI(g, 'JAP');
    expect(covering(g)).toEqual([]);
    for (const nm of ['Combined Fleet', 'Mobile Fleet']) expect(g.regions[fleet(g, 'JAP', nm).base].name, nm).toBe('Hiroshima');
    const { g: p } = darwin('JAP', true);
    navalAI(p, 'JAP');
    expect(p.regions[fleet(p, 'JAP', '2nd Fleet').base].name).toBe('Formosa');
  });

  it("brings a main fleet that went to a beach in the opening home the month the ring is drawn, though the beach is still named and in its reach", () => {
    const { g, beach } = darwin();
    navalAI(g, 'JAP');
    const went = covering(g);
    expect(went.length).toBe(1);
    const f = fleet(g, 'JAP', went[0]);
    const home = g.regions[g.flags[`fl:cover:${f.id}`] - 1].name;
    expect(g.regions[f.base].name).toBe('Koepang');
    // (Release 2: from home waters it is a voyage of three months.) A month on it is still on its way, covering the beach
    // it will reach: no second fleet is sent after it.
    expect(f.passage?.due).toBe(38);
    g.turn = 37;
    navalUpkeep(g, 'JAP');
    navalAI(g, 'JAP');
    expect(f.stance).toBe('passage');
    expect(covering(g)).toEqual([f.name]);
    // It comes in, covers the same beach, and then the ring is drawn.
    g.turn = 38;
    navalUpkeep(g, 'JAP');
    expect(f.zone).toBe(f.home);
    expect(fleetReach(g, f)).toContain(beach);
    g.flags['war:JAP:USA'] = 30; g.flags['st:gain:JAP'] = 30;
    expect(ringDrawn(g)).toBe(true);
    navalAI(g, 'JAP');
    expect(g.regions[f.base].name).toBe(home);
    expect(g.flags[`fl:cover:${f.id}`]).toBeUndefined();
    expect(g.headlines.some((h) => h.turn === 38 && h.text === `The ${f.name} returns to ${home}.`)).toBe(true);
    // Another fleet goes to the beach instead.
    expect(covering(g).length).toBe(1);
    expect(covering(g)).not.toContain(f.name);
  });

  it("turns back a main fleet still on its way to the beach's harbour the month the ring is drawn (release 2, the far chart)", () => {
    const { g } = darwin();
    navalAI(g, 'JAP');
    const f = fleet(g, 'JAP', covering(g)[0]);
    const home = g.regions[g.flags[`fl:cover:${f.id}`] - 1];
    expect(f.passage?.from).toBe(home.id);
    g.turn = 37;
    navalUpkeep(g, 'JAP');
    g.flags['war:JAP:USA'] = 30; g.flags['st:gain:JAP'] = 30;
    const news = g.headlines.length;
    navalAI(g, 'JAP');
    expect(f.base).toBe(home.id);
    expect(f.passage?.back).toBe(true);
    expect(g.flags[`fl:cover:${f.id}`]).toBeUndefined();
    expect(g.headlines.slice(news).some((h) => h.text.startsWith(`The ${f.name} turns back for ${f.port}, `))).toBe(true);
    expect(covering(g).length).toBe(1);
    expect(covering(g)).not.toContain(f.name);
  });

  it("keeps a covering fleet's cover while a row of the book with a sea takes it out and brings it back: it still goes home when its beaches are done", () => {
    const { g } = august();
    const pf = fleet(g, 'USA', 'Pacific Fleet');
    navalAI(g, 'USA');
    const samoa = g.byName['Samoa'], nc = g.byName['New Caledonia'], pearl = g.byName['Pearl Harbor'];
    expect(pf.base).toBe(samoa.id);
    // The book's row takes it from Samoa to New Caledonia for a sea, keeping Samoa as the base it goes back to.
    g.turn = 37;
    pf.bookBase = [pf.base, pf.port, pf.home, pf.station];
    pf.base = nc.id; pf.port = nc.name; pf.home = nc.seas[0]; pf.zone = pf.home;
    navalAI(g, 'USA', [{ fleet: 'Pacific Fleet', zone: g.seas[nc.seas[0]].key!, stance: 'patrol' }]);
    expect(g.flags[`fl:cover:${pf.id}`]).toBe(pearl.id + 1);
    expect(g.flags[`fl:coverAt:${pf.id}`]).toBe(samoa.id + 1);
    // The row ends: the book brings it back to Samoa. Its beaches done, it goes home to Pearl Harbor.
    g.turn = 38;
    const [base, port, home, station] = pf.bookBase;
    pf.base = base; pf.port = port; pf.home = home; pf.zone = home; pf.station = station;
    delete pf.bookBase;
    delete g.flags['st:hop:USA:0'];
    delete g.flags['st:hopQ:USA:0'];
    navalAI(g, 'USA');
    expect(g.regions[pf.base].name).toBe('Pearl Harbor');
    expect(Object.keys(g.flags).filter((k) => k.startsWith('fl:cover'))).toEqual([]);
  });

  it("with the beat-in-reach switch on, a fleet moved to cover takes the beach's sea for its beat where the one it had lies out of reach; off, it keeps it (the design director, 20:02Z, 2)", () => {
    // (Its beat the Hawaiian Waters, which Samoa reaches: kept, on or off. Its beat the California Coast, which Samoa does not.)
    for (const far of [false, true]) for (const on of [false, true]) {
      const { g, beach } = august();
      if (on) setBeatReach(g, true);
      const pf = fleet(g, 'USA', 'Pacific Fleet');
      if (far) pf.station = g.seas.findIndex((z) => z.name === 'California Coast');
      const was = pf.station;
      navalAI(g, 'USA');
      expect(g.regions[pf.base].name).toBe('Samoa');
      expect(fleetReach(g, pf, true).includes(was)).toBe(!far);
      expect(pf.station, `far ${far} on ${on}`).toBe(on && far ? beach : was);
      if (!(on && far)) continue;
      // (Its beaches done, it goes back to Pearl Harbor: the beach's sea lies beyond its reach there, so its home sea.)
      g.turn++;
      delete g.flags['st:hop:USA:0'];
      delete g.flags['st:hopQ:USA:0'];
      navalAI(g, 'USA');
      expect(g.regions[pf.base].name).toBe('Pearl Harbor');
      expect(fleetReach(g, pf, true)).not.toContain(beach);
      expect(pf.station).toBe(pf.home);
    }
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
    // Away again; then the book moves it to Espiritu Santo (a row that is a move of base, done this month): that is
    // its base now, and it is not sent back.
    const { g: h } = august();
    const hf = fleet(h, 'USA', 'Pacific Fleet');
    navalAI(h, 'USA');
    const es = h.byName['Espiritu Santo'];
    h.turn = 37;
    hf.base = es.id; hf.port = es.name;
    h.flags['bf:USA:Pacific Fleet:37'] = 37;
    delete h.flags['st:hop:USA:0'];
    navalAI(h, 'USA');
    expect(h.regions[hf.base].name).toBe('Espiritu Santo');
    expect(Object.keys(h.flags).filter((k) => k.startsWith('fl:cover'))).toEqual([]);
  });

  it('keeps a covering fleet at its cover base while the beach is still named: worn, it rests there; given an order, it carries it out', () => {
    const { g, beach } = august();
    const pf = fleet(g, 'USA', 'Pacific Fleet');
    navalAI(g, 'USA');
    g.turn = 37;
    pf.ready = 0.3;
    navalAI(g, 'USA');
    expect(g.regions[pf.base].name).toBe('Samoa');
    expect(pf.stance).toBe('port');
    expect(g.flags[`fl:cover:${pf.id}`]).toBe(g.byName['Pearl Harbor'].id + 1);
    expect(g.headlines.some((h) => h.text === 'The Pacific Fleet returns to Pearl Harbor.')).toBe(false);
    // An order of the plan or the book that month: carried out from where it lies, and not sent home first.
    g.turn = 38;
    pf.ready = 1;
    navalAI(g, 'USA', [{ fleet: 'Pacific Fleet', zone: g.seas[beach].key!, stance: 'patrol' }]);
    expect(g.regions[pf.base].name).toBe('Samoa');
    expect(pf.zone).toBe(beach);
    expect(g.flags[`fl:cover:${pf.id}`]).toBe(g.byName['Pearl Harbor'].id + 1);
  });

  it('covers from the harbour the enemy drives it to, and still goes home when its beaches are done', () => {
    const { g } = august();
    const pf = fleet(g, 'USA', 'Pacific Fleet');
    navalAI(g, 'USA');
    // Samoa lost: the fleet makes for New Caledonia (no row of the book moved it).
    const nc = g.byName['New Caledonia'];
    pf.base = nc.id; pf.port = nc.name; pf.home = nc.seas[0]; pf.zone = pf.home;
    g.turn = 37;
    navalAI(g, 'USA');
    expect(g.flags[`fl:cover:${pf.id}`]).toBe(g.byName['Pearl Harbor'].id + 1);
    expect(g.flags[`fl:coverAt:${pf.id}`]).toBe(pf.base + 1);
    delete g.flags['st:hop:USA:0'];
    g.turn = 38;
    navalAI(g, 'USA');
    expect(g.regions[pf.base].name).toBe('Pearl Harbor');
    expect(Object.keys(g.flags).filter((k) => k.startsWith('fl:cover'))).toEqual([]);
  });

  it("covers only the Pacific's beaches: a landing the plan names elsewhere moves no fleet of the Pacific's", () => {
    const { g } = august();
    delete g.flags['st:hop:USA:0'];
    delete g.flags['st:hopQ:USA:0'];
    declareWar(g, 'GER', 'USA');
    const brittany = g.byName['Brittany'], lancs = g.byName['Lancashire'];
    brittany.ctrl = 'GER';
    g.flags['st:hop:USA:0'] = brittany.id + 1;
    g.flags['st:hopQ:USA:0'] = lancs.id + 1;
    navalAI(g, 'USA');
    for (const nm of ['Pacific Fleet', 'Pacific Carrier Fleet']) expect(g.regions[fleet(g, 'USA', nm).base].name).toBe('Pearl Harbor');
    expect(Object.keys(g.flags).filter((k) => k.startsWith('fl:cover'))).toEqual([]);
  });

  it('moves nothing where a fleet of its own already reaches the beach, and that fleet covers it from where it lies; nothing for a player, nor with no beach named', () => {
    const { g, beach } = august();
    const cf = fleet(g, 'USA', 'Pacific Carrier Fleet');
    const samoa = g.byName['Samoa'];
    cf.base = samoa.id; cf.port = samoa.name; cf.home = samoa.seas[0]; cf.zone = cf.home; cf.station = cf.home;
    expect(fleetReach(g, cf)).toContain(beach);
    navalAI(g, 'USA');
    expect(g.regions[fleet(g, 'USA', 'Pacific Fleet').base].name).toBe('Pearl Harbor');
    // A fleet whose own duty is the harbour stands off the beach all the same.
    expect(cf.duty).toBe('port');
    expect(g.regions[cf.base].name).toBe('Samoa');
    expect(cf.zone).toBe(beach);
    expect(cf.stance).toBe('patrol');
    const { g: p } = august('USA');
    navalAI(p, 'USA');
    expect(p.regions[fleet(p, 'USA', 'Pacific Fleet').base].name).toBe('Pearl Harbor');
    const q = pacificWar(1, 36);
    atPearl(q);
    navalAI(q, 'USA');
    expect(q.regions[fleet(q, 'USA', 'Pacific Fleet').base].name).toBe('Pearl Harbor');
  });
});

describe('the Marines first on the run south, and the Atlantic Fleet after the Atlantic army only (review of rider b585 (a))', () => {
  /** August 1942 by hand, America at war with Germany and Japan, its army at home fit to sail. */
  const ready = (): { g: Game; us: (nm: string) => Corps } => {
    const g = pacificWar(1, 36);
    if (!atWar(g, 'USA', 'GER')) declareWar(g, 'GER', 'USA');
    g.turn = 36;
    for (const c of g.corps) if (c.nat === 'USA') { c.str = 1; c.supplied = true; c.ap = c.apMax; }
    endStaffMonth();
    return { g, us: (nm) => g.corps.find((c) => c.nat === 'USA' && c.name === nm)! };
  };

  it('brings a Marine corps down to the Pacific quay though another corps already stands on it', () => {
    const { g, us } = ready();
    expect(names(oceanRuns(g, 'USA').map((r) => r.from))).toContain('San Francisco');
    const m = us('Second Army');
    m.elite = 'marines';
    relocate(g, m, g.byName['Sacramento']);
    ferry(g, 'USA');
    expect(g.regions[m.reg].name).toBe('San Francisco');
  });

  it('keeps no Marine corps on the Atlantic quay while the run south is worked: its own run brings it west', () => {
    const { g, us } = ready();
    const quay = oceanRuns(g, 'USA').find((r) => r.ocean === 'atlantic')!.from;
    const m = us('First Army');
    m.elite = 'marines';
    relocate(g, m, quay);
    ferry(g, 'USA');
    expect(g.regions[m.reg].name).not.toBe(quay.name);
  });

  it("marks a crossing of the Pacific's run apart: the first is news, and the Atlantic Fleet waits for the Atlantic army", () => {
    const { g } = ready();
    g.flags['fy:USA:south'] = 1.5;
    g.flags['fyAt:USA:south'] = g.turn;
    ferry(g, 'USA');
    expect(g.flags['fy:pac:USA']).toBe(1);
    expect(g.flags['fy:first:USA']).toBeUndefined();
    expect(g.headlines.some((h) => h.text.startsWith('The first American troops to cross the ocean come ashore at'))).toBe(true);
    expect(fleet(g, 'USA', 'Atlantic Fleet').port).toBe('Norfolk');
  });

  it("puts the run south's army down only on its lane, near its base too", () => {
    for (const t of [30, 36, 42]) {
      const g = pacificWar(1, t);
      for (const run of oceanRuns(g, 'USA')) if (run.ocean !== 'atlantic') for (const p of run.to) expect(acrossOcean(g, run.ocean, p), `${t} ${p.name}`).toBe(true);
    }
  });
});

