// The Options row for 'fromWait' (release 2; the UI director's words of 21:49Z, 25 November 2026, section 6, its close
// as rewritten at 22:13Z: "It can be switched on or off in any month."). Arrivals wait while their corps are at the
// front: the setter `setFromWait` (arrivalwaits.ts), the bridge's act('fromwaitrule:on' / ':off') and TC.fromWait(on),
// the view's `fromWait`, and the ship list's 'fromwaitrule' (the entry only: the rule ships off until the design
// director's gate). Off is the war as one that never had it where it can: the flag goes, and the telling's own flags
// with it; a row waiting this month forms next month wherever its corps stand. On in a running war, a row waits from
// the next month it comes due.
//
// The board is tests/from-wait-r2's: September 1939, Grenzmark beside Poznan, Poland at war with Germany; Bavaria and
// Magdeburg quiet. Germany is the player's. The rows are added at the foot of the data's list and taken off after each test.
import { afterEach, describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { FROMWAIT_ROW, fromWaitOn, setFromWait } from '../src/campaign/arrivalwaits';
import { addCorps, createGame, startPhase } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { Corps, Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
const grand = (seed = 3, human: string | null = 'GER'): Game => startOpen(createGame(seed, human));

const ROWS = (setup as any).expeditions as any[];
let added = 0;
function addRow(row: any[]): number { ROWS.push(row); added++; return ROWS.length - 1; }
afterEach(() => { while (added > 0) { ROWS.pop(); added--; } });

const month = (g: Game, turn: number): void => { g.turn = turn; startPhase(g, 'GER'); };
const living = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.nat === 'GER' && !c.dead && c.name === name);
const telFlags = (g: Game): string[] => Object.keys(g.flags).filter((k) => /^row(Told|At|Front):/.test(k));
const tellings = (g: Game): string[] => g.headlines.filter((h) => /formed from|left the front|has not formed yet|without your|The arrival/.test(h.text)).map((h) => h.text);
const fromSave = (): Game => loadGame((TC().save() as { text: string }).text)!;
/** The three 3D notes on pressing the row: the UI director's section 7, case 1 (00:44Z, 26 November 2026), word for word. */
const NOTES = {
  on: 'Arrivals now wait while their corps are at the front: one formed from a corps in the field forms the month after that corps leaves the front.',
  off: 'Arrivals no longer wait: a corps an arrival is formed from is taken wherever it stands, as before.',
  refused: 'Arrivals wait for their corps only in a grand campaign.',
};

/** A grand campaign of Germany's with "Test Corps" in contact at Grenzmark, and a row of month `due` taking it whole to form "Test Army" at Magdeburg. */
function board(on: boolean, due = 2, corps = 'Test Corps', army = 'Test Army'): { g: Game; i: number; held: Corps } {
  const g = startOpen(createGame(1, 'GER'));
  if (on) expect(setFromWait(g, true)).toBe(true);
  const held = addCorps(g, 'GER', corps, 'inf', 2, g.byName.Grenzmark.id, {});
  const i = addRow([due, 'GER', army, 'inf', 2, 'Magdeburg', { from: [corps] }]);
  return { g, i, held };
}

describe('the switch through the bridge', () => {
  it('on and off in a running grand campaign, by act() and by TC.fromWait()', () => {
    TC().load(saveGame(grand()));
    expect((TC().view() as { fromWait: boolean }).fromWait).toBe(false);
    const on = TC().act('fromwaitrule:on') as { ok: boolean; note: string; view: { fromWait: boolean } };
    expect(on.ok).toBe(true);
    expect(on.note).toBe(NOTES.on);
    expect(on.view.fromWait).toBe(true);
    expect(fromSave().flags.fromWait).toBe(1);
    const off = TC().act('fromwaitrule:off') as { ok: boolean; note: string; view: { fromWait: boolean } };
    expect(off.ok).toBe(true);
    expect(off.note).toBe(NOTES.off);
    expect(off.view.fromWait).toBe(false);
    expect(fromSave().flags.fromWait).toBeUndefined();
    expect(TC().fromWait(true)).toEqual({ set: true, on: true });
    expect((TC().view() as { fromWait: boolean }).fromWait).toBe(true);
    expect(TC().fromWait(false)).toEqual({ set: true, on: false });
    expect((TC().view() as { fromWait: boolean }).fromWait).toBe(false);
  });

  it('refused outside a grand campaign', () => {
    const h = createGame(3, 'GER');
    expect(setFromWait(h, true)).toBe(false);
    expect(h.flags.fromWait).toBeUndefined();
    TC().load(saveGame(createGame(3, 'GER')));
    const on = TC().act('fromwaitrule:on') as { ok: boolean; note: string; view: { fromWait: boolean } };
    expect(on.ok).toBe(false);
    expect(on.note).toBe(NOTES.refused);
    expect(on.view.fromWait).toBe(false);
    expect(TC().fromWait(true)).toEqual({ set: false, on: false });
    expect(fromSave().flags.fromWait).toBeUndefined();
    expect(TC().r2Ship(['fromwaitrule'])).toBeNull();
  });

  it("the three notes on pressing the row are arrivalwaits.ts's (FROMWAIT_ROW.on, .off, .refused): the bridge holds no words of its own", () => {
    // (words-01: every word the player reads of the rule lives in the module. The notes are swapped for marks here, so
    // a bridge that wrote its own words, even the same ones, would not show the marks.)
    const row = FROMWAIT_ROW as unknown as Record<'on' | 'off' | 'refused', string>;
    const kept = { on: row.on, off: row.off, refused: row.refused };
    expect([typeof kept.on, typeof kept.off, typeof kept.refused]).toEqual(['string', 'string', 'string']);
    try {
      Object.assign(row, { on: '(on)', off: '(off)', refused: '(refused)' });
      TC().load(saveGame(grand()));
      expect((TC().act('fromwaitrule:on') as { note: string }).note).toBe('(on)');
      expect((TC().act('fromwaitrule:off') as { note: string }).note).toBe('(off)');
      TC().load(saveGame(createGame(3, 'GER')));
      expect((TC().act('fromwaitrule:on') as { note: string }).note).toBe('(refused)');
    } finally { Object.assign(row, kept); }
    TC().load(saveGame(grand()));
    expect((TC().act('fromwaitrule:on') as { note: string }).note).toBe(FROMWAIT_ROW.on);
    expect((TC().act('fromwaitrule:off') as { note: string }).note).toBe(FROMWAIT_ROW.off);
  });

  it("the ship list takes it once; a later off stays off", () => {
    TC().load(saveGame(grand()));
    expect(TC().r2Ship(['fromwaitrule'])).not.toBeNull();
    expect((TC().view() as { fromWait: boolean }).fromWait).toBe(true);
    expect((TC().act('fromwaitrule:off') as { ok: boolean }).ok).toBe(true);
    expect(TC().r2Ship(['fromwaitrule'])).toBeNull();
    expect((TC().view() as { fromWait: boolean }).fromWait).toBe(false);
    const back = fromSave();
    expect(back.flags['r2:fromwaitrule']).toBe(1);
    expect(back.flags.fromWait).toBeUndefined();
  });

  it("the Options row's name and whole line are the UI director's section 6, and its three notes her section 7, case 1", () => {
    // (words-09: the whole line, not its ending.)
    expect(FROMWAIT_ROW.id).toBe('fromwaitrule');
    expect(FROMWAIT_ROW.name).toBe('Arrivals wait while their corps are at the front');
    expect(FROMWAIT_ROW.line).toBe("An arrival due on a date and formed from a corps already in the field, yours or any other side's, waits while that corps is at the front, and forms the month after the corps leaves it. Where an arrival is offered to you, you decide instead. Off, the corps is taken wherever it stands, as before. This changes the month some arrivals form. It works only in a grand campaign: from History's shape to Anything goes, and at By the book and Close to history once History's book has closed where the arrival is due. It can be switched on or off in any month.");
    expect(FROMWAIT_ROW.on).toBe(NOTES.on);
    expect(FROMWAIT_ROW.off).toBe(NOTES.off);
    expect(FROMWAIT_ROW.refused).toBe(NOTES.refused);
  });
});

describe('off and on in a running war', () => {
  it("off in a waiting month: the waiting row forms the next month where its corps stands, and the telling's flags are cleared", () => {
    const { g, i, held } = board(true);
    month(g, 2);
    expect(g.flags[`wait:${i}`]).toBe(2);
    expect(telFlags(g).sort()).toEqual([`rowAt:${i}`, `rowFront:${i}`, `rowTold:${i}`]);
    expect(tellings(g)).toHaveLength(1);
    // Off through the bridge, as Options does it, in the month the row waits.
    TC().load(saveGame(g));
    expect((TC().act('fromwaitrule:off') as { ok: boolean }).ok).toBe(true);
    const h = fromSave();
    expect(h.flags.fromWait).toBeUndefined();
    expect(telFlags(h)).toEqual([]);
    // (The rule's own month stays: its half year counts from it, as before.)
    expect(h.flags[`wait:${i}`]).toBe(2);
    // Next month the corps is still in contact at Grenzmark, and is taken all the same: today's line, nothing told.
    month(h, 3);
    expect(living(h, held.name)).toBeUndefined();
    expect(h.regions[living(h, 'Test Army')!.reg].name).toBe('Magdeburg');
    expect(h.flags[`sent:${i}`]).toBe(1);
    expect(tellings(h)).toHaveLength(1);
    expect(telFlags(h)).toEqual([]);
    // And in the rules alone, the same.
    expect(setFromWait(g, false)).toBe(true);
    expect(fromWaitOn(g)).toBe(false);
    expect(telFlags(g)).toEqual([]);
  });

  it('on in a running war: a row formed before it took its corps from the front; the next row due waits', () => {
    const { g, i: first, held: a } = board(false, 2);
    const b = addCorps(g, 'GER', 'Second Corps', 'inf', 2, g.byName.Grenzmark.id, {});
    const second = addRow([3, 'GER', 'Second Army', 'inf', 2, 'Magdeburg', { from: ['Second Corps'] }]);
    month(g, 2);
    expect(a.dead).toBe(true);
    expect(g.flags[`sent:${first}`]).toBe(1);
    expect(setFromWait(g, true)).toBe(true);
    expect(fromWaitOn(g)).toBe(true);
    month(g, 3);
    expect(b.dead).toBeFalsy();
    expect(g.regions[b.reg].name).toBe('Grenzmark');
    expect(living(g, 'Second Army')).toBeUndefined();
    expect(g.flags[`wait:${second}`]).toBe(3);
    expect(g.flags[`rowTold:${second}`]).toBe(3);
  });

  // The design director's answer 2 of 00:43Z, 26 November 2026: "Keep the rule's own wait marker. Off acts from the
  // month it is turned off, and what already happened stays in the save, as everywhere. Two tests: a war never
  // switched on is the whole save of off; a war switched off mid-war plays by off from that month with its past kept."
  // The second, a far-side war, is tests/from-wait-switch-war.test.ts.
  it('a war never switched on is the whole save of off: the setter turned on and off before any row waits leaves the save byte-equal to the untouched war', () => {
    const war = (): Game => startOpen(createGame(1, 'GER'));
    const untouched = war(), quick = war(), later = war();
    // On and off at once; and on for the war's first months, off before any row has waited.
    expect(setFromWait(quick, true) && setFromWait(quick, false)).toBe(true);
    expect(setFromWait(later, true)).toBe(true);
    for (let t = 1; t < 4; t++) { runTurnAI(untouched); runTurnAI(quick); runTurnAI(later); }
    expect(Object.keys(later.flags).filter((k) => k.startsWith('wait:')).sort()).toEqual(Object.keys(untouched.flags).filter((k) => k.startsWith('wait:')).sort());
    expect(telFlags(later)).toEqual([]);
    expect(setFromWait(later, false)).toBe(true);
    for (let t = 4; t < 7; t++) { runTurnAI(untouched); runTurnAI(quick); runTurnAI(later); }
    expect(untouched.turn).toBe(7);
    const text = saveGame(untouched);
    expect(text).not.toMatch(/fromWait|rowTold|rowAt|rowFront/);
    expect(saveGame(quick)).toBe(text);
    expect(saveGame(later)).toBe(text);
  }, 1800000);

  it('with the switch never touched the save holds no new key', () => {
    const { g } = board(false);
    month(g, 2);
    const text = saveGame(g);
    expect(text).not.toMatch(/fromWait|fromwaitrule|rowTold|rowAt|rowFront/);
    TC().load(text);
    TC().view();
    expect((TC().save() as { text: string }).text).not.toMatch(/fromWait|fromwaitrule|rowTold|rowAt|rowFront/);
  });
});
