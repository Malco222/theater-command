/**
 * A dominion makes no separate peace while its mother country fights on (the design director, 20 November 2026,
 * 08:12Z). At the sandbox's last stop the Soviet Union's war on Turkey brings Britain in, and Canada follows her. The
 * quiet year's armistice used to sign Canada out the month it came in, and the month's diplomacy took it back in
 * beside Britain: a war card and an armistice every month for up to a year. Canada now stays in beside Britain and
 * signs with her.
 */
import { describe, expect, it } from 'vitest';
import { createGame, setWar } from '../src/campaign/game';
import { RIVALS, stalemates, startOpen } from '../src/campaign/staff';
import { followsInto, worldMonth } from '../src/campaign/world';
import type { Game } from '../src/campaign/types';

/** A grand campaign nobody plays, every battle forgotten, and Britain newly at war with a Soviet Union nobody can reach, Canada beside her. */
function farWar(): Game {
  const g = startOpen(createGame(3, null));
  for (const n of Object.values(g.nations)) n.lastFight = {};
  for (const k of Object.keys(g.flags)) if (k.startsWith('ff:')) delete g.flags[k];
  g.turn += RIVALS.stalemate!.after;
  setWar(g, 'UK', 'SOV');
  g.flags['war:UK:SOV'] = g.turn;
  worldMonth(g);
  return g;
}
const fights = (g: Game, a: string, b: string): boolean => g.nations[a].atWar.includes(b) && g.nations[b].atWar.includes(a);
const pairs = (s: [string, string][]): string[] => s.map(([a, b]) => `${a}:${b}`);

describe('a dominion makes no separate peace while its mother country fights on', () => {
  it('Canada follows Britain into her war, and does not sign out of it alone', () => {
    const g = farWar();
    expect(fights(g, 'CAN', 'SOV')).toBe(true);
    expect(followsInto(g, 'CAN', 'SOV')).toBe('UK');
    // Canada's own war has no date of its own, so its quiet year is long past: it would have signed this month.
    expect(g.flags['war:CAN:SOV']).toBeUndefined();
    expect(pairs(stalemates(g))).not.toContain('CAN:SOV');
    expect(fights(g, 'CAN', 'SOV')).toBe(true);
    expect(fights(g, 'UK', 'SOV')).toBe(true);
  });

  it('a year of quiet: one war card, no armistice until Britain\'s, and then both sign the same month', () => {
    const g = farWar();
    for (let k = 1; k < RIVALS.stalemate!.after; k++) {
      g.turn++;
      worldMonth(g);
      expect(pairs(stalemates(g)).filter((x) => x.includes('SOV')), `month ${k}`).toEqual([]);
    }
    g.turn++;
    worldMonth(g);
    const signed = pairs(stalemates(g));
    expect(signed).toContain('CAN:SOV');
    expect(signed).toContain('SOV:UK');
    expect(fights(g, 'CAN', 'SOV')).toBe(false);
    expect(fights(g, 'UK', 'SOV')).toBe(false);
    // Britain is at peace with the Soviet Union, so nothing takes Canada back in.
    g.turn++;
    worldMonth(g);
    expect(fights(g, 'CAN', 'SOV')).toBe(false);
    const news = g.headlines.map((x) => x.text);
    expect(news.filter((t) => /^Canada is at war with the Soviet Union/.test(t))).toHaveLength(1);
    expect(news.filter((t) => /^Canada and the Soviet Union sign an armistice/.test(t))).toHaveLength(1);
  });

  it('a dominion with no mother country at war with the enemy signs on its own', () => {
    const g = farWar();
    g.nations.UK.atWar = g.nations.UK.atWar.filter((x) => x !== 'SOV');
    g.nations.SOV.atWar = g.nations.SOV.atWar.filter((x) => x !== 'UK');
    expect(followsInto(g, 'CAN', 'SOV')).toBeUndefined();
    expect(pairs(stalemates(g))).toContain('CAN:SOV');
  });
});
