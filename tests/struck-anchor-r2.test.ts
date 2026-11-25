// A fleet struck at anchor withdraws out of the strikers' reach (release 2, the far chart; the design director's ruling
// of 04:33Z, 25 November 2026, 2; the 1.5.1 playtest's finding 42: America's battle fleet lay at Pearl Harbor through
// twelve monthly carrier strikes, its worn ships rested in the harbour being struck). One rule: the losses a fleet took
// at anchor this month and last are the cost of staying, against the move's months (moveCost). It goes to the nearest
// friendly harbour beyond the strikers' reach, from where they were seen to fly; with none it stays, and its side's air
// staff puts fighters over the harbour first. A fleet the player keeps by hand waits for him: told once, and its card
// says so while it is struck. Played on the whole-world chart, America at war with Japan.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
import { describe, expect, it } from 'vitest';
import { airAI, seaKm } from '../src/campaign/air';
import { atWar, createGame, declareWar } from '../src/campaign/game';
import { anchorLoss, inPassage, navalAI, voyageMonths, navalUpkeep, safeHarbour, shipCount, strikeMoves, struckLine } from '../src/campaign/navy';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Fleet, Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

const fleet = (g: Game, nat: string, name: string): Fleet => g.fleets.find((f) => f.nat === nat && f.name === name)!;

/** America at war with Japan in its first month, its Pacific Fleet at anchor at its base. */
function war(human: string | null = null): { g: Game; f: Fleet } {
  const g = startOpen(createGame(3, human));
  if (human) for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  if (!atWar(g, 'JAP', 'USA')) declareWar(g, 'JAP', 'USA');
  const f = fleet(g, 'USA', 'Pacific Fleet');
  expect(f.stance).toBe('port');
  return { g, f };
}
/** A strike at anchor on the fleet this month, sinking `lost` of its surface ships, flown from its own sea by aircraft reaching `km`. */
function strike(g: Game, f: Fleet, lost: number, km = 600): void {
  const n = Math.round(lost * (shipCount(f.ships) - f.ships.ss));
  (f.anchor ??= []).push({ t: g.turn, at: f.base, n, lost, w: 300, from: [{ sea: f.home, field: -1, km }] });
}

describe('a fleet struck at anchor', () => {
  it('counts its losses there this month and last as the cost of staying, and nothing older', () => {
    const { g, f } = war();
    expect(anchorLoss(g, f)).toBe(0);
    strike(g, f, 0.2);
    expect(anchorLoss(g, f)).toBeCloseTo(0.2, 9);
    g.turn++;
    strike(g, f, 0.1);
    expect(anchorLoss(g, f)).toBeCloseTo(0.3, 9);
    g.turn++;
    expect(anchorLoss(g, f)).toBeCloseTo(0.1, 9);
    g.turn++;
    expect(anchorLoss(g, f)).toBe(0);
  });

  it('withdraws, under its staff, to the nearest friendly harbour beyond the strikers\' reach', () => {
    const { g, f } = war();
    const from = f.base, port = f.port;
    strike(g, f, 0.25);
    const to = safeHarbour(g, f)!;
    expect(to).toBeTruthy();
    expect(seaKm(g, to.id, f.home)).toBeGreaterThan(600);
    expect(strikeMoves(g, f, to)).toBe(true);
    const months = voyageMonths(g, f, to);
    const news = g.headlines.length;
    navalAI(g, 'USA');
    expect(f.base).toBe(to.id);
    expect(f.base).not.toBe(from);
    expect(g.headlines.slice(news).some((h) => h.text === `The Pacific Fleet withdraws from ${port}, struck at anchor there, to ${to.name}, out of the strikers' reach.`)).toBe(true);
    // A voyage, where the harbour lies beyond a month's steaming.
    expect(inPassage(f)).toBe(months > 0);
  });

  it('stays where staying costs less than the move: a light strike', () => {
    const { g, f } = war();
    const from = f.base;
    strike(g, f, 0.02);
    const to = safeHarbour(g, f)!;
    expect(strikeMoves(g, f, to)).toBe(false);
    navalAI(g, 'USA');
    expect(f.base).toBe(from);
  });

  it('with no harbour beyond their reach it stays, and the harbour is marked for its air staff\'s fighters first', () => {
    const { g, f } = war();
    const from = f.base;
    strike(g, f, 0.3, 1e9);
    expect(safeHarbour(g, f)).toBeNull();
    navalAI(g, 'USA');
    expect(f.base).toBe(from);
    expect(g.flags[`anc:${f.id}`]).toBe(g.turn);
    expect(() => airAI(g, 'USA')).not.toThrow();
    expect(struckLine(g, f)).toMatch(/No friendly harbour is out of the strikers' reach: fighters over the harbour keep them off\.$/);
  });

  it('with an army to cover it keeps to its lodgment', () => {
    const { g, f } = war();
    const from = f.base;
    strike(g, f, 0.4);
    g.flags[`withArmy:${f.id}`] = 1;
    navalAI(g, 'USA');
    expect(f.base).toBe(from);
  });
});

// ---- A fleet the player keeps by hand -----------------------------------------------------------------------------
type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
interface Panel { lines: string[] }
interface PageOut { note: string }

describe("a fleet of the player's he keeps by hand", () => {
  it('waits for his order: he is told once, and its card says so while it is struck', () => {
    const { g, f } = war('USA');
    f.ordered = true;
    const from = f.base, port = f.port;
    strike(g, f, 0.3);
    const to = safeHarbour(g, f)!;
    const news = g.headlines.length;
    navalUpkeep(g, 'USA');
    navalUpkeep(g, 'USA');
    const told = g.headlines.slice(news).filter((h) => /is struck from the air at anchor/.test(h.text));
    expect(told.length).toBe(1);
    expect(told[0].text).toBe(`The Pacific Fleet is struck from the air at anchor at ${port}. You keep it under your own orders, so it stays there: the nearest friendly harbour out of the strikers' reach is ${to.name}, and its page offers the move.`);
    navalAI(g, 'USA');
    expect(f.base).toBe(from);
    beginHuman(g);
    const line = struckLine(g, f);
    expect(line).toMatch(new RegExp(`^Struck at anchor at ${port}: \\w+ ships? lost there this month and last\\. Nearest friendly harbour out of the strikers' reach: ${to.name}`));
    TC().load(saveGame(g));
    expect((TC().force('fleet', f.id) as Panel).lines).toContain(line);
    expect((TC().page(`fleet:${f.id}`) as PageOut).note.startsWith(`${line} `)).toBe(true);
    // Two months on, with no strike since, the line is gone.
    g.turn += 2;
    expect(struckLine(g, f)).toBe('');
  });
});
