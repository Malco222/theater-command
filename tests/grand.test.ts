// The grand campaign: the game's front door. The player picks a country and a year, history is played to the
// first day of that year, and from then on nobody follows it. These tests cover the menu's data and the hand-over
// from history to free play in a later year. Free play itself is tested in tests/open.test.ts.
import { describe, expect, it } from 'vitest';
import grand from '../data/campaign/grand.json';
import { turnToDate } from '../src/core/ticket';
import { phaseFor } from '../src/campaign/ai';
import { createGame } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { isOpen, startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import { FLAGS } from '../src/campaign/ui/flags';
import type { Game } from '../src/campaign/types';

const YEARS = grand.years as { year: number; turn: number; date: string; text: string }[];
const NATIONS = grand.nations as unknown as { id: string; years: Record<string, string> }[];

// One war played by history's plan, with the map kept at the first day of each year on offer.
const history = createGame(7, null);
const kept = new Map<number, string>();
for (const y of YEARS) {
  while (history.turn < y.turn) runTurnAI(history);
  kept.set(y.year, saveGame(history));
}
const at = (year: number): Game => loadGame(kept.get(year)!)!;

describe('the grand campaign menu', () => {
  it('offers September 1939 and the first of January of each year after it', () => {
    expect(YEARS.map((y) => y.year)).toEqual([1939, 1940, 1941, 1942]);
    for (const y of YEARS) {
      const d = turnToDate(y.turn);
      expect(d.year, y.date).toBe(y.year);
      expect(d.month, y.date).toBe(y.year === 1939 ? 8 : 0);
      expect(y.text.length, y.date).toBeGreaterThan(40);
    }
  });

  it('offers a country only in years when history still has it in the war or able to join it', () => {
    for (const n of NATIONS) {
      expect(Object.keys(n.years).length, n.id).toBeGreaterThan(0);
      for (const year of Object.keys(n.years)) {
        const g = at(Number(year));
        const nat = g.nations[n.id];
        expect(nat, n.id).toBeTruthy();
        expect(nat.alive && !nat.dormant, `${n.id} in ${year}`).toBe(true);
        expect(g.corps.some((c) => c.nat === n.id && !c.dead), `${n.id} has an army in ${year}`).toBe(true);
      }
    }
    // France has asked for terms by 1941 and Poland by 1940: neither is on offer then.
    expect(at(1941).nations.FRA.alive).toBe(false);
    expect(at(1940).nations.POL.alive).toBe(false);
    expect(NATIONS.find((n) => n.id === 'FRA')!.years['1941']).toBeUndefined();
    expect(NATIONS.find((n) => n.id === 'POL')!.years['1940']).toBeUndefined();
  });

  it('has something to say for a war with nobody at the controls', () => {
    expect(grand.watch['1939'].length).toBeGreaterThan(40);
    expect(grand.watch.later.length).toBeGreaterThan(40);
  });
});

describe('a grand campaign begun in a later year', () => {
  it('hands the war over as history left it, and from then on nobody follows history', () => {
    const g = startOpen(at(1941));
    expect(isOpen(g)).toBe(true);
    // The opening month's standing orders belong to September 1939 only.
    expect(g.flags['st:on:GER:POL']).toBeUndefined();
    expect(g.nations.FRA.alive).toBe(false);
    expect(g.nations.GER.atWar).toContain('UK');
    expect(g.nations.GER.atWar).not.toContain('SOV');
    const before = g.headlines.length;
    for (let k = 0; k < 14; k++) runTurnAI(g);
    expect(g.turn).toBe(31);
    // The staffs have been at work: there are orders of their own making, and the war has gone on.
    expect(Object.keys(g.flags).some((k) => k.startsWith('st:n:'))).toBe(true);
    expect(g.headlines.length).toBeGreaterThan(before);
    const ph = phaseFor(g, 'GER');
    expect(ph === null || typeof ph.posture === 'string').toBe(true);
  }, 20_000);

  it('leaves the historical war it was cut from untouched', () => {
    const a = at(1942), b = at(1942);
    startOpen(b);
    for (let k = 0; k < 3; k++) { runTurnAI(a); runTurnAI(b); }
    expect(isOpen(a)).toBe(false);
    // Two wars from the same day: one keeps to the plan, the other does not have to.
    expect(a.flags.open).toBeUndefined();
    expect(b.flags.open).toBe(1);
  });

  it('can be saved and picked up again', () => {
    const g = startOpen(at(1940));
    for (let k = 0; k < 2; k++) runTurnAI(g);
    const again = loadGame(saveGame(g))!;
    expect(isOpen(again)).toBe(true);
    expect(again.turn).toBe(g.turn);
    runTurnAI(g); runTurnAI(again);
    expect(again.regions.map((r) => r.ctrl).join()).toBe(g.regions.map((r) => r.ctrl).join());
  });
});

describe('the flags the armies fly', () => {
  it('has one for every nation in the game', () => {
    for (const id of Object.keys(history.nations)) expect(FLAGS[id], id).toBeTruthy();
  });

  it('draws each one from plain shapes that fill the flag', () => {
    for (const [id, svg] of Object.entries(FLAGS)) {
      expect(svg.startsWith('<rect') || svg.startsWith('<svg'), id).toBe(true);
      expect(svg, id).not.toMatch(/<image|<script|href=/);
    }
  });
});
