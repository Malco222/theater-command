// Supply dumps that burn (release 2; Brian's yes of 27 November 2026 13:40Z): a forward dump that enemy aircraft over it
// burn badly is set burning: it smokes on the map, and in the month after its side's corps near it have a move less.
// Every army alike; no die is thrown. With the switch off the war is the one it always was.
import { describe, expect, it } from 'vitest';
import { FIRES, bookFire, burningAt, dumpFiresOn, setDumpFires, slowedByFire } from '../src/campaign/dumpfires';
import { createGame, startPhase } from '../src/campaign/game';
import { columnsMonth, setColumns } from '../src/campaign/columns';
import { LOGI, dumpPlaces, dumpRoads, setLogistics, tendDumps } from '../src/campaign/logistics';
import { startOpen } from '../src/campaign/staff';
import { loadGame, saveGame } from '../src/campaign/save';
import type { Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
/** Masuria (East Prussia) in September 1939: the Polish air force over it, a full German dump on it. */
const MASURIA = 28;
function war(human: string | null = null): Game {
  const g = startOpen(createGame(1, human));
  expect(setColumns(g, true)).toBe(true);
  expect(setLogistics(g, 'dumps', true)).toBe(true);
  for (const f of g.airFleets) if (f.nat === 'POL') f.mission = 'ground';
  g.dumps = [{ nat: 'GER', region: MASURIA, stock: LOGI.dumps.size, fill: 0, turn: g.turn - 1 }];
  return g;
}

describe('supply dumps that burn (release 2)', () => {
  it('is off unless switched on: the dump loses what burned, and nothing more is booked', () => {
    const g = war();
    expect(dumpFiresOn(g)).toBe(false);
    tendDumps(g, 'GER');
    expect(g.dumps![0].stock).toBeLessThan(LOGI.dumps.size);
    expect(g.dumpFires).toBeUndefined();
    expect(slowedByFire(g, 'GER')).toBeNull();
    expect(burningAt(g, 'GER', MASURIA)).toBe(false);
  });

  it('a dump badly bombed is set burning, and its side near it is slowed this month', () => {
    const g = war();
    setDumpFires(g, true);
    tendDumps(g, 'GER');
    const burnt = LOGI.dumps.size - g.dumps![0].stock;
    expect(burnt).toBeGreaterThanOrEqual(FIRES.least);
    expect(g.dumpFires).toEqual([{ nat: 'GER', region: MASURIA, turn: g.turn, burnt: Math.round(burnt * 10) / 10 }]);
    expect(burningAt(g, 'GER', MASURIA)).toBe(true);
    // Nobody is slowed until the columns go out and corps draw on it.
    expect(slowedByFire(g, 'GER')).toBeNull();
    expect(slowedByFire(g, 'POL')).toBeNull();
  });

  it('only the corps that draw on it lose a move: one merely standing in its reach keeps its moves', () => {
    // (The design director's gate, 27 Nov 14:12Z, item 2.) Masuria's corps stand at the railhead, so they draw on the
    // railway and not on the dump: the dump burns, and nobody is slowed.
    const off = war(), on = war();
    setDumpFires(on, true);
    startPhase(off, 'GER');
    startPhase(on, 'GER');
    expect(on.dumpFires?.some((f) => f.region === MASURIA && f.turn === on.turn)).toBe(true);
    expect(on.corps.some((c) => c.nat === 'GER' && !c.dead && c.reg === MASURIA)).toBe(true);
    expect(slowedByFire(on, 'GER')).toBeNull();
    for (const c of on.corps) expect(c.apMax).toBe(off.corps.find((x) => x.id === c.id)!.apMax);
  });

  it('the corps that drew on a burning dump are the columns\' own, and the month opens with a move less for them', () => {
    // Germany's railways laid as if they ran only to its westernmost province (as the forward dumps' own test does), a
    // full dump where it saves most road, and that dump set burning this month.
    const g = startOpen(createGame(2, null));
    expect(setColumns(g, true)).toBe(true);
    expect(setLogistics(g, 'dumps', true)).toBe(true);
    setDumpFires(g, true);
    const own = g.regions.filter((r) => r.ctrl === 'GER');
    const west = own.reduce((a, b) => (b.x < a.x ? b : a)).id;
    const road = new Int16Array(g.regions.length).fill(-1);
    road[west] = 0;
    const q = [west];
    for (let i = 0; i < q.length; i++) for (const j of g.regions[q[i]].nb) if (road[j] < 0 && g.regions[j].ctrl === 'GER') { road[j] = road[q[i]] + 1; q.push(j); }
    g.railhead.GER = road;
    columnsMonth(g, 'GER');
    const at = dumpPlaces(g, 'GER')[0].region;
    g.dumps = [{ nat: 'GER', region: at, stock: LOGI.dumps.size, fill: 0, turn: g.turn - 1 }];
    bookFire(g, { nat: 'GER', region: at, turn: g.turn, burnt: 5 });
    columnsMonth(g, 'GER');
    const slow = slowedByFire(g, 'GER')!;
    expect(slow).not.toBeNull();
    // Who drew on it, by the columns' own book: the corps at the end of each column that set out from it, and those
    // standing on the dump itself (they take their supply there and send no column).
    const from = new Set<number>([at, ...(g.columns?.GER ?? []).filter((r) => r.dump === at && !r.fill).map((r) => r.path[r.path.length - 1])]);
    const drew = g.corps.filter((c) => c.nat === 'GER' && !c.dead && c.kind !== 'fort' && c.supplied && from.has(c.reg) && road[c.reg] >= 1).map((c) => c.id);
    expect([...slow].sort((a, b) => a - b)).toEqual(drew.sort((a, b) => a - b));
    // Corps within its road reach that drew elsewhere, or on the railway, are not among them.
    const reach = dumpRoads(g, 'GER', at);
    const others = g.corps.filter((c) => c.nat === 'GER' && !c.dead && reach[c.reg] >= 0 && !slow.has(c.id));
    expect(others.length).toBeGreaterThan(0);
  });

  it('the move is taken from the corps noted on the fire, by id', () => {
    const off = war(), on = war();
    setDumpFires(on, true);
    const first = on.corps.find((c) => c.nat === 'GER' && !c.dead && c.reg === MASURIA)!;
    bookFire(on, { nat: 'GER', region: MASURIA, turn: on.turn, burnt: 5, fed: [first.id] });
    startPhase(off, 'GER');
    startPhase(on, 'GER');
    for (const c of on.corps) expect(c.apMax).toBe(off.corps.find((x) => x.id === c.id)!.apMax - (c.id === first.id ? FIRES.slow : 0));
  });

  it('needs the forward dumps: it will not go on without them, and goes off with them', () => {
    const g = startOpen(createGame(1, null));
    expect(setColumns(g, true)).toBe(true);
    expect(setDumpFires(g, true)).toBe(false);
    expect(dumpFiresOn(g)).toBe(false);
    expect(setLogistics(g, 'dumps', true)).toBe(true);
    expect(setDumpFires(g, true)).toBe(true);
    expect(setLogistics(g, 'dumps', false)).toBe(true);
    expect(dumpFiresOn(g)).toBe(false);
  });

  it('a fire is out after the month that follows, and the save keeps it until then', () => {
    const g = war();
    setDumpFires(g, true);
    tendDumps(g, 'GER');
    const back = loadGame(saveGame(g))!;
    expect(back.dumpFires).toEqual(g.dumpFires);
    g.turn += 1;
    expect(burningAt(g, 'GER', MASURIA)).toBe(true);
    expect(slowedByFire(g, 'GER')).toBeNull();
    g.turn += 1;
    g.dumps = [];
    g.dumps.push({ nat: 'GER', region: MASURIA, stock: 0.5, fill: 0, turn: 0 });
    tendDumps(g, 'GER');
    expect(g.dumpFires).toBeUndefined();
    expect(saveGame(war())).not.toContain('dumpFires');
  });

  it('tells the player when his dump burns, and when his aircraft set the enemy\'s burning', () => {
    const g = war('GER');
    setDumpFires(g, true);
    tendDumps(g, 'GER');
    expect(g.headlines.some((h) => /It is still burning, and the corps it feeds will move slower this month/.test(h.text))).toBe(true);
    const p = war('POL');
    setDumpFires(p, true);
    tendDumps(p, 'GER');
    expect(p.headlines.some((h) => /^Our aircraft have set the German forward dump at .* burning/.test(h.text))).toBe(true);
    setDumpFires(p, false);
    expect(p.dumpFires).toBeUndefined();
  });

  it('the 3D map smokes over it, and the 3D ship list turns it on once, and a switch turned off stays off', () => {
    const g = war('GER');
    setDumpFires(g, true);
    tendDumps(g, 'GER');
    TC().load(saveGame(g));
    const v = TC().view() as { bombed: number[]; dumps: { region: number; burning?: number }[] };
    const smoke = new Map<number, number>();
    for (let i = 0; i < v.bombed.length; i += 2) smoke.set(v.bombed[i], v.bombed[i + 1]);
    expect(smoke.get(MASURIA)).toBeGreaterThanOrEqual(FIRES.smoke);
    expect(v.dumps.find((d) => d.region === MASURIA)?.burning).toBe(1);
    const read = (): Game => loadGame((TC().save() as { text: string }).text)!;
    // A war without the forward dumps does not take it (no dead switch), and the act says why.
    TC().start({ view: 'grand', human: 'GER', seed: 11 });
    expect(TC().r2Ship(['dumpfiresrule'])).toBeNull();
    expect(dumpFiresOn(read())).toBe(false);
    expect((TC().act('dumpfiresrule:on') as { note: string }).note).toMatch(/need the forward dumps, which a war takes only as it opens/);
    // The Play list's order: the columns and the dumps, then the fires.
    TC().start({ view: 'grand', human: 'GER', seed: 11 });
    expect(dumpFiresOn(read())).toBe(false);
    expect(TC().r2Ship(['columns', 'logistics:dumps', 'dumpfiresrule'])).not.toBeNull();
    expect(dumpFiresOn(read())).toBe(true);
    TC().act('dumpfiresrule:off');
    expect(TC().r2Ship(['dumpfiresrule'])).toBeNull();
    expect(dumpFiresOn(read())).toBe(false);
  });
});
