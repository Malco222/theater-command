// The marked army (release 2; Brian, 24 November 2026 18:01Z, "yes to all": the third idea of the enemy's month near
// you). The enemy corps that takes one of the player's towns is marked, and wears a red tag on the table until he beats
// it. Beating it, or its end, clears the mark with a line of news; a peace clears it without one. The flag 'marked'
// is off unless set, and a mark changes no number of the war.
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { loadGame, saveGame } from '../src/campaign/save';
import { MARKS_MOST, markBattle, markFallen, markedOn, marksMonth, marksOf, setMarked } from '../src/campaign/marked';
import type { BattleReport, Corps, Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type ViewCorps = { id: number; nat: string; dim: boolean; mark?: string };
const TC = (globalThis as unknown as { TC: { start: (o: object) => unknown; endMonth: () => { view: { corps: ViewCorps[]; markedOn: boolean } }; marked: (on: boolean) => { on: boolean }; act: (a: string) => { ok: boolean; note: string; view: { markedOn: boolean } } } }).TC;

const open = (nat: string, seed = 1939): Game => startOpen(createGame(seed, nat));
/** A battle report for a fight at Region between these corps; taken: the attack won the field (and, without siege, the place). */
const report = (g: Game, region: number, att: Corps[], def: Corps[], over: Partial<BattleReport>): BattleReport => ({
  id: 1, turn: g.turn, week: 1, region, regionName: g.regions[region].name, attNat: att[0].nat, defNat: def[0]?.nat ?? g.regions[region].ctrl,
  attackers: att.map((c) => ({ name: c.name, nat: c.nat, kind: c.kind, divs: c.divs, men: 10000, lost: 0, fate: '' })),
  defenders: def.map((c) => ({ name: c.name, nat: c.nat, kind: c.kind, divs: c.divs, men: 10000, lost: 0, fate: '' })),
  lines: [], aPower: 1, dPower: 1, odds: 1, level: 'marginal' as BattleReport['level'], taken: false, attLost: 0, defLost: 0, prisoners: 0, ...over,
}) as BattleReport;
const field = (g: Game, nat: string): Corps[] => g.corps.filter((c) => !c.dead && c.nat === nat && c.kind !== 'fort').sort((a, b) => b.divs - a.divs || a.id - b.id);
const frenchTown = (g: Game): number => g.regions.find((r) => r.ctrl === 'FRA')!.id;
const news = (g: Game, from: number): string[] => g.headlines.slice(from).map((h) => h.text);

describe('the marked army', () => {
  it('is off unless set: nothing is marked, and its switch turns it on and off in any month', () => {
    const g = open('FRA');
    expect(markedOn(g)).toBe(false);
    const [ger] = field(g, 'GER'), [fra] = field(g, 'FRA');
    const place = frenchTown(g);
    markBattle(g, report(g, place, [ger], [fra], { taken: true }), g.regions[place], [ger], [fra]);
    expect(ger.mark).toBeUndefined();
    setMarked(g, true);
    expect(markedOn(g)).toBe(true);
    setMarked(g, false);
    expect(g.flags.marked).toBeUndefined();
  });

  it('marks the corps that led the taking of his town, once, with a line of news; a siege marks nobody', () => {
    const g = open('FRA');
    setMarked(g, true);
    const [big, small] = field(g, 'GER'), [fra] = field(g, 'FRA');
    const place = frenchTown(g);
    const at = g.headlines.length;
    markBattle(g, report(g, place, [small], [fra], { taken: true, siege: true }), g.regions[place], [small], [fra]);
    expect(marksOf(g)).toEqual([]);
    markBattle(g, report(g, place, [small, big], [fra], { taken: true }), g.regions[place], [small, big], [fra]);
    expect(big.mark).toEqual({ place, turn: g.turn });
    expect(small.mark).toBeUndefined();
    expect(news(g, at)).toEqual([`The German ${big.name} took ${g.regions[place].name}. Your staff has marked it in red on the map until you beat it.`]);
    // (taking a second town does not mark it again, nor move its mark)
    const other = g.regions.filter((r) => r.ctrl === 'FRA')[1].id;
    markBattle(g, report(g, other, [big], [fra], { taken: true }), g.regions[other], [big], [fra]);
    expect(big.mark!.place).toBe(place);
    // (his own attacks mark nobody)
    const [ger2] = field(g, 'GER').filter((c) => !c.mark);
    markBattle(g, report(g, ger2.reg, [fra], [ger2], { taken: true, attNat: 'FRA', defNat: 'GER' }), g.regions[ger2.reg], [fra], [ger2]);
    expect(fra.mark).toBeUndefined();
  });

  it(`marks no more than ${MARKS_MOST} at once`, () => {
    const g = open('FRA');
    setMarked(g, true);
    const germans = field(g, 'GER').slice(0, MARKS_MOST + 2), [fra] = field(g, 'FRA');
    const towns = g.regions.filter((r) => r.ctrl === 'FRA').slice(0, germans.length);
    germans.forEach((c, i) => markBattle(g, report(g, towns[i].id, [c], [fra], { taken: true }), towns[i], [c], [fra]));
    expect(marksOf(g).length).toBe(MARKS_MOST);
  });

  it('comes off when he beats it, attacking or holding, and stays when only his allies do', () => {
    const g = open('FRA');
    setMarked(g, true);
    const [a, b] = field(g, 'GER'), [fra] = field(g, 'FRA'), [uk] = field(g, 'UK');
    a.mark = { place: frenchTown(g), turn: g.turn };
    b.mark = { place: frenchTown(g), turn: g.turn };
    const at = g.headlines.length;
    // its attack on him is thrown back
    markBattle(g, report(g, fra.reg, [a], [fra], { taken: false }), g.regions[fra.reg], [a], [fra]);
    expect(a.mark).toBeUndefined();
    expect(news(g, at)[0]).toMatch(new RegExp(`^You have beaten the German ${a.name}, the army that took ${g.regions[frenchTown(g)].name}, at (the )?${g.regions[fra.reg].name}\\. Its red mark comes off the map\\.$`));
    // the British alone throw it back: it keeps its mark
    markBattle(g, report(g, uk.reg, [b], [uk], { taken: false }), g.regions[uk.reg], [b], [uk]);
    expect(b.mark).toBeDefined();
    // he drives it off the field
    markBattle(g, report(g, b.reg, [fra], [b], { taken: true, attNat: 'FRA', defNat: 'GER' }), g.regions[b.reg], [fra], [b]);
    expect(b.mark).toBeUndefined();
  });

  it('tells its end, by his troops or anyone\'s, and a peace takes the mark off without a word', () => {
    const g = open('FRA');
    setMarked(g, true);
    const [a, b, c] = field(g, 'GER');
    for (const x of [a, b, c]) x.mark = { place: frenchTown(g), turn: g.turn };
    const at = g.headlines.length;
    markFallen(g, a, 'FRA', a.reg);
    markFallen(g, b, 'UK', -1);
    expect(a.mark).toBeUndefined();
    expect(b.mark).toBeUndefined();
    const told = news(g, at);
    expect(told.length).toBe(2);
    // (a place's name is given its article by the news, as every line's is: "at the Saar")
    expect(told[0]).toMatch(new RegExp(`^The German ${a.name}, the army that took ${g.regions[frenchTown(g)].name}, has been destroyed by your troops at (the )?${g.regions[a.reg].name}\\. Its red mark comes off the map\\.$`));
    expect(told[1]).toBe(`The German ${b.name}, the army that took ${g.regions[frenchTown(g)].name}, is no more. Its red mark comes off the map.`);
    // still at war: kept
    marksMonth(g);
    expect(c.mark).toBeDefined();
    // at peace: gone, quietly
    const said = g.headlines.length;
    g.nations.FRA.atWar = g.nations.FRA.atWar.filter((n) => n !== 'GER');
    g.nations.GER.atWar = g.nations.GER.atWar.filter((n) => n !== 'FRA');
    marksMonth(g);
    expect(c.mark).toBeUndefined();
    expect(g.headlines.length).toBe(said);
  });

  it('is kept by a save', () => {
    const g = open('FRA');
    setMarked(g, true);
    const [a] = field(g, 'GER');
    a.mark = { place: frenchTown(g), turn: g.turn };
    const back = loadGame(saveGame(g))!;
    expect(back.flags.marked).toBe(1);
    expect(back.corps.find((x) => x.id === a.id)!.mark).toEqual(a.mark);
  });
});

describe('the marked army in a war played through the 3D bridge', () => {
  it('in the spring of 1940 played as France: the Germans who take French towns are marked and seen marked, and nothing else of the war changes', () => {
    const play = (on: boolean): { save: string; marks: number; seen: string[] } => {
      TC.start({ view: 'grand', human: 'FRA', seed: 1939, from: 5 });
      if (on) expect(TC.act('marked:on').view.markedOn).toBe(true);
      const seen: string[] = [];
      let marks = 0;
      for (let m = 0; m < 6; m++) {
        const v = TC.endMonth().view;
        for (const c of v.corps) {
          if (c.mark !== undefined) {
            expect(c.nat).not.toBe('FRA');
            expect(c.dim).toBe(false);
            seen.push(c.mark);
          }
        }
      }
      const back = loadGame((globalThis as unknown as { TC: { save: () => { text: string } } }).TC.save().text)!;
      marks = back.corps.filter((c) => c.mark).length;
      // (the war without its marks and its news: compared between the two runs)
      for (const c of back.corps) delete c.mark;
      delete back.flags.marked;
      back.headlines = [];
      return { save: saveGame(back), marks, seen };
    };
    const off = play(false);
    const on = play(true);
    expect(off.marks).toBe(0);
    expect(off.seen).toEqual([]);
    expect(on.marks).toBeGreaterThan(0);
    expect(on.marks).toBeLessThanOrEqual(MARKS_MOST);
    expect(on.seen.length).toBeGreaterThan(0);
    expect(on.save).toBe(off.save);
  }, 240000);
});
