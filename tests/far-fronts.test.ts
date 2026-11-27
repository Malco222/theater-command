// The fronts of a plan taken from the record (rider b633 to ruling 234, with the design director's notes of 11:07Z,
// 11:17Z and 11:28Z of 19 November 2026; src/campaign/planfronts.ts, and `frontsMonth` in ai.ts). The historian's
// table is historian/japan-plan-nov41-b631.md ("Addendum for b633").
//
// Where no book dates Japan's rows and the strike south has gone: each front has its listed rows until its place is
// Japan's or its force is gone; a row on a live front goes no farther from that place; the plan walks the rows that
// join by road, and makes the follow-ons by their months once the hop before is Japan's. The states are set by hand
// on the first day's chart; one month of Japan's own is played where the plan's moves are read. No war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import { frontWatch, garrisons, runAI } from '../src/campaign/ai';
import { addCorps, createGame, declareWar } from '../src/campaign/game';
import { raisedQuality, setStop } from '../src/campaign/history';
import { byThePlan, followKey, frontsClear, frontsState, frontsTick, hopKey, hopMade, landMarches, offFront, onFront, planFronts, planWaits, plansHold, sailedKey, walkedKey } from '../src/campaign/planfronts';
import { garrison, garrisonOf } from '../src/campaign/siege';
import { ops, startOpen } from '../src/campaign/staff';
import farWorld from '../data/campaign/far-world.json';
import farShores from '../data/campaign/far-shores.json';
import type { Corps, Game } from '../src/campaign/types';

const D5 = '25th Army (5th Division)';
const D18 = '25th Army (18th Division)';
const GUARDS = '25th Army (Imperial Guards Division)';
const T14 = '14th Tank Regiment';
const D48 = '14th Army (48th Division)';
const T4 = '4th Tank Regiment';
const D16 = '14th Army (16th Division)';
const A15 = '15th Army (33rd and 55th Divisions)';
const UNO = 'Uno Detachment (143rd Infantry Regiment, of the 55th Division)';
// (The 38th Division's three regimental rows, where no book dates Japan's rows: ruling 234's notes of 22:05Z and 22:10Z of 20 November 2026, far-shores.json.)
const R228 = '228th Infantry Regiment (the Ito Detachment)';
const R229 = "229th Infantry Regiment (with the 38th Division's headquarters)";
const R230 = '230th Infantry Regiment (the Shoji Detachment)';
const GUNS = '23rd Army artillery (1st Artillery Group)';
const SAKAGUCHI = 'Sakaguchi Detachment (56th Mixed Infantry Group)';
const KURE = 'Naval landing forces (Kure 1st and 2nd)';
const SASEBO = 'Naval landing forces (Sasebo 1st and 2nd, Yokosuka 1st)';
const D2 = '16th Army (2nd Division)';
const KAWAGUCHI = 'Kawaguchi Detachment (35th Brigade, of the 18th Division)';
const TANAKA = 'Tanaka and Kanno Detachments (2nd Formosa Infantry, of the 48th Division)';
const SOUTH_SEAS = 'South Seas Detachment';
const B65 = '14th Army (65th Brigade)';

const SOUTH = () => ops('JAP').find((op) => op.id === 'south')!;
const front = (id: string) => SOUTH().fronts!.find((F) => F.id === id)!;

/** A war at a stop of the dial in month `turn`, the strike south gone in month `went` (none where 0). */
const war = (stop: number, turn: number, went: number, human: string | null = null): Game => {
  const g = startOpen(createGame(3, human));
  if (stop !== 3) setStop(g, stop, true);
  g.turn = turn;
  if (went) {
    g.flags['st:done:JAP:south'] = went;
    for (const e of ['USA', 'UK', 'NED']) declareWar(g, 'JAP', e);
  }
  return g;
};
const put = (g: Game, name: string, place: string, o: { hold?: boolean; stay?: boolean; str?: number; kind?: 'inf' | 'arm' | 'art' } = {}): Corps => {
  const c = addCorps(g, 'JAP', name, o.kind ?? 'inf', 1, place, { xp: 0.3, hold: o.hold || o.stay });
  if (o.stay) c.stay = true;
  if (o.str !== undefined) c.str = o.str;
  return c;
};
/** A place in Japan's hands, with nobody of the other side in it. */
const japans = (g: Game, name: string): void => {
  const r = g.byName[name];
  r.ctrl = 'JAP';
  r.units = r.units.filter((u) => u.nat === 'JAP');
};
/** A place with nobody in its field and no town guard: a landing there walks in. */
const open = (g: Game, name: string): void => {
  const r = g.byName[name];
  r.units = r.units.filter((u) => u.nat === 'JAP');
  g.flags[`guard:${r.id}`] = 100;
};
const state = (g: Game, id: string) => frontsState(g, 'JAP').find((f) => f.id === id)!;
const where = (g: Game, c: Corps): string => g.regions[c.reg].name;

type Told = { what: string; front: string; force: string[]; to: string; from: string; taken?: boolean; why?: string };
const told: Told[] = [];
const listen = (): void => { told.length = 0; frontWatch.tell = (e) => { told.push({ what: e.what, front: e.front, force: e.force.map((c) => c.name), to: e.to.name, from: e.from.name, taken: e.taken, why: e.why }); }; };
afterEach(() => { frontWatch.tell = null; frontsClear(); });

describe("the fronts of Japan's plan (rider b633 to ruling 234)", () => {
  it("lays the seven fronts: each its place, the November plan's aim, and its rows as the book has them", () => {
    expect(SOUTH().fronts!.map((F) => [F.id, F.place, F.rows.length])).toEqual([
      ['malaya', 'Singapore', 7], ['luzon', 'Manila', 9], ['burma', 'Myitkyina', 4], ['hongkong', 'Hong Kong', 4],
      ['indies', 'Batavia', 5], ['southseas', 'Rabaul', 1], ['wake', 'Wake', 1],
    ]);
    // (Luzon's nine: the six, and the 16th Division's Kimura Detachment and 9th Infantry and the 14th Army's heavy
    // guns, rows of their own from the design director's rulings of 22:34Z and 22:42Z of 21 November 2026.)
    expect(front('luzon').rows.slice(6)).toEqual(['Kimura Detachment (16th Division)', '16th Division (9th Infantry)', '14th Army artillery (1st and 8th Field Heavy Artillery)']);
    // By road: the Guards with their tanks to Singora, the 143rd Regiment to the Kra (the design director's of 04:58Z,
    // 21 November 2026), and Hong Kong's rows to Tungkun (the book's own step).
    expect(SOUTH().fronts!.flatMap((F) => (F.land ?? []).map((m) => [F.id, m.corps, m.with, m.to]))).toEqual([
      ['malaya', GUARDS, [T14], 'Singora'], ['burma', UNO, undefined, 'Kra'], ['hongkong', R229, [R228, R230, GUNS], 'Tungkun'],
    ]);
    expect(front('indies').held).toEqual([D2]);
    // Every row and every place the table names is the world's.
    const g = war(3, 28, 0);
    const rows = new Set<string>([...(farWorld as any)['setup-1939'].expeditions.map((r: any[]) => r[2]), ...(farShores as any)['setup-1939'].expeditions.map((r: any[]) => r[2]), ...g.corps.map((c) => c.name)]);
    for (const F of SOUTH().fronts!) {
      expect(g.byName[F.place], F.place).toBeTruthy();
      for (const nm of [...F.rows, ...(F.land ?? []).flatMap((m) => [m.corps, ...(m.with ?? [])]), ...(F.then ?? []).flatMap((f) => [f.corps, ...(f.with ?? [])])]) expect(rows.has(nm), nm).toBe(true);
      for (const nm of [...(F.land ?? []).map((m) => m.to), ...(F.then ?? []).flatMap((f) => [...f.to, ...(f.after ?? []), ...(f.from ?? [])])]) expect(g.byName[nm], nm).toBeTruthy();
    }
  });

  it("lays the follow-ons by month from the blow, each with the hop before it that the historian names", () => {
    const T = front('indies').then!.map((f) => [f.month, f.corps, f.to[0], f.after ?? [], f.stage ?? false]);
    expect(T).toEqual([
      [1, SAKAGUCHI, 'Tarakan', ['Jolo'], false],
      [1, SAKAGUCHI, 'Balikpapan', ['Tarakan'], false],
      [1, SASEBO, 'Menado', ['Davao'], false],
      [1, SASEBO, 'Kendari', ['Menado'], false],
      [1, KURE, 'Ambon', ['Kendari'], false],
      [1, R228, 'Ambon', ['Hong Kong', 'Kendari'], false],
      [2, SASEBO, 'Makassar', ['Kendari'], false],
      [2, R229, 'Palembang', ['Hong Kong', 'Johore'], false],
      [2, D48, 'Jolo', ['Manila'], true],
      [3, D48, 'Semarang', ['Makassar'], false],
      [3, SAKAGUCHI, 'Semarang', ['Makassar'], false],
      [3, R230, 'Bandoeng', ['Hong Kong', 'Palembang'], false],
      [3, D2, 'Batavia', ['Palembang'], false],
      [3, GUARDS, 'Medan', ['Singapore'], false],
      // (Timor, the last of the list so that the others keep their places: the 228th goes on from Ambon.)
      [2, R228, 'Koepang', ['Ambon'], false],
      // (The Kawaguchi Detachment on the record's dates, after the list's others: the Indies' record of 21 November 2026.)
      [0, KAWAGUCHI, 'Miri', [], false],
      [0, KAWAGUCHI, 'Kuching', [], false],
      [1, KAWAGUCHI, 'Jesselton', [], false],
      [1, KAWAGUCHI, 'Sandakan', [], false],
      [1, KAWAGUCHI, 'Singkawang', [], false],
      [1, KAWAGUCHI, 'Pontianak', [], false],
      [4, KAWAGUCHI, 'Cebu', [], false],
      [4, KAWAGUCHI, 'Del Monte', [], false],
    ]);
    expect(front('southseas').then!.map((f) => [f.month, f.corps, f.to[0], f.after])).toEqual([[1, SOUTH_SEAS, 'Rabaul', ['Guam']]]);
    // (Luzon's one follow-on is a move: the 65th Brigade to the beach, the month after the blow. The director's order for Luzon, (3).)
    expect(front('luzon').then!.map((f) => [f.month, f.corps, f.to, f.after ?? [], f.stage ?? false])).toEqual([[1, B65, ['Lingayen'], [], true]]);
    // The 48th takes its tank regiment; a row with several follow-ons has them in the order it makes them.
    // (The 48th goes whole: its 2nd Formosa Infantry, the Tanaka and Kanno Detachments' row, sails with it as its tanks do.)
    expect(front('indies').then!.filter((f) => f.corps === D48).map((f) => f.with)).toEqual([[T4, TANAKA], [T4, TANAKA]]);
  });

  it('there are none before the plan goes, where the book dates the rows, or for a player: nothing is read and nothing is held', () => {
    const before = war(3, 27, 0);
    expect(planFronts(before, 'JAP')).toBeNull();
    expect(frontsTick(before, 'JAP')).toBeNull();
    expect(frontsState(before, 'JAP')).toEqual([]);
    const gone = war(3, 29, 28);
    expect(planFronts(gone, 'JAP')!.at).toBe(28);
    expect(planFronts(gone, 'JAP')!.fronts).toHaveLength(7);
    expect(planFronts(gone, 'GER')).toBeNull();
    const book = war(1, 29, 28);
    const c = put(book, D5, 'Kota Bharu');
    expect(planFronts(book, 'JAP')).toBeNull();
    expect(frontsTick(book, 'JAP')).toBeNull();
    expect(onFront(c)).toBe(false);
    expect(offFront(book, c, book.byName.Singora)).toBe(false);
    const his = war(3, 29, 28, 'JAP');
    expect(planFronts(his, 'JAP')).toBeNull();
    // The quay the plan keeps a row on from the day it stands: where no book dates it, and never a player's.
    for (const [g, held] of [[war(3, 20, 0), true], [war(1, 20, 0), false], [war(3, 20, 0, 'JAP'), false]] as [Game, boolean][]) {
      const d2 = put(g, D2, 'Sendai');
      plansHold(g, 'JAP');
      expect(d2.hold).toBe(held);
    }
  });

  it("a front has its rows while its place stands, and a front whose rows are still to come has not lost its force", () => {
    const g = war(3, 29, 28);
    const d5 = put(g, D5, 'Kota Bharu');
    const d38 = put(g, R229, 'Canton', { stay: true });
    frontsTick(g, 'JAP');
    expect(frontsState(g, 'JAP').map((f) => [f.id, f.live])).toEqual([
      ['malaya', true], ['luzon', true], ['burma', true], ['hongkong', true], ['indies', true], ['southseas', true], ['wake', true],
    ]);
    expect(state(g, 'malaya').rows).toEqual([D5]);
    expect(state(g, 'burma').rows).toEqual([]);
    expect(onFront(d5) && onFront(d38)).toBe(true);
    // The army comes, stands, and is lost: the front's force is gone and the front is over.
    const a15 = put(g, A15, 'Bangkok');
    frontsTick(g, 'JAP');
    expect(state(g, 'burma').rows).toEqual([A15]);
    a15.dead = true;
    g.turn = 30;
    frontsTick(g, 'JAP');
    expect([state(g, 'burma').live, state(g, 'burma').over]).toEqual([false, 30]);
  });

  it("is over for good once its place is Japan's: its rows are their staff's again, and its follow-ons are ended", () => {
    const g = war(3, 29, 28);
    const d16 = put(g, D16, 'Legaspi', { hold: true });
    const ss = put(g, SOUTH_SEAS, 'Guam', { hold: true });
    frontsTick(g, 'JAP');
    expect(onFront(d16) && onFront(ss)).toBe(true);
    japans(g, 'Manila');
    japans(g, 'Rabaul');
    g.turn = 30;
    frontsTick(g, 'JAP');
    expect([state(g, 'luzon').live, state(g, 'luzon').over, state(g, 'southseas').over]).toEqual([false, 30, 30]);
    expect(onFront(d16) || onFront(ss)).toBe(false);
    expect(d16.hold || ss.hold).toBe(false);
    expect(g.flags[followKey('JAP', 5, 0)]).toBe(-30);
    // Lost again, it is not a front again.
    g.byName.Manila.ctrl = 'USA';
    g.turn = 31;
    frontsTick(g, 'JAP');
    expect([state(g, 'luzon').live, state(g, 'luzon').over]).toEqual([false, 30]);
  });

  it("a follow-on that waits for its row's own front to fall leaves the row that front's until it does: the 48th is Luzon's while Manila holds", () => {
    const g = war(3, 29, 28);
    const d48 = put(g, D48, 'Lingayen');
    // (The tank regiment is on the chart from the first day: the row the table names is that one.)
    const t4 = g.corps.find((c) => c.nat === 'JAP' && c.name === T4)!;
    const d38 = put(g, R228, 'Tungkun');
    g.flags[walkedKey(d38)] = 28;
    // The month after the blow the 48th is Luzon's; the 228th is Hong Kong's, though its Ambon is dated this month.
    frontsTick(g, 'JAP');
    expect(state(g, 'luzon').rows).toEqual([D48, T4]);
    expect(state(g, 'hongkong').rows).toEqual([R228]);
    expect(planWaits(d48) || planWaits(d38)).toBe(false);
    // Two months after it, the month its sailing for Jolo is dated, the 48th and its tanks are Luzon's still: Manila
    // holds, and the sailing waits for it (the director's order for Luzon, (2), of 21 November 2026).
    g.turn = 30;
    frontsTick(g, 'JAP');
    expect(g.byName.Manila.ctrl).toBe('USA');
    expect(state(g, 'luzon').rows).toEqual([D48, T4]);
    expect(state(g, 'indies').waiting).toEqual([]);
    expect(planWaits(d48) || planWaits(t4)).toBe(false);
    expect(state(g, 'hongkong').rows).toEqual([R228]);
    // Hong Kong falls: the 228th is the Indies' for its two landings, Ambon and Timor.
    japans(g, 'Hong Kong');
    g.turn = 31;
    frontsTick(g, 'JAP');
    expect(state(g, 'hongkong').live).toBe(false);
    expect(state(g, 'indies').waiting).toEqual([R228]);
    // Manila falls: Luzon's front is over, and the 48th and its tanks are the Indies', for Jolo and then Java.
    japans(g, 'Manila');
    g.turn = 32;
    frontsTick(g, 'JAP');
    expect(state(g, 'luzon').live).toBe(false);
    expect(state(g, 'indies').waiting).toEqual([D48, T4, R228]);
    expect(planWaits(d48) && planWaits(t4)).toBe(true);
    // A row lost: its follow-ons are ended, and the rest go on.
    d38.dead = true;
    g.turn = 33;
    frontsTick(g, 'JAP');
    const T = front('indies').then!;
    expect(T.map((f, i) => (f.corps === R228 ? g.flags[followKey('JAP', 4, i)] : undefined)).filter((x) => x !== undefined)).toEqual([-33, -33]);
    expect(state(g, 'indies').waiting).toEqual([D48, T4]);
  });

  // (The design director's order for Luzon, (3), of 21 November 2026. Luzon's trace of that day, seed 19: its dated row
  // put the brigade down on Formosa, the beach being America's on its date, and it stood there to month 37: on the
  // front's list, so no staff's to ship, and under no order of the plan's. The record's brigade landed at Lingayen on
  // 1 January 1942, ten days after the landing: 6,651 men, a garrison brigade of older reservists.)
  it("the 65th Brigade follows the Luzon landing: it comes to Lingayen once the beach is Japan's and not before, and is the front's in the field", () => {
    const g = war(3, 29, 28);
    const b = put(g, B65, 'Formosa', { str: 0.44 });
    expect(raisedQuality(g, b)).toBe(0.65);
    expect((farWorld as any)['setup-1939'].expeditions.find((r: any[]) => r[2] === B65)).toMatchObject([29, 'JAP', B65, 'inf', 1, 'Lingayen', { str: 0.44, est: 0.44 }]);
    // The beach is still America's: it waits on Formosa as the plan's, and nothing sails.
    expect(g.byName.Lingayen.ctrl).toBe('USA');
    listen();
    runAI(g, 'JAP');
    expect(g.byName.Lingayen.ctrl).toBe('USA');
    expect(told.filter((t) => t.force.includes(B65))).toEqual([]);
    expect([where(g, b), b.hold, g.flags[followKey('JAP', 1, 0)]]).toEqual(['Formosa', true, undefined]);
    // The beach taken: it comes by sea that month.
    japans(g, 'Lingayen');
    g.turn = 30;
    told.length = 0;
    runAI(g, 'JAP');
    expect(told.filter((t) => t.force.includes(B65)).map((t) => [t.what, t.front, t.from, t.to])).toEqual([['stage', 'luzon', 'Formosa', 'Lingayen']]);
    expect([where(g, b), g.flags[followKey('JAP', 1, 0)]]).toEqual(['Lingayen', 30]);
    // Ashore it is Luzon's row, and the plan has it wait for nothing more: the staff's offensive there lets it go in.
    g.turn = 31;
    runAI(g, 'JAP');
    frontsTick(g, 'JAP');
    expect([onFront(b), planWaits(b), b.hold]).toEqual([true, false, false]);
    expect(['Lingayen', 'Manila', 'Bataan']).toContain(where(g, b));
    frontsClear();
  });

  it('a row on a live front goes no farther from its place than it stands; the plan\'s own orders are not bound', () => {
    const g = war(3, 29, 28);
    const d5 = put(g, D5, 'Kota Bharu');
    const sasebo = put(g, SASEBO, 'Davao');
    const other = put(g, '21st Division', 'Kota Bharu');
    const far = landMarches(g, g.byName.Singapore.id);
    expect([far[g.byName.Singora.id], far[g.byName['Kota Bharu'].id], far[g.byName.Kuantan.id], far[g.byName.Johore.id]]).toEqual([4, 3, 2, 1]);
    // (Before the month is read nobody is bound.)
    expect(offFront(g, d5, g.byName.Singora)).toBe(false);
    frontsTick(g, 'JAP');
    expect(offFront(g, d5, g.byName.Singora)).toBe(true);
    expect(offFront(g, d5, g.byName.Penang)).toBe(true);
    expect(offFront(g, d5, g.byName.Ipoh)).toBe(false);
    expect(offFront(g, d5, g.byName.Kuantan)).toBe(false);
    expect(byThePlan(() => offFront(g, d5, g.byName.Singora))).toBe(false);
    // A row of the staff's own beside it is free; so is a front's row that no road leads from to its place.
    expect(onFront(other)).toBe(false);
    expect(offFront(g, other, g.byName.Singora)).toBe(false);
    expect(far[g.byName.Davao.id]).toBe(-1);
    expect(landMarches(g, g.byName.Batavia.id)[g.byName.Davao.id]).toBe(-1);
    expect(onFront(sasebo)).toBe(true);
    expect(offFront(g, sasebo, g.byName.Davao)).toBe(false);
    frontsClear();
    expect(offFront(g, d5, g.byName.Singora)).toBe(false);
  });

  it("the plan walks Hong Kong's rows to Tungkun the month it goes and takes up the division held for the book (rider b423)", () => {
    const g = war(3, 28, 28);
    const d38 = put(g, R229, 'Canton', { stay: true });
    const guns = put(g, GUNS, 'Canton', { hold: true, kind: 'art', str: 0.4 });
    expect([g.byName.Canton.ctrl, g.byName.Tungkun.ctrl]).toEqual(['JAP', 'JAP']);
    listen();
    runAI(g, 'JAP');
    expect(told.filter((t) => t.what === 'walk').map((t) => [t.front, t.force[0], t.from, t.to])).toEqual([
      ['hongkong', R229, 'Canton', 'Tungkun'], ['hongkong', GUNS, 'Canton', 'Tungkun'],
    ]);
    expect([g.flags[walkedKey(d38)], g.flags[walkedKey(guns)]]).toEqual([28, 28]);
    expect(d38.hold || guns.hold).toBe(false);
    // In the field they are Hong Kong's: at Tungkun or on the island, and nowhere behind it.
    for (const c of [d38, guns]) expect(['Tungkun', 'Hong Kong']).toContain(where(g, c));
  });

  it('makes two landings in a month where the table gives both that month, and the next the month after: Menado, Kendari, then Makassar', () => {
    const g = war(3, 29, 28);
    japans(g, 'Davao');
    for (const nm of ['Menado', 'Kendari', 'Makassar']) open(g, nm);
    const sasebo = put(g, SASEBO, 'Davao', { str: 0.2 });
    listen();
    runAI(g, 'JAP');
    const lands = (): (string | boolean | undefined)[][] => told.filter((t) => t.what === 'land').map((t) => [t.force[0], t.from, t.to, t.taken]);
    expect(lands()).toEqual([[SASEBO, 'Davao', 'Menado', true], [SASEBO, 'Menado', 'Kendari', true]]);
    expect([g.byName.Menado.ctrl, g.byName.Kendari.ctrl, g.byName.Makassar.ctrl === 'JAP']).toEqual(['JAP', 'JAP', false]);
    expect([g.flags[followKey('JAP', 4, 2)], g.flags[followKey('JAP', 4, 3)], g.flags[followKey('JAP', 4, 6)]]).toEqual([29, 29, undefined]);
    // Ashore at Kendari it waits there as the plan's for Makassar, and no staff takes it.
    expect([where(g, sasebo), sasebo.hold]).toEqual(['Kendari', true]);
    frontsTick(g, 'JAP');
    expect(planWaits(sasebo)).toBe(true);
    frontsClear();
  });

  it("a follow-on waits as the plan's until the hop before it is Japan's, and is not freed", () => {
    const g = war(3, 29, 28);
    japans(g, 'Davao');
    open(g, 'Tarakan');
    const sak = put(g, SAKAGUCHI, 'Davao', { str: 0.25 });
    // (Its own landing of the blow, at Davao, is made: a follow-on comes after it.)
    g.flags[sailedKey(sak)] = 28;
    expect(g.byName.Jolo.ctrl).not.toBe('JAP');
    listen();
    runAI(g, 'JAP');
    expect(told.filter((t) => t.force[0] === SAKAGUCHI).map((t) => [t.what, t.to, t.why])).toEqual([['wait', 'Tarakan', 'the hop before, Jolo']]);
    expect([where(g, sak), sak.hold, g.byName.Tarakan.ctrl === 'JAP']).toEqual(['Davao', true, false]);
    expect(g.flags[followKey('JAP', 4, 0)]).toBeUndefined();
    // A hop that was never made is never marked.
    expect(g.flags[hopKey('JAP', 'Jolo')]).toBeUndefined();
    expect(hopMade(g, 'JAP', 'Jolo')).toBe(false);
  });

  // (Ruling 234, note 22:05Z of 20 November 2026. As built the hop before read who held the place today: in 17 wars
  // of 24 Hong Kong was taken and lost again, and the 38th Division's row waited on it for the rest of the war.)
  it('the hop before is met once it is made: a place lost again later holds no row behind it', () => {
    const g = war(3, 29, 28);
    japans(g, 'Davao');
    japans(g, 'Jolo');
    open(g, 'Tarakan');
    const sak = put(g, SAKAGUCHI, 'Davao', { str: 0.25 });
    g.flags[sailedKey(sak)] = 28;
    // The staff's month reads Jolo as made, as it opens and as it closes...
    sak.hold = true;
    listen();
    runAI(g, 'JAP');
    expect(g.flags[hopKey('JAP', 'Jolo')]).toBe(29);
    const first = told.filter((t) => t.force[0] === SAKAGUCHI).map((t) => [t.what, t.to, t.why]);
    expect(first.some(([, , why]) => /the hop before/.test(String(why)))).toBe(false);
    // ...and with Jolo lost again the hop is still made: the row is not put back to wait on it.
    const h = war(3, 30, 28);
    japans(h, 'Davao');
    open(h, 'Tarakan');
    const row = put(h, SAKAGUCHI, 'Davao', { str: 0.25 });
    h.flags[sailedKey(row)] = 28;
    h.flags[hopKey('JAP', 'Jolo')] = 29;
    expect(h.byName.Jolo.ctrl).not.toBe('JAP');
    expect(hopMade(h, 'JAP', 'Jolo')).toBe(true);
    listen();
    runAI(h, 'JAP');
    const after = told.filter((t) => t.force[0] === SAKAGUCHI).map((t) => [t.what, t.to, t.why]);
    expect(after.some(([, , why]) => /the hop before/.test(String(why)))).toBe(false);
    // (What it does next is the landing rules' to say; with the hop unmade it would have waited on it: the test above.)
    expect(after).toEqual(first);
  });
  // (The design director's ruling of 23:46Z, 20 November 2026, on Hong Kong's trace, 1. In 24 wars on the regiments'
  // rows Hong Kong fell in 13, where the division's one row had taken it in 22: the 230th, the smallest foot row at
  // Canton, was Canton's town row from its second month, the 21st Army's row was free and marched away, and the blow
  // took the regiment out a day late and left Canton empty.)
  it("a row a plan has spoken for is never taken as a town's garrison, before its front is live or after: Canton's own row keeps the town", () => {
    const A21 = '21st Army (Canton)';
    const inTown = (g: Game): string[] => garrisonOf(g.byName.Canton).map((c) => c.name);
    const enemyBeside = (g: Game): void => { const w = g.byName.Waichow; w.ctrl = 'CHN'; w.units = w.units.filter((u) => u.nat !== 'JAP'); addCorps(g, 'CHN', 'a test army', 'inf', 1, 'Waichow', { xp: 0.1 }); };
    // Before the plan goes: the three regiments stand at Canton, held for it, each smaller than the 21st Army's row.
    const g = war(3, 3, 0);
    enemyBeside(g);
    const rows = [R228, R229, R230].map((nm, i) => put(g, nm, 'Canton', { stay: true, str: i === 2 ? 0.27 : 0.3 }));
    const own = g.corps.find((c) => c.nat === 'JAP' && c.name === A21)!;
    expect([where(g, own), own.town, state(g, 'hongkong')]).toEqual(['Canton', undefined, undefined]);
    garrisons(g, 'JAP', true);
    expect(inTown(g)).toEqual([A21]);
    for (const c of rows) expect(c.town).toBeUndefined();
    // (A free row of the staff's own, smaller still, is the town's as the rule has it: the smallest foot row first.)
    const h = war(3, 3, 0);
    enemyBeside(h);
    for (const nm of [R228, R229, R230]) put(h, nm, 'Canton', { stay: true, str: 0.3 });
    const small = put(h, 'a test brigade', 'Canton', { str: 0.2 });
    garrisons(h, 'JAP', true);
    expect(inTown(h)).toEqual([small.name]);
    // The blow's month: the regiments and the guns walk for Hong Kong together, and Canton's town row stays where it is.
    const w = war(3, 28, 28);
    enemyBeside(w);
    const regs = [R229, R228, R230].map((nm, i) => put(w, nm, 'Canton', { stay: true, str: i === 2 ? 0.27 : 0.3 }));
    const guns = put(w, GUNS, 'Canton', { hold: true, kind: 'art', str: 0.4 });
    // (Each at its record's size, as the data lays them: a row at its full is not folded into its neighbour.)
    for (const c of [...regs, guns]) c.full = c.str;
    expect(garrison(w, w.corps.find((c) => c.nat === 'JAP' && c.name === A21)!)).toBe(true);
    runAI(w, 'JAP');
    expect(inTown(w)).toEqual([A21]);
    expect(w.byName.Canton.ctrl).toBe('JAP');
    expect([...regs, guns].map((c) => w.flags[walkedKey(c)])).toEqual([28, 28, 28, 28]);
    for (const c of [...regs, guns]) expect(['Tungkun', 'Hong Kong']).toContain(where(w, c));
  });
  // (The design director's ruling of 23:46Z, 20 November 2026, on Hong Kong's trace, 2. As built the move walked "once
  // the place is its side's": in wars 25, 35 and 38 of 24 China held Tungkun, empty, from before the blow, and Hong
  // Kong's rows waited at Canton to the end. The same fault as "the hop before": a precondition read from today's holder.)
  it("a move by land does not wait for its staging ground to be its side's: empty enemy ground is marched into, and ground an enemy stands on is the front's to fight for", () => {
    const rows = (g: Game): Corps[] => {
      const cs = [...[R229, R228, R230].map((nm, i) => put(g, nm, 'Canton', { stay: true, str: i === 2 ? 0.27 : 0.3 })), put(g, GUNS, 'Canton', { hold: true, kind: 'art', str: 0.4 })];
      for (const c of cs) c.full = c.str;
      return cs;
    };
    const chinas = (g: Game, name: string, army = 0): void => {
      const r = g.byName[name];
      r.ctrl = 'CHN';
      r.units = [];
      if (army) addCorps(g, 'CHN', 'a test army', 'inf', army, name, { xp: 0.1 });
    };
    // Tungkun in China's hands with nobody on it: the rows march in, the foot first and the guns behind, and are Hong Kong's in the field.
    const g = war(3, 29, 28);
    chinas(g, 'Tungkun');
    const cs = rows(g);
    listen();
    runAI(g, 'JAP');
    expect(told.filter((t) => t.what === 'walk').map((t) => [t.front, t.force[0], t.from, t.to])).toEqual([
      ['hongkong', R229, 'Canton', 'Tungkun'], ['hongkong', R228, 'Canton', 'Tungkun'], ['hongkong', R230, 'Canton', 'Tungkun'], ['hongkong', GUNS, 'Canton', 'Tungkun'],
    ]);
    expect(g.byName.Tungkun.ctrl).toBe('JAP');
    expect(cs.map((c) => g.flags[walkedKey(c)])).toEqual([29, 29, 29, 29]);
    expect(cs.some((c) => c.hold)).toBe(false);
    for (const c of cs) expect(['Tungkun', 'Hong Kong']).toContain(where(g, c));
    // With China's army standing on Tungkun the road waits, and the rows are in the field for that ground and no other.
    const h = war(3, 29, 28);
    chinas(h, 'Tungkun', 3);
    const hs = rows(h);
    frontsTick(h, 'JAP');
    expect(state(h, 'hongkong').waiting).toEqual([]);
    for (const c of hs) {
      expect([onFront(c), planWaits(c)]).toEqual([true, false]);
      expect([offFront(h, c, h.byName.Tungkun), offFront(h, c, h.byName.Waichow), offFront(h, c, h.byName['Hong Kong'])]).toEqual([false, true, true]);
    }
    frontsClear();
    runAI(h, 'JAP');
    expect(hs.some((c) => c.hold)).toBe(false);
    expect(hs.map((c) => h.flags[walkedKey(c)])).toEqual([undefined, undefined, undefined, undefined]);
    for (const c of hs) if (!c.dead) expect(where(h, c)).toBe('Canton');
    // The enemy gone from it, the road is open again the next month: the rows are held for the walk and make it.
    // The enemy gone from it, the road is open again: rows that stood free for the fight are the plan's for the walk, and make it.
    const k = war(3, 30, 28);
    chinas(k, 'Tungkun', 3);
    const ks = rows(k);
    frontsTick(k, 'JAP');
    expect(ks.map((c) => planWaits(c))).toEqual([false, false, false, false]);
    frontsClear();
    for (const c of ks) c.hold = false;
    k.byName.Tungkun.units = [];
    runAI(k, 'JAP');
    expect(ks.map((c) => k.flags[walkedKey(c)])).toEqual([30, 30, 30, 30]);
    expect(k.byName.Tungkun.ctrl).toBe('JAP');
  });
});
