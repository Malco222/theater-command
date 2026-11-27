// The war diary (release 2).
//
// Brian, 27 November 2026 13:40Z, yes to the playtester's idea: "a war diary: once a month, one line in a period voice
// about your biggest win and worst loss". As the player's month opens, one entry is set down for the month gone:
//
// 1. Only the land battles his own corps fought in that month (not a walk-in, not an ally's fight without him).
// 2. The biggest win is the one that cost the enemy most (men lost and taken prisoner); the worst loss is the defeat
//    that cost him most men. A month without either says so; a month without any battle is a quiet one.
// 3. Its words are picked from data/campaign/diary.json by a hash of the war's seed and the month: no die of the war's is
//    thrown, so the war is the one it would have been without it.
// 4. It is the player's own, and words only: no rule reads it. It is told in the month's news and kept on the diary page,
//    whose door is on the month's report.
//
// With the switch off nothing here is read and nothing is written.

import data from '../../data/campaign/diary.json';
import { hashString } from '../core/rng';
import { turnToDate } from '../core/ticket';
import type { BattleReport, Game } from './types';

export const DIARY = data as { keep: number; shown: number; round: number; win: string[]; loss: string[]; noWin: string[]; noLoss: string[]; quiet: string[] };

/** The switch: a grand campaign's own, off unless the release 2 ship list or the player turns it on. */
export const diaryOn = (g: Game): boolean => g.flags.diary === 1;

/** Turns the switch on or off at any time. Off forgets the diary. */
export function setDiary(g: Game, on: boolean): boolean {
  if (on) { g.flags.diary = 1; return true; }
  delete g.flags.diary;
  delete g.stories.diary;
  return true;
}

const pick = (g: Game, what: string, list: string[]): string => list[hashString(`diary:${g.seed}:${g.turn}:${what}`) % list.length];
const round = (n: number): string => Math.max(DIARY.round, Math.round(n / DIARY.round) * DIARY.round).toLocaleString('en-US');
const fill = (t: string, v: Record<string, string>): string => t.replace(/\{(\w+)\}/g, (_, k: string) => v[k] ?? '');

/** One of his battles, as the diary sees it: whether he won it, what it cost each side, and his corps that fought. */
interface Fought { r: BattleReport; won: boolean; ours: number; theirs: number; prisoners: number; unit: string; foe: string }

function fought(g: Game, r: BattleReport, me: string): Fought | null {
  if (r.yielded) return null;
  const att = r.attackers.some((u) => u.nat === me && !u.guard), def = r.defenders.some((u) => u.nat === me && !u.guard);
  if (!att && !def) return null;
  const attWon = r.level.endsWith('victory');
  const mine = att ? r.attackers : r.defenders, theirs = att ? r.defenders : r.attackers;
  const own = mine.filter((u) => u.nat === me && !u.guard).sort((a, b) => b.divs - a.divs || b.men - a.men);
  const foeNat = theirs.reduce<{ nat: string; men: number } | null>((best, u) => (!best || u.men > best.men ? { nat: u.nat, men: u.men } : best), null)?.nat ?? (att ? r.defNat : r.attNat);
  return {
    r, won: att ? attWon : !attWon,
    ours: own.reduce((s, u) => s + u.lost, 0),
    theirs: theirs.reduce((s, u) => s + u.lost, 0) + (att && attWon ? r.prisoners : 0),
    prisoners: att && attWon ? r.prisoners : 0,
    unit: own[0]?.name ?? 'our troops',
    foe: g.nations[foeNat]?.adj ?? foeNat,
  };
}

/** The entry for the month before 'g.turn', from his battles in it. Null where nobody plays. */
export function diaryEntry(g: Game): string | null {
  const me = g.human;
  if (!me) return null;
  const month = g.turn - 1;
  const all = g.reports.filter((r) => r.turn === month).map((r) => fought(g, r, me)).filter((x): x is Fought => !!x);
  const head = `${turnToDate(month).label}.`;
  if (!all.length) return `${head} ${pick(g, 'quiet', DIARY.quiet)}`;
  const win = all.filter((x) => x.won).sort((a, b) => b.theirs - a.theirs || a.r.id - b.r.id)[0];
  const loss = all.filter((x) => !x.won).sort((a, b) => b.ours - a.ours || a.r.id - b.r.id)[0];
  const words = (x: Fought): Record<string, string> => ({ place: x.r.regionName, unit: x.unit, adj: x.foe, theirs: round(x.theirs), ours: round(x.ours), prisoners: round(x.prisoners) });
  // (A line that names a number is used only where that number comes to at least one 'round': a win with no prisoners
  // is not told by the words that count them, nor a fight that cost almost nothing by the words that count the cost.)
  const fit = (x: Fought, list: string[]): string[] => {
    const left = list.filter((t) => (['theirs', 'ours', 'prisoners'] as const).every((k) => !t.includes(`{${k}}`) || x[k] >= DIARY.round / 2));
    return left.length ? left : list;
  };
  const winText = win ? fill(pick(g, 'win', fit(win, DIARY.win)), words(win)) : pick(g, 'nowin', DIARY.noWin);
  const lossText = loss ? fill(pick(g, 'loss', fit(loss, DIARY.loss)), words(loss)) : pick(g, 'noloss', DIARY.noLoss);
  return `${head} ${winText} ${lossText}`;
}

/** The player's month opens: the month gone is set down, and told in the news. */
export function diaryMonth(g: Game, nat: string, tell: (text: string, region?: number) => void): void {
  if (!diaryOn(g) || nat !== g.human || g.turn <= 0) return;
  const book = (g.stories.diary ??= []);
  if (book.some((e) => e.t === g.turn - 1)) return;
  const text = diaryEntry(g);
  if (!text) return;
  book.push({ t: g.turn - 1, text });
  if (book.length > DIARY.keep) book.splice(0, book.length - DIARY.keep);
  tell(`From the war diary: ${text}`);
}

/** The diary page's entries, the latest first. */
export const diaryEntries = (g: Game): { t: number; text: string }[] => (diaryOn(g) ? (g.stories.diary ?? []).slice(-DIARY.shown).reverse() : []);
