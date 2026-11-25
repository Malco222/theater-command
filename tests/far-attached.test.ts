// Japan's armour roster at every stop of the dial, and a row that lands in the lift of the corps it goes with, with
// the far side's switch on (riders b591, b593 and b604 to ruling 234).
//
// The rows are facts of the day and belong to every stop: six tank regiments on the first day, three more in July
// 1941, the south's five on their quays in November 1941, the 2nd in Malaya the month after the war begins, the tank
// divisions made up out of their regiments in June and December 1942. The roster counts no tank twice.
// The rule: a row the stations table gives to a corps ('beside', src/campaign/stations.ts) lands in that corps' lift
// the month it lands, for any country, and is its staff's own after. The attachment of the day is the first the
// table gives, and holds while the row is still held for it; later re-attachments are the book's, at the stops that
// keep it. With the far side's switch off there is no table and nothing here happens (tests/pacific-war-off.test.ts).
// The states are set by hand on a made-up December 1941 (far-ready.test.ts); one month of Japan's own is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { bookMonth } from '../src/campaign/claims';
import { monthlyDiplomacy } from '../src/campaign/diplomacy';
import { addCorps, atWar, capitulate, createGame, declareWar, hostileUnits, removeCorps, startPhase } from '../src/campaign/game';
import { release } from '../src/campaign/ai';
import { raisedQuality, setStop } from '../src/campaign/history';
import { landCorps, liftLeft, navalUpkeep, whyNotLand } from '../src/campaign/navy';
import { readinessMonth } from '../src/campaign/readiness';
import { ops, startOpen } from '../src/campaign/staff';
import { alongside, goesWith, riding, stations } from '../src/campaign/stations';
import { runTurnAI } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';

const corps = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.nat === 'JAP' && c.name === name && !c.dead);
const at = (g: Game, name: string): string | undefined => { const c = corps(g, name); return c && g.regions[c.reg].name; };
const reg = (n: string): string => `${n} Tank Regiment`;
const G5 = '25th Army (5th Division)', G18 = '25th Army (18th Division)', G48 = '14th Army (48th Division)', GUARDS = '25th Army (Imperial Guards Division)';
const G16 = '14th Army (16th Division)', KIMURA = 'Kimura Detachment (16th Division)', NINTH = '16th Division (9th Infantry)';
const HEAVY = '14th Army artillery (1st and 8th Field Heavy Artillery)', B65 = '14th Army (65th Brigade)';

/**
 * A made-up December 1941 (far-ready.test.ts): the Netherlands and France struck down by hand, the months read from
 * June 1941, Japan's dated rows taken in (July's among them), America's fleet put at Pearl Harbor.
 */
const december = (stop: number, human: string | null = null): Game => {
  const g = startOpen(createGame(17, human));
  if (stop !== 3) setStop(g, stop, true);
  g.turn = 9;
  declareWar(g, 'GER', 'NED');
  capitulate(g, 'NED', 'GER');
  g.turn = 10;
  capitulate(g, 'FRA', 'GER');
  for (const t of [22, 23, 24, 25, 27]) { g.turn = t; bookMonth(g); monthlyDiplomacy(g); if (t === 23 || t === 24 || t === 27) startPhase(g, 'JAP'); }
  for (const op of ops('JAP')) if (op.id !== 'south') g.flags[`st:done:JAP:${op.id}`] = 1;
  const sea = g.seas.findIndex((z) => z.key === 'hawaii');
  for (const f of g.fleets.filter((x) => x.nat === 'USA' && (x.name === 'Pacific Fleet' || x.name === 'Pacific Carrier Fleet'))) {
    f.base = g.byName['Pearl Harbor'].id; f.port = 'Pearl Harbor'; f.home = sea; f.zone = sea; f.station = sea; f.stance = 'port';
  }
  g.turn = 28;
  bookMonth(g);
  monthlyDiplomacy(g);
  navalUpkeep(g, 'USA');
  return g;
};
/**
 * America in the strike south's way. (At stops 3 to 5 the strike declares on those in its way and its landings are cut
 * to their ground, Units decide audit item 45: America, at war with Germany, stands in it, so the Philippine landings
 * sail. Left out by the flank's draw, they would not.)
 */
const inTheWay = (g: Game): Game => { declareWar(g, 'USA', 'GER'); return g; };
/** The same war where history is loose, the freeze six months old and the striking fleet gathered: the staff's word is given this month. */
const ready = (g: Game): Game => { g.flags['pac:freeze'] = g.turn - 2; readinessMonth(g, 'JAP'); g.flags['pac:freeze'] = g.turn - 6; return g; };
/** Japan at war with Britain this month, and Kota Bharu's beach with nobody on it: a landing there is unfought. */
const beach = (g: Game): Region => {
  declareWar(g, 'JAP', 'UK');
  const kb = g.byName['Kota Bharu'];
  for (const u of [...hostileUnits(g, kb, 'JAP')]) removeCorps(g, u);
  return kb;
};
/** So many of Japan's six lifts are spent already this month. */
const spend = (g: Game, n: number): void => { g.flags[`lift:JAP:${g.turn}`] = n; };

describe("Japan's armour roster (riders b591 and b593): the rows, at every stop", () => {
  it('six regiments stand on the first day, a quarter of a division each and whole at that', () => {
    for (const stop of [1, 3]) {
      const g = startOpen(createGame(17, null));
      if (stop !== 3) setStop(g, stop, true);
      const day: Record<string, string> = { '3rd': 'Szepingkai', '4th': 'Szepingkai', '5th': 'Mutankiang', '8th': 'Mukden', '12th': 'Taiyuan', '13th': 'Hankow' };
      for (const [n, place] of Object.entries(day)) {
        const c = corps(g, reg(n))!;
        expect([n, at(g, reg(n)), c.kind, c.str, c.full], `stop ${stop}`).toEqual([n, place, 'arm', 0.25, 0.25]);
        // (Manchuria's are the Kwantung Army's and held; China's two march with the armies there.)
        expect(c.hold, n).toBe(place !== 'Taiyuan' && place !== 'Hankow');
      }
      expect(g.corps.filter((c) => c.nat === 'JAP' && /Tank/.test(c.name)).length).toBe(6);
    }
  });

  it("July 1941 brings three more to Manchuria, and November the south's five to their quays: the 4th is the one that stood at Szepingkai", () => {
    for (const stop of [1, 2, 3]) {
      const g = december(stop);
      expect([at(g, reg('9th')), at(g, reg('10th')), at(g, reg('11th'))], `stop ${stop}`).toEqual(['Mutankiang', 'Szepingkai', 'Szepingkai']);
      expect([at(g, reg('1st')), at(g, reg('6th')), at(g, reg('14th')), at(g, reg('4th')), at(g, reg('7th'))], `stop ${stop}`).toEqual(['Hainan', 'Hainan', 'Phnom Penh', 'Formosa', 'Formosa']);
      // Each beside its division, held for it; the 14th where the Guards stand.
      expect([at(g, G5), at(g, GUARDS), at(g, G48)]).toEqual(['Hainan', 'Phnom Penh', 'Formosa']);
      for (const n of ['1st', '6th', '14th', '4th', '7th']) expect(corps(g, reg(n))!.hold, n).toBe(true);
      expect(corps(g, reg('14th'))).toMatchObject({ str: 0.2, full: 0.2, xp: 0.3 });
      expect(g.corps.filter((c) => c.name === reg('4th') && !c.dead).length).toBe(1);
      // Manchuria holds 1.5 of armour in December 1941, the south 1.2 and China 0.5 (the historian's totals).
      const worth = (places: string[]): number => g.corps.filter((c) => c.nat === 'JAP' && !c.dead && c.kind === 'arm' && places.includes(g.regions[c.reg].name)).reduce((s, c) => s + c.str * c.divs, 0);
      expect(worth(['Szepingkai', 'Mutankiang', 'Mukden'])).toBeCloseTo(1.5, 6);
      expect(worth(['Hainan', 'Phnom Penh', 'Formosa'])).toBeCloseTo(1.2, 6);
      // (No 2nd Regiment before the war: its row waits for it.)
      expect(corps(g, reg('2nd'))).toBeUndefined();
    }
  });

  it('the tank divisions are made up out of their regiments, and no tank is counted twice', () => {
    const g = december(3);
    const kb = beach(g);
    kb.ctrl = 'JAP';
    // The 2nd lands in Malaya the month after the war begins, on ground that is Japan's by then.
    g.turn = 29; startPhase(g, 'JAP');
    expect([at(g, reg('2nd')), corps(g, reg('2nd'))!.hold]).toEqual(['Kota Bharu', false]);
    // June 1942: the 1st and 2nd Tank Divisions at Mutankiang, out of the regiments of Manchuria.
    g.turn = 34; startPhase(g, 'JAP');
    const d1 = corps(g, '1st Tank Division')!, d2 = corps(g, '2nd Tank Division')!;
    // (The director's rulings of 04:01Z and 04:26Z, 22 November 2026: the divisions form in their month at every
    // stop, each short of the regiments lent to the south, whose shares are kept for them. At this stop nothing ends
    // by date alone, and with Singapore and Bataan standing the three stay lent. far-lent-rows.test.ts has the
    // make-up, and the book's months at stops 1 and 2.)
    expect([at(g, d1.name), d1.kind, at(g, d2.name)]).toEqual(['Mutankiang', 'arm', 'Mutankiang']);
    expect(d1.full).toBeCloseTo(0.55, 9); expect(d2.full).toBeCloseTo(0.3, 9);
    expect(d1.str).toBeGreaterThanOrEqual(0.45); expect(d1.str).toBeLessThanOrEqual(0.55 + 1e-9);
    expect(d2.str).toBeGreaterThanOrEqual(0.25); expect(d2.str).toBeLessThanOrEqual(0.3 + 1e-9);
    for (const n of ['3rd', '5th', '9th', '10th', '11th']) expect(corps(g, reg(n)), n).toBeUndefined();
    expect(at(g, reg('8th'))).toBe('Mukden');
    g.turn = 38; startPhase(g, 'JAP');
    for (const n of ['1st', '6th', '7th']) expect(corps(g, reg(n)), n).toBeDefined();
    for (const n of ['2nd', '4th', '14th']) expect(corps(g, reg(n)), n).toBeDefined();
    // December 1942: the 3rd Tank Division at Paotow, out of the Cavalry Group and the regiments of north China.
    expect(corps(g, 'Mongolia Garrison Army (3rd Tank Division)')).toBeUndefined();
    g.turn = 40; startPhase(g, 'JAP');
    const d3 = corps(g, 'Mongolia Garrison Army (3rd Tank Division)')!;
    expect([d3.kind, d3.full]).toEqual(['arm', 0.8]);
    expect(d3.str).toBeGreaterThanOrEqual(0.6); expect(d3.str).toBeLessThan(0.7);
    for (const nm of ['Cavalry Group', reg('8th'), reg('12th'), reg('13th')]) expect(corps(g, nm), nm).toBeUndefined();
    expect(corps(g, 'Mongolia Garrison Army (4th Cavalry Brigade)')).toMatchObject({ kind: 'cav', full: 0.3 });
  });
});

describe('a row lands in the lift of the corps it goes with (rider b604)', () => {
  it('the attachment of the day is the first the table gives, at every stop, while the row is still held for it', () => {
    expect(stations()!.rows.find((r) => r.corps === reg('1st'))!.beside![0]).toEqual([28, G5]);
    for (const stop of [1, 3]) {
      const g = december(stop);
      const lead = (n: string): string | undefined => goesWith(g, corps(g, reg(n))!)?.name;
      expect([lead('1st'), lead('6th'), lead('14th'), lead('4th'), lead('7th')], `stop ${stop}`).toEqual([G5, G5, GUARDS, G48, G48]);
      // A row that goes with nobody, and a division itself.
      expect(goesWith(g, corps(g, reg('8th'))!)).toBeNull();
      expect(goesWith(g, corps(g, G5)!)).toBeNull();
    }
    // Where no book binds, a row let off its hold is its staff's own and goes with nobody.
    const g = december(3);
    corps(g, reg('1st'))!.hold = false;
    expect(goesWith(g, corps(g, reg('1st'))!)).toBeNull();
    // Later re-attachments are the book's: from April 1942 the 1st is the 15th Army's at a stop that keeps it, and nowhere else.
    for (const stop of [1, 3]) {
      const h = december(stop);
      h.flags['st:done:JAP:south'] = 28;
      const a15 = addCorps(h, 'JAP', '15th Army (33rd and 55th Divisions)', 'inf', 2, 'Hainan', {});
      h.turn = 32;
      expect(goesWith(h, corps(h, reg('1st'))!)?.name, `stop ${stop}`).toBe(stop === 1 ? a15.name : G5);
    }
  });

  it('takes none of the shipping where it sails with its corps, and all it would alone', () => {
    const g = december(3);
    const kb = beach(g);
    const d5 = corps(g, G5)!, d18 = corps(g, G18)!, t1 = corps(g, reg('1st'))!, t6 = corps(g, reg('6th'))!;
    expect(riding(g, [d5, t1, t6])).toEqual([t1, t6]);
    expect(riding(g, [d18, t1])).toEqual([]);
    // (Japan's lift is the record's eleven in December 1941, rider b630 to ruling 234: all but one is spent.)
    spend(g, liftLeft(g, 'JAP') - 1);
    // One lift left: the division with both its regiments sails, two divisions do not, and a regiment with another's division is a second corps.
    expect(whyNotLand(g, [d5, t1, t6], kb)).toBeNull();
    expect(whyNotLand(g, [d5, d18], kb)).toMatch(/shipping for only 1 more corps/);
    expect(whyNotLand(g, [d18, t1], kb)).toMatch(/shipping for only 1 more corps/);
    // Its staff's own, it pays its way like any corps.
    t6.hold = false;
    expect(whyNotLand(g, [d5, t1, t6], kb)).toMatch(/shipping for only 1 more corps/);
  });

  it("a computer's staff takes the rows along where they stand with the corps on the quay; ashore they are its own", () => {
    const g = december(3);
    const kb = beach(g);
    const d5 = corps(g, G5)!, t1 = corps(g, reg('1st'))!, t6 = corps(g, reg('6th'))!;
    expect(alongside(g, [d5])).toEqual([t1, t6]);
    // (One that has marched this month does not embark, and waits for nobody.)
    t6.ap = 0;
    expect(alongside(g, [d5])).toEqual([t1]);
    const before = liftLeft(g, 'JAP');
    const res = landCorps(g, [d5], kb);
    expect([res.ok, res.taken]).toEqual([true, true]);
    expect([at(g, G5), at(g, reg('1st')), at(g, reg('6th'))]).toEqual(['Kota Bharu', 'Kota Bharu', 'Hainan']);
    expect(before - liftLeft(g, 'JAP')).toBe(1);
    expect([t1.hold, t6.hold]).toEqual([false, true]);
    expect(goesWith(g, t1)).toBeNull();
  });

  it('held for its corps, it is let go with that corps and not before: an offensive that takes the hold off its quay leaves it beside a division that waits', () => {
    const g = december(3);
    const d5 = corps(g, G5)!, t1 = corps(g, reg('1st'))!, t6 = corps(g, reg('6th'))!;
    const other = addCorps(g, 'JAP', 'Test Brigade', 'inf', 1, 'Formosa', { hold: 1 });
    // (Where history is loose the 5th Division waits held and stayed for the strike south: ruling b423, china-stop3.json.)
    expect([d5.hold, d5.stay, t1.hold, t1.stay]).toEqual([true, true, true, undefined]);
    release(g, 'JAP', ['Hainan', 'Formosa']);
    expect([d5.hold, t1.hold, t6.hold, other.hold]).toEqual([true, true, true, false]);
    // The 48th Division on Formosa has a plain hold: let go there, its regiments go with it, whichever the list reads first.
    expect([corps(g, G48)!.hold, corps(g, reg('4th'))!.hold, corps(g, reg('7th'))!.hold]).toEqual([false, false, false]);
    // And once the division is let go, so are they.
    d5.hold = false;
    release(g, 'JAP', ['Hainan']);
    expect([t1.hold, t6.hold]).toEqual([false, false]);
  });

  it("the player's force is his to choose: nothing sails he did not send, and a row he sends with its corps takes no shipping", () => {
    const g = december(3, 'JAP');
    const kb = beach(g);
    // (A player's rows of the south form up at home, as his other dated rows do, and shipping them is his business:
    // here the four are put on the quay by hand, held as their rows have them.)
    expect(at(g, reg('1st'))).not.toBe('Hainan');
    for (const u of [...g.corps]) if (u.nat === 'JAP' && ([G5, G18, reg('1st'), reg('6th')].includes(u.name) || g.regions[u.reg].name === 'Hainan')) removeCorps(g, u);
    const d5 = addCorps(g, 'JAP', G5, 'inf', 1, 'Hainan', { hold: 1 }), d18 = addCorps(g, 'JAP', G18, 'inf', 1, 'Hainan', { hold: 1 });
    const t1 = addCorps(g, 'JAP', reg('1st'), 'arm', 1, 'Hainan', { hold: 1, est: 0.25 }), t6 = addCorps(g, 'JAP', reg('6th'), 'arm', 1, 'Hainan', { hold: 1, est: 0.25 });
    for (const c of [d5, d18, t1, t6]) { c.ap = c.apMax; c.supplied = true; }
    spend(g, liftLeft(g, 'JAP') - 1);
    expect(whyNotLand(g, [d5, t1], kb)).toBeNull();
    expect(whyNotLand(g, [d5, d18], kb)).toMatch(/shipping for only 1 more corps/);
    const res = landCorps(g, [d5, t1], kb);
    expect([res.ok, res.taken]).toEqual([true, true]);
    expect(liftLeft(g, 'JAP')).toBe(0);
    expect([at(g, G5), at(g, reg('1st')), at(g, reg('6th'))]).toEqual(['Kota Bharu', 'Kota Bharu', 'Hainan']);
    // Landed with its corps, the 1st is his own; the 6th, left on the quay, still goes with the 5th Division.
    expect(goesWith(g, t1)).toBeNull();
    expect(goesWith(g, t6)).toBe(d5);
  });

  it("where history is loose: the month Japan strikes, the 5th Division's regiments land with it, the record's lift of eleven carries all ten landings, and the 14th is let go with the Guards", () => {
    const g = ready(inTheWay(december(3)));
    g.phase = g.order.indexOf('JAP');
    runTurnAI(g);
    expect(g.flags['st:done:JAP:south']).toBe(28);
    // (Siam stands with Japan the day of the blow at stops 1 to 3, riders b591 and b593: Singora is a friendly harbour,
    // the 5th Division is shipped there and its regiments behind it, for no shipping of their own.)
    expect(atWar(g, 'THA', 'UK')).toBe(true);
    expect([at(g, G5), at(g, reg('1st')), at(g, reg('6th'))]).toEqual(['Singora', 'Singora', 'Singora']);
    for (const n of ['1st', '6th', '14th']) expect(corps(g, reg(n))!.hold, n).toBe(false);
    // Japan's lift is the record's eleven from October 1941 (rider b630 to ruling 234), so all ten landings sail in
    // the strike's month: the 48th Division lands at Lingayen, and its regiments land with it for no shipping of their own.
    expect([at(g, G48), at(g, reg('4th')), at(g, reg('7th'))]).toEqual(['Lingayen', 'Lingayen', 'Lingayen']);
    for (const n of ['4th', '7th']) expect(corps(g, reg(n))!.hold, n).toBe(false);
    const south = ops('JAP').find((op) => op.id === 'south')!;
    expect(south.landings!.filter((l) => l.to.includes(at(g, l.corps) ?? '')).length).toBeGreaterThanOrEqual(5);
    // (The plan's own list frees no regiment by name: each goes with its division.)
    expect((south.frees ?? []).some((nm) => /Tank/.test(nm))).toBe(false);
  });

  it("by the book: the 5th Division's regiments cross to Singora behind it in December, and are the book's again from the next month", () => {
    const g = december(1);
    g.phase = g.order.indexOf('JAP');
    runTurnAI(g);
    expect(g.flags['st:done:JAP:south']).toBe(28);
    // (The 5th is carried down the west coast within the month by the record's dated fights: far-pacific-south.test.ts.)
    expect(at(g, G5)).toBe('Ipoh');
    for (const n of ['1st', '6th']) expect(['Singora', 'Penang', 'Ipoh'], n).toContain(at(g, reg(n)));
    // (By the book the great blow's landings sail on the record's own shipping, each in its record month: rider b626
    // to ruling 234. The 48th Division lands at Lingayen in December, as it did on the 22nd, its regiments with it.)
    expect([at(g, G48), at(g, reg('4th')), at(g, reg('7th'))]).toEqual(['Lingayen', 'Lingayen', 'Lingayen']);
    for (const n of ['1st', '6th', '14th', '4th', '7th']) expect(corps(g, reg(n))!.stay, n).toBe(true);
  });
});

// The Lingayen reading (pacific-r2/indies/TRACE-LUZON-LINGAYEN-1121.md) and the design director's rulings of 22:34Z,
// 22:42Z and 22:50Z of 21 November 2026, with the historian's marks: the record put about twice the game's men ashore
// at Lingayen. The 48th Division is 0.8; the 16th Division is its three landings, the main body and the Kimura
// Detachment for Legaspi's province and its 9th Infantry for Lingayen with the 48th; the 14th Army's heavy guns are a
// row, in the form of the 23rd Army's at Canton. The new rows are facts of the day and belong to every stop: each
// stands on its division's quay and lands in its lift, and by the book stands beside the corps the record gave it.
describe("the 14th Army's rows that land with a division (the Lingayen reading)", () => {
  it('the rows, at every stop: the 16th Division by its three landings with no man counted twice, the guns on the 48th Division\'s quay', () => {
    for (const stop of [1, 2, 3]) {
      const g = december(stop);
      const fig = (nm: string) => { const c = corps(g, nm)!; return [at(g, nm), c.kind, c.str, c.full, c.hold]; };
      expect(fig(G48), `stop ${stop}`).toEqual(['Formosa', 'inf', 0.8, 0.8, true]);
      expect(fig(G16), `stop ${stop}`).toEqual(['Okinawa', 'inf', 0.5, 0.5, true]);
      expect(fig(KIMURA), `stop ${stop}`).toEqual(['Okinawa', 'inf', 0.15, 0.15, true]);
      expect(fig(NINTH), `stop ${stop}`).toEqual(['Formosa', 'inf', 0.25, 0.25, true]);
      expect(fig(HEAVY), `stop ${stop}`).toEqual(['Formosa', 'art', 0.4, 0.4, true]);
      // The division's three rows are the 0.9 its one row read.
      expect(corps(g, G16)!.str + corps(g, KIMURA)!.str + corps(g, NINTH)!.str).toBeCloseTo(0.9, 9);
      // Each goes with the division it sailed with: the first the table gives. (At stop 2 the table's months slide with its war's own date for the blow.)
      if (stop !== 2) expect([KIMURA, NINTH, HEAVY].map((nm) => goesWith(g, corps(g, nm)!)?.name), `stop ${stop}`).toEqual([G16, G48, G48]);
      // (Formosa's quay holds six rows of the 14th Army: the two that came last stand there whatever stands there.)
      expect(g.regions[corps(g, G48)!.reg].units.filter((u) => u.nat === 'JAP' && !u.dead).length).toBe(6);
    }
  });

  it('the 48th Division\'s lift is five rows for one sailing, the most a beach takes, and the 16th\'s is two', () => {
    const g = december(3);
    const d48 = corps(g, G48)!, d16 = corps(g, G16)!;
    const t4 = corps(g, reg('4th'))!, t7 = corps(g, reg('7th'))!, i9 = corps(g, NINTH)!, guns = corps(g, HEAVY)!, kimura = corps(g, KIMURA)!;
    expect(alongside(g, [d48])).toEqual([t4, t7, i9, guns]);
    expect(alongside(g, [d16])).toEqual([kimura]);
    expect(riding(g, [d48, t4, t7, i9, guns])).toEqual([t4, t7, i9, guns]);
    declareWar(g, 'JAP', 'USA');
    const lingayen = g.byName['Lingayen'];
    for (const u of [...hostileUnits(g, lingayen, 'JAP')]) removeCorps(g, u);
    spend(g, liftLeft(g, 'JAP') - 1);
    expect(whyNotLand(g, [d48, t4, t7, i9, guns], lingayen)).toBeNull();
    const res = landCorps(g, [d48], lingayen);
    expect([res.ok, res.taken]).toEqual([true, true]);
    expect([G48, reg('4th'), reg('7th'), NINTH, HEAVY].map((nm) => at(g, nm))).toEqual(['Lingayen', 'Lingayen', 'Lingayen', 'Lingayen', 'Lingayen']);
    expect(liftLeft(g, 'JAP')).toBe(0);
    // Ashore they are the staff's own.
    expect([i9.hold, guns.hold, goesWith(g, i9), goesWith(g, guns)]).toEqual([false, false, null, null]);
  });

  it('where history is loose: the month Japan strikes they land with their divisions, the 9th Infantry and the guns at Lingayen and Kimura at Legaspi', () => {
    const g = ready(inTheWay(december(3)));
    g.phase = g.order.indexOf('JAP');
    runTurnAI(g);
    expect(g.flags['st:done:JAP:south']).toBe(28);
    expect([G48, NINTH, HEAVY].map((nm) => at(g, nm))).toEqual(['Lingayen', 'Lingayen', 'Lingayen']);
    expect([G16, KIMURA].map((nm) => at(g, nm))).toEqual(['Legaspi', 'Legaspi']);
    for (const nm of [NINTH, HEAVY, KIMURA]) expect(corps(g, nm)!.hold, nm).toBe(false);
  });

  it('by the book: they land with their divisions in December, stand beside the corps the record gave them after, and the guns leave after May 1942', () => {
    const g = december(1);
    g.phase = g.order.indexOf('JAP');
    runTurnAI(g);
    expect(g.flags['st:done:JAP:south']).toBe(28);
    expect([G48, NINTH, HEAVY].map((nm) => at(g, nm))).toEqual(['Lingayen', 'Lingayen', 'Lingayen']);
    expect([G16, KIMURA].map((nm) => at(g, nm))).toEqual(['Legaspi', 'Legaspi']);
    for (const nm of [NINTH, HEAVY, KIMURA]) expect(corps(g, nm)!.stay, nm).toBe(true);
    // January 1942: the 9th Infantry and the guns are the 65th Brigade's (Abucay); April, the 9th is its division's again.
    const h = december(1);
    h.flags['st:done:JAP:south'] = 28;
    const b65 = addCorps(h, 'JAP', B65, 'inf', 1, 'Formosa', {});
    const lead = (nm: string): string | undefined => goesWith(h, corps(h, nm)!)?.name;
    h.turn = 29;
    expect([lead(NINTH), lead(HEAVY), lead(KIMURA)]).toEqual([b65.name, b65.name, G16]);
    h.turn = 32;
    expect([lead(NINTH), lead(HEAVY)]).toEqual([G16, b65.name]);
    // The guns stand through May 1942 (month 33) and are struck off in June, as Canton's are.
    h.turn = 33; startPhase(h, 'JAP');
    expect(corps(h, HEAVY)).toBeDefined();
    h.turn = 34; startPhase(h, 'JAP');
    expect(corps(h, HEAVY)).toBeUndefined();
    expect(corps(h, NINTH)).toBeDefined();
  });

  it("America's rows beside the record: the 26th Cavalry at its 838 men, the 4th Marines at about 1,500 on Bataan", () => {
    for (const stop of [1, 3]) {
      const g = december(stop);
      const cav = g.corps.find((c) => c.nat === 'USA' && c.name === '26th Cavalry, Philippine Scouts' && !c.dead)!;
      expect([cav.str, cav.full], `stop ${stop}`).toEqual([0.06, 0.06]);
      startPhase(g, 'USA');
      const marines = g.corps.find((c) => c.nat === 'USA' && c.name === '4th Marines' && !c.dead)!;
      expect([g.regions[marines.reg].name, marines.str, marines.full], `stop ${stop}`).toEqual(['Bataan', 0.1, 0.1]);
    }
  });

  it('the Philippine Army by row (the director\'s order (4)): men by the record where it gives them, else 6,750, and each division at its December figure', () => {
    for (const stop of [1, 3]) {
      const g = december(stop);
      // (A formation of the opening armies comes in its own month: September 1941, the call-up.)
      g.turn = 25; startPhase(g, 'USA'); g.turn = 28;
      const us = (nm: string): Corps => g.corps.find((c) => c.nat === 'USA' && c.name === nm && !c.dead)!;
      const PA = (n: string): string => `${n} Division, Philippine Army`;
      // The North Luzon Force is one row of four: the 11th 6,750, the 21st 8,500, the 31st 6,000, the 71st 6,750.
      const nlf = us('North Luzon Force');
      expect([nlf.divs, nlf.full], `stop ${stop}`).toEqual([4, 0.467]);
      expect(Math.abs(nlf.divs * nlf.full! * 15000 - 28000)).toBeLessThan(50);
      expect([PA('91st'), PA('41st'), PA('51st'), PA('61st'), PA('81st'), PA('101st')].map((nm) => us(nm).full), `stop ${stop}`).toEqual([0.4, 0.45, 0.45, 0.35, 0.35, 0.35]);
      // The first-day row is the 1st Regular Division: 0.45 at its most, from the small regular army of 1939.
      expect(us('Philippine Army').full, `stop ${stop}`).toBe(0.45);
      // Quality is the row's own at every stop, weeks into the call-up; nobody has fought.
      const q = (nm: string): number | null => raisedQuality(g, us(nm));
      expect([q('North Luzon Force'), q(PA('91st')), q(PA('41st')), q(PA('51st')), q(PA('61st')), q(PA('81st')), q(PA('101st')), q('Philippine Army')], `stop ${stop}`).toEqual([0.35, 0.3, 0.45, 0.3, 0.35, 0.35, 0.35, 0.5]);
      expect([q('Philippine Division'), q('26th Cavalry, Philippine Scouts')]).toEqual([1, 1]);
      for (const nm of ['North Luzon Force', PA('91st'), PA('41st'), PA('51st'), PA('61st'), PA('81st'), PA('101st'), 'Philippine Army']) expect(us(nm).xp, nm).toBe(0);
    }
  });
});
