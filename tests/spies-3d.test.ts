// The 3D game's THE SECRET SERVICE page (unreal/TheaterCommand/Rules/campaign-bridge.ts), release 2: the service's
// file shows the newest notes, where a turned agent's telltale report is (Brian, 22 November 2026, yes (3): a
// telltale sign so a false report is never unfair), and the note gives the chance by country only where there is
// more than one country, as the browser does.
import { describe, expect, it } from 'vitest';
import { createGame, setWar } from '../src/campaign/game';
import { spyRoom } from '../src/campaign/intel';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { turnToDate } from '../src/core/ticket';
import type { Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Page = { note: string; sections: { heading: string; text: string }[] };
const TC = (globalThis as unknown as { TC: { load: (t: string) => unknown; page: (id: string) => Page } }).TC;
const show = (g: Game): Page => { expect(TC.load(saveGame(g))).not.toBeNull(); return TC.page('spies'); };

describe("THE SERVICE'S FILE in the 3D game", () => {
  it('shows the ten newest notes, newest first, so the tell is never pushed off the page', () => {
    const g = startOpen(createGame(7, 'UK'));
    setWar(g, 'UK', 'GER');
    const ruhr = g.byName.Ruhr.id;
    for (let k = 0; k < 15; k++) g.intel.notes.push({ turn: g.turn - 15 + k, agent: 'Kestrel', nat: 'UK', reg: ruhr, what: 'seen', text: `Report number ${k}.`, good: true });
    g.intel.notes.push({ turn: g.turn, agent: 'Kestrel', nat: 'UK', reg: ruhr, what: 'quiet', text: 'Agent Kestrel, Ruhr: all quiet, and the German forces there are weaker than we feared.', good: true });
    const file = show(g).sections.find((s) => s.heading === "THE SERVICE'S FILE")!.text.split('\n');
    expect(file.length).toBe(10);
    expect(file[0]).toBe(`${turnToDate(g.turn).label}: Agent Kestrel, Ruhr: all quiet, and the German forces there are weaker than we feared.`);
    expect(file[1]).toMatch(/Report number 14\.$/);
    expect(file[9]).toMatch(/Report number 6\.$/);
  });
});

describe("the page's note", () => {
  it('gives the chance by country only where more than one country spies on him', () => {
    const g = startOpen(createGame(7, 'UK'));
    setWar(g, 'UK', 'GER');
    g.flags.spyfrom = -99;
    expect(spyRoom(g)!.threat.foes).toEqual(['GER']);
    const note = show(g).note;
    expect(note).toMatch(/ This month the chance of an enemy strike at home is \d+%, with 0 of your agents on guard\.$/);
    expect(note).not.toMatch(/by country/);
    setWar(g, 'UK', 'HUN');
    expect(spyRoom(g)!.threat.foes.length).toBeGreaterThan(1);
    expect(show(g).note).toMatch(/ The chance each month, by country: .*Germany \d+%.*\.$/);
  });
});
