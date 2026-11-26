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
// What frees a row on the old chart is the form's cause (the design director's ruling of 15:09Z of 25 November 2026,
// "Held rows on the old chart: four readings before the gate", point 1): an enemy at war with its country holds ground
// of its side on the row's own province or beside it. Not the war alone, as the far side reads it: the old chart's
// fronts are joined, and as first built France's 3rd, 4th, 5th and 8th Armies and Germany's garrisons answered in the
// war's first month with no enemy near. A beach is no such ground, and with the far side off a landing and a drop on
// the old chart now leave the mark of the shore they won, under Units decide at the stops that keep no book (point 4).
//
// The far side's own tests are far-front-answer's. Here: France 1940 on the old chart, the far side off. At the
// book's stops (1 and 2) the book's release is the record's hand and nothing answers (14:15Z, (a)); with Units decide
// off nothing answers on the old chart at all. The Metz and Lauter fortified regions are fortress troops, held by
// their kind (the historian's read, 14:19Z). Never the player's corps. A landing is b641's, not this rule's; b641
// reads only the far side today (ai.ts `answering`), so on the old chart nobody answers a beach (reported, not widened).
import { describe, expect, it } from 'vitest';
import { answerWatch, answering, frontAnswering, homeBars, withAnswer } from '../src/campaign/ai';
import { addCorps, allied, atWar, createGame, declareWar, relocate } from '../src/campaign/game';
import { heldFirst } from '../src/campaign/far';
import { setStop, stopDef } from '../src/campaign/history';
import { landCorps } from '../src/campaign/navy';
import { fieldCtrl } from '../src/campaign/siege';
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
const take = (g: Game, r: Region, by: string, to = 'Paris'): void => {
  for (const u of [...r.units]) if (u.nat !== by) relocate(g, u, g.byName[to]);
  r.prev = r.ctrl; r.ctrl = by; r.since = g.turn;
};
/** A German army in French ground, its French corps sent to Paris (by default Champagne, beside the 2nd Army at Sedan, the XVIII Corps at Verdun and the Paris garrison). */
const lose = (g: Game, nm: string): Region => {
  const r = g.byName[nm];
  take(g, r, 'GER');
  addCorps(g, 'GER', `Test army in ${nm}`, 'inf', 4, r.id, {});
  return r;
};
const champagne = (g: Game): Region => {
  expect(['Sedan', 'Verdun', 'Paris'].every((nm) => g.byName['Champagne'].nb.includes(g.byName[nm].id))).toBe(true);
  return lose(g, 'Champagne');
};
/** Every nation's held rows that answer an attack by land now, by nation. */
const everyone = (g: Game): string[] => Object.keys(g.nations).flatMap((nat) => frontAnswering(g, nat).map((c) => `${nat} ${c.name}`));
/** True where an enemy at war with the row's country holds (the field of) ground of its side on its province or beside it. */
const entered = (g: Game, c: Corps): boolean => [c.reg, ...g.regions[c.reg].nb].some((id) => {
  const r = g.regions[id];
  return (atWar(g, c.nat, r.ctrl) || r.units.some((u) => !u.dead && atWar(g, c.nat, u.nat))) && (allied(g, c.nat, r.owner) || allied(g, c.nat, heldFirst(r)));
});
/** Ground of France's side: a country of its side's (France's own, an ally's), or held by its side on the first day. */
const ofSide = (g: Game, r: Region): boolean => allied(g, 'FRA', r.owner) || allied(g, 'FRA', heldFirst(r));

describe('held armies answer an attack by land on the old chart, under Units decide (audit item 38)', () => {
  it('stop 3, Units decide on: in the war\'s first month nobody\'s ground of its side is entered, and nobody answers on the old chart', () => {
    // (As first built the war itself freed every held row on a front: France's 3rd, 4th, 5th and 8th Armies and the
    // XVIII Corps, Germany's Konigsberg, Berlin, Vienna and Protectorate garrisons, Poland's four, Slovakia's one.)
    const g = war(true);
    expect([atWar(g, 'GER', 'FRA'), atWar(g, 'GER', 'POL')]).toEqual([true, true]);
    const held = g.corps.filter((c) => !c.dead && c.hold && c.kind !== 'fort' && ['FRA', 'GER', 'POL'].includes(c.nat));
    expect(held.length).toBeGreaterThan(10);
    expect(held.filter((c) => entered(g, c))).toEqual([]);
    expect(everyone(g)).toEqual([]);
    // Germany's month and Poland's: Germany marches into Poland, but when Poland's month opens no held Polish row
    // stands on or beside Polish ground he holds, so nobody answers, Poland included; Germany's own ground is entered
    // by nobody, and France's neither. (A row that did answer would have to be entered: checked as each month opens.)
    const seen: { nat: string; corps: Corps; entered: boolean }[] = [];
    answerWatch.tell = (e) => { for (const x of e.rows) seen.push({ nat: e.nat, corps: x.corps, entered: entered(g, x.corps) }); };
    try { upTo(g, 'FRA'); } finally { answerWatch.tell = null; }
    expect(seen.filter((x) => !x.entered)).toEqual([]);
    expect(seen).toEqual([]);
    expect(everyone(g).filter((k) => !k.startsWith('POL '))).toEqual([]);
  }, 300000);

  it('stop 3, Units decide on: a German army in Champagne frees the held rows on or beside it, on French ground only; not one two provinces back, nor one beside German ground only', () => {
    const g = war(true);
    upTo(g, 'FRA');
    expect(frontAnswering(g, 'FRA')).toEqual([]);
    const lost = champagne(g);
    const rows = frontAnswering(g, 'FRA');
    // The 2nd Army at Sedan and the XVIII Corps at Verdun beside it, and the Paris garrison next behind (rear rows
    // keep one province: the cause, beside it).
    expect(names(rows)).toEqual(['2nd Army', 'Paris garrison', 'XVIII Corps']);
    for (const c of rows) expect([c.name, g.regions[c.reg].nb.includes(lost.id)]).toEqual([c.name, true]);
    // The 4th Army at Vosges is two provinces from Champagne; the 3rd at Lorraine stands beside the Saar, German
    // ground the enemy holds as he always did, and no ground of France's side is lost beside it.
    const vosges = called(g, '4th Army'), third = called(g, '3rd Army');
    expect([g.regions[vosges.reg].name, g.regions[vosges.reg].nb.includes(lost.id), vosges.hold]).toEqual(['Vosges', false, true]);
    expect([g.regions[third.reg].name, g.regions[third.reg].nb.includes(g.byName['Saar'].id), third.hold]).toEqual(['Lorraine', true, true]);
    expect(entered(g, third)).toBe(false);
    expect(names(rows)).not.toContain(vosges.name);
    expect(names(rows)).not.toContain(third.name);

    // France's month, read as the staff reads it: who answers, and on what ground.
    const seen: { corps: Corps; on: Set<number> }[] = [];
    answerWatch.tell = (e) => { if (e.nat === 'FRA') seen.push(...e.rows); };
    try { stepAI(g); } finally { answerWatch.tell = null; }
    // (The staff opens its own offensive on Champagne the same month, and that offensive's release takes the hold off
    // the rows on the front it opens, for good, as it always did: those are no longer held when the answer is read.
    // The row that answers is the held row that remains: the Paris garrison next behind the lost ground.)
    expect(seen.length).toBeGreaterThan(0);
    expect(names(seen.map((x) => x.corps))).toContain('Paris garrison');
    for (const { corps: c } of seen) expect(rows, c.name).toContain(c);
    for (const { corps: c, on } of seen) {
      // Its ground: its side's own, never the Saar or Belgium.
      expect(on.size, c.name).toBeGreaterThan(0);
      for (const id of on) expect([c.name, g.regions[id].name, ofSide(g, g.regions[id])]).toEqual([c.name, g.regions[id].name, true]);
      expect(['Saar', 'Baden', 'Namur', 'Ardennes', 'Luxembourg'].map((nm) => on.has(g.byName[nm].id)), c.name).toEqual([false, false, false, false, false]);
      // Champagne, French ground the enemy took beside it, is the front's to win back.
      expect(on.has(lost.id), c.name).toBe(true);
      if (c.dead) continue;
      // After the month the hold is back on each answering row that lives,
      expect(c.hold, c.name).toBe(true);
      // and the staff sent none of them beyond its ground: it stands on it still.
      expect(on.has(c.reg), `${c.name} at ${g.regions[c.reg].name}`).toBe(true);
    }
  }, 300000);

  it('the cause is read each month: Champagne given back, the next month the Paris garrison does not answer and its hold stands', () => {
    const g = war(true);
    upTo(g, 'FRA');
    const lost = champagne(g);
    const paris = called(g, 'Paris garrison');
    const first: Corps[] = [];
    answerWatch.tell = (e) => { if (e.nat === 'FRA') first.push(...e.rows.map((x) => x.corps)); };
    try { stepAI(g); } finally { answerWatch.tell = null; }
    expect(first).toContain(paris);
    // The enemy driven off it, by hand: France holds Champagne again and no German stands there.
    for (const u of [...lost.units]) if (u.nat !== 'FRA') relocate(g, u, g.byName['Saar']);
    lost.prev = lost.ctrl; lost.ctrl = 'FRA'; lost.since = g.turn;
    expect(frontAnswering(g, 'FRA')).not.toContain(paris);
    const turn = g.turn;
    const next: Corps[] = [], idle: string[] = [];
    answerWatch.tell = (e) => {
      if (e.nat === 'FRA') next.push(...e.rows.map((x) => x.corps));
      // (Every nation's month, read as it opens: a row that answers with no enemy on ground of its side on or beside it.)
      idle.push(...e.rows.filter((x) => !entered(g, x.corps)).map((x) => `${x.corps.name} at ${g.regions[x.corps.reg].name}`));
    };
    try { upTo(g, 'FRA'); expect(g.turn).toBe(turn + 1); stepAI(g); } finally { answerWatch.tell = null; }
    expect(entered(g, paris)).toBe(false);
    expect(next).not.toContain(paris);
    expect(idle).toEqual([]);
    expect([paris.hold, g.regions[paris.reg].name]).toEqual([true, 'Paris']);
  }, 300000);

  it('the tie holds an answering row to its ground against its own staff\'s moves: Poland holds Lower Silesia, and the Protectorate garrison is not sent to Lodz', () => {
    // (In Germany's first month, with Lower Silesia, German ground beside Bohemia, in Polish hands and no army of
    // either side on it, the Protectorate garrison answers, and with the movers' tie checks dropped (`moveCorps`,
    // `blocked`, `railCorps`: offHome) the staff sends it on to Lodz in Poland's own ground, beyond its side's.)
    const g = war(true);
    upTo(g, 'GER');
    const prot = called(g, 'Protectorate garrison');
    expect(frontAnswering(g, 'GER')).toEqual([]);
    const silesia = g.byName['Lower Silesia'];
    expect([silesia.owner, g.regions[prot.reg].name, silesia.nb.includes(prot.reg)]).toEqual(['GER', 'Bohemia', true]);
    take(g, silesia, 'POL', 'Berlin');
    expect(names(frontAnswering(g, 'GER'))).toContain(prot.name);
    const seen: { corps: Corps; on: Set<number> }[] = [];
    answerWatch.tell = (e) => { if (e.nat === 'GER') seen.push(...e.rows); };
    try { stepAI(g); } finally { answerWatch.tell = null; }
    expect(seen.map((x) => x.corps)).toContain(prot);
    expect(seen.find((x) => x.corps === prot)!.on.has(g.byName['Lodz'].id)).toBe(false);
    for (const { corps: c, on } of seen) {
      if (c.dead) continue;
      expect([c.name, g.regions[c.reg].name, on.has(c.reg), c.hold]).toEqual([c.name, g.regions[c.reg].name, true, true]);
    }
  }, 300000);

  it('the Metz and Lauter fortified regions do not answer; engineers do not; a row marked to stay, kept in its town or lent to the player does not', () => {
    const g = war(true);
    // A German army in the Vosges, beside Lorraine, Alsace, Verdun and Belfort.
    lose(g, 'Vosges');
    const rows = names(frontAnswering(g, 'FRA'));
    expect(rows).toEqual(['3rd Army', '5th Army', '8th Army', 'XVIII Corps']);
    // Fortress troops, held by their kind (the historian's read of 14:19Z): the field armies beside them answer.
    for (const nm of ['Metz fortified region', 'Lauter fortified region', 'Montmedy sector']) {
      const c = called(g, nm);
      expect([nm, c.kind, c.hold, entered(g, c), rows.includes(nm)]).toEqual([nm, 'fort', true, true, false]);
    }
    const plain = addCorps(g, 'FRA', 'Test held division in Lorraine', 'inf', 1, 'Lorraine', { hold: 1 });
    addCorps(g, 'FRA', 'Test engineers in Lorraine', 'eng', 1, 'Lorraine', { hold: 1 });
    addCorps(g, 'FRA', 'Test garrison marked to stay', 'inf', 1, 'Lorraine', { hold: 1, stay: 1 });
    addCorps(g, 'FRA', 'Test garrison kept inside its town', 'inf', 1, 'Lorraine', { hold: 1 }).fortress = true;
    addCorps(g, 'FRA', 'Test division lent to the player', 'inf', 1, 'Lorraine', { hold: 1 }).joint = 'UK';
    const now = names(frontAnswering(g, 'FRA'));
    expect(now).toEqual([...rows, plain.name].sort());
  });

  it('a town of France\'s the enemy holds is his though a French army stands round it: the 4th Army in the Vosges answers, on its own province', () => {
    // (The cause is true wherever the form's count of the 6,108 row-months was, save a beach: that count read who
    // holds the town. As first built the cause read only who holds the field, and here nobody answered.)
    const g = war(true);
    const vosges = g.byName['Vosges'], fourth = called(g, '4th Army');
    expect(fourth.reg).toBe(vosges.id);
    vosges.prev = vosges.ctrl; vosges.ctrl = 'GER'; vosges.since = g.turn;
    expect([fieldCtrl(g, vosges), vosges.units.map((u) => u.name)]).toEqual(['FRA', ['4th Army']]);
    // Nothing beside the Vosges is the enemy's: the 4th Army's cause is its own province alone.
    expect(vosges.nb.map((j) => g.regions[j]).filter((r) => atWar(g, 'FRA', r.ctrl) || atWar(g, 'FRA', fieldCtrl(g, r)))).toEqual([]);
    const rows = names(frontAnswering(g, 'FRA'));
    expect(rows).toEqual(['3rd Army', '4th Army', '5th Army', '8th Army', 'XVIII Corps']);
  });

  it('ground its side held on the first day is ground of its side: Karelia ceded to the Soviet Union, its town his and Finland\'s army round it, frees Finland\'s rows', () => {
    const g = war(true);
    const karelia = g.byName['Karelia'];
    karelia.owner = 'SOV';
    expect([heldFirst(karelia), allied(g, 'FIN', karelia.owner)]).toEqual(['FIN', false]);
    expect(frontAnswering(g, 'FIN')).toEqual([]);
    declareWar(g, 'SOV', 'FIN');
    // The war alone frees nobody on the old chart.
    expect(frontAnswering(g, 'FIN')).toEqual([]);
    karelia.prev = karelia.ctrl; karelia.ctrl = 'SOV'; karelia.since = g.turn;
    expect([fieldCtrl(g, karelia), karelia.units.map((u) => u.name)]).toEqual(['FIN', ['Army of the Isthmus']]);
    const rows = frontAnswering(g, 'FIN');
    expect(names(rows)).toEqual(['Army of the Isthmus', 'Helsinki reserve']);
    for (const c of rows) expect([c.name, c.reg === karelia.id || g.regions[c.reg].nb.includes(karelia.id)]).toEqual([c.name, true]);
  });

  it('an enemy at war with its country, not a neutral: a Soviet corps in German Lower Silesia frees nobody; a Polish one frees the Protectorate garrison', () => {
    const g = war(true);
    const silesia = g.byName['Lower Silesia'], prot = called(g, 'Protectorate garrison');
    take(g, silesia, 'SOV', 'Berlin');
    addCorps(g, 'SOV', 'Test Soviet corps in Lower Silesia', 'inf', 2, silesia.id, {});
    expect([atWar(g, 'GER', 'SOV'), allied(g, 'GER', 'SOV'), fieldCtrl(g, silesia)]).toEqual([false, false, 'SOV']);
    expect(frontAnswering(g, 'GER')).toEqual([]);
    for (const u of [...silesia.units]) relocate(g, u, g.byName['Moscow']);
    silesia.ctrl = 'POL';
    expect(names(frontAnswering(g, 'GER'))).toContain(prot.name);
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
      // (At stop 3 the same position frees France's rows beside it: the stop is what holds them, not the switch.)
      expect(setStop(g, 3)).toBe(true);
      expect(names(frontAnswering(g, 'FRA'))).toContain('2nd Army');
    });
  }

  it('the player\'s own held corps never answer; the computer\'s across the line do, beside the ground the player took', () => {
    const g = war(true, 3, 'FRA');
    champagne(g);
    expect(frontAnswering(g, 'FRA')).toEqual([]);
    expect(called(g, '2nd Army').hold).toBe(true);
    // One rule for every nation: a French army in the Saar frees Germany's held rows beside it.
    expect(frontAnswering(g, 'GER')).toEqual([]);
    const saar = g.byName['Saar'];
    take(g, saar, 'FRA', 'Berlin');
    addCorps(g, 'FRA', 'Test army in the Saar', 'inf', 4, saar.id, {});
    const germany = frontAnswering(g, 'GER');
    expect(germany.length).toBeGreaterThan(0);
    expect(germany.every((c) => c.nat === 'GER' && c.hold && !c.stay && c.kind !== 'fort' && g.regions[c.reg].nb.includes(saar.id))).toBe(true);
  });

  it('an enemy who has only come ashore frees nobody: a German beach on the coast beside the Bordeaux garrison', () => {
    const g = war(true);
    const bordeaux = called(g, 'Bordeaux garrison');
    const gascony = g.regions[bordeaux.reg].nb.map((j) => g.regions[j]).find((r) => r.owner === 'FRA' && r.port)!;
    expect([g.regions[bordeaux.reg].name, !!gascony]).toEqual(['Bordeaux', true]);
    expect(names(frontAnswering(g, 'FRA'))).not.toContain(bordeaux.name);
    // A German army ashore there, the shore won from the sea this month (the mark navy.ts leaves as a landing
    // wins its shore, laid here by hand; a real landing below).
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

  /** A German landing party at Hamburg, the Danzig Corridor emptied of its Poles, and a Polish held division beside it at Pomorze. */
  const corridor = (g: Game): { shore: Region; party: Corps; row: Corps } => {
    const shore = g.byName['Danzig Corridor'];
    expect([shore.port, atWar(g, 'GER', shore.ctrl), shore.nb.includes(g.byName['Pomorze'].id)]).toEqual([true, true, true]);
    for (const u of [...shore.units]) relocate(g, u, g.byName['Warsaw']);
    const row = addCorps(g, 'POL', 'Test held division at Pomorze', 'inf', 1, 'Pomorze', { hold: 1 });
    const party = addCorps(g, 'GER', 'Test landing party', 'inf', 1, 'Hamburg', {});
    return { shore, party, row };
  };

  it('with the far side off and Units decide on at stop 3, a landing that wins its shore marks it, and the held row beside the beach does not answer by land', () => {
    // (The design director's ruling of 15:09Z, 25 November 2026, point 4: "written under Units decide at stops 3 to
    // 5, so a beach reads as a landing (b641) ... never as an entry by land". As first built nothing was marked with
    // the far side off, and the row beside the beach answered as if the enemy had marched in.)
    const g = war(true);
    const { shore, party, row } = corridor(g);
    expect(frontAnswering(g, 'POL')).toEqual([]);
    const res = landCorps(g, [party], shore);
    // (The town holds, and the landing's army is laid round it: the field is his, ground of Poland's he holds.)
    expect([res.ok, res.taken, fieldCtrl(g, shore)]).toEqual([true, true, 'GER']);
    expect(g.flags[`shoreWon:${shore.id}`]).toBe(g.turn);
    expect(frontAnswering(g, 'POL')).not.toContain(row);
    // b641's landing answer reads the far side only (`answering`, with the far switch on): on the old chart it frees
    // nobody, so the row beside the beach stands held. (Reported to the design director; b641 is not widened here.)
    expect(answering(g, 'POL')).toEqual([]);
    // Without the mark the same ground reads as entered by land, and the row answers.
    delete g.flags[`shoreWon:${shore.id}`];
    expect(frontAnswering(g, 'POL')).toContain(row);
  });

  it('with Units decide off, or at stop 1 or 2 with it on, the same landing leaves no mark', () => {
    for (const [units, stop] of [[false, 3], [true, 1], [true, 2]] as const) {
      const g = war(units, stop);
      const { shore, party } = corridor(g);
      const res = landCorps(g, [party], shore);
      expect([units, stop, res.ok, res.taken]).toEqual([units, stop, true, true]);
      expect([units, stop, g.flags[`shoreWon:${shore.id}`]]).toEqual([units, stop, undefined]);
    }
  });

  it('a drop from the air on the old chart is marked the same way: German airborne troops from Brittany take Poitou beside the Bordeaux garrison', () => {
    for (const units of [true, false]) {
      const g = war(units);
      // Brittany in German hands by hand, its only neighbour Rennes, where no French row is held.
      const brittany = g.byName['Brittany'];
      expect(brittany.nb.map((j) => g.regions[j].name)).toEqual(['Rennes']);
      take(g, brittany, 'GER');
      const poitou = g.byName['Poitou'], bordeaux = called(g, 'Bordeaux garrison');
      expect(poitou.nb.includes(bordeaux.reg)).toBe(true);
      for (const u of [...poitou.units]) relocate(g, u, g.byName['Paris']);
      const para = addCorps(g, 'GER', 'Test airborne', 'inf', 1, brittany.id, { airborne: true });
      const res = landCorps(g, [para], poitou);
      expect([units, res.ok, res.air, res.taken, fieldCtrl(g, poitou)]).toEqual([units, true, true, true, 'GER']);
      expect([units, g.flags[`shoreWon:${poitou.id}`]]).toEqual([units, units ? g.turn : undefined]);
      expect(frontAnswering(g, 'FRA')).toEqual([]);
      if (!units) continue;
      // Read as entered by land, Poitou would free the garrison beside it.
      delete g.flags[`shoreWon:${poitou.id}`];
      expect(names(frontAnswering(g, 'FRA'))).toContain(bordeaux.name);
    }
  });
});
