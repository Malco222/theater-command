// Gate 5's words (b482 item 5): the works by name (ruling 145(b), `works-shown`) and the line when men run out
// (ruling 133(d)(1), `fl-men-out`). Words only: no rule reads them.
import { describe, expect, it } from 'vitest';
import { offers } from '../src/campaign/arsenal';
import { assess, createGame } from '../src/campaign/game';
import { ledger } from '../src/campaign/economy';
import { cardSeen, markFor, menOutLine, moneyBuys, monthCard } from '../src/campaign/monthcard';
import { ENGINEERING, worksName } from '../src/campaign/rivers';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';

const free = (seed: number, human: string | null = 'GER'): Game => startOpen(createGame(seed, human));

describe('the works say what they are (ruling 145(b))', () => {
  it('each level by name, and the mines', () => {
    expect(worksName(0)).toBe('');
    expect(worksName(1)).toBe('barbed wire, gun pits and an anti-tank ditch');
    expect(worksName(2)).toBe("pillboxes and dragon's teeth");
    expect(worksName(3)).toBe('a line of concrete forts');
    expect(ENGINEERING.named.mines).toBe('minefields and booby traps');
  });

  it("on the engineers' jobs", () => {
    expect(ENGINEERING.jobs.works.blurb).toContain('Barbed wire, gun pits and an anti-tank ditch, then pillboxes and dragon\'s teeth');
    expect(ENGINEERING.jobs.works.blurb).toContain('A line of concrete forts is ordered from the treasury');
    expect(ENGINEERING.jobs.mines.blurb.startsWith('Minefields and booby traps on every road in')).toBe(true);
  });

  it('on the attack card, and in the arsenal', () => {
    const g = free(5);
    const target = g.byName['Lorraine'] ?? g.regions.find((r) => r.ctrl === 'FRA' && r.nb.some((j) => g.regions[j].ctrl === 'GER'))!;
    target.fort = { lv: 2, vs: ['GER'] } as typeof target.fort;
    const att = g.corps.filter((c) => !c.dead && c.nat === 'GER' && target.nb.includes(c.reg)).slice(0, 2);
    expect(att.length).toBeGreaterThan(0);
    expect(assess(g, att, target).lines.find((l) => l.label === 'Fortifications')?.value).toBe("Level 2: pillboxes and dragon's teeth");
    const home = g.regions.find((r) => r.ctrl === 'GER' && r.owner === 'GER' && r.cap)!;
    const fort = offers(g, 'GER', home.id).find((o) => o.key === 'work:fort');
    expect(fort?.blurb).toContain("barbed wire, gun pits and an anti-tank ditch, then pillboxes and dragon's teeth, then a line of concrete forts");
    expect(fort?.note.startsWith(`Level ${(home.fort?.lv ?? 0) + 1} is `)).toBe(true);
  });
});

describe('the line when men run out (ruling 133(d)(1))', () => {
  const empty = (g: Game, nat = 'GER'): void => { g.flags[`mp:${nat}`] = 0.4; };
  const rich = (g: Game, nat = 'GER'): void => { g.nations[nat].pp = ledger(g, nat).gross * 3 + 500; };

  it('says it when the reserve cannot fill a corps of one division and over three months of income lie unspent', () => {
    const g = free(5);
    expect(menOutLine(g, 'GER')).toBeNull();
    empty(g);
    g.nations.GER.pp = ledger(g, 'GER').gross * 2;
    expect(menOutLine(g, 'GER')).toBeNull();
    rich(g);
    const line = menOutLine(g, 'GER')!;
    expect(line).toMatch(/^The reserve of men is empty and [\d,]+ lies unspent\. Money alone still buys: anti-aircraft guns, fortifications, factories, airfields, aircraft/);
    expect(line.endsWith('.')).toBe(true);
    // Engineers take men, so money alone does not buy them.
    expect(moneyBuys(g, 'GER')).not.toContain('engineers');
    g.flags['mp:GER'] = 1;
    expect(menOutLine(g, 'GER')).toBeNull();
  });

  it('is the first line AT HOME the first month, then every third, and goes when the men come back', () => {
    const g = free(5);
    empty(g);
    rich(g);
    const card = (): ReturnType<typeof monthCard> => monthCard(g, markFor(g), 'GER');
    const first = card();
    expect(first.menOut).toBe(menOutLine(g, 'GER'));
    cardSeen(g, first);
    expect(g.flags['menout:GER']).toBe(g.turn);
    const seen: boolean[] = [];
    for (let k = 1; k <= 6; k++) {
      g.turn++;
      rich(g);
      const c = card();
      cardSeen(g, c);
      seen.push(!!c.menOut);
    }
    expect(seen).toEqual([false, false, true, false, false, true]);
    // The men come back: the line goes, and a new stretch starts its count afresh.
    g.flags['mp:GER'] = 5;
    g.turn++;
    cardSeen(g, card());
    expect(g.flags['menout:GER']).toBeUndefined();
    empty(g);
    g.turn++;
    rich(g);
    expect(card().menOut).toBeTruthy();
  });

  it('never in a historical scenario, where no reserve is counted', () => {
    const g = createGame(5, 'GER');
    g.nations.GER.pp = 1e6;
    expect(menOutLine(g, 'GER')).toBeNull();
  });
});
