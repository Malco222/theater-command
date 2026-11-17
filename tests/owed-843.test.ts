// Version 84.3's small mends. b385 (the design director, 17 November 2026; the UI director's words, words-ruling-211.md
// section 1): home ground still in enemy hands with no troops of theirs in it keeps its red row until it is his again,
// after every row with an enemy army, and the row names the cure.
import { describe, expect, it } from 'vitest';
import { gunsAlone, homeAlarms } from '../src/campaign/danger';
import { fights, relocate } from '../src/campaign/game';
import { monthCard, monthMark } from '../src/campaign/monthcard';
import { loadGame } from '../src/campaign/save';
import { beginHuman, endHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';
import brianSave from './ahead/saves/brian-usa-1940-12.json?raw';
// (The Playtester's staged saves of 84.2.6, /mnt/project-files/playtest/feel/v8426/.)
import pittsburghSave from './ahead/saves/staged-canadian-corps-at-pittsburgh.save.txt?raw';
import vancouverSave from './ahead/saves/staged-american-guns-alone-round-vancouver.save.txt?raw';

const brian = (): Game => loadGame(brianSave)!;

describe('home ground still in enemy hands with no troops in it keeps its row (b385)', () => {
  it('Buffalo emptied by the Canadians: the cure in two sentences, after every enemy army', () => {
    const g = brian();
    const B = g.byName.Buffalo, T = g.byName.Toronto;
    expect(B.ctrl).toBe('CAN');
    for (const u of [...B.units, ...(B.garr ?? [])]) relocate(g, u, T);
    const all = homeAlarms(g, 'USA');
    const row = all.find((a) => a.region === B.id)!;
    expect(row.case).toBe('empty');
    expect(row.by).toBe('CAN');
    expect(row.lines[0]).toBe('Buffalo, on your home ground, is still in Canadian hands, with no troops in it.');
    expect(row.lines[1]).toBe('');
    expect(row.lines[2]).toMatch(/^(.+ can march in and take it back (this month|in \d+ months?, by (rail|road))\.|No army of yours can reach it\. A new corps takes \d+ months?\.)$/);
    if (row.help) expect(row.hand).toMatch(/ is in hand\. (Right-click .+ to (march in|send it|send it on towards Buffalo)|It can march into Buffalo next month)\.$/);
    // Every row with an enemy army comes first.
    const firstEmpty = all.findIndex((a) => a.case === 'empty');
    expect(all.slice(firstEmpty).every((a) => a.case === 'empty')).toBe(true);
  });

  it('ground he holds again, or ground of a country at peace with him, has no row', () => {
    const g = brian();
    const B = g.byName.Buffalo;
    for (const u of [...B.units, ...(B.garr ?? [])]) relocate(g, u, g.byName.Toronto);
    B.ctrl = 'USA';
    expect(homeAlarms(g, 'USA').some((a) => a.region === B.id)).toBe(false);
  });
});

describe("the Playtester's 84.2.6 finds", () => {
  it('a siege laid on his town with nobody in the field reads BESIEGED by its besieger, never a nameless LOST', () => {
    const g = loadGame(pittsburghSave)!;
    const mark = monthMark(g, 'USA');
    endHuman(g);
    for (let k = 0; !humanUp(g) && k < 400; k++) stepAI(g);
    beginHuman(g);
    const rows = monthCard(g, mark, 'USA').rows.filter((r) => r.arm === 'land');
    expect(rows.find((r) => r.region === g.byName.Cleveland.id)).toMatchObject({
      place: 'Cleveland', tag: 'BESIEGED', tone: 'warn', head: 'Cleveland is besieged', line: 'Canadian troops close round the town.', join: { word: 'by', name: 'Canada' }, theirs: 'CAN',
    });
    // (Every land row that names the other side has its name after the flag.)
    for (const r of rows) if (r.theirs) expect(r.join?.name).toBeTruthy();
  });

  it("the guns' own row picks up the infantry that can reach them this month, not the guns", () => {
    const g = loadGame(vancouverSave)!;
    const row = gunsAlone(g, 'USA').find((x) => x.region === g.byName.Vancouver.id)!;
    expect(row).toMatchObject({ what: 'guns', red: true, from: g.byName['Puget Sound'].id, line: 'Only guns are left round Vancouver: send infantry or tanks this month, or the siege is raised.' });
    const help = row.help.map((id) => g.corps.find((c) => c.id === id)!);
    expect(help.length).toBeGreaterThan(0);
    for (const c of help) {
      expect(c.reg).toBe(row.from);
      expect(fights(c)).toBe(true);
    }
  });
});
