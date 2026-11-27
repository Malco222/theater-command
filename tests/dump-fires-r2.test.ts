// Supply dumps that burn (release 2; Brian's yes of 27 November 2026 13:40Z): a forward dump that enemy aircraft over it
// burn badly is set burning: it smokes on the map, and in the month after its side's corps near it have a move less.
// Every army alike; no die is thrown. With the switch off the war is the one it always was.
import { describe, expect, it } from 'vitest';
import { FIRES, burningAt, dumpFiresOn, setDumpFires, slowedByFire } from '../src/campaign/dumpfires';
import { createGame, startPhase } from '../src/campaign/game';
import { setColumns } from '../src/campaign/columns';
import { LOGI, setLogistics, tendDumps } from '../src/campaign/logistics';
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
    const slow = slowedByFire(g, 'GER')!;
    expect(slow.has(MASURIA)).toBe(true);
    // Only its own side's ground: never a province the enemy holds.
    for (const p of slow) expect(g.regions[p].ctrl).not.toBe('POL');
    expect(slowedByFire(g, 'POL')).toBeNull();
  });

  it("the month opens with a move less for the corps near it, and the same for everyone else's", () => {
    const off = war(), on = war();
    setDumpFires(on, true);
    startPhase(off, 'GER');
    startPhase(on, 'GER');
    const slow = slowedByFire(on, 'GER')!;
    const near = on.corps.filter((c) => c.nat === 'GER' && !c.dead && slow.has(c.reg));
    expect(near.length).toBeGreaterThan(0);
    for (const c of on.corps.filter((x) => x.nat === 'GER' && !x.dead)) {
      const o = off.corps.find((x) => x.id === c.id)!;
      expect(c.apMax).toBe(o.apMax - (slow.has(c.reg) ? FIRES.slow : 0));
    }
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
    TC().start({ view: 'grand', human: 'GER', seed: 11 });
    const read = (): Game => loadGame((TC().save() as { text: string }).text)!;
    expect(dumpFiresOn(read())).toBe(false);
    expect(TC().r2Ship(['dumpfiresrule'])).not.toBeNull();
    expect(dumpFiresOn(read())).toBe(true);
    TC().act('dumpfiresrule:off');
    expect(TC().r2Ship(['dumpfiresrule'])).toBeNull();
    expect(dumpFiresOn(read())).toBe(false);
  });
});
