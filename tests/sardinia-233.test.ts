// Ruling 233, rider b609(a): Sardinia's garrison by the record. The roster gave the island the Sardinia Command's two
// divisions for the whole war; the record (historian/italy-islands-1943-b609.md) has 2.3 divisions' worth from January
// 1943 and 3.0 from July. Two dated rows carry the difference: coast troops in January 1943 (0.3 of a division) and
// the Nembo Parachute Division in July (0.7), both held on the island. Sicily's garrison already met its record and
// is given nothing. The rows are marked for the open grand campaign: the history campaign's roster stays the book's.
// On 96 wars the Ally held Sardinia as July 1943 opened in 52 wars (68 before the rows) and Italy's
// seat rose from 4 to 12.
import { describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { capture, createGame, startPhase } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';

const JAN_43 = 41, JUL_43 = 47;
type Row = [number, string, string, string, number, string, { hold?: number; str?: number; est?: number; xp?: number; open?: number }];
const rows = ((setup as unknown as { formations: Row[] }).formations).filter((r) => r[1] === 'ITA' && r[5] === 'Sardinia');
const italians = (g: Game, place: string): Corps[] => g.byName[place].units.concat(g.corps.filter((c) => c.reg === g.byName[place].id && !g.byName[place].units.includes(c))).filter((c) => c.nat === 'ITA' && !c.dead && c.kind !== 'fort');
const worth = (cs: Corps[]): number => cs.reduce((s, c) => s + c.divs * c.str, 0);
/** Italy's month opens in the month named, in a war nobody plays. */
const monthOpens = (g: Game, turn: number): void => { g.turn = turn; startPhase(g, 'ITA'); };

describe('ruling 233, rider b609(a): Sardinia\'s garrison by the record', () => {
  it('two dated rows, the record\'s months and sizes, both held on the island, both the open campaign\'s alone', () => {
    expect(rows).toEqual([
      [JAN_43, 'ITA', 'Sardinia coastal divisions', 'inf', 1, 'Sardinia', { hold: 1, str: 0.3, est: 0.3, xp: 0.05, open: 1 }],
      [JUL_43, 'ITA', 'Nembo Parachute Division', 'inf', 1, 'Sardinia', { hold: 1, str: 0.7, est: 0.7, xp: 0.15, open: 1 }],
    ]);
  });

  it('the island holds 2 divisions\' worth until 1943, 2.3 from January and 3.0 from July', () => {
    const g = startOpen(createGame(3, null));
    expect(worth(italians(g, 'Sardinia'))).toBeCloseTo(2, 6);
    monthOpens(g, JAN_43);
    expect(italians(g, 'Sardinia').map((c) => c.name)).toContain('Sardinia coastal divisions');
    expect(worth(italians(g, 'Sardinia'))).toBeCloseTo(2.3, 6);
    monthOpens(g, JUL_43);
    const nembo = italians(g, 'Sardinia').find((c) => c.name === 'Nembo Parachute Division')!;
    expect(nembo).toBeDefined();
    expect(nembo.hold).toBe(true);
    expect(nembo.str).toBeCloseTo(0.7, 6);
    expect(worth(italians(g, 'Sardinia'))).toBeCloseTo(3, 6);
  });

  it('the history campaign keeps the book\'s roster: neither row comes there', () => {
    const g = createGame(3, null);
    expect(g.flags.open).not.toBe(1);
    monthOpens(g, JAN_43);
    monthOpens(g, JUL_43);
    expect(g.corps.some((c) => c.name === 'Sardinia coastal divisions' || c.name === 'Nembo Parachute Division')).toBe(false);
  });

  it('an island already lost gets nobody: the row forms at home instead', () => {
    const g = startOpen(createGame(3, null));
    capture(g, g.byName.Sardinia, 'UK');
    const before = italians(g, 'Sardinia').length;
    monthOpens(g, JUL_43);
    expect(italians(g, 'Sardinia')).toHaveLength(before);
    const nembo = g.corps.find((c) => c.name === 'Nembo Parachute Division')!;
    expect(nembo).toBeDefined();
    expect(g.regions[nembo.reg].owner).toBe('ITA');
    expect(g.regions[nembo.reg].name).not.toBe('Sardinia');
  });
});
