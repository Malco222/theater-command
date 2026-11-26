// The islands' rows the rules read (rulings 173 and 179; form B3 of 25 November 2026) are the historian's table and
// nothing else: data/campaign/far-islands.json is written from data/campaign/tables/islands-draft.csv by
// tools/make-far-islands.mjs, and this fails when the table has changed and the file has not been written again.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { islandNames, islandOf } from '../src/campaign/islands';
import { startOpen } from '../src/campaign/staff';
import file from '../data/campaign/far-islands.json';
// @ts-expect-error a tool script, plain JavaScript
import { islandRows } from '../tools/make-far-islands.mjs';

describe("the islands' rows", () => {
  it('are the table, row for row', () => {
    const made = islandRows() as { islands: Record<string, unknown>; noRow: string[] };
    expect((file as { islands: unknown }).islands).toEqual(made.islands);
    expect((file as { _noRow: string[] })._noRow).toEqual(made.noRow);
    expect(Object.keys(made.islands)).toHaveLength(43);
    expect(made.noRow).toEqual(['Addu Atoll', 'Seychelles']);
  });

  it("give an atoll room for one division's worth of men and a floor of a quarter, a large island more room and a half", () => {
    const g = startOpen(createGame(3, null));
    const of = (name: string) => islandOf(g, g.byName[name]);
    expect(of('Wake')).toEqual({ room: 1, floor: 0.25, islet: true });
    expect(of('Iwo Jima')).toEqual({ room: 1, floor: 0.25, islet: true });
    expect(of('Truk')).toEqual({ room: 2, floor: 0.5, islet: true });
    expect(of('Okinawa')).toEqual({ room: 4, floor: 0.5, islet: false });
    expect(of('Guadalcanal')).toEqual({ room: 3, floor: 0.5, islet: false });
    expect(of('Pearl Harbor')).toEqual({ room: 2, floor: 0.5, islet: false });
    // The design director's stand-ins of 14:28Z, 25 November 2026, (c), flagged G in the table: Singapore 5, Formosa's
    // south (Takao) 3; Taihoku stays 2.
    expect(of('Singapore')).toEqual({ room: 5, floor: 0.25, islet: false });
    expect(of('Formosa')).toEqual({ room: 3, floor: 0.5, islet: false });
    expect(of('Taihoku')).toEqual({ room: 2, floor: 0.5, islet: false });
    // Ground that is no island of the table has no row: Tokyo, Honolulu's neighbour, a province of Europe.
    for (const name of ['Tokyo', 'Maui', 'Paris']) expect(of(name), name).toBeNull();
    expect(islandNames()).toHaveLength(43);
  }, 300000);
});
