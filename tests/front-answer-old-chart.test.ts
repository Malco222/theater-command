// Held armies answer an attack by land on the old chart, under Units decide, at the stops that keep no book (the
// design director's ruling of 14:15Z of 25 November 2026, audit item 38, design-director/pacific-japan-1125.md, "B11
// on the old chart: the size and the form ruled"; the form, pacific-japan/dd-form-held-rows-old-chart-1125.md).
//
// "B11's one rule on both charts, behind Units decide, stops 3 to 5; off byte-identical. Book-held rows answer as on
// the far side (their front's ground and no other, never beyond their side's own ground, not sailed home while
// answering, engineers left out). Fortress troops are held by kind and unchanged." The measure behind it: in 24 wars
// nobody played at stop 3, a median 13 of France's 18 held rows stood on or beside French ground a German army held,
// and 31 of their 35 row-months a war were followed by the loss of ground they could have reached.
//
// The far side's own tests are far-front-answer's. Here: France 1940 on the old chart, the far side off. At the
// book's stops (1 and 2) the book's release is the record's hand and nothing answers (14:15Z, (a)); with Units decide
// off nothing answers on the old chart at all. The Metz and Lauter fortified regions are fortress troops, held by
// their kind (the historian's read, 14:19Z). Never the player's corps. A landing is b641's, not this rule's.
import { describe, expect, it } from 'vitest';
import { answerWatch, frontAnswering, homeBars, withAnswer } from '../src/campaign/ai';
import { addCorps, allied, atWar, createGame, relocate } from '../src/campaign/game';
import { heldFirst } from '../src/campaign/far';
import { setStop, stopDef } from '../src/campaign/history';
import { landCorps } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import { current, stepAI } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';
import { setUnitsDecide, unitsDecide } from '../src/campaign/units';
import { farOn } from '../src/campaign/world';

const names = (cs: Corps[]): string[] => cs.map((c) => c.name).sort();
const called = (g: Game, name: string): Corps => g.corps.find((c) => c.name === name && !c.dead)!;
/** A war nobody plays, opened in September 1939 at the stop named, with Units decide on or off. */
const war = (units: boolean, stop = 3, human: string | null = null): Game => {
  const g = startOpen(createGame(1, human));
  expect(setUnitsDecide(g, units)).toBe(true);
  if (stop !== 3) expect(setStop(g, stop, true)).toBe(true);
  expect([farOn(g), unitsDecide(g), !!stopDef(g).book]).toEqual([false, units, stop <= 2]);
  return g;
};
/** Plays the months of the computer's nations up to the one named, and stops before it. */
const upTo = (g: Game, nat: string): void => { for (let k = 0; current(g) !== nat && k < 200; k++) stepAI(g); expect(current(g)).toBe(nat); };
/** Ground taken this month by the nation named, its own men cleared out of it. */
const take = (g: Game, r: Region, by: string): void => {
  for (const u of [...r.units]) if (u.nat !== by) relocate(g, u, g.byName['Paris']);
  r.prev = r.ctrl; r.ctrl = by; r.since = g.turn;
};
/** A German army in Champagne, beside the 2nd Army at Sedan, the XVIII Corps at Verdun and the Paris garrison. */
const champagne = (g: Game): Region => {
  const r = g.byName['Champagne'];
  expect(['Sedan', 'Verdun', 'Paris'].every((nm) => r.nb.includes(g.byName[nm].id))).toBe(true);
  take(g, r, 'GER');
  addCorps(g, 'GER', 'Test army in Champagne', 'inf', 4, r.id, {});
  return r;
};
/** Ground of France's side: a country of its side's (France's own, an ally's), or held by its side on the first day. */
const ofSide = (g: Game, r: Region): boolean => allied(g, 'FRA', r.owner) || allied(g, 'FRA', heldFirst(r));

describe('held armies answer an attack by land on the old chart, under Units decide (audit item 38)', () => {
  it('stop 3, Units decide on: a French held army answers the month a German army holds French ground beside it, on French ground only', () => {
    const g = war(true);
    expect(atWar(g, 'GER', 'FRA')).toBe(true);
    upTo(g, 'FRA');
    const second = called(g, '2nd Army'), paris = called(g, 'Paris garrison');
    // Sedan faces Luxembourg and the Ardennes, neutral ground: before Champagne falls its army is on no front.
    expect([second.hold, paris.hold]).toEqual([true, true]);
    expect(names(frontAnswering(g, 'FRA'))).not.toContain(second.name);
    expect(names(frontAnswering(g, 'FRA'))).not.toContain(paris.name);
    const lost = champagne(g);
    const rows = frontAnswering(g, 'FRA');
    expect(names(rows)).toEqual(expect.arrayContaining([second.name, paris.name, 'XVIII Corps']));

    // France's month, read as the staff reads it: who answers, and on what ground.
    const seen: { corps: Corps; on: Set<number> }[] = [];
    answerWatch.tell = (e) => { if (e.nat === 'FRA') seen.push(...e.rows); };
    try { stepAI(g); } finally { answerWatch.tell = null; }
    // (The staff opens its own offensive on Champagne the same month, and that offensive's release takes the hold off
    // the rows on the front it opens, for good, as it always did: those are no longer held when the answer is read.
    // The rows that answer are the held rows that remain: the 4th Army beside the lost ground and the Paris garrison
    // next behind it.)
    expect(seen.length).toBeGreaterThan(0);
    expect(names(seen.map((x) => x.corps))).toEqual(expect.arrayContaining(['4th Army', paris.name]));
    for (const { corps: c } of seen) expect(rows, c.name).toContain(c);
    for (const { corps: c, on } of seen) {
      // Its ground: its side's own, never the Saar or Belgium. (On the old chart the front against Germany is one
      // front, as it always was, so the Polish ground Germany has taken on it, Warsaw's, is its side's ground to win
      // back too: it is no further from France by land for that.)
      expect(on.size, c.name).toBeGreaterThan(0);
      for (const id of on) expect([c.name, g.regions[id].name, ofSide(g, g.regions[id])]).toEqual([c.name, g.regions[id].name, true]);
      expect(['Saar', 'Baden', 'Namur', 'Ardennes', 'Luxembourg'].map((nm) => on.has(g.byName[nm].id)), c.name).toEqual([false, false, false, false, false]);
      // Champagne, French ground the enemy took beside it, is the front's to win back.
      if (['4th Army', paris.name].includes(c.name)) expect(on.has(lost.id), c.name).toBe(true);
      if (c.dead) continue;
      // After the month the hold is back on each answering row that lives,
      expect(c.hold, c.name).toBe(true);
      // and the staff sent none of them beyond its ground: it stands on it still.
      expect(on.has(c.reg), `${c.name} at ${g.regions[c.reg].name}`).toBe(true);
    }
  }, 300000);

  it('the tie holds an answering row to its ground against its own staff\'s moves: Germany\'s first month, the Vienna garrison is not sent to Lodz', () => {
    // (The skeptic's finding of 25 November 2026: the month above never shows the tie stopping a move, since nothing
    // tries to move those French rows off their ground. In Germany's first month the staff would send the Vienna
    // garrison, a held row answering on the front with Poland, to Lodz in Poland's own ground, beyond its side's.)
    const g = war(true);
    upTo(g, 'GER');
    const vienna = called(g, 'Vienna garrison');
    const seen: { corps: Corps; on: Set<number> }[] = [];
    answerWatch.tell = (e) => { if (e.nat === 'GER') seen.push(...e.rows); };
    try { stepAI(g); } finally { answerWatch.tell = null; }
    expect(seen.map((x) => x.corps)).toContain(vienna);
    expect(seen.find((x) => x.corps === vienna)!.on.has(g.byName['Lodz'].id)).toBe(false);
    for (const { corps: c, on } of seen) {
      if (c.dead) continue;
      expect([c.name, g.regions[c.reg].name, on.has(c.reg), c.hold]).toEqual([c.name, g.regions[c.reg].name, true, true]);
    }
  }, 300000);

  it('the Metz and Lauter fortified regions do not answer; engineers do not; a row marked to stay does not', () => {
    const g = war(true);
    champagne(g);
    const rows = names(frontAnswering(g, 'FRA'));
    expect(rows).toEqual(expect.arrayContaining(['3rd Army', '5th Army', '4th Army', '8th Army', 'XVIII Corps', '2nd Army']));
    // Fortress troops, held by their kind (the historian's read of 14:19Z): the field armies beside them answer.
    for (const nm of ['Metz fortified region', 'Lauter fortified region', 'Montmedy sector']) {
      const c = called(g, nm);
      expect([nm, c.kind, c.hold, rows.includes(nm)]).toEqual([nm, 'fort', true, false]);
    }
    const plain = addCorps(g, 'FRA', 'Test held division in Lorraine', 'inf', 1, 'Lorraine', { hold: 1 });
    addCorps(g, 'FRA', 'Test engineers in Lorraine', 'eng', 1, 'Lorraine', { hold: 1 });
    addCorps(g, 'FRA', 'Test garrison marked to stay', 'inf', 1, 'Lorraine', { hold: 1, stay: 1 });
    addCorps(g, 'FRA', 'Test garrison kept inside its town', 'inf', 1, 'Lorraine', { hold: 1 }).fortress = true;
    addCorps(g, 'FRA', 'Test division lent to the player', 'inf', 1, 'Lorraine', { hold: 1 }).joint = 'UK';
    const now = names(frontAnswering(g, 'FRA'));
    expect(now).toEqual([...rows, plain.name].sort());
  });

  it('the same position with Units decide off: nobody answers on the old chart', () => {
    const g = war(false);
    upTo(g, 'FRA');
    champagne(g);
    for (const nat of ['FRA', 'GER', 'UK', 'POL']) expect([nat, frontAnswering(g, nat)]).toEqual([nat, []]);
    const seen: string[] = [];
    answerWatch.tell = (e) => { seen.push(e.nat); };
    try { stepAI(g); } finally { answerWatch.tell = null; }
    expect(seen).toEqual([]);
    // The rows that answer under Units decide stand held where they stood.
    for (const [nm, at] of [['Paris garrison', 'Paris'], ['4th Army', 'Vosges'], ['8th Army', 'Belfort']]) {
      const c = called(g, nm);
      expect([nm, c.hold, g.regions[c.reg].name]).toEqual([nm, true, at]);
    }
  }, 300000);

  for (const stop of [1, 2]) {
    it(`the same position at stop ${stop}, the book's, with Units decide on: nobody answers on the old chart`, () => {
      const g = war(true, stop);
      champagne(g);
      for (const nat of ['FRA', 'GER', 'UK', 'POL']) expect([nat, frontAnswering(g, nat)]).toEqual([nat, []]);
      // (At stop 3 the same position frees France's armies: the stop is what holds them, not the switch.)
      expect(setStop(g, 3)).toBe(true);
      expect(names(frontAnswering(g, 'FRA'))).toContain('2nd Army');
    });
  }

  it('the player\'s own held corps never answer; the computer\'s across the line do', () => {
    const g = war(true, 3, 'FRA');
    champagne(g);
    expect(frontAnswering(g, 'FRA')).toEqual([]);
    expect(called(g, '2nd Army').hold).toBe(true);
    // One rule for every nation: Germany's held rows on the same front answer under its computer staff.
    const germany = frontAnswering(g, 'GER');
    expect(germany.length).toBeGreaterThan(0);
    expect(germany.every((c) => c.nat === 'GER' && c.hold && !c.stay && c.kind !== 'fort')).toBe(true);
  });

  it('an enemy who has only come ashore frees nobody: a German beach on the coast beside the Bordeaux garrison', () => {
    const g = war(true);
    const bordeaux = called(g, 'Bordeaux garrison');
    const gascony = g.regions[bordeaux.reg].nb.map((j) => g.regions[j]).find((r) => r.owner === 'FRA' && r.port)!;
    expect([g.regions[bordeaux.reg].name, !!gascony]).toEqual(['Bordeaux', true]);
    expect(names(frontAnswering(g, 'FRA'))).not.toContain(bordeaux.name);
    // A German army ashore there, the shore won from the sea this month (the mark navy.ts leaves as a landing
    // wins its shore, laid here by hand: with the far side off a landing leaves none, below).
    take(g, gascony, 'GER');
    addCorps(g, 'GER', 'Test army ashore', 'inf', 3, gascony.id, {});
    g.flags[`shoreWon:${gascony.id}`] = gascony.since;
    expect(names(frontAnswering(g, 'FRA'))).not.toContain(bordeaux.name);
    // Read as entered by land, the same ground frees the garrison next to it.
    delete g.flags[`shoreWon:${gascony.id}`];
    expect(names(frontAnswering(g, 'FRA'))).toContain(bordeaux.name);
    const on = withAnswer(g, 'FRA', () => g.regions.filter((r) => !homeBars(bordeaux, r)).map((r) => r.name));
    expect(on).toEqual(expect.arrayContaining(['Bordeaux', gascony.name]));
    expect(on.every((nm) => ofSide(g, g.byName[nm]))).toBe(true);
  });

  it('with the far side off a landing on the old chart leaves no mark of the shore it won (flagged: such a beach reads as entered by land)', () => {
    // (navy.ts `landCorps` writes the mark only with the far switch on. Reported to the director, not mended here:
    // the ruling names that mark and no second one.)
    const g = war(true);
    const shore = g.byName['Danzig Corridor'];
    expect([shore.port, atWar(g, 'GER', shore.ctrl)]).toEqual([true, true]);
    for (const u of [...shore.units]) relocate(g, u, g.byName['Warsaw']);
    const party = addCorps(g, 'GER', 'Test landing party', 'inf', 1, 'Hamburg', {});
    const res = landCorps(g, [party], shore);
    expect([res.ok, res.taken]).toEqual([true, true]);
    expect(g.flags[`shoreWon:${shore.id}`]).toBeUndefined();
  });
});
