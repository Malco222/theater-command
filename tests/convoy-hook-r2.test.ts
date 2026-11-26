// The wolf pack and convoy battle's campaign side (release 2; ruling 241 of 22 November 2026, on the production
// lead's form naval-battles/wolfpack-r2/form.md): the month's swing on a convoy route, the seam a fought night is
// booked through, and the look-ahead that finds the nights a player can fight. Switched by stage.json
// 'convoyBattle': off, no campaign carries any of it.
import { afterEach, describe, expect, it } from 'vitest';
import stage from '../data/campaign/stage.json';
import { createGame } from '../src/campaign/game';
import { CONVOY_BATTLE, NAVY, convoyField, convoyMeeting, convoyRules, convoyWar, navalUpkeep, zoneOf, type ConvoyFought, type ConvoyMeeting } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { CONVOY_STAGE } from '../src/campaign/stage';
import { beginHuman, humanUp, passMonth, stepAI } from '../src/campaign/turn';
import { canFight, convoyAhead, convoyCloseWith, convoyKey, convoyPlayedHook, type ConvoyPlayed } from '../src/campaign/convoyhand';
import type { Game } from '../src/campaign/types';

afterEach(() => { convoyField.fought = undefined; });

/** A grand campaign with the convoy battle on (or off), at this seat's first month of orders. */
function seat(nat: string | null, seed = 1, on = true): Game {
  const g = startOpen(createGame(seed, nat));
  if (on) g.flags.convoyBattle = 1; else delete g.flags.convoyBattle;
  let guard = 0;
  if (nat) { while (!humanUp(g) && guard++ < 200) stepAI(g); beginHuman(g); }
  return g;
}
const twin = (g: Game): Game => loadGame(saveGame(g))!;
const atlantic = NAVY.convoys.find((r) => r.id === 'atlantic')!;
const ships = (g: Game, id: number): string => JSON.stringify(g.fleets.find((f) => f.id === id)!.ships);
/** Every fleet's ships and losses. (A raider's tally of tons sunk, its story's record, follows the tons.) */
const navies = (g: Game): string => JSON.stringify(g.fleets.map((f) => [f.id, f.ships, f.lost]));
const run = (x: number): [number, number] => [Math.floor(x), Math.floor(x) + 1];

describe('the switch', () => {
  it('is off in release 1: no campaign carries the convoy battle', () => {
    expect((stage as { convoyBattle?: boolean }).convoyBattle).toBe(false);
    if (CONVOY_STAGE) return; // (a run with TC_CONVOY set)
    const g = startOpen(createGame(1, null));
    expect(g.flags.convoyBattle).toBeUndefined();
    expect(convoyRules(g)).toBe(false);
  });

  it('never reaches a history campaign, and a save keeps the flag', () => {
    const h = createGame(1, null);
    h.flags.convoyBattle = 1;
    expect(convoyRules(h)).toBe(false);
    const g = seat(null);
    expect(convoyRules(g)).toBe(true);
    expect(convoyRules(twin(g))).toBe(true);
  });

  it('off, the seam is never asked and the month is release 1\'s', () => {
    const a = seat(null, 1, false), b = twin(a);
    let asked = 0;
    convoyField.fought = () => { asked++; return { swing: 1.2, ss: {}, dd: {} }; };
    navalUpkeep(b, 'UK');
    convoyField.fought = undefined;
    navalUpkeep(a, 'UK');
    expect(asked).toBe(0);
    expect(b.convoys.atlantic.swing).toBeUndefined();
    expect(b.convoys.atlantic.fought).toBeUndefined();
    expect(saveGame(b)).toBe(saveGame(a));
  });
});

describe('the swing', () => {
  it('moves only the share sunk, and with it tons and loss: every roll and every ship is as it was', () => {
    const a = seat(null, 1, false), b = twin(a);
    b.flags.convoyBattle = 1;
    navalUpkeep(a, 'UK'); navalUpkeep(b, 'UK');
    const A = a.convoys.atlantic, B = b.convoys.atlantic;
    expect(B.swing).toBeGreaterThanOrEqual(CONVOY_BATTLE.swing.low);
    expect(B.swing).toBeLessThanOrEqual(CONVOY_BATTLE.swing.high);
    expect(B.loss).toBeCloseTo(Math.min(0.95, A.loss! * B.swing!), 9);
    expect(B.share).toBeCloseTo(atlantic.share * B.loss!, 9);
    expect(B.tons).toBe(Math.round((atlantic.tons * B.loss!) / 1000) * 1000);
    expect([B.raid, B.guard, B.subsLost, B.escortsLost]).toEqual([A.raid, A.guard, A.subsLost, A.escortsLost]);
    expect(navies(b)).toBe(navies(a));
  });

  it('is even about 1 over many months', () => {
    const g = seat(null);
    let s = 0;
    const N = 400;
    for (let t = 1; t <= N; t++) {
      g.turn = t;
      convoyWar(g, 'UK');
      s += g.convoys.atlantic.swing!;
    }
    expect(s / N).toBeGreaterThan(0.98);
    expect(s / N).toBeLessThan(1.02);
  });
});

describe('a fought night', () => {
  function fight(fought: (m: ConvoyMeeting) => ConvoyFought): { a: Game; b: Game; m: ConvoyMeeting } {
    const a = seat(null), b = twin(a);
    const m = convoyMeeting(b, 'UK', atlantic);
    convoyField.fought = (x) => (x.route === 'atlantic' ? fought(x) : undefined);
    navalUpkeep(b, 'UK');
    convoyField.fought = undefined;
    navalUpkeep(a, 'UK');
    return { a, b, m };
  }

  it('books its boats and destroyers lost inside each roll\'s run, and its swing; fleets outside it roll as ever', () => {
    const { a, b, m } = fight((x) => ({
      swing: 1.1,
      ss: Object.fromEntries(x.raiders.map((r) => [r.id, run(r.expect)[1]])),
      dd: Object.fromEntries(x.escorts.map((e) => [e.id, run(e.expect)[0]])),
    }));
    const B = b.convoys.atlantic;
    expect(B.fought).toBe('field');
    expect(B.swing).toBeCloseTo(1.1, 12);
    for (const r of m.raiders) {
      const was = a.fleets.find((f) => f.id === r.id)!;
      const lost = b.fleets.find((f) => f.id === r.id)!.lost.ss - (was.lost.ss - (r.ss - was.ships.ss));
      expect(lost).toBe(Math.min(r.ss, run(r.expect)[1]));
    }
    const inNight = new Set([...m.raiders.map((r) => r.id), ...m.escorts.map((e) => e.id)]);
    for (const f of a.fleets) if (!inNight.has(f.id)) expect(ships(b, f.id)).toBe(ships(a, f.id));
  });

  it('W5b: books a fought night\'s own ships on the month as a fact no rule reads', () => {
    const plain = fight(() => ({ swing: 1.1, ss: {}, dd: {} }));
    const told = fight(() => ({ swing: 1.1, ss: {}, dd: {}, mv: 3 }));
    expect(plain.b.convoys.atlantic.nightShips).toBeUndefined();
    expect(told.b.convoys.atlantic.nightShips).toBe(3);
    // (record only: every fleet, and the month's own record but for the night's ships, as the same night without them)
    expect(navies(told.b)).toBe(navies(plain.b));
    const { nightShips, ...month } = told.b.convoys.atlantic;
    expect(nightShips).toBe(3);
    expect(month).toEqual(plain.b.convoys.atlantic);
    // (a month nobody fought keeps no such figure)
    expect(told.a.convoys.atlantic.nightShips).toBeUndefined();
  });

  it('W5b: the month headline tells the night\'s ships from the month\'s tons as booked, only in a month a night was fought', () => {
    const head = `${atlantic.name[0].toUpperCase()}${atlantic.name.slice(1)}: `;
    const said = (night: boolean, mv?: number): string => {
      const g = seat('UK');
      const h0 = g.headlines.length;
      if (night) convoyField.fought = (x) => (x.route === 'atlantic' ? { swing: 1, ss: {}, dd: {}, ...(mv === undefined ? {} : { mv }) } : undefined);
      navalUpkeep(g, 'UK');
      convoyField.fought = undefined;
      const lines = g.headlines.slice(h0).map((h) => h.text).filter((t) => t.startsWith(head) && t.includes('tons of shipping sunk this month'));
      expect(lines.length).toBe(1);
      return lines[0];
    };
    const tonsOf = (t: string): string => t.slice(head.length).split(' tons')[0];
    const none = said(false);
    expect(none).not.toContain('night you fought');
    // (a seam that books no ships for the night says nothing of them: release 1's line)
    expect(said(true)).not.toContain('night you fought');
    const zero = said(true, 0);
    expect(zero).toContain(`. On the night you fought, the convoy lost no ships; the route's other nights this month lost ${tonsOf(zero)} tons.`);
    const one = said(true, 1);
    expect(one).toContain(`. On the night you fought, the convoy lost one ship; the route lost ${tonsOf(one)} tons this month in all.`);
    expect(said(true, 3)).toContain('the convoy lost three ships; the route lost ');
    expect(said(true, 10)).toContain('the convoy lost ten ships;');
    expect(said(true, 12)).toContain('the convoy lost 12 ships;');
    // (a seam that hands back no whole count books none, and says nothing of the night)
    for (const bad of [2.5, -1, Number.NaN]) expect(said(true, bad)).not.toContain('night you fought');
    // (the month after: no night, no figure, release 1's line)
    const g = seat('UK');
    convoyField.fought = (x) => (x.route === 'atlantic' ? { swing: 1, ss: {}, dd: {}, mv: 3 } : undefined);
    navalUpkeep(g, 'UK');
    convoyField.fought = undefined;
    expect(g.convoys.atlantic.nightShips).toBe(3);
    const h1 = g.headlines.length;
    navalUpkeep(g, 'UK');
    expect(g.convoys.atlantic.nightShips).toBeUndefined();
    expect(g.headlines.slice(h1).some((h) => h.text.includes('night you fought'))).toBe(false);
  }, 60_000);

  it('W5b: an ally whose escorts fought the night reads it in the month headline too', () => {
    const g = seat('CAN');
    const f = g.fleets.find((x) => x.nat === 'CAN' && x.ships.dd > 0)!;
    f.stance = 'escort'; f.lane = 'atlantic'; f.zone = zoneOf(g, 'natl');
    const ahead = convoyAhead(g).filter((a) => a.meeting.route === 'atlantic');
    expect(ahead.length).toBe(1);
    const played: ConvoyPlayed = new Map(ahead.map((a) => [a.key, { meeting: a.meeting, fought: { swing: 1.1, ss: {}, dd: {}, mv: 3 } }]));
    const h0 = g.headlines.length;
    convoyCloseWith(g, played);
    expect(g.convoys.atlantic.nightShips).toBe(3);
    const lines = g.headlines.slice(h0).map((h) => h.text).filter((t) => t.includes('night you fought'));
    expect(lines.length).toBe(1);
    expect(lines[0]).toContain(`On the night you fought, the convoy lost three ships; the route lost ${g.convoys.atlantic.tons.toLocaleString('en-US')} tons this month in all.`);
  }, 60_000);

  it('holds a count outside its run into it, and then reads as a bare close result', () => {
    const { b, m } = fight((x) => ({ swing: 1.6, ss: Object.fromEntries(x.raiders.map((r) => [r.id, 99])), dd: {} }));
    const B = b.convoys.atlantic;
    expect(B.fought).toBe('clamped');
    expect(B.swing).toBe(CONVOY_BATTLE.swing.high);
    for (const r of m.raiders) expect(b.fleets.find((f) => f.id === r.id)!.ships.ss).toBeGreaterThanOrEqual(r.ss - run(r.expect)[1]);
  });

  it('leaves to the staff every route a hand may not fight', () => {
    expect(CONVOY_BATTLE.hand).toEqual(['atlantic']);
    const g = seat(null);
    const m = { ...convoyMeeting(g, 'UK', atlantic), route: 'libya' };
    expect(canFight(m, 'UK')).toBe(false);
  });
});

describe('the look-ahead', () => {
  it('finds the night a British seat can fight in his next month, and changes nothing in the game', () => {
    const g = seat('UK', 1);
    const before = saveGame(g);
    const ahead = convoyAhead(g);
    expect(ahead.map((a) => a.key)).toEqual([`${g.turn + 1}:atlantic`]);
    expect(ahead[0].meeting.escorts.some((e) => e.nat === 'UK')).toBe(true);
    expect(saveGame(g)).toBe(before);
    expect(convoyField.fought).toBeUndefined();
  });

  it('finds it for the pack\'s side in the same month, and nothing for a seat with nothing there or the switch off', () => {
    const ger = seat('GER', 7);
    expect(convoyAhead(ger).map((a) => a.key)).toEqual([`${ger.turn}:atlantic`]);
    expect(convoyAhead(seat('USA', 1))).toEqual([]);
    expect(convoyAhead(seat('UK', 1, false))).toEqual([]);
  });

  it('books what he fought once, on the same fleets, and the rest of the month is the staff\'s', () => {
    const g = seat('UK', 1), staff = twin(g);
    const [a] = convoyAhead(g);
    const fought: ConvoyFought = { swing: 1.15, ss: {}, dd: {} };
    const played: ConvoyPlayed = new Map([[a.key, { meeting: a.meeting, fought }]]);
    convoyCloseWith(g, played);
    passMonth(staff);
    expect(played.size).toBe(0);
    expect(convoyField.fought).toBeUndefined();
    expect(g.convoys.atlantic.fought).toBe('field');
    expect(g.convoys.atlantic.swing).toBeCloseTo(1.15, 12);
    expect(staff.convoys.atlantic.fought).toBeUndefined();
    expect(navies(g)).toBe(navies(staff));
  });

  it('a night seen with other fleets in it is the staff\'s', () => {
    const g = seat('UK', 1);
    const [a] = convoyAhead(g);
    const other = { ...a.meeting, escorts: a.meeting.escorts.slice(1) };
    const played: ConvoyPlayed = new Map([[a.key, { meeting: other, fought: { swing: 1.2, ss: {}, dd: {} } }]]);
    const hook = convoyPlayedHook(played);
    expect(hook(a.meeting)).toBeUndefined();
    expect(played.size).toBe(0);
    expect(convoyKey(a.meeting)).toBe(a.key);
  });
});
