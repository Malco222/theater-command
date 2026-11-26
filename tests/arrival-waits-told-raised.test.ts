// A corps destroyed while its row waits, then raised again from the memorial wall under its old name, is not the corps
// the player was told of (arrivalwaits.ts `waitedOn`). The telling keeps the id of each corps it named at the front
// (`rowId:<i>:<k>`); a corps standing under the name with another id was never at the front and never left it, and the
// one he was told of is on the wall. Nothing true is said of it here, so the row forms with today's bare line, not
// "now that the Test Corps ... have left the front". The round 3 truth sceptic's Q2n, with the build time the rules give
// (economy.ts `orderCorps` with the fallen corps' id).
//
// The board is tests/arrival-waits-told-wall's: September 1939, Grenzmark beside Poznan, the Saar beside France;
// Bavaria and Magdeburg quiet. Germany is the player's, the switch on by hand.
import { afterEach, describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { orderCorps } from '../src/campaign/economy';
import { addCorps, createGame, relocate, resolveBattle, startPhase } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';

const ROWS = (setup as any).expeditions as any[];
let added = 0;
afterEach(() => { while (added > 0) { ROWS.pop(); added--; } });
const addRow = (row: any[]): number => { ROWS.push(row); added++; return ROWS.length - 1; };
const month = (g: Game, t: number): void => { g.turn = t; startPhase(g, 'GER'); };
const said = (g: Game, t: number): string[] => g.headlines.filter((h) => h.turn === t && h.nat === 'GER' && /Test/.test(h.text)).map((h) => h.text);
const open = (): Game => { const g = startOpen(createGame(1, 'GER')); g.flags.fromWait = 1; return g; };

function destroy(g: Game, c: Corps): void {
  const gz = g.regions[c.reg];
  for (const u of [...gz.units]) if (u !== c && u.nat === 'GER') relocate(g, u, g.byName.Bavaria);
  c.str = 0.05;
  const att = g.byName.Poznan.units.filter((u) => u.nat === 'POL');
  for (const u of att) { u.ap = u.apMax; u.str = 1; u.org = 1; }
  resolveBattle(g, att, gz);
  expect(c.dead).toBe(true);
}

describe('a corps destroyed, then raised again under its name, while its row waits', () => {
  it('forms with the bare line: the new corps never left the front, the old one is on the wall', () => {
    const g = open();
    const c = addCorps(g, 'GER', 'Test Corps', 'inf', 2, g.byName.Grenzmark.id, {});
    const o = addCorps(g, 'GER', 'Other Test Corps', 'inf', 2, g.byName.Saar.id, {});
    const i = addRow([2, 'GER', 'Test Army', 'inf', 2, 'Magdeburg', { from: ['Test Corps', 'Other Test Corps'] }]);
    month(g, 2);
    expect(said(g, 2)).toEqual(['The Test Army is due in Magdeburg this month, formed from two of your corps. The Test Corps in the Grenzmark and the Other Test Corps in the Saar are at the front, so the arrival waits, and forms the month after both leave the front.']);
    const oldId = c.id;
    destroy(g, c);
    expect(said(g, 2)[1]).toBe("Test Corps is struck off the army's list and written on the memorial wall.");
    const n = g.nations.GER; n.pp = 1e6;
    const f = g.stories.fallen.find((x) => x.name === 'Test Corps')!;
    const b = orderCorps(g, 'GER', 'inf', n.capital, f.id)!;
    expect(b.eta).toBe(7);
    let t = 3;
    for (; t <= b.eta; t++) {
      if (g.corps.includes(o) && o.reg !== g.byName.Saar.id) relocate(g, o, g.byName.Saar);
      month(g, t);
      expect(g.flags[`sent:${i}`]).toBeUndefined();
    }
    expect(said(g, 7)).toEqual(['Test Corps is raised again at Berlin, 5 months after it was lost at the Grenzmark. It carries the old colours and is ready for orders.']);
    const nc = g.corps.find((x) => x.name === 'Test Corps' && !x.dead)!;
    expect(nc.id).not.toBe(oldId);
    relocate(g, o, g.byName.Bavaria);
    month(g, 8);
    expect(g.flags[`sent:${i}`]).toBe(1);
    expect(said(g, 8)).toEqual(['Test Army has arrived in Magdeburg.']);
  });
});
