// flags.raidsFrom is a picture mark, not a rule's fact (the design director, 07:50Z, 27 November 2026).
//
// The 3D game's bridge writes flags.raidsFrom as the player ends his month: the last raid report filed before the
// month closed, so the reports page knows which raids are his to read next (airpages.ts raidsToRead). A gate that
// pairs a war stepped by the rules' own passMonth with the war the bridge stepped takes the mark out before it
// compares the two, as it takes out sight:*, wreck:* and subLook. That is honest only while no rule reads the mark.
// This file pins it twice: by reading the source (the one reader is the reports page's list), and by playing the
// same seated war with the mark set and unset (every month's whole save is the same once the mark is taken out).
// @ts-ignore node's own module: the tree's type check loads three's types only
import { readdirSync, readFileSync } from 'node:fs';
import { describe, expect, it } from 'vitest';
import { loadGame, saveGame } from '../src/campaign/save';
import { passMonth } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;

const SRC = decodeURIComponent(new URL('../src/', import.meta.url).pathname);
/** Every TypeScript file under src, as a path from src. */
function sources(under = ''): string[] {
  const out: string[] = [];
  for (const e of readdirSync(SRC + under, { withFileTypes: true }) as { name: string; isDirectory(): boolean }[]) {
    if (e.isDirectory()) out.push(...sources(`${under}${e.name}/`));
    else if (e.name.endsWith('.ts')) out.push(under + e.name);
  }
  return out;
}
/** The file's code with its comments taken out (a comment may name the mark; only code can read it). */
const code = (file: string): string => (readFileSync(SRC + file, 'utf8') as string).replace(/\/\*[\s\S]*?\*\//g, '').replace(/^\s*\/\/.*$/gm, '');
/** Reads the mark off a game's flags: flags.raidsFrom, flags?.raidsFrom, flags['raidsFrom'], or taken out by name. */
const READS = /flags\s*\??\.\s*raidsFrom|flags\s*\[\s*['"`]raidsFrom['"`]\s*\]|\{[^{}]*\braidsFrom\b[^{}]*\}\s*=\s*[\w.?]*flags\b/g;

/** The war with the mark taken out, as a gate takes it out before it compares. */
function unmarked(g: Game): string {
  const c = loadGame(saveGame(g))!;
  delete c.flags.raidsFrom;
  return saveGame(c);
}

describe('flags.raidsFrom is read by the reports page alone', () => {
  it('no file under src reads the mark but airpages.ts, and there only raidsToRead does', () => {
    const readers = sources().filter((f) => (code(f).match(READS) ?? []).length > 0);
    // A tree from before the mark has no reader at all; a tree with it has the one.
    expect(readers.filter((f) => f !== 'campaign/airpages.ts')).toEqual([]);
    if (readers.length === 0) return;
    const air = code('campaign/airpages.ts');
    const hits = [...air.matchAll(READS)];
    expect(hits.length).toBe(1);
    // The one read sits inside raidsToRead: after its opening line and before the next function begins.
    const at = hits[0].index!;
    const open = air.lastIndexOf('function ', at);
    expect(air.slice(open, air.indexOf('(', open))).toBe('function raidsToRead');
  });

  it('the same seated war with the mark set and unset is the same war every month, once the mark is taken out', () => {
    // His front door's own start: a seat, colonel, theatre (what the bridge's start is given by the 3D game).
    TC().start({ view: 'grand', human: 'GER', seed: 7, level: 'colonel', depth: 'theatre' });
    const text = (TC().save() as { text: string }).text;
    const plain = loadGame(text)!;
    const marked = loadGame(text)!;
    expect(plain.flags.raidsFrom).toBeUndefined();
    let mark = 0;
    marked.flags.raidsFrom = mark;
    expect(unmarked(marked)).toBe(unmarked(plain));
    for (let m = 1; m <= 3; m++) {
      expect(passMonth(plain)).toBe(true);
      expect(passMonth(marked)).toBe(true);
      // No rule moved the mark, and no rule moved anything because of it.
      expect(marked.flags.raidsFrom).toBe(mark);
      expect(plain.flags.raidsFrom).toBeUndefined();
      expect(unmarked(marked), `month ${m}`).toBe(unmarked(plain));
      // The mark as the bridge would write it at this month's end: the last report filed so far.
      mark = marked.airReports.length ? marked.airReports[marked.airReports.length - 1].id : m;
      marked.flags.raidsFrom = mark;
    }
  });
});
