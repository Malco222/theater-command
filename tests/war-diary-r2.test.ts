// The war diary (release 2; Brian's yes of 27 November 2026 13:40Z): as the player's month opens, one entry for the
// month gone, its biggest win and its worst loss among his own corps' land battles, in the voice of the time. Words only,
// the player's own; its words are picked by a hash, never by the war's dice. With the switch off nothing is written.
import { describe, expect, it } from 'vitest';
import { DIARY, diaryEntries, diaryEntry, diaryMonth, diaryOn, setDiary } from '../src/campaign/diary';
import { createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import { loadGame, saveGame } from '../src/campaign/save';
import { turnToDate } from '../src/core/ticket';
import type { BattleReport, Game, ReportUnit } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
const war = (seed = 3, human: string | null = 'GER'): Game => startOpen(createGame(seed, human));
const unit = (name: string, nat: string, men: number, lost: number): ReportUnit => ({ name, nat, kind: 'inf', divs: 2, men, lost, fate: '' });
let ids = 1;
const report = (g: Game, place: string, level: BattleReport['level'], attackers: ReportUnit[], defenders: ReportUnit[], extra: Partial<BattleReport> = {}): BattleReport =>
  ({ id: ids++, turn: g.turn - 1, week: 1, region: 5, regionName: place, attNat: attackers[0].nat, defNat: defenders[0].nat, attackers, defenders, lines: [], aPower: 1, dPower: 1, odds: 1, level, taken: level.endsWith('victory'), attLost: 0, defLost: 0, prisoners: 0, ...extra }) as unknown as BattleReport;

describe('the war diary (release 2)', () => {
  it('is off unless switched on: nothing set down, nothing told, no door', () => {
    const g = war();
    g.turn = 6;
    expect(diaryOn(g)).toBe(false);
    diaryMonth(g, 'GER', () => { throw new Error('told'); });
    expect(g.stories.diary).toBeUndefined();
    expect(diaryEntries(g)).toEqual([]);
  });

  it("sets down the month's biggest win and worst loss of his own corps, by what each cost", () => {
    const g = war();
    g.turn = 6;
    g.reports.push(
      report(g, 'Lodz', 'Decisive victory', [unit('XIV Corps', 'GER', 40000, 2000)], [unit('Army Lodz', 'POL', 30000, 9000)], { prisoners: 12000 }),
      report(g, 'Kutno', 'Close victory', [unit('VIII Corps', 'GER', 30000, 1000)], [unit('Army Poznan', 'POL', 20000, 3000)]),
      report(g, 'Bzura', 'Decisive victory', [unit('Army Pomorze', 'POL', 30000, 5000)], [unit('X Corps', 'GER', 25000, 6400)]),
      // An ally's fight without him, and a walk-in, are not his.
      report(g, 'Elsewhere', 'Decisive victory', [unit('Some Corps', 'SOV', 30000, 100)], [unit('Army Far', 'POL', 30000, 20000)]),
      report(g, 'Walkover', 'Close victory', [unit('XX Corps', 'GER', 30000, 0)], [unit('Nobody', 'POL', 1, 0)], { yielded: true }),
    );
    const text = diaryEntry(g)!;
    expect(text.startsWith(`${turnToDate(5).label}. `)).toBe(true);
    expect(text).toContain('Lodz');
    expect(text).toContain('XIV Corps');
    expect(text).toContain('Bzura');
    expect(text).toContain('X Corps');
    expect(text).not.toMatch(/Kutno|Elsewhere|Walkover/);
    expect(text).not.toMatch(/\{\w+\}/);
  });

  it('a month without a win, without a loss, or without a battle says so', () => {
    const g = war();
    g.turn = 6;
    expect(DIARY.quiet.some((q) => diaryEntry(g) === `${turnToDate(5).label}. ${q}`)).toBe(true);
    g.reports.push(report(g, 'Lodz', 'Decisive victory', [unit('XIV Corps', 'GER', 40000, 2000)], [unit('Army Lodz', 'POL', 30000, 9000)]));
    expect(DIARY.noLoss.some((t) => diaryEntry(g)!.endsWith(t))).toBe(true);
    // (No prisoners taken: the words that count prisoners are not used.)
    expect(diaryEntry(g)).not.toMatch(/ 100 prisoners/);
    g.reports.length = 0;
    g.reports.push(report(g, 'Bzura', 'Decisive victory', [unit('Army Pomorze', 'POL', 30000, 5000)], [unit('X Corps', 'GER', 25000, 6400)]));
    expect(DIARY.noWin.some((t) => diaryEntry(g)!.includes(t))).toBe(true);
  });

  it('is set down once a month, told in the news, and throws none of the war\'s dice', () => {
    const g = war();
    setDiary(g, true);
    g.turn = 6;
    const rng = g.rng.state;
    const told: string[] = [];
    diaryMonth(g, 'GER', (t) => told.push(t));
    diaryMonth(g, 'GER', (t) => told.push(t));
    diaryMonth(g, 'SOV', (t) => told.push(t));
    expect(told.length).toBe(1);
    expect(told[0]).toMatch(/^From the war diary: /);
    expect(g.stories.diary).toEqual([{ t: 5, text: told[0].replace('From the war diary: ', '') }]);
    expect(g.rng.state).toEqual(rng);
    setDiary(g, false);
    expect(g.stories.diary).toBeUndefined();
  });

  it('a war played with it on keeps a month-by-month diary through a save, and its page reads it back', () => {
    const g = war(17, 'GER');
    setDiary(g, true);
    for (let m = 0; m < 8; m++) runTurnAI(g);
    const book = g.stories.diary!;
    expect(book.length).toBeGreaterThanOrEqual(6);
    expect(new Set(book.map((e) => e.t)).size).toBe(book.length);
    expect(g.headlines.filter((h) => h.text.startsWith('From the war diary: ')).length).toBe(book.length);
    const back = loadGame(saveGame(g))!;
    expect(back.stories.diary).toEqual(book);
    TC().load(saveGame(g));
    const p = TC().page('diary') as { title: string; headline: string; sections: { heading: string; text: string }[] };
    expect(p.title).toBe('THE WAR DIARY  ·  GERMANY');
    expect(p.headline).toBe(`${book.length} months set down`);
    expect(p.sections[0].text.startsWith(book[book.length - 1].text)).toBe(true);
    // The door is on the month's report, once a month has ended.
    TC().endMonth();
    const r = TC().page('reports') as { top?: { label: string; act: string }[] };
    expect(r.top?.some((b) => b.label === 'THE WAR DIARY' && b.act === 'open:diary')).toBe(true);
  });

  it('the 3D ship list turns it on once, and a switch the player turns off stays off', () => {
    TC().start({ view: 'grand', human: 'GER', seed: 11 });
    const read = (): Game => loadGame((TC().save() as { text: string }).text)!;
    expect(diaryOn(read())).toBe(false);
    expect(TC().r2Ship(['diaryrule'])).not.toBeNull();
    expect(diaryOn(read())).toBe(true);
    TC().act('diaryrule:off');
    expect(TC().r2Ship(['diaryrule'])).toBeNull();
    expect(diaryOn(read())).toBe(false);
  });
});
