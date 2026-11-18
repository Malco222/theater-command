// The nation's footing (ruling 172 and its two riders; the grand campaign, behind a switch). One lever for the share
// of industry on war work, four footings: Peacetime economy, Rearmament (at peace only), War footing (comes of itself
// at war), All-out war (a year at war or home ground lost). A ramp a step a month; down at once and free, then six
// months before raising again; political capital to order and to hold; the cabinet's trust where it sits; all-out
// war wears the will a twentieth a year, a tenth at most, as a count of its own. Untouched means unchanged, and with
// the switch off nothing moves at all. The computer's staffs use it when their own switch is on.
import { describe, expect, it } from 'vitest';
import { income, ledger } from '../src/campaign/economy';
import { FOOTING, footingId, footingLine, footingMonth, footingView, footShare, footWorn, orderFooting, setFooting, whyNotFooting } from '../src/campaign/footing';
import { collapseScore, createGame, setWar } from '../src/campaign/game';
import { government, govMonth, govOf, setCabinet, whyNotDecree } from '../src/campaign/government';
import { setStop } from '../src/campaign/history';
import { willLost, stores } from '../src/campaign/resources';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, endHuman, footingOf, humanUp, orderFootingTo, runTurnAI, stepAI, whyNotOrderFooting } from '../src/campaign/turn';
import { warFooting, warWorkMonth } from '../src/campaign/world';
import type { Game } from '../src/campaign/types';

const free = (seed: number, human: string | null = null): Game => startOpen(createGame(seed, human));
function playing(human: string, seed = 3, on = true): Game {
  const g = free(seed, human);
  if (on) setFooting(g, true);
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
function nextMonth(g: Game): void {
  endHuman(g);
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
}
const copy = (g: Game): Game => loadGame(saveGame(g))!;
const said = (g: Game, re: RegExp, from = 0): boolean => g.headlines.slice(from).some((h) => re.test(h.text));
const peaceLine = (g: Game, nat: string): string | undefined => ledger(g, nat).changes.find((l) => l.key === 'peace')?.label;
/** At war for a year: the war's first month set back twelve months. */
function aYearAtWar(g: Game, nat: string): void {
  for (const e of g.nations[nat].atWar) { delete g.flags[`war:${e}:${nat}`]; g.flags[`war:${nat}:${e}`] = g.turn - 12; }
}

describe('the switch', () => {
  it('is off in the data, and a campaign begun with it off has no lever', () => {
    expect(FOOTING.on).toBe(false);
    expect(FOOTING.staffs).toBe(false);
    const g = free(3, 'ITA');
    expect(g.flags.footing).toBeUndefined();
    expect(footingView(g, 'ITA')).toBeNull();
    expect(footingOf(g)).toBeNull();
    expect(whyNotFooting(g, 'ITA', 'rearm')).toMatch(/grand campaign/);
    expect(orderFooting(g, 'ITA', 'rearm')).toBe(false);
    expect(peaceLine(g, 'ITA')).toBe('At peace: industry at half speed');
  });

  it('a historical scenario never has it', () => {
    const g = createGame(3, 'GER');
    setFooting(g, true, true);
    expect(g.flags.footing).toBeUndefined();
    expect(footingView(g, 'GER')).toBeNull();
  });

  it('untouched means unchanged: switched on, nobody earns a point more or less', () => {
    const off = free(5, 'ITA'), on = free(5, 'ITA');
    setFooting(on, true);
    for (const nat of ['ITA', 'GER', 'UK', 'FRA', 'SOV', 'USA', 'CAN', 'BRA']) {
      if (!off.nations[nat]) continue;
      expect(income(on, nat)).toBe(income(off, nat));
      expect(ledger(on, nat).gross).toBe(ledger(off, nat).gross);
    }
  });

  it('a war nobody plays, with the lever on and the staffs off, is the same war', () => {
    const off = free(11), on = free(11);
    setFooting(on, true, false);
    for (let t = 0; t < 14; t++) { runTurnAI(off); runTurnAI(on); }
    const state = (g: Game): string => [g.corps.map((c) => `${c.nat}${c.reg}${c.str.toFixed(4)}${c.dead ? 1 : 0}`).join(';'), g.regions.map((r) => r.ctrl).join(''), Object.values(g.nations).map((n) => `${n.id}${n.alive ? 1 : 0}${n.pp.toFixed(3)}${n.pc.toFixed(3)}${n.atWar.join('')}`).join(';')].join('#');
    expect(state(on)).toBe(state(off));
    expect(Object.values(on.nations).some((n) => n.foot)).toBe(false);
  });
});

describe('the ledger line is the door', () => {
  it('names the footing in numbers at peace, and never drops out at war', () => {
    const g = playing('ITA');
    expect(peaceLine(g, 'ITA')).toBe('At peace: 50 in 100 of industry on war work');
    const ger = ledger(g, 'GER').changes.find((l) => l.key === 'peace');
    expect(ger?.label).toBe('At war: all of industry on war work');
    expect(ger?.amount).toBe(0);
  });

  it('the view lists the four footings with their locks, and asks about one move', () => {
    const g = playing('ITA');
    const v = footingOf(g)!;
    expect(v.id).toBe('peace');
    expect(v.rows.map((r) => [r.id, r.index, r.state])).toEqual([['peace', 50, 'on'], ['rearm', 75, 'open'], ['war', 100, 'locked'], ['allout', 125, 'locked']]);
    expect(v.rows[2].why).toBe('Comes of itself the day you are at war: no order, no price.');
    expect(v.rows[3].why).toMatch(/^At war only/);
    expect(v.move?.to).toBe('rearm');
    expect(v.move?.up).toBe(true);
    expect(v.move?.months).toBe(7);
    expect(v.move?.cost).toBe(FOOTING.rearm.cost);
    expect(v.move?.hold).toBe(FOOTING.rearm.hold);
    expect(v.move?.will).toBeNull();
    expect(v.move!.brings).toBeGreaterThan(0);
    expect(v.tip).toBe('door');
  });
});

describe('rearmament', () => {
  it('costs political capital to order, climbs a step a month and says how far it has to go', () => {
    const g = playing('ITA');
    g.nations.ITA.pc = 20;
    const before = income(g, 'ITA');
    expect(whyNotOrderFooting(g, 'rearm')).toBeNull();
    expect(orderFootingTo(g, 'rearm')).toBe(true);
    expect(g.nations.ITA.pc).toBe(20 - FOOTING.rearm.cost);
    expect(footingId(g, 'ITA')).toBe('rearm');
    // Nothing is earned until the first step is taken, as the month turns.
    expect(income(g, 'ITA')).toBe(before);
    nextMonth(g);
    expect(footShare(g, 'ITA', 0.5)).toBeCloseTo(0.5 + FOOTING.step, 9);
    expect(peaceLine(g, 'ITA')).toBe('Rearmament: 54 in 100 of industry on war work so far, 75 in 100 in 6 months');
    for (let k = 0; k < 7; k++) nextMonth(g);
    if (g.nations.ITA.atWar.length) return;
    expect(peaceLine(g, 'ITA')).toBe('Rearmament: 75 in 100 of industry on war work');
    expect(said(g, /Rearmament is complete: 75 in 100/)).toBe(true);
  });

  it('is paid for every month it is held, and slips the month there is nothing to pay with', () => {
    const g = playing('ITA');
    g.nations.ITA.pc = 20;
    const plain = copy(g);
    orderFootingTo(g, 'rearm');
    nextMonth(g);
    nextMonth(plain);
    if (g.nations.ITA.atWar.length) return;
    // The order's price, and a month's hold.
    expect(plain.nations.ITA.pc - g.nations.ITA.pc).toBeCloseTo(FOOTING.rearm.cost + FOOTING.rearm.hold, 9);
    const from = g.headlines.length;
    g.nations.ITA.pc = -5;
    nextMonth(g);
    if (g.nations.ITA.atWar.length) return;
    expect(footingId(g, 'ITA')).toBe('peace');
    expect(said(g, /slips back to the peacetime economy/, from)).toBe(true);
    expect(whyNotFooting(g, 'ITA', 'rearm')).toMatch(/^Stepped down lately: it cannot be raised again for 6 months\.$/);
  });

  it('steps down at once and free, and cannot be raised again for six months', () => {
    const g = playing('ITA');
    g.nations.ITA.pc = 20;
    orderFootingTo(g, 'rearm');
    nextMonth(g);
    nextMonth(g);
    const pc = g.nations.ITA.pc;
    expect(orderFootingTo(g, 'peace')).toBe(true);
    expect(g.nations.ITA.pc).toBe(pc);
    expect(footingId(g, 'ITA')).toBe('peace');
    expect(peaceLine(g, 'ITA')).toBe('At peace: 50 in 100 of industry on war work');
    expect(whyNotFooting(g, 'ITA', 'rearm')).toBe('Stepped down lately: it cannot be raised again for 6 months.');
    // A new climb starts from where stepping down left it: the peace table.
    g.nations[g.human!].foot!.wait = g.turn;
    orderFootingTo(g, 'rearm');
    expect(g.nations.ITA.foot?.share).toBe(0.5);
  });

  it('ends the day war comes: the war footing comes of itself, and America climbs from where rearmament left it', () => {
    const g = playing('USA');
    g.nations.USA.pc = 30;
    const table = warFooting(g, 'USA');
    orderFootingTo(g, 'rearm');
    for (let k = 0; k < 4; k++) footingMonth(g);
    const rearmed = g.nations.USA.foot!.share!;
    expect(rearmed).toBeGreaterThan(table! + 0.1);
    setWar(g, 'USA', 'GER');
    expect(warFooting(g, 'USA')).toBeCloseTo(rearmed, 9);
    warWorkMonth(g);
    footingMonth(g);
    expect(footingId(g, 'USA')).toBe('war');
    expect(g.nations.USA.foot?.step).toBeUndefined();
    expect(warFooting(g, 'USA')).toBeCloseTo(rearmed, 9);
    expect(footingLine(g, 'USA')).toMatch(/^At war: \d+ in 100 of industry on war work so far, all of it in \d+ months$/);
  });
});

describe('all-out war', () => {
  it('is locked until a year at war or home ground lost, then costs, climbs past all and wears the will', () => {
    const g = playing('GER');
    g.nations.GER.pc = 30;
    expect(whyNotOrderFooting(g, 'allout')).toBe('The country will bear this after 12 months of war, or once home ground is lost.');
    expect(whyNotOrderFooting(g, 'rearm')).toBe('At peace only.');
    expect(footingOf(g)!.rows.map((r) => r.state)).toEqual(['locked', 'locked', 'on', 'locked']);
    aYearAtWar(g, 'GER');
    expect(whyNotOrderFooting(g, 'allout')).toBeNull();
    const v = footingOf(g)!;
    expect(v.tip).toBe('allout');
    expect(v.move?.will).toEqual({ year: FOOTING.allout.wear * 12, most: FOOTING.allout.most });
    expect(v.move?.months).toBe(7);
    const before = income(g, 'GER');
    expect(orderFootingTo(g, 'allout')).toBe(true);
    expect(g.nations.GER.pc).toBe(30 - FOOTING.allout.cost);
    footingMonth(g);
    expect(footingLine(g, 'GER')).toBe('All-out war: all of industry on war work, and the second shift adds 4 in 100 so far, 25 in 100 in 6 months');
    expect(income(g, 'GER')).toBeGreaterThan(before);
    for (let k = 0; k < 6; k++) footingMonth(g);
    expect(footingLine(g, 'GER')).toBe('All-out war: all of industry on war work, on two shifts');
    // The top step is worth a fifth to a third more than the war footing (172(1)).
    expect(FOOTING.allout.share).toBeGreaterThanOrEqual(1.2);
    expect(FOOTING.allout.share).toBeLessThanOrEqual(4 / 3);
  });

  it('wears the will a twentieth a year, a tenth at most, says so once in amber, and mends at twice the pace', () => {
    const g = playing('GER');
    g.nations.GER.pc = 30;
    aYearAtWar(g, 'GER');
    orderFootingTo(g, 'allout');
    const base = collapseScore(g, 'GER') - footWorn(g, 'GER');
    for (let k = 0; k < 12; k++) footingMonth(g);
    expect(footWorn(g, 'GER')).toBeCloseTo(0.05, 4);
    expect(willLost(g, 'GER')).toBeCloseTo(footWorn(g, 'GER') + (stores(g, 'GER')?.hungerLost ?? 0), 12);
    expect(stores(g, 'GER')?.footWorn).toBeCloseTo(0.05, 4);
    expect(collapseScore(g, 'GER')).toBeCloseTo(base + 0.05, 4);
    expect(g.headlines.filter((h) => /All-out war wears at home/.test(h.text)).length).toBe(1);
    expect(footingOf(g)!.amber).toBe(true);
    for (let k = 0; k < 60; k++) footingMonth(g);
    expect(footWorn(g, 'GER')).toBeCloseTo(FOOTING.allout.most, 9);
    // Stepping down mends it at twice the pace it wore: a tenth in a year.
    orderFootingTo(g, 'war');
    for (let k = 0; k < 6; k++) footingMonth(g);
    expect(footWorn(g, 'GER')).toBeCloseTo(0.05, 3);
    for (let k = 0; k < 7; k++) footingMonth(g);
    expect(footWorn(g, 'GER')).toBe(0);
    expect(g.headlines.filter((h) => /All-out war wears at home/.test(h.text)).length).toBe(1);
  });

  it('costs the war cabinet trust each month where it sits, and nothing of trust with the cabinet switched off', () => {
    const g = playing('GER');
    g.nations.GER.pc = 30;
    aYearAtWar(g, 'GER');
    orderFootingTo(g, 'allout');
    const st = govOf(g, 'GER')!;
    delete st.dir; st.next = g.turn + 99;
    govMonth(g, 'GER');
    expect(st.last?.find((c) => c.label === 'The nation\'s footing')?.amount).toBe(FOOTING.allout.trust);
    expect(footingOf(g)!.move?.trust).toBeNull();
    expect(setCabinet(g, false)).toBe(true);
    // The lever is still there, and its political capital still bites.
    expect(footingId(g, 'GER')).toBe('allout');
    const h = copy(g);
    expect(footingId(h, 'GER')).toBe('allout');
  });

  it('home ground lost unlocks it at once (the Soviet Union the month it is invaded)', () => {
    const g = playing('SOV', 3);
    g.nations.SOV.pc = 30;
    setWar(g, 'SOV', 'GER');
    expect(whyNotFooting(g, 'SOV', 'allout')).toMatch(/12 months of war/);
    const r = g.regions.find((x) => x.owner === 'SOV' && !x.overseas && x.id !== g.nations.SOV.capital)!;
    r.ctrl = 'GER';
    expect(whyNotFooting(g, 'SOV', 'allout')).toBeNull();
  });
});

describe('the cabinet keeps the call-up and points to the footing', () => {
  it('the economy ladder is gone from the decrees, and cannot be decreed', () => {
    const g = playing('GER');
    g.nations.GER.pc = 30;
    const v = government(g)!;
    expect(v.tracks.map((t) => t.id)).toEqual(['service']);
    expect(whyNotDecree(g, 'GER', 'economy', 1)).toMatch(/footing/);
    const old = playing('GER', 3, false);
    expect(government(old)!.tracks.map((t) => t.id)).toEqual(['economy', 'service']);
  });
});

describe('saves', () => {
  it('keep the footing, the climb, the wait and the wear', () => {
    const g = playing('GER');
    g.nations.GER.pc = 30;
    aYearAtWar(g, 'GER');
    orderFootingTo(g, 'allout');
    footingMonth(g); footingMonth(g);
    const h = copy(g);
    expect(h.flags.footing).toBe(1);
    expect(h.nations.GER.foot).toEqual(g.nations.GER.foot);
    expect(footingLine(h, 'GER')).toBe(footingLine(g, 'GER'));
  });
});

describe('the computer staffs', () => {
  it('leave the lever alone while their switch is off', () => {
    const g = free(7);
    setFooting(g, true, false);
    g.turn = 42;
    for (const n of Object.values(g.nations)) n.pc = 30;
    footingMonth(g);
    expect(Object.values(g.nations).some((n) => n.foot)).toBe(false);
  });

  it('by the book: Germany in February 1943, the Soviet Union rearms from 1939, Italy and France never', () => {
    const g = free(7);
    setFooting(g, true, true);
    setStop(g, 1, true);
    for (const n of Object.values(g.nations)) n.pc = 30;
    footingMonth(g);
    expect(footingId(g, 'SOV')).toBe('rearm');
    expect(footingId(g, 'GER')).toBe('war');
    g.turn = 41;
    footingMonth(g);
    expect(footingId(g, 'GER')).toBe('war');
    g.turn = 42;
    footingMonth(g);
    expect(footingId(g, 'GER')).toBe('allout');
    expect(said(g, /Germany goes over to all-out war/)).toBe(true);
    setWar(g, 'ITA', 'UK');
    g.turn = 60;
    footingMonth(g);
    expect(footingId(g, 'ITA')).toBe('war');
    expect(footingId(g, 'FRA')).not.toBe('allout');
  });

  it('by judgment: up when the will can bear a tenth more and the price can be held, down within a fifth of the limit', () => {
    const g = free(7);
    setFooting(g, true, true);
    setStop(g, 3, true);
    for (const n of Object.values(g.nations)) n.pc = 30;
    aYearAtWar(g, 'GER');
    footingMonth(g);
    expect(footingId(g, 'GER')).toBe('allout');
    // A minor cannot hold it from what comes in each month.
    expect(Object.values(g.nations).filter((n) => !['GER', 'UK', 'FRA', 'SOV', 'ITA', 'USA', 'JAP'].includes(n.id)).some((n) => n.foot?.step === 'allout')).toBe(false);
    // Near its limit the staff steps down.
    g.nations.GER.foot!.worn = FOOTING.allout.most;
    g.nations.GER.resilience = (collapseScore(g, 'GER') * 0.99) / 0.8;
    footingMonth(g);
    expect(footingId(g, 'GER')).toBe('war');
  });

  it('rearm at peace only at stops 4 and 5, with cause, and the Soviet Union keeps its peace table there (the rider\'s (p))', () => {
    expect(FOOTING.judge.peaceKept).toEqual(['SOV']);
    const at = (stop: number): Game => {
      const g = free(7);
      setFooting(g, true, true);
      setStop(g, stop, true);
      for (const n of Object.values(g.nations)) n.pc = 30;
      footingMonth(g);
      return g;
    };
    for (const stop of [3, 4, 5]) expect(footingId(at(stop), 'SOV')).toBe('peace');
    expect(Object.values(at(3).nations).some((n) => n.foot?.step === 'rearm')).toBe(false);
  });
});
