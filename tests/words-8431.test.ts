// Version 84.3.1's words. The Playtester's check of 84.3 (/mnt/project-files/playtest/feel/v843-check.md): in January
// the army stands at Cleveland beside an empty Buffalo, and the paper said "Right-click Cleveland to send it", its own
// province. The UI director's check of the same version: where the right-click goes to ground of his on the way, the
// line says it is on the way ("Right-click Cleveland to send it on towards Pittsburgh."). The chance at the ends of the
// scale is in words on the cards; that is the page's, checked in the browser.
import { describe, expect, it } from 'vitest';
import { homeAlarms } from '../src/campaign/danger';
import { relocate } from '../src/campaign/game';
import { loadGame } from '../src/campaign/save';
import type { Game } from '../src/campaign/types';
import brianSave from './ahead/saves/brian-usa-1940-12.json?raw';

const brian = (): Game => loadGame(brianSave)!;
/** Buffalo emptied, the Canadians gone to Toronto. */
function emptied(): Game {
  const g = brian();
  const B = g.byName.Buffalo;
  for (const u of [...B.units, ...(B.garr ?? [])]) relocate(g, u, g.byName.Toronto);
  return g;
}

describe("the paper's line once the help is in hand names the right place (84.3.1)", () => {
  it('an army already beside the town with no moves left: it says when it can go in, never its own province', () => {
    const g = emptied();
    const B = g.byName.Buffalo, C = g.byName.Cleveland;
    // Every American corps but one stands far off; that one stands at Cleveland with its moves spent.
    const mine = g.corps.filter((c) => !c.dead && c.nat === 'USA' && c.kind === 'inf' && !g.regions[c.reg].overseas);
    const c = mine[0];
    relocate(g, c, C);
    c.ap = 0;
    const row = homeAlarms(g, 'USA').find((a) => a.region === B.id)!;
    expect(row.case).toBe('empty');
    expect(row.help).toBeTruthy();
    if (row.help!.region === C.id) {
      expect(row.help!.to).toBe(C.id);
      expect(row.hand).toMatch(/ is in hand\. It can march into Buffalo next month\.$/);
    }
    // Whatever the help, the line never sends it to the province it already stands in.
    for (const a of homeAlarms(g, 'USA')) if (a.help) expect(a.hand.includes(`Right-click ${g.regions[a.help.region].name} `)).toBe(false);
  });

  it('every line is one of the three forms, and a right-click short of the town says it is on the way', () => {
    const g = brian();
    const rows = homeAlarms(g, 'USA').filter((a) => a.help);
    expect(rows.length).toBeGreaterThan(0);
    for (const a of rows) {
      const town = g.regions[a.region].name, to = g.regions[a.help!.to].name;
      const ok = a.help!.to === a.region ? new RegExp(` is in hand\\. Right-click ${town} to (send it|march in)\\.$`)
        : a.help!.to === a.help!.region ? new RegExp(` is in hand\\. It can (march into|storm|attack) ${town} next month\\.$`)
        : new RegExp(` is in hand\\. Right-click ${to} to send it on towards ${town}\\.$`);
      expect(a.hand, a.hand).toMatch(ok);
    }
  });
});
