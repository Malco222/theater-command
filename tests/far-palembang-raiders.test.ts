// Palembang's whole force: the 2nd Raiding Regiment beside the 229th (the design director's notes of 05:30Z and
// 05:35Z of 21 November 2026 to ruling 234; the historian's far-chart-airfields.md, section 7, and
// palembang-tarakan-record.md). Data only: far-shores.json and the plan in far-pacific-war.json.
//
// As measured (war 22 at stop 3, the airfields laid): the 229th Regiment alone read Palembang 1.46 against the ask
// of 1.5 with Japan's sky, and 1.39 from month 39 on; Palembang never fell. The record's force had about 360
// paratroops on the P1 airfield and the refineries the day before the landing, 0.025. The rules land an airborne
// force by itself or not at all, so the regiment joins the 229th's fight in the same month: the plan's follow-on
// names it, and it rides in the 229th's lift. The states are set by hand; one month of Japan's own is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import { frontWatch, runAI } from '../src/campaign/ai';
import { addCorps, createGame, declareWar, startPhase } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { airborne } from '../src/campaign/navy';
import { frontsClear, frontsState, frontsTick, planWaits } from '../src/campaign/planfronts';
import { ops, startOpen } from '../src/campaign/staff';
import { bookAttacks, bookWatch, riding, stations } from '../src/campaign/stations';
import type { Corps, Game } from '../src/campaign/types';
import setup from '../data/campaign/setup-1939.json';
import farWar from '../data/campaign/far-pacific-war.json';

const RAID = '2nd Raiding Regiment (paratroops of the 1st Raiding Group)';
const R229 = "229th Infantry Regiment (with the 38th Division's headquarters)";
const BOOK229 = '229th Infantry (of the 38th Division)';

const dated = (name: string): any[][] => ((setup as any).expeditions as any[][]).filter((r) => r[2] === name);
const of = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.nat === 'JAP' && c.name === name && !c.dead);
const where = (g: Game, c: Corps): string => g.regions[c.reg].name;
/** A place in Japan's hands, with nobody of the other side in it. */
const japans = (g: Game, ...names: string[]): void => { for (const nm of names) { const r = g.byName[nm]; r.ctrl = 'JAP'; r.units = r.units.filter((u) => u.nat === 'JAP'); } };
/** A place with nobody in its field and no town guard: a landing there walks in. */
const open = (g: Game, name: string): void => { const r = g.byName[name]; r.units = r.units.filter((u) => u.nat === 'JAP'); g.flags[`guard:${r.id}`] = 100; };
/** A war at stop 3 in month `turn`, the strike south gone in month 28, Hong Kong and Johore taken, Indochina Japan's. */
const war = (turn: number): Game => {
  const g = startOpen(createGame(3, null));
  g.turn = turn;
  g.flags['st:done:JAP:south'] = 28;
  for (const e of ['USA', 'UK', 'NED']) declareWar(g, 'JAP', e);
  japans(g, 'Hong Kong', 'Johore', 'Cam Ranh', 'Phnom Penh', 'Saigon');
  return g;
};
const put = (g: Game, name: string, place: string, str: number, o: any = {}): Corps => {
  const c = addCorps(g, 'JAP', name, 'inf', 1, place, { xp: 0.3, ...o });
  c.str = str;
  return c;
};

type Told = { what: string; force: string[]; to: string; from: string };
const told: Told[] = [];
const listen = (): void => { told.length = 0; frontWatch.tell = (e) => { told.push({ what: e.what, force: e.force.map((c) => c.name), to: e.to.name, from: e.from.name }); }; };
afterEach(() => { frontWatch.tell = null; frontsClear(); });

describe("the data: Palembang's paratroops", () => {
  it('the row: 0.03 at Phnom Penh a month after the war with Britain begins, airborne troops, at every stop of the dial', () => {
    // (At every stop, the director's 16:18Z of 21 November 2026: the record's rows belong wherever their landing is made from rows.)
    expect(dated(RAID)).toEqual([[29, 'JAP', RAID, 'inf', 1, 'Phnom Penh', { xp: 0.3, str: 0.03, est: 0.03, airborne: 1, war: ['JAP', 'UK'], after: 1 }]]);
    // (About 360 men dropped on 14 February 1942 and about 100 more on the 15th, at 15,000 to a division: the Indies'
    // record of 21 November 2026. The first day's drop alone was 0.025.)
    expect(460 / 15000).toBeCloseTo(0.03, 2);
    // The 229th stays whole at 0.3: the battalion that landed at Muntok is in it, the chart having no Banka.
    expect(dated(R229)[0][6]).toMatchObject({ str: 0.3, est: 0.3 });
  });

  it("the plan: the 229th's follow-on for Palembang names it, after Hong Kong and Johore, and it goes in the 229th's lift", () => {
    const T = ops('JAP').find((op) => op.id === 'south')!.fronts!.find((F) => F.id === 'indies')!.then!;
    const f = T.find((x) => x.corps === R229)!;
    expect([f.month, f.to, f.with, f.after, f.from]).toEqual([2, ['Palembang'], [RAID], ['Hong Kong', 'Johore'], ['Cam Ranh']]);
    const row = ((farWar as any).history.stations.rows as any[]).find((r) => r.corps === RAID);
    // (The day's attachment is the first the table gives, the 229th's row of stops 3 to 5; while the book binds, the
    // last of the month: the book's own row for the regiment.)
    expect(row).toMatchObject({ nat: 'JAP', beside: [[30, R229], [30, BOOK229]], steps: [] });
    expect(stations()!.attacks!.find((x) => x.name === 'Palembang')!.force).toEqual([BOOK229, RAID]);
  });

  it("it comes a month after the strike at stop 3, and at the book's stops as well", () => {
    const g = startOpen(createGame(5, null));
    // (Indochina in Japan's hands, as after the demand: the strike's rows stand on their own quays there.)
    japans(g, 'Phnom Penh', 'Saigon', 'Cam Ranh');
    g.turn = 28;
    declareWar(g, 'JAP', 'UK');
    startPhase(g, 'JAP');
    expect(of(g, RAID)).toBeUndefined();
    g.turn = 29; startPhase(g, 'JAP');
    const c = of(g, RAID)!;
    expect([where(g, c), c.divs, c.str, c.full, !!c.airborne]).toEqual(['Phnom Penh', 1, 0.03, 0.03, true]);
    for (const stop of [1, 2]) {
      const h = startOpen(createGame(5, null));
      setStop(h, stop, true);
      japans(h, 'Phnom Penh', 'Saigon', 'Cam Ranh');
      h.turn = 28;
      declareWar(h, 'JAP', 'UK');
      for (const t of [28, 29]) { h.turn = t; startPhase(h, 'JAP'); }
      const r = of(h, RAID)!;
      expect([where(h, r), r.str, !!r.airborne], `stop ${stop}`).toEqual(['Phnom Penh', 0.03, true]);
    }
  });

  it("by the book it sails in the lift of the book's own 229th and stands in Palembang after; the record's fight is as it was", () => {
    const fight = (withRaid: boolean): { taken: boolean; a: number; d: number; at: string[] } => {
      const g = startOpen(createGame(3, null));
      setStop(g, 1, true);
      g.turn = 30;
      g.flags['st:done:JAP:south'] = 28;
      for (const e of ['USA', 'UK', 'NED']) declareWar(g, 'JAP', e);
      stations()!.attacks!.forEach((x, i) => { if (x.name !== 'Palembang') g.flags[`ba:${i}`] = 1; });
      japans(g, 'Cam Ranh', 'Phnom Penh', 'Saigon');
      const lead = put(g, BOOK229, 'Cam Ranh', 0.25);
      const raid = withRaid ? put(g, RAID, 'Phnom Penh', 0.03, { airborne: 1 }) : null;
      let rep: any = null, taken = false;
      bookWatch.tell = (e) => { if (e.what === 'made') { rep = e.report; taken = !!e.taken; } };
      bookAttacks(g, 'JAP');
      bookWatch.tell = null;
      return { taken, a: +(rep?.aPower ?? 0).toFixed(6), d: +(rep?.dPower ?? 0).toFixed(6), at: [where(g, lead), raid ? where(g, raid) : ''] };
    };
    const without = fight(false), beside = fight(true);
    expect(without.a).toBeGreaterThan(0);
    // Japan's side comes to the record's 0.25 with the paratroops aboard or not: the met figures are untouched.
    expect([beside.a, beside.d, beside.taken]).toEqual([without.a, without.d, without.taken]);
    // They went where the regiment went.
    expect(beside.at[1]).toBe(beside.at[0]);
  });
});

describe("the plan takes the paratroops to Palembang with the 229th", () => {
  it("it is the Indies' front's row, waiting as the plan's for its follow-on", () => {
    const g = war(30);
    put(g, R229, 'Hong Kong', 0.3);
    const raid = put(g, RAID, 'Phnom Penh', 0.025, { airborne: 1 });
    frontsTick(g, 'JAP');
    expect(frontsState(g, 'JAP').find((f) => f.id === 'indies')!.waiting).toEqual([R229, RAID]);
    expect(planWaits(raid)).toBe(true);
    // With the 229th lost its follow-on is ended, and the paratroops are their staff's own.
    of(g, R229)!.dead = true;
    g.turn = 31;
    frontsTick(g, 'JAP');
    expect(planWaits(raid)).toBe(false);
  });

  it('standing apart from the 229th it comes to its quay by sea', () => {
    const g = war(30);
    const lead = put(g, R229, 'Cam Ranh', 0.3);
    const raid = put(g, RAID, 'Phnom Penh', 0.025, { airborne: 1 });
    open(g, 'Palembang');
    listen();
    runAI(g, 'JAP');
    expect(told.filter((t) => t.force.includes(RAID) && t.what === 'stage').map((t) => [t.from, t.to])).toEqual([['Phnom Penh', 'Cam Ranh']]);
    expect([where(g, raid), where(g, lead)]).toEqual(['Cam Ranh', 'Cam Ranh']);
  });

  // (The director's check of 16:36Z of 21 November 2026: at stops 3 to 5 a rider sails only with its lead's landing,
  // never on its date alone.)
  it("its date come and the 229th still before Hong Kong, it makes no landing of its own: Palembang is not landed on, and it waits as the plan's", () => {
    const g = war(30);
    g.byName['Hong Kong'].ctrl = 'UK';
    put(g, R229, 'Canton', 0.3);
    const raid = put(g, RAID, 'Cam Ranh', 0.025, { airborne: 1 });
    open(g, 'Palembang');
    listen();
    runAI(g, 'JAP');
    expect(told.filter((t) => t.to === 'Palembang' && t.what === 'land')).toEqual([]);
    expect(g.byName.Palembang.ctrl).not.toBe('JAP');
    expect([where(g, raid), !!raid.hold]).toEqual(['Cam Ranh', true]);
  });

  it("on the 229th's quay it lands in its wave, in its lift, as a force from the sea and not from the air", () => {
    const g = war(31);
    const lead = put(g, R229, 'Cam Ranh', 0.3);
    const raid = put(g, RAID, 'Cam Ranh', 0.025, { airborne: 1 });
    open(g, 'Palembang');
    frontsTick(g, 'JAP');
    lead.hold = raid.hold = true;
    expect(airborne([lead, raid])).toBe(false);
    expect(riding(g, [lead, raid])).toEqual([raid]);
    frontsClear();
    listen();
    runAI(g, 'JAP');
    expect(told.filter((t) => t.to === 'Palembang' && t.what === 'land').map((t) => t.force)).toEqual([[R229, RAID]]);
    expect(g.byName['Palembang'].ctrl).toBe('JAP');
    expect([where(g, lead), where(g, raid)]).toEqual(['Palembang', 'Palembang']);
  });
});
