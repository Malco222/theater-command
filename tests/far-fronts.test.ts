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
import { frontWatch, runAI } from '../src/campaign/ai';
import { addCorps, createGame, declareWar } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { byThePlan, followKey, frontsClear, frontsState, frontsTick, landMarches, offFront, onFront, planFronts, planWaits, plansHold, sailedKey, walkedKey } from '../src/campaign/planfronts';
import { ops, startOpen } from '../src/campaign/staff';
import farWorld from '../data/campaign/far-world.json';
import type { Corps, Game } from '../src/campaign/types';

const D5 = '25th Army (5th Division)';
const D18 = '25th Army (18th Division)';
const GUARDS = '25th Army (Imperial Guards Division)';
const T14 = '14th Tank Regiment';
const D48 = '14th Army (48th Division)';
const T4 = '4th Tank Regiment';
const D16 = '14th Army (16th Division)';
const A15 = '15th Army (33rd and 55th Divisions)';
const D38 = '38th Division (Canton)';
const GUNS = '23rd Army artillery (1st Artillery Group)';
const SAKAGUCHI = 'Sakaguchi Detachment (56th Mixed Infantry Group)';
const KURE = 'Naval landing forces (Kure 1st and 2nd)';
const SASEBO = 'Naval landing forces (Sasebo 1st and 2nd, Yokosuka 1st)';
const D2 = '16th Army (2nd Division)';
const SOUTH_SEAS = 'South Seas Detachment';

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
      ['malaya', 'Singapore', 7], ['luzon', 'Manila', 6], ['burma', 'Mergui', 1], ['hongkong', 'Hong Kong', 2],
      ['indies', 'Batavia', 5], ['southseas', 'Rabaul', 1], ['wake', 'Wake', 1],
    ]);
    // By road: the Guards with their tanks to Singora, and Hong Kong's rows to Tungkun (the book's own step).
    expect(SOUTH().fronts!.flatMap((F) => (F.land ?? []).map((m) => [F.id, m.corps, m.with, m.to]))).toEqual([
      ['malaya', GUARDS, [T14], 'Singora'], ['hongkong', D38, [GUNS], 'Tungkun'],
    ]);
    expect(front('indies').held).toEqual([D2]);
    // Every row and every place the table names is the world's.
    const g = war(3, 28, 0);
    const rows = new Set<string>([...(farWorld as any)['setup-1939'].expeditions.map((r: any[]) => r[2]), ...g.corps.map((c) => c.name)]);
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
      [1, D38, 'Ambon', ['Hong Kong', 'Kendari'], false],
      [2, SASEBO, 'Makassar', ['Kendari'], false],
      [2, D38, 'Palembang', ['Hong Kong', 'Johore'], false],
      [2, D48, 'Jolo', [], true],
      [3, D48, 'Semarang', ['Makassar'], false],
      [3, SAKAGUCHI, 'Semarang', ['Makassar'], false],
      [3, D38, 'Bandoeng', ['Hong Kong', 'Palembang'], false],
      [3, D2, 'Batavia', ['Palembang'], false],
      [3, GUARDS, 'Medan', ['Singapore'], false],
    ]);
    expect(front('southseas').then!.map((f) => [f.month, f.corps, f.to[0], f.after])).toEqual([[1, SOUTH_SEAS, 'Rabaul', ['Guam']]]);
    // The 48th takes its tank regiment; a row with several follow-ons has them in the order it makes them.
    expect(front('indies').then!.filter((f) => f.corps === D48).map((f) => f.with)).toEqual([[T4], [T4]]);
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
    const d38 = put(g, D38, 'Canton', { stay: true });
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

  it('a follow-on takes its row by its date whatever the state of its front, unless it waits for that front to fall', () => {
    const g = war(3, 29, 28);
    const d48 = put(g, D48, 'Lingayen');
    // (The tank regiment is on the chart from the first day: the row the table names is that one.)
    const t4 = g.corps.find((c) => c.nat === 'JAP' && c.name === T4)!;
    const d38 = put(g, D38, 'Tungkun');
    g.flags[walkedKey(d38)] = 28;
    // The month after the blow the 48th is Luzon's; the 38th is Hong Kong's, though its Ambon is dated this month.
    frontsTick(g, 'JAP');
    expect(state(g, 'luzon').rows).toEqual([D48, T4]);
    expect(state(g, 'hongkong').rows).toEqual([D38]);
    expect(planWaits(d48) || planWaits(d38)).toBe(false);
    // Two months after it the 48th and its tanks are the Indies', waiting where they stand, though Manila holds.
    g.turn = 30;
    frontsTick(g, 'JAP');
    expect(g.byName.Manila.ctrl).toBe('USA');
    expect(state(g, 'luzon').rows).toEqual([]);
    expect(state(g, 'indies').waiting).toEqual([D48, T4]);
    expect(planWaits(d48) && planWaits(t4)).toBe(true);
    expect(state(g, 'hongkong').rows).toEqual([D38]);
    // Hong Kong falls: the 38th is the Indies' for its three landings.
    japans(g, 'Hong Kong');
    g.turn = 31;
    frontsTick(g, 'JAP');
    expect(state(g, 'hongkong').live).toBe(false);
    expect(state(g, 'indies').waiting).toEqual([D48, T4, D38]);
    // A row lost: its follow-ons are ended, and the rest go on.
    d38.dead = true;
    g.turn = 32;
    frontsTick(g, 'JAP');
    const T = front('indies').then!;
    expect(T.map((f, i) => (f.corps === D38 ? g.flags[followKey('JAP', 4, i)] : undefined)).filter((x) => x !== undefined)).toEqual([-32, -32, -32]);
    expect(state(g, 'indies').waiting).toEqual([D48, T4]);
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
    const d38 = put(g, D38, 'Canton', { stay: true });
    const guns = put(g, GUNS, 'Canton', { hold: true, kind: 'art', str: 0.4 });
    expect([g.byName.Canton.ctrl, g.byName.Tungkun.ctrl]).toEqual(['JAP', 'JAP']);
    listen();
    runAI(g, 'JAP');
    expect(told.filter((t) => t.what === 'walk').map((t) => [t.front, t.force[0], t.from, t.to])).toEqual([
      ['hongkong', D38, 'Canton', 'Tungkun'], ['hongkong', GUNS, 'Canton', 'Tungkun'],
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
  });
});
