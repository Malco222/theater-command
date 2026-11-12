// Free play ('open history'). The war begins as it did on 1 September 1939, and after that no computer-run
// nation follows history's plan: each one's staff weighs its fronts every month and decides for itself.
// These tests cover the rules of the mode. What whole wars look like is checked in tests/exit/open-history.test.ts.
import { describe, expect, it } from 'vitest';
import diplomacy from '../data/campaign/diplomacy.json';
import open from '../data/campaign/open.json';
import setup from '../data/campaign/setup-1939.json';
import { phaseFor, theatrePhases } from '../src/campaign/ai';
import { airPlanFor } from '../src/campaign/air';
import { createGame } from '../src/campaign/game';
import { loadGame, peekSave, saveGame } from '../src/campaign/save';
import { fronts, isOpen, startOpen, temper } from '../src/campaign/staff';
import { beginHuman, declarable, endHuman, humanUp, runTurnAI, stepAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

const MAY_40 = 9, DEC_41 = 28;

function free(seed: number, human: string | null = null): Game {
  return startOpen(createGame(seed, human));
}

function play(g: Game, until: number): Game {
  while (g.turn <= until) runTurnAI(g);
  return g;
}

/** The player's months go by without an order given. */
function sitOut(g: Game, months: number): number {
  let played = 0;
  for (let k = 0; k < months && g.nations[g.human!].alive; k++) {
    for (let guard = 0; !humanUp(g) && guard < 80 && g.nations[g.human!].alive; guard++) stepAI(g);
    if (!humanUp(g)) break;
    beginHuman(g);
    endHuman(g);
    played++;
  }
  return played;
}

const print = (g: Game): string => `${g.regions.map((r) => r.ctrl).join(',')}|${g.corps.filter((c) => !c.dead).length}|${g.headlines.length}|${g.reports.length}`;
/** Every declaration of war made in a game, as 'A:B' with the month. */
const wars = (g: Game): [string, number][] => Object.entries(g.flags).filter(([k]) => k.startsWith('war:')).map(([k, v]) => [k.slice(4), v]);

// Six wars to the end of 1941, shared by the tests below.
const SEEDS = [1, 2, 3, 4, 5, 6];
const runs = SEEDS.map((seed) => play(free(seed), DEC_41));

describe('the mode', () => {
  it('is a mark on the game, and a new game does not carry it', () => {
    const g = createGame(1, null);
    expect(isOpen(g)).toBe(false);
    expect(g.flags.open).toBeUndefined();
    expect(isOpen(startOpen(g))).toBe(true);
  });

  it('leaves the historical campaign on history\'s plan', () => {
    const g = createGame(3, null);
    expect(phaseFor(g, 'GER')?.declare ?? phaseFor(g, 'GER')?.enemies).toContain('POL');
    play(g, MAY_40 - 1);
    expect(phaseFor(g, 'GER')?.name).toBe('Case Yellow');
    expect(g.flags['war:GER:BEL']).toBeUndefined();
    play(g, MAY_40);
    expect(g.flags['war:GER:BEL']).toBe(MAY_40);
  });

  it('begins as the war did: Germany falls on Poland in the first month, and Poland is beaten that autumn', () => {
    for (const seed of [1, 2, 3]) {
      const g = free(seed);
      expect(g.nations.GER.atWar).toEqual(expect.arrayContaining(['POL', 'UK', 'FRA']));
      runTurnAI(g);
      const blows = g.reports.filter((r) => r.attNat === 'GER' && g.regions[r.region].owner === 'POL');
      expect(blows.length, `seed ${seed}`).toBeGreaterThanOrEqual(4);
      play(g, 3);
      expect(g.nations.POL.alive, `seed ${seed}`).toBe(false);
    }
  });

  it('follows no dated plan after that: on land, overseas or in the air', () => {
    const g = play(free(3), MAY_40 - 1);
    for (const nat of ['GER', 'UK', 'FRA', 'ITA', 'SOV']) {
      expect(phaseFor(g, nat)?.name ?? '', nat).not.toBe('Case Yellow');
      expect(theatrePhases(g, nat), nat).toEqual([]);
      expect(airPlanFor(g, nat), nat).toBeNull();
    }
  });
});

describe('the staffs', () => {
  it('give the same war for the same seed', () => {
    expect(print(play(free(5), 30))).toBe(print(play(free(5), 30)));
  });

  it('give a different war for a different seed', () => {
    const west = new Set(runs.map((g) => g.flags['war:GER:BEL'] ?? 0));
    expect(west.size, [...west].join(',')).toBeGreaterThanOrEqual(3);
    expect(new Set(runs.map(print)).size).toBe(SEEDS.length);
  });

  it('do not all attack in the west on history\'s date', () => {
    expect(runs.some((g) => g.flags['war:GER:BEL'] !== undefined && g.flags['war:GER:BEL'] !== MAY_40)).toBe(true);
  });

  it('declare war only where a choice written in open.json, or an event, allows it', () => {
    const ops = open.ops as unknown as Record<string, { declare?: string[] }[]>;
    const byEvent = new Set((diplomacy.events as { war?: string[][] }[]).flatMap((e) => (e.war ?? []).map(([a, b]) => `${a}:${b}`)));
    const begun = new Set(wars(free(1)).map(([k]) => k));
    for (const g of runs) for (const [pair, month] of wars(g)) {
      if (begun.has(pair)) continue;
      const [a, b] = pair.split(':');
      const chosen = (ops[a] ?? []).some((op) => op.declare?.includes(b));
      expect(chosen || byEvent.has(pair), `${pair} in month ${month}`).toBe(true);
    }
  });

  it('have a temper: a cautious army wants better odds than a bold one, and waits behind its forts', () => {
    const g = free(2);
    expect(temper(g, 'FRA').attackAt).toBeGreaterThan(temper(g, 'GER').attackAt);
    // Belgium has no wish to attack anybody: no odds the map can offer would be enough.
    expect(temper(g, 'BEL').attackAt).toBeGreaterThan(50);
    for (const run of runs) {
      const sorties = run.reports.filter((r) => r.turn <= 6 && r.attNat === 'FRA' && run.regions[r.region].owner === 'GER');
      expect(sorties.length).toBe(0);
    }
  });

  it('weigh a front by what stands on both sides of it', () => {
    const g = free(4);
    const east = fronts(g, 'GER').find((f) => f.enemies.includes('POL'))!;
    const west = fronts(g, 'GER').find((f) => f.enemies.includes('FRA'))!;
    expect(east.ratio).toBeGreaterThan(1.2);
    // The French frontier is the Maginot Line: there is no way round it without going through a neutral.
    expect(west.ratio).toBeLessThan(east.ratio / 2);
  });

  it('call up the armies history raised for a war when that war comes, however late', () => {
    // Italy attacks Greece in May 1941 in this war, seven months after history's date: the Greek army still mobilises.
    const g = runs[0];
    const month = g.flags['war:ITA:GRE'];
    expect(month).toBeGreaterThan(20);
    const rows = (setup.expeditions as unknown as [number, string][]).map((row, i) => ({ i, due: row[0], nat: row[1] })).filter((row) => row.nat === 'GRE');
    expect(rows.length).toBeGreaterThan(0);
    // By history's calendar every one of them would have been struck off before the war began.
    expect(Math.max(...rows.map((row) => row.due)) + 6).toBeLessThanOrEqual(month + 1);
    const sent = rows.filter((row) => g.flags[`sent:${row.i}`] === 1 && g.flags[`due:${row.i}`] >= month);
    expect(sent.length).toBeGreaterThanOrEqual(3);
  });
});

describe('the player in free play', () => {
  it('is handed no plan, and may declare war on any neighbour', () => {
    const g = free(2, 'UK');
    sitOut(g, 1);
    for (let guard = 0; !humanUp(g) && guard < 80; guard++) stepAI(g);
    expect(phaseFor(g, 'UK')).toBeNull();
    expect(declarable(g).length).toBeGreaterThan(3);
  });

  it('is never brought into the war by the calendar: an Italy left alone is still at peace in 1942', () => {
    const g = free(3, 'ITA');
    expect(sitOut(g, DEC_41 + 2)).toBe(DEC_41 + 2);
    expect(g.nations.ITA.atWar).toEqual([]);
    // America is in the war by then, against Germany only.
    expect(g.nations.USA.atWar).toContain('GER');
    expect(g.nations.USA.atWar).not.toContain('ITA');
  });

  it('can play the Soviet Union from the first day, at peace until someone chooses otherwise', () => {
    const g = free(6, 'SOV');
    expect(sitOut(g, 12)).toBe(12);
    expect(g.nations.SOV.alive).toBe(true);
  });
});

describe('saving a free-play war', () => {
  it('keeps the mode and everything the staffs have decided', () => {
    const a = play(free(4), 24);
    const text = saveGame(a);
    expect(peekSave(text)?.open).toBe(true);
    const b = loadGame(text)!;
    expect(isOpen(b)).toBe(true);
    play(a, 32); play(b, 32);
    expect(print(b)).toBe(print(a));
  });

  it('says a historical save is not one', () => {
    expect(peekSave(saveGame(play(createGame(4, null), 2)))?.open).toBe(false);
  });
});
