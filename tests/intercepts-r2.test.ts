// Radio intercepts (release 2, words only; Brian's yes of 27 November 2026 17:10Z; the design director's ruling of
// 17:41Z): an enemy staff's main effort named against the player is overheard where his front touches it and his
// Intelligence branch outnumbers its counter-intelligence, never where he reads its plans already; a false one only
// where that enemy runs one of his agents turned, and always with its telltale. One news line, nothing else.
import { describe, expect, it } from 'vitest';
import { counterRows, deceives, intelRows, interceptEffort, interceptsOn, setIntercepts } from '../src/campaign/intercepts';
import { createGame } from '../src/campaign/game';
import { effortMonths, startOpen, type Effort } from '../src/campaign/staff';
import { techOf } from '../src/campaign/tech';
import { loadGame, saveGame } from '../src/campaign/save';
import type { Agent, Game, Region } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
const war = (seed = 3, human: string | null = 'GER'): Game => startOpen(createGame(seed, human));
/** A German province on the Polish border, and one deep in Germany with no enemy near. */
function places(g: Game): { border: Region; deep: Region } {
  const border = g.regions.find((r) => r.ctrl === 'GER' && r.nb.some((j) => g.regions[j].ctrl === 'POL'))!;
  const deep = g.regions.find((r) => r.ctrl === 'GER' && r.owner === 'GER' && !r.nb.some((j) => g.regions[j].ctrl !== 'GER') && !g.corps.some((c) => c.nat === 'GER' && (c.reg === r.id || r.nb.includes(c.reg))))!;
  return { border, deep };
}
const effort = (g: Game, first: number): Effort => ({ nat: 'POL', enemy: 'GER', target: first, first, from: g.turn, to: g.turn + 3, name: 'test', won: 0 });
const heard = (g: Game, from: number): string[] => g.headlines.slice(from).map((h) => h.text).filter((t) => t.startsWith('Intercepted'));
const build = (g: Game, nat: string, ...ids: string[]): void => { techOf(g, nat)!.done.push(...ids); };

describe('radio intercepts (release 2)', () => {
  it('is off unless switched on: nothing is told', () => {
    const g = war();
    const { border } = places(g);
    build(g, 'GER', 'agents', 'cover');
    const n = g.headlines.length;
    expect(interceptsOn(g)).toBe(false);
    interceptEffort(g, effort(g, border.id));
    expect(heard(g, n)).toEqual([]);
  });

  it('is heard only where the front touches it and his Intelligence branch outnumbers their counter-intelligence', () => {
    const g = war();
    setIntercepts(g, true);
    const { border, deep } = places(g);
    const n0 = g.headlines.length;
    // No Intelligence rows of his own against none of theirs: nothing.
    expect(intelRows(g, 'GER')).toBe(0);
    interceptEffort(g, effort(g, border.id));
    expect(heard(g, n0)).toEqual([]);
    // One row against none: heard, and it names the staff's own target and its last month.
    build(g, 'GER', 'agents');
    interceptEffort(g, effort(g, border.id));
    const h = heard(g, n0);
    expect(h).toHaveLength(1);
    // The UI director's words (18:11Z): what the signal says, with the effort's own months from the one reader.
    const m = effortMonths(effort(g, border.id));
    expect(h[0]).toBe(`Intercepted: a signal from the ${g.nations.POL.adj} staff orders its army to mass opposite ${border.name} in ${m.mass} and attack there from ${m.strike} through ${m.last}.`);
    expect(m.mass).not.toBe(m.strike);
    expect(h[0]).not.toMatch(/unusually clear/);
    // A sector his front does not touch: nothing.
    const n1 = g.headlines.length;
    if (deep) { interceptEffort(g, effort(g, deep.id)); expect(heard(g, n1)).toEqual([]); }
    // Their counter-intelligence matches his one row: nothing. Two rows against one: heard.
    build(g, 'POL', 'counterintel');
    expect(counterRows(g, 'POL')).toBe(1);
    interceptEffort(g, effort(g, border.id));
    expect(heard(g, n1)).toEqual([]);
    build(g, 'GER', 'cover');
    interceptEffort(g, effort(g, border.id));
    expect(heard(g, n1)).toHaveLength(1);
  });

  it('a false one only where the enemy runs one of his agents turned before this month, and always with its telltale', () => {
    const g = war();
    setIntercepts(g, true);
    const { border } = places(g);
    build(g, 'GER', 'agents');
    g.flags['sw:double'] = 1;
    const a: Agent = { id: 9001, nat: 'GER', name: 'Test', xp: 0, reg: g.nations.POL.capital, ready: g.turn - 2, mission: 'watch', born: g.turn - 3, turned: g.turn, turnedFor: 'POL' };
    g.intel.agents.push(a);
    // Turned this month: its first false report is not filed yet.
    expect(deceives(g, 'POL', 'GER')).toBe(false);
    a.turned = g.turn - 1;
    expect(deceives(g, 'POL', 'GER')).toBe(true);
    const n = g.headlines.length;
    interceptEffort(g, effort(g, border.id));
    const h = heard(g, n);
    expect(h).toHaveLength(1);
    expect(h[0]).toMatch(/The signal came through unusually clear\.$/);
    expect(h[0]).not.toContain(`opposite ${border.name} in`);
    expect(h[0]).toMatch(/^Intercepted: a signal from the .* staff orders its army to mass opposite .* through .*\. The signal came through unusually clear\.$/);
    // His own double agent in their service: no trick.
    (g.intel.doubles ??= []).push({ id: 999, name: 'Test', of: 'POL', by: 'GER', since: g.turn - 1 });
    expect(deceives(g, 'POL', 'GER')).toBe(false);
    // With the double agents off there is never a trick.
    g.intel.doubles = [];
    delete g.flags['sw:double'];
    expect(deceives(g, 'POL', 'GER')).toBe(false);
  });

  it('writes one news line and nothing else', () => {
    const g = war();
    setIntercepts(g, true);
    const { border } = places(g);
    build(g, 'GER', 'agents');
    const before = JSON.parse(saveGame(g));
    interceptEffort(g, effort(g, border.id));
    const after = JSON.parse(saveGame(g));
    const strip = (x: { headlines: unknown[] }) => ({ ...x, headlines: x.headlines.length });
    expect(after.headlines.length).toBe(before.headlines.length + 1);
    expect({ ...strip(after), headlines: 0 }).toEqual({ ...strip(before), headlines: 0 });
    expect(loadGame(saveGame(g))).not.toBeNull();
  });

  it('the 3D ship list turns it on once, and a switch the player turns off stays off', () => {
    TC().start({ view: 'grand', human: 'GER', seed: 11 });
    const read = (): Game => loadGame((TC().save() as { text: string }).text)!;
    expect(interceptsOn(read())).toBe(false);
    expect(TC().r2Ship(['interceptsrule'])).not.toBeNull();
    expect(interceptsOn(read())).toBe(true);
    TC().act('interceptsrule:off');
    expect(TC().r2Ship(['interceptsrule'])).toBeNull();
    expect(interceptsOn(read())).toBe(false);
  });
});
