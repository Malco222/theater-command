// Operation Torch by the book (strategy.json 'torch'; the design director, 22 November 2026; after release 1).
// At the dial's two stops that keep to the book, Britain and America have the record's operation of November 1942
// in the form Germany's Norway has: its date in history.json, the declaration on Vichy, and the landings in Morocco
// and Algeria by named corps kept at their quays until the day. America's are picked from its own corps, never
// added; the French turn runs as the record wrote it in the landing's month; and the Axis answer in Tunisia is the
// book's dated rows, nothing twice. It ships switched off, and off it leaves no trace in a game. Nowhere is it a
// staff's own choice: at the stops that keep no book, the operation is on no menu.

import { afterEach, describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import open from '../data/campaign/open.json';
import { bareShore, createGame, groundWorks, men, torchRowsOnly, worksOf } from '../src/campaign/game';
import { HISTORY, leash, setStop } from '../src/campaign/history';
import { lineShut, lines } from '../src/campaign/line';
import type { Line } from '../src/campaign/line';
import { bookLanding, startOpen } from '../src/campaign/staff';
import { STRATEGY, torchBook, torchWeek } from '../src/campaign/strategy';
import { techMul } from '../src/campaign/research';
import { runTurnAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

const start = (seed: number, stop: number, human: string | null = null): Game => {
  const g = startOpen(createGame(seed, human));
  if (stop !== 3) setStop(g, stop, true);
  return g;
};
type Row = [number, string, string, string, number, string, Record<string, unknown>?];
const TORCH = (setup as unknown as { torch: { instead: string[]; rows: Row[]; vichy: { nat: string; from: number; ground: string[]; places: [string, number, string][]; bare: string[]; designs: { field: string; of: string; to: string }; air: [string, number, number][] }; harbours: { rowsOnly: boolean } } }).torch;
const ROSTER = (setup as unknown as { expeditions: Row[] }).expeditions;
const LINE = (setup as unknown as { torch: { line: Line } }).torch.line;
type Landing = { corps: string; own?: number; to: string[] };
const LANDINGS = (nat: string): Landing[] => (open as unknown as { ops: Record<string, { id: string; landings?: Landing[] }[]> }).ops[nat].find((o) => o.id === 'torch')!.landings!;

describe('Operation Torch by the book', () => {
  const was = STRATEGY.torch!.on, week = STRATEGY.torch!.week;
  afterEach(() => { STRATEGY.torch!.on = was; STRATEGY.torch!.ceasefire = false; STRATEGY.torch!.week = week; });

  it('ships switched off, with its data in order', () => {
    const T = STRATEGY.torch!;
    expect(T.on).toBe(false);
    // (The step struck on 22 November 2026 is behind its own key, and that key ships off.)
    expect(T.ceasefire).toBe(false);
    // (The French turn comes in the landing's week, the first of the month: the design director, 13:27Z.)
    expect(T.week).toBe(1);
    // Only the two stops that keep to the book.
    expect(T.stops.every((s) => s < 3)).toBe(true);
    // The record's line for French North Africa (the design director, 14:45Z): drawn on Vichy's African ground and on
    // no other of Vichy's, open to the Allies at the three landing shores alone, ended by the turn's own flag, in
    // Torch's own theatre; and with the switch off no line is drawn at all.
    expect(LINE.land).toBe(TORCH.vichy.nat);
    expect(LINE.ends).toBe(`event:${T.turn}`);
    for (const key of T.ops) expect(HISTORY.theatre[key], key).toBe(LINE.theatre);
    // (Vichy's African ground: every province of Vichy's that a walk over Vichy's ground reaches from Algiers.)
    const map = start(1, 1), walk = [map.byName.Algiers.id], reached = new Set(walk);
    for (let i = 0; i < walk.length; i++) for (const j of map.regions[walk[i]].nb) if (map.regions[j].vichy && !reached.has(j)) { reached.add(j); walk.push(j); }
    const africa = walk.map((id) => map.regions[id].name).sort();
    expect([...LINE.ground!].sort()).toEqual(africa);
    for (const nm of TORCH.vichy.ground) expect(LINE.ground, nm).toContain(nm);
    expect(Object.keys(LINE.spans).sort()).toEqual([...TORCH.vichy.bare].sort());
    for (const sp of Object.values(LINE.spans)) expect(sp).toEqual([{ from: 1, kind: 'landing' }]);
    // No date ends it (the design director, 17:47Z): the turn's flag does, or Vichy holding none of this ground.
    expect((LINE as { until?: number }).until).toBeUndefined();
    expect(lines().some((L) => L.id === LINE.id)).toBe(false);
    // Each operation named is on its staff's menu, dated by the book, and marked the book's alone.
    for (const key of T.ops) {
      const [nat, id] = key.split(':');
      const op = ((open as unknown as { ops: Record<string, { id: string; declare?: string[]; landings?: { corps: string; own?: number; to: string[] }[] }[]> }).ops[nat] ?? []).find((o) => o.id === id);
      expect(op, key).toBeTruthy();
      expect(op!.declare).toEqual(['VIC']);
      expect((op!.landings ?? []).length).toBeGreaterThan(0);
      expect((HISTORY.dates as Record<string, Record<string, number>>)[nat][id]).toBe(39);
      expect(HISTORY.bookOnly!.ops).toContain(key);
    }
    // America's landings are made from its own corps: every one names a worth, and none names a roster row.
    const usa = (open as unknown as { ops: Record<string, { id: string; landings?: { corps: string; own?: number }[] }[]> }).ops.USA.find((o) => o.id === 'torch')!;
    for (const L of usa.landings!) { expect(L.own).toBeGreaterThan(0); expect(ROSTER.some((r) => r[2] === L.corps)).toBe(false); }
    // The first shore each landing names is the record's: Algiers for the Eastern force, Casablanca and Oran for America's two.
    expect(LANDINGS('UK').map((L) => L.to[0])).toEqual(['Algiers']);
    expect(LANDINGS('USA').map((L) => L.to[0])).toEqual(['Casablanca', 'Oran']);
    // Nothing twice: a row of Torch's that carries a roster row's name stands in for it, and every name stood in for is a roster row's.
    for (const nm of TORCH.instead) expect(ROSTER.some((r) => r[2] === nm), nm).toBe(true);
    for (const row of TORCH.rows) if (ROSTER.some((r) => r[1] === row[1] && r[2] === row[2])) expect(TORCH.instead, row[2]).toContain(row[2]);
    expect(new Set(TORCH.rows.map((r) => `${r[1]}:${r[2]}`)).size).toBe(TORCH.rows.length);
    // The Axis answer is rows with a date; Pistoia and La Spezia were Libya's and do not come to Tunis again.
    expect(TORCH.instead).toContain('Pistoia and La Spezia Divisions');
    expect(TORCH.rows.some((r) => /Pistoia/.test(r[2]))).toBe(false);
    // The Axis rows hold the bridgehead they land in: every one is held and marked to stay. No Allied row is.
    for (const r of TORCH.rows) { const axis = r[1] === 'GER' || r[1] === 'ITA'; expect(!!r[6]?.hold && !!r[6]?.stay, r[2]).toBe(axis); }
    // Vichy's placing on the day: the record's commands, eight divisions as the 1939 rows have, every place on the
    // ground named and laid before the day; and America is the nation whose divisions there are the book's rows.
    const V = TORCH.vichy;
    expect(V.places.reduce((n, x) => n + x[1], 0)).toBe(8);
    expect((setup as unknown as { corps: [string, string, string, number, string][] }).corps.filter((r) => r[0] === 'FRA' && V.ground.includes(r[4])).reduce((n, r) => n + r[3], 0)).toBe(8);
    for (const [at] of V.places) expect(V.ground, at).toContain(at);
    expect(V.places.slice(0, 3).map((x) => x[0])).toEqual(['Algiers', 'Oran', 'Casablanca']);
    expect(V.from).toBeLessThan(39);
    expect(TORCH.harbours.rowsOnly).toBe(true);
    // Its arms are the designs of 1940, read on France's own clock; its air is laid at the three landing seats.
    expect(V.designs).toEqual({ field: 'infantry', of: 'FRA', to: '1940-06' });
    expect(V.air.map((x) => x[0])).toEqual(['Casablanca', 'Oran', 'Algiers']);
    for (const [at, ftr, bmr] of V.air) { expect(V.ground, at).toContain(at); expect(ftr).toBeGreaterThan(0); expect(bmr).toBeGreaterThan(0); }
    expect(V.air.find((x) => x[0] === 'Casablanca')!.slice(1)).toEqual([86, 78]);
    // America's follow-up is three dated rows found from its own reserve of men, each kept at its own size; no other row is.
    const usaRows = TORCH.rows.filter((r) => r[1] === 'USA');
    expect(usaRows.map((r) => r[2])).toEqual(['1st Armored Division', '2nd Armored Division', '34th Infantry Division']);
    for (const r of TORCH.rows) expect(!!r[6]?.reserve, r[2]).toBe(r[1] === 'USA');
    for (const r of usaRows) { expect(r[0]).toBeGreaterThanOrEqual(40); expect(r[6]!.est).toBe(r[6]!.str); expect(r[6]!.str as number).toBeLessThan(1); }
  });

  it('Vichy stands where the record had it on the day, with no division added or struck; never with the switch off', () => {
    const where = (g: Game): string => g.corps.filter((c) => c.nat === 'VIC' && !c.dead && TORCH.vichy.ground.includes(g.regions[c.reg].name)).map((c) => `${g.regions[c.reg].name} ${c.divs}`).sort().join(', ');
    const count = (g: Game): number => g.corps.filter((c) => c.nat === 'VIC' && !c.dead && TORCH.vichy.ground.includes(g.regions[c.reg].name)).reduce((n, c) => n + men(c), 0);
    // Off: the 1939 rows where they have stood since.
    const off = start(17, 1);
    while (off.turn < 39) runTurnAI(off);
    expect(where(off)).toBe('Algiers 3, Casablanca 2, Mareth 3');
    expect(off.flags['torch:vichy']).toBeUndefined();
    // (And their works are the finished works of three years' standing.)
    const works = (x: Game, at: string): number => Math.max(0, ...x.corps.filter((c) => c.nat === 'VIC' && !c.dead && x.regions[c.reg].name === at).map(worksOf));
    expect(works(off, 'Algiers')).toBe(3);
    expect(works(off, 'Casablanca')).toBe(3);
    expect(off.corps.some((c) => bareShore(off, c))).toBe(false);
    // (And its arms are its staff's own, with no air group anywhere.)
    expect(techMul(off, 'VIC', 'infantry')).toBe(1);
    expect(off.airFleets.some((f) => f.nat === 'VIC')).toBe(false);
    // On, at stop 1: the 1939 rows until the month the data names, then the record's commands.
    STRATEGY.torch!.on = true;
    const g = start(17, 1);
    while (g.turn < TORCH.vichy.from - 1) runTurnAI(g);
    expect(where(g)).toBe('Algiers 3, Casablanca 2, Mareth 3');
    expect(techMul(g, 'VIC', 'infantry')).toBe(1);
    expect(g.airFleets.some((f) => f.nat === 'VIC')).toBe(false);
    const before = count(g);
    runTurnAI(g);
    expect(g.flags['torch:vichy']).toBe(TORCH.vichy.from);
    expect(where(g)).toBe('Algiers 1, Casablanca 1, Constantine 1, Fez 2, Marrakech 1, Oran 1, Tunis 1');
    expect(count(g)).toBe(before);
    for (const c of g.corps) if (c.nat === 'VIC' && TORCH.vichy.ground.includes(g.regions[c.reg].name)) expect(c.hold, c.name).toBe(true);
    // No fieldworks at the three landing ports, the corps' own or left standing; the seats inland keep or dig theirs.
    expect(TORCH.vichy.bare).toEqual(['Algiers', 'Oran', 'Casablanca']);
    for (const at of TORCH.vichy.bare) { expect(works(g, at), at).toBe(0); expect(groundWorks(g, g.byName[at].id), at).toBeNull(); }
    // Its infantry reads the designs of 1940: in October 1942 two behind France's own clock, 0.88. No other nation's changes.
    expect(techMul(g, 'VIC', 'infantry')).toBeCloseTo(0.88, 6);
    expect(techMul(g, 'VIC', 'armour')).toBe(techMul(off, 'VIC', 'armour'));
    // Its air by seat: a group of fighters and one of bombers at each, with the aircraft the data names.
    const wings = (x: Game): string => x.airFleets.filter((f) => f.nat === 'VIC').map((f) => `${x.regions[f.base].name} ${f.kind} ${f.ftr + f.bmr}`).sort().join(', ');
    const laid = TORCH.vichy.air.flatMap(([at, ftr, bmr]) => [`${at} bmr ${bmr}`, `${at} ftr ${ftr}`]).sort().join(', ');
    expect(wings(g)).toBe(laid);
    // Once: the next month it is not laid again, and a month's standing has dug nothing at the ports.
    runTurnAI(g);
    expect(g.flags['torch:vichy']).toBe(TORCH.vichy.from);
    expect(g.turn).toBe(39);
    for (const at of TORCH.vichy.bare) expect(works(g, at), at).toBe(0);
    expect(works(g, 'Fez')).toBeGreaterThan(0);
    // (And every air row is still at its seat: Vichy's staff has moved none.)
    expect(wings(g)).toBe(laid);
    // At a stop that keeps no book, never.
    const free = start(17, 3);
    while (free.turn < 39) runTurnAI(free);
    expect(free.flags['torch:vichy']).toBeUndefined();
    // No staff's own shipping sails a corps into a harbour of that ground where Torch is the book's, and only there:
    // the armies there are the rows' (every nation's: a Canadian corps once sailed from Cornwall into Constantine).
    expect(torchRowsOnly(g, 'USA', g.byName.Oran)).toBe(true);
    expect(torchRowsOnly(g, 'USA', g.byName.Cornwall)).toBe(false);
    expect(torchRowsOnly(g, 'UK', g.byName.Oran)).toBe(true);
    expect(torchRowsOnly(g, 'CAN', g.byName.Constantine)).toBe(true);
    expect(torchRowsOnly(g, 'CAN', g.byName.Cornwall)).toBe(false);
    expect(torchRowsOnly(free, 'USA', free.byName.Oran)).toBe(false);
    expect(torchRowsOnly(start(17, 1, 'USA'), 'USA', g.byName.Oran)).toBe(false);
    STRATEGY.torch!.on = false;
    expect(torchRowsOnly(g, 'USA', g.byName.Oran)).toBe(false);
  }, 300_000);

  it('is on no menu with the switch off, nor at a stop that keeps no book, nor for a player', () => {
    for (const stop of [1, 3, 5]) {
      const g = start(31, stop);
      expect(torchBook(g)).toBe(false);
      expect(leash(g, 'UK', 'torch').kind).toBe('never');
      expect(leash(g, 'USA', 'torch').kind).toBe('never');
    }
    STRATEGY.torch!.on = true;
    for (const stop of [3, 4, 5]) {
      const g = start(31, stop);
      expect(torchBook(g)).toBe(false);
      expect(leash(g, 'UK', 'torch').kind).toBe('never');
    }
    // On, at the book's stops: dated, November 1942.
    for (const stop of [1, 2]) {
      const g = start(31, stop);
      expect(torchBook(g)).toBe(true);
      const L = leash(g, 'UK', 'torch'), A = leash(g, 'USA', 'torch');
      expect(L.kind).toBe('dated');
      expect(A.kind).toBe('dated');
      // (At stop 1 the book's own month; at stop 2 give or take its month of slack, and never two months for the one operation.)
      if (stop === 1) expect(L.date).toBe(39);
      else expect(Math.abs(L.date! - 39)).toBeLessThanOrEqual(1);
      expect(A.date).toBe(L.date);
    }
    for (let seed = 40; seed < 52; seed++) { const g = start(seed, 2); expect(leash(g, 'USA', 'torch').date).toBe(leash(g, 'UK', 'torch').date); }
    // By the book a landing of Torch's goes to its first-named shore (on its day whatever the odds); with the switch
    // off, at a stop that keeps no book, or for a player, it is a landing like any other.
    for (const nat of ['UK', 'USA']) for (const L of LANDINGS(nat)) {
      expect(bookLanding(start(31, 1), nat, L)).toBe('after');
      expect(bookLanding(start(31, 3), nat, L)).toBeNull();
      expect(bookLanding(start(31, 1, nat), nat, L)).toBeNull();
    }
    STRATEGY.torch!.on = false;
    for (const nat of ['UK', 'USA']) for (const L of LANDINGS(nat)) expect(bookLanding(start(31, 1), nat, L)).toBeNull();
    STRATEGY.torch!.on = true;
    // Germany's own dated operation is untouched by any of it.
    expect(leash(start(31, 1), 'GER', 'weser').kind).toBe('dated');
  });

  it('on, at stop 1: the landings go in November 1942 at the record\'s three ports from America\'s own corps, the French turn in the landing\'s week, and nothing comes twice', () => {
    STRATEGY.torch!.on = true;
    const g = start(17, 1);
    while (g.turn < 39) runTurnAI(g);
    // America's force is its own: two corps picked, standing where its convoys can reach the shore, and no row of the roster's sent.
    const picked = Object.keys(g.flags).filter((k) => k.startsWith('st:own:USA:'));
    expect(picked.length).toBe(2);
    const usaBefore = g.corps.filter((c) => c.nat === 'USA' && !c.dead).length;
    for (const k of picked) expect(g.corps.find((c) => c.id === g.flags[k] - 1 && c.nat === 'USA' && !c.dead)).toBeTruthy();
    expect(g.corps.some((c) => c.nat === 'USA' && /Task Force|II Corps \(1st Infantry/.test(c.name))).toBe(false);
    expect(g.nations.VIC.alive).toBe(true);
    expect(g.flags['event:darlan']).toBeUndefined();
    expect(torchWeek(g)).toBe(1);
    const seen = g.reports.length;
    const wingIds = g.airFleets.filter((f) => f.nat === 'VIC').map((f) => f.id);
    expect(wingIds.length).toBe(2 * TORCH.vichy.air.length);
    // The record's line: before the turn Vichy's African ground is shut to a computer staff of Britain's side, all
    // but the three landing shores. Vichy's other ground is not the line's, and the Axis is not bound by it.
    expect(lines().some((L) => L.id === LINE.id)).toBe(true);
    for (const nm of LINE.ground!) for (const nat of ['UK', 'USA']) expect(lineShut(g, nat, g.byName[nm]), `${nat} ${nm}`).toBe(!TORCH.vichy.bare.includes(nm));
    for (const nm of ['Syria', 'Marseille', 'Corsica']) expect(lineShut(g, 'UK', g.byName[nm]), nm).toBe(false);
    for (const nm of LINE.ground!) expect(lineShut(g, 'GER', g.byName[nm]), `GER ${nm}`).toBe(false);
    // No date ends the line: were Vichy to hold none of this ground, it would be ended with the turn not yet come.
    const held = LINE.ground!.map((nm) => g.byName[nm].ctrl);
    for (const nm of LINE.ground!) g.byName[nm].ctrl = 'ITA';
    for (const nm of LINE.ground!) expect(lineShut(g, 'UK', g.byName[nm]), `none held ${nm}`).toBe(false);
    LINE.ground!.forEach((nm, k) => { g.byName[nm].ctrl = held[k]; });
    expect(lineShut(g, 'UK', g.byName.Tunis)).toBe(true);
    // (Britain's desert army stands at Tunisia's door this war: Tripoli fell to it, or falls this month.)
    const britishBefore = new Set(g.corps.filter((c) => c.nat === 'UK' && !c.dead).map((c) => c.id));
    runTurnAI(g);
    // Britain moved before the turn, so by land it entered none of it: the one British fight on that ground is the
    // landing at Algiers, and no British corps that was in the field before the day stands on it anywhere else.
    const british = g.reports.slice(seen).filter((r) => r.turn === 39 && r.attNat === 'UK' && LINE.ground!.includes(r.regionName));
    expect(british.map((r) => r.regionName)).toEqual(['Algiers']);
    for (const c of g.corps) if (c.nat === 'UK' && !c.dead && britishBefore.has(c.id) && LINE.ground!.includes(g.regions[c.reg].name)) expect(g.regions[c.reg].name, c.name).toBe('Algiers');
    // The turn ended the line: from now the staffs decide for themselves.
    for (const nm of LINE.ground!) expect(lineShut(g, 'UK', g.byName[nm]), `after the turn ${nm}`).toBe(false);
    // The day: both staffs went, somebody is ashore in Morocco or Algeria, and the French turned in that month.
    expect(g.flags['st:done:UK:torch']).toBe(39);
    expect(g.flags['st:done:USA:torch']).toBe(39);
    expect(g.flags['event:darlan']).toBe(39);
    expect(g.nations.VIC.alive).toBe(false);
    for (const nm of ['Casablanca', 'Oran', 'Algiers', 'Constantine']) expect(['UK', 'USA'], nm).toContain(g.byName[nm].ctrl);
    // (In the landing's own week: both nations' landings were made that month, and the turn did not wait for its end.)
    expect(g.flags['torch:day:UK']).toBe(39);
    expect(g.flags['torch:day:USA']).toBe(39);
    // The landings went where the record's did, and nowhere else. The turn cut the fights short in their first week,
    // so every force is ashore at its own shore, won or undecided, and the day cost a week's toll of the month's.
    const at = (c: { reg: number } | undefined): string | undefined => (c ? g.regions[c.reg].name : undefined);
    expect(at(g.corps.find((c) => c.name === 'First Army: 78th Division' && !c.dead))).toBe('Algiers');
    expect(at(g.corps.find((c) => c.id === g.flags['st:own:USA:Western Task Force'] - 1 && !c.dead))).toBe('Casablanca');
    expect(at(g.corps.find((c) => c.id === g.flags['st:own:USA:Center Task Force'] - 1 && !c.dead))).toBe('Oran');
    const day = g.reports.slice(seen).filter((r) => r.turn === 39 && r.defNat === 'VIC' && ['Algiers', 'Casablanca', 'Oran'].includes(r.regionName));
    expect(day.map((r) => r.regionName).sort()).toEqual(['Algiers', 'Casablanca', 'Oran']);
    for (const r of day) {
      expect(r.lines.some((l) => l.label === 'Cut short'), r.regionName).toBe(true);
      // (A month's fight costs the attacker 2% at the very least; a week of it, on 15,000 or 30,000 men, far under that.)
      expect(r.attLost, r.regionName).toBeLessThan(1500);
    }
    // (No force is left noted as lying off its shore.)
    expect(Object.keys(g.flags).some((k) => k.startsWith('torch:off:'))).toBe(false);
    // Vichy's air is struck at the turn with the rest of its air force: no group is Vichy's, and none flies on for the Allies.
    expect(g.airFleets.some((f) => f.nat === 'VIC')).toBe(false);
    expect(Object.keys(g.flags).some((k) => k.startsWith('torch:wing:'))).toBe(false);
    expect(g.airFleets.some((f) => wingIds.includes(f.id))).toBe(false);
    // America's divisions in French North Africa are the book's rows: nothing of its army stands there but the two picks.
    const picks = new Set(picked.map((k) => g.flags[k] - 1));
    for (const c of g.corps) if (c.nat === 'USA' && !c.dead && TORCH.vichy.ground.includes(g.regions[c.reg].name)) expect(picks.has(c.id), c.name).toBe(true);
    // No new American men for it: the count of its corps moved only by what its own factories and ships did (none struck, none added by Torch).
    expect(g.corps.filter((c) => c.nat === 'USA' && !c.dead && /Task Force/.test(c.name)).length).toBe(0);
    expect(g.corps.filter((c) => c.nat === 'USA' && !c.dead).length).toBeGreaterThanOrEqual(usaBefore);
    // The turn made none of its three Axis divisions: the Axis answer is the dated rows.
    expect(g.corps.some((c) => /von Broich|Saharan Group/.test(c.name))).toBe(false);
    // The 78th came once, by Torch's row, and the roster's own row for it was never sent.
    expect(g.corps.filter((c) => c.name === 'First Army: 78th Division').length).toBe(1);
    const i78 = ROSTER.findIndex((r) => r[2] === 'First Army: 78th Division');
    expect(g.flags[`sent:${i78}`]).toBeUndefined();
    // XIX Corps is made from the French army that came over, never added to it, and at the turn itself: it stands
    // on ground the French held, now the Allies', before the Axis moves again.
    expect(g.flags['torch:turned']).toBeGreaterThan(0);
    const xix = g.corps.filter((c) => c.name === 'French XIX Corps' && !c.dead);
    expect(xix.length).toBe(1);
    expect(xix[0].nat).toBe('UK');
    expect(xix[0].divs).toBeLessThanOrEqual(g.flags['torch:turned']);
    expect(['UK', 'USA']).toContain(g.regions[xix[0].reg].ctrl);
    expect(['Casablanca', 'Oran', 'Algiers', 'Constantine', 'Fez', 'Marrakech', 'Mareth', 'Sfax', 'Tunis']).toContain(g.regions[xix[0].reg].name);
    // The Axis rows that arrive by the week (the historian's weekly file; the design director, 17:58Z and 18:29Z). A
    // week rounds to the nearest month's opening. XC Corps and the airlift, of November's second week, came into Tunis at the turn
    // (Tunis was Vichy's when Germany's November opened); the 10th Panzer, of its last week, and the 334th, of late
    // December, are not in the game yet.
    const weekly = TORCH.rows.filter((r) => (r[6] as { week?: number } | undefined)?.week !== undefined).map((r) => [r[0], r[2].slice(0, 8), (r[6] as { week: number }).week]);
    expect(weekly).toEqual([[39, 'XC Corps', 2], [39, 'Tunis ai', 2], [39, '10th Pan', 4], [40, '334th In', 4], [40, '50th Spe', 2], [40, 'Heavy Ta', 2]]);
    const sent = (nm: string): number | undefined => g.flags[`sent:t${TORCH.rows.findIndex((r) => r[2].startsWith(nm))}`];
    expect(g.byName.Tunis.ctrl).toBe('GER');
    // (The airlift is a row of its own, worth a division: 15,273 men had come by air by 30 November, 1,867 by sea.)
    expect(g.byName.Tunis.units.filter((c) => !c.dead).map((c) => c.name.slice(0, 8)).sort()).toEqual(['Tunis ai', 'XC Corps']);
    expect(g.byName.Tunis.units.filter((c) => !c.dead).reduce((w, c) => w + c.divs * c.str, 0)).toBeCloseTo(1.3, 1);
    expect(sent('XC Corps')).toBeTruthy();
    expect(sent('Tunis airlift')).toBeTruthy();
    expect(sent('10th Panzer Division')).toBeUndefined();
    expect(g.corps.some((c) => c.name === '10th Panzer Division' || c.name === '334th Infantry Division')).toBe(false);
    // (December's units by unit, the design director, 18:46Z: none of them is in the game in November.)
    for (const nm of ['Heavy Tank Battalion 501', '50th Special Brigade', 'Panzer Battalion 190']) expect(sent(nm), nm).toBeUndefined();
    // The next month it is not made again.
    runTurnAI(g);
    expect(g.corps.filter((c) => c.name === 'French XIX Corps').length).toBe(1);
    // December: the 10th Panzer came when Germany's month opened (the record has it fighting at Tebourba on 1 to 4
    // December), once, and XC Corps was not made a second time. The 334th is in no fight of December: it comes when
    // Germany's January opens (to Tunis, or to the nearest harbour its side still holds).
    expect(sent('10th Panzer Division')).toBeTruthy();
    for (const nm of ['XC Corps', 'Tunis airlift', '10th Panzer Division']) expect(g.corps.filter((c) => c.name.startsWith(nm)).length, nm).toBeLessThanOrEqual(1);
    expect(sent('334th Infantry Division')).toBeUndefined();
    expect(g.corps.some((c) => c.name === '334th Infantry Division')).toBe(false);
    for (const r of g.reports) if (r.turn === 40) expect(r.defenders.some((u) => /^334th/.test(u.name)), r.regionName).toBe(false);
    // December's own rows (18:46Z): the rest of Heavy Tank Battalion 501, a sixth of an armoured division by the rule
    // for a tank battalion, and the 50th Special Brigade, each once; Panzer Battalion 190 is January's.
    const dec = TORCH.rows.filter((r) => /^(Heavy Tank Battalion 501|50th Special Brigade|Panzer Battalion 190)/.test(r[2]));
    expect(dec.map((r) => [r[0], r[1], r[3], r[5], (r[6] as { str: number }).str])).toEqual([[40, 'ITA', 'inf', 'Mareth', 0.3], [40, 'GER', 'arm', 'Tunis', 0.17], [41, 'GER', 'arm', 'Sfax', 0.17]]);
    expect(sent('Heavy Tank Battalion 501')).toBeTruthy();
    expect(sent('50th Special Brigade')).toBeTruthy();
    expect(sent('Panzer Battalion 190')).toBeUndefined();
    for (const nm of ['Heavy Tank Battalion 501', '50th Special Brigade']) expect(g.corps.filter((c) => c.name.startsWith(nm)).length, nm).toBeLessThanOrEqual(1);
    expect(g.corps.some((c) => c.name === 'Panzer Battalion 190')).toBe(false);
    // December: the rest of the two armoured divisions come, once each, as the book's rows, at their own size; and
    // still nothing of America's army stands in French North Africa but its picks and those rows.
    const rows = new Set(TORCH.rows.filter((r) => r[1] === 'USA').map((r) => r[2]));
    for (const nm of ['1st Armored Division', '2nd Armored Division']) {
      const c = g.corps.filter((x) => x.nat === 'USA' && x.name === nm);
      expect(c.length, nm).toBe(1);
      const size = TORCH.rows.find((r) => r[1] === 'USA' && r[2] === nm)![6]!.est as number;
      expect(size, nm).toBe(0.5);
      expect(c[0].full, nm).toBe(size);
      expect(c[0].str, nm).toBeLessThanOrEqual(size + 1e-9);
    }
    expect(g.corps.some((c) => c.name === '34th Infantry Division')).toBe(false);
    for (const c of g.corps) if (c.nat === 'USA' && !c.dead && TORCH.vichy.ground.includes(g.regions[c.reg].name)) expect(picks.has(c.id) || rows.has(c.name), c.name).toBe(true);
    runTurnAI(g);
    expect(g.corps.filter((c) => c.nat === 'USA' && c.name === '34th Infantry Division').length).toBe(1);
  }, 300_000);

  it('the struck step, with its own key on: a force thrown back on the day is ashore at its shore after the turn', () => {
    STRATEGY.torch!.on = true;
    STRATEGY.torch!.ceasefire = true;
    // (With the week's key off: the turn at the month's end, as the step was read.)
    STRATEGY.torch!.week = 0;
    const g = start(17, 1);
    while (g.turn < 40) runTurnAI(g);
    const at = (c: { reg: number } | undefined): string | undefined => (c ? g.regions[c.reg].name : undefined);
    expect(at(g.corps.find((c) => c.name === 'First Army: 78th Division' && !c.dead))).toBe('Algiers');
    expect(at(g.corps.find((c) => c.id === g.flags['st:own:USA:Western Task Force'] - 1 && !c.dead))).toBe('Casablanca');
    expect(at(g.corps.find((c) => c.id === g.flags['st:own:USA:Center Task Force'] - 1 && !c.dead))).toBe('Oran');
    expect(Object.keys(g.flags).some((k) => k.startsWith('torch:off:'))).toBe(false);
  }, 300_000);
});
