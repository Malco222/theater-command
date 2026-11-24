// A retreat onto far-side ground is weighed by whether that ground's feed survives the retreat (the design
// director's 11:40Z of 22 November 2026 to ruling 234, cause A of Tripoli changing hands early with the far side on;
// src/campaign/game.ts, `retreatTarget` and `fedWithout`).
//
// Beaten at Nalut with Misrata gone, Italy's 5th Army fell back on Sebha, not on Tripoli: Tripoli had no way out
// left, the open desert had four. Sebha read fed on the picture as it stood, but its line ran through Nalut, the
// ground just lost: unfed the next month, the army died there, in every one of the six wars read on bdc0c86a. Sebha
// is far-side ground; with the far side off it is not on the board. Now a far-side candidate reads fed only if it is
// still joined to a source of supply with the ground the corps is leaving left out. And an old-chart candidate counts
// its ways out on the old chart alone (11:53Z): with Sebha beside it Hon read one way out and took the army, whose
// line there ran through Nalut too; with the far side off Hon is a dead end and the army goes to Tripoli. The old
// chart's scoring is otherwise as it was. The board is set by hand on the first day's chart; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { computeSupply, createGame, declareWar, retreatTarget } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { shoreOf } from '../src/campaign/world';
import type { Game, Region } from '../src/campaign/types';

/** Libya's old-chart ground: Italy's own, joined by land to Tripoli. */
const libya = (g: Game): Region[] => {
  const seen = new Set([g.byName['Tripoli'].id]), q = [g.byName['Tripoli'].id];
  for (let i = 0; i < q.length; i++) for (const j of g.regions[q[i]].nb) { const r = g.regions[j]; if (!seen.has(j) && r.owner === 'ITA' && shoreOf(r) === 0) { seen.add(j); q.push(j); } }
  return [...seen].map((id) => g.regions[id]);
};
/** Italy at war with Britain, who holds the named ground of Libya's (or all of it but what is kept). */
const board = (lost: (r: Region) => boolean): Game => {
  const g = startOpen(createGame(11, null));
  g.turn = 25;
  declareWar(g, 'UK', 'GER');
  declareWar(g, 'ITA', 'UK');
  for (const r of libya(g)) if (lost(r)) { r.ctrl = 'UK'; for (const c of [...r.units]) if (c.nat === 'ITA') { c.dead = true; r.units.splice(r.units.indexOf(c), 1); } }
  computeSupply(g);
  return g;
};
const fifth = (g: Game) => g.corps.find((c) => c.nat === 'ITA' && c.name.startsWith('5th Army'))!;
const enemy = (g: Game): Set<number> => new Set(g.regions.filter((r) => r.ctrl === 'UK').map((r) => r.id));

describe('a retreat onto far-side ground reads its feed after the move (the design director, 11:40Z of 22 November 2026)', () => {
  it('does not fall back from Nalut on Sebha when Sebha is fed only through Nalut', () => {
    // Britain holds all of Libya's old-chart ground but Tripoli, Nalut and Hon: Tripoli is Italy's last source there.
    const g = board((r) => !['Tripoli', 'Nalut', 'Hon'].includes(r.name));
    const nalut = g.byName['Nalut'], sebha = g.byName['Sebha'];
    expect(g.regions[fifth(g).reg].name).toBe('Nalut');
    expect(shoreOf(sebha)).toBe(2);
    expect(nalut.nb).toContain(sebha.id);
    // As the picture stands Sebha reads fed, through Nalut: Tripoli is the source, and it touches Nalut and Misrata alone.
    expect(g.supply['ITA'][sebha.id]).toBe(1);
    expect(g.supply['ITA'][g.byName['Tripoli'].id]).toBe(2);
    expect(g.byName['Tripoli'].nb.filter((j) => g.regions[j].ctrl === 'ITA').map((j) => g.regions[j].name)).toEqual(['Nalut']);
    // Hon is Italy's too, beside Nalut and Sebha, and with Misrata and Sirte gone it has no way out on the old chart.
    const hon = g.byName['Hon'];
    expect(shoreOf(hon)).toBe(0);
    expect(hon.nb.filter((j) => j !== nalut.id && g.regions[j].ctrl === 'ITA').map((j) => g.regions[j].name)).toEqual(['Sebha']);
    // Sebha is refused for its feed, Hon for its dead end, and the army falls back on its port: as with the far side off.
    expect(retreatTarget(g, fifth(g), nalut, enemy(g))!.name).toBe('Tripoli');
  });

  it('still falls back on Sebha when its feed does not run through the ground lost', () => {
    // Only Misrata and Sirte are Britain's: Cyrenaica's ports feed the desert behind by El Agheila and by Kufra.
    const g = board((r) => ['Misrata', 'Sirte'].includes(r.name));
    const to = retreatTarget(g, fifth(g), g.byName['Nalut'], enemy(g))!;
    expect(to.name).toBe('Sebha');
  });

  it('reads no far-side candidate as fed that is not fed now', () => {
    const g = board((r) => !['Tripoli', 'Nalut', 'Hon'].includes(r.name));
    // (Sebha cut off by hand: nothing reads it fed, before or after.)
    g.supply['ITA'][g.byName['Sebha'].id] = 0;
    const to = retreatTarget(g, fifth(g), g.byName['Nalut'], enemy(g))!;
    expect(to.name).not.toBe('Sebha');
  });
});
