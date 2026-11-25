// China's rifles: a seller's fall is news once (the design director's 18:30Z of 25 November 2026, on stage B1; the
// stage B2 build's addendum, section 1). A fallen seller sells nothing, as B1 built it, "but its fall is news once":
// the month a road is first found dry because its seller has fallen, a human receiver is told so, once, and never
// again for that road while the seller stays fallen. A seller that still stands and sends nothing (at war with a great
// power, B1's interim test) tells nothing, and a computer receiver is told nothing. The state is set by hand on the
// first day's chart; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { capitulate, createGame, setWar } from '../src/campaign/game';
import { RIFLES, riflesMonth, roadsNow } from '../src/campaign/rifles';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';

const grand = (human: string | null = null): Game => startOpen(createGame(3, human));
const month = (g: Game): void => { g.turn++; riflesMonth(g, 'CHN'); };
const said = (g: Game): string[] => g.headlines.filter((h) => /Sinkiang/.test(h.text)).map((h) => `T${h.turn} ${h.text}`);
const sinkiang = (g: Game) => roadsNow(g, 'CHN').find((r) => r.id === 'sinkiang-road')!;
const FELL = 'The Soviet Union has fallen. The Sinkiang road carries no more rifles.';

describe("a seller's fall is news once", () => {
  it('the words are the far side\'s rows\', beside the roads', () => {
    expect(RIFLES.fallen).toBe('{seller} has fallen. {road} carries no more rifles.');
  });

  it('told once, in the month of the fall, and not the next month or after', () => {
    const g = grand('CHN');
    month(g);
    capitulate(g, 'SOV', 'GER');
    month(g);
    expect(sinkiang(g)).toMatchObject({ state: 'dry', rifles: 0 });
    expect(said(g)).toEqual([`T3 ${FELL}`]);
    month(g); month(g);
    expect(said(g)).toEqual([`T3 ${FELL}`]);
    // (Kept in the road's own flag: dry since month 3, told.)
    expect(g.flags['rflc:sinkiang-road']).toBe(-3.5);
    // And a save keeps it told.
    const h = loadGame(saveGame(g))!;
    h.turn++; riflesMonth(h, 'CHN');
    expect(said(h)).toEqual([`T3 ${FELL}`]);
  });

  it('not told for a seller that stands and is at war with a great power, nor for one with nothing to spare', () => {
    const g = grand('CHN');
    month(g);
    setWar(g, 'SOV', 'GER');
    month(g); month(g);
    expect(sinkiang(g)).toMatchObject({ state: 'dry', rifles: 0 });
    expect(said(g)).toEqual([]);
    expect(Number.isInteger(g.flags['rflc:sinkiang-road'])).toBe(true);
  });

  it('not told to a computer China, though its road is marked as a played one\'s would be', () => {
    for (const human of [null, 'JAP']) {
      const g = grand(human);
      month(g);
      capitulate(g, 'SOV', 'GER');
      month(g); month(g);
      expect(said(g), String(human)).toEqual([]);
      expect(g.flags['rflc:sinkiang-road']).toBe(-3.5);
    }
  });

  it('a seller already fallen when the road is first read is no news', () => {
    const g = grand('CHN');
    capitulate(g, 'SOV', 'GER');
    month(g); month(g);
    expect(said(g)).toEqual([]);
  });

  it('cut while the seller falls: told when the road is freed, never again while it stays fallen; running again told when the seller stands', () => {
    const g = grand('CHN');
    month(g);
    g.byName.Lanchow.ctrl = 'JAP'; month(g);
    capitulate(g, 'SOV', 'GER'); month(g);
    // (Cut: the place is read before the seller.)
    expect(sinkiang(g).state).toBe('cut');
    g.byName.Lanchow.ctrl = 'CHN'; month(g);
    g.byName.Lanchow.ctrl = 'JAP'; month(g);
    g.byName.Lanchow.ctrl = 'CHN'; month(g); month(g);
    // The seller stands again (freed): the road runs, and a second fall is news again.
    g.nations.SOV.alive = true; month(g);
    g.nations.SOV.alive = false; month(g); month(g);
    expect(said(g)).toEqual([
      'T3 The Sinkiang road is cut: Japan holds Lanchow. 1,300 fewer rifles reach your store each month.',
      `T5 ${FELL}`,
      // (Cut again in month 6: a road kept as cut while it came back dry is no news, as B1 built it.)
      'T9 The Sinkiang road runs again: 1,300 rifles a month can reach your store.',
      `T10 ${FELL}`,
    ]);
  });
});
