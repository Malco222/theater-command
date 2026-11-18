// The hand-back between the campaign and the field for fleet actions (src/battle/sea/handoff.ts, navy.ts seaField).
// With the seam unset, or set and handing nothing back, the rules fight every action exactly as before; a played
// action's ratio and ships lost stand in place of the dice, and nothing else changes.
import { describe, expect, it } from 'vitest';
import { runOut } from '../src/battle/core/clock';
import { closeWith, foughtOf, meetingKey, playedHook, seaAhead, type SeaPlayed } from '../src/battle/sea/handoff';
import { diceRange } from '../src/battle/sea/result';
import { SeaSim, type SeaLosses } from '../src/battle/sea/sim';
import { createGame } from '../src/campaign/game';
import { sail, seaBattles, seaField, shipCount, zoneOf, type SeaFought, type SeaMeeting } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { fieldFights, plainTicket } from '../src/battle/sea/ticket';
import { current, stepAI } from '../src/campaign/turn';
import type { Fleet, Game } from '../src/campaign/types';

const fleet = (g: Game, name: string): Fleet => g.fleets.find((f) => f.name === name)!;
/** The Home Fleet and the Battle Fleet both on patrol in the Norwegian Sea: the British seek an action there (as in navy.test.ts). */
function meet(seed: number): { g: Game; home: Fleet; battle: Fleet } {
  const g = createGame(seed, null);
  const home = fleet(g, 'Home Fleet'), battle = fleet(g, 'Battle Fleet');
  const norw = zoneOf(g, 'norw');
  expect(sail(g, home, norw, 'patrol')).toBe(true);
  expect(sail(g, battle, norw, 'patrol')).toBe(true);
  return { g, home, battle };
}
const ukClose = (g: Game): void => { seaBattles(g, 'UK'); };
const loadOf = (g: Game): Game => loadGame(saveGame(g))!;
const SURFACE = ['bb', 'cv', 'ca', 'dd'] as const;

describe('with nothing played, the rules are untouched', () => {
  it('a close with an empty hand-back is the same, ship for ship and word for word, as the dice alone', () => {
    const a = meet(5), b = meet(5);
    seaBattles(a.g, 'UK');
    closeWith(b.g, new Map(), ukClose);
    expect(saveGame(b.g)).toBe(saveGame(a.g));
    expect(JSON.stringify(b.g.seaReports)).toBe(JSON.stringify(a.g.seaReports));
    expect(seaField.fought).toBeUndefined();
  });

  it('the seam is put back as it was even when the close fails', () => {
    const { g } = meet(5);
    expect(() => closeWith(g, new Map(), () => { throw new Error('boom'); })).toThrow('boom');
    expect(seaField.fought).toBeUndefined();
    expect(() => seaAhead(g, () => { throw new Error('boom'); })).toThrow('boom');
    expect(seaField.fought).toBeUndefined();
  });
});

describe('the look-ahead', () => {
  it('finds the action on a copy, writes its ticket, and changes nothing in the game', () => {
    const { g, home, battle } = meet(5);
    const before = saveGame(g);
    const ahead = seaAhead(g, ukClose);
    expect(saveGame(g)).toBe(before);
    expect(g.seaReports.length).toBe(0);
    expect(ahead.length).toBe(1);
    const { meeting: m, ticket: t } = ahead[0];
    expect(m.mine).toEqual([home.id]);
    expect(m.met).toEqual([battle.id]);
    expect(t.place.zoneName).toBe('Norwegian Sea');
    expect(t.title).toBe('The action in the Norwegian Sea');
    expect(t.reckoning.A).toBe(m.A);
    expect(t.reckoning.D).toBe(m.D);
    // The field meets at the rules' own odds: the fleets' weights on the ticket add up to the rules' strengths.
    const weight = (side: 'att' | 'def'): number => t.place.fleets.filter((f) => f.side === side)
      .reduce((s, f) => s + SURFACE.reduce((n, c) => n + f.ships[c] * ({ bb: 10, cv: 7, ca: 3, dd: 1 })[c] * (c === 'cv' ? f.deck : 1) * f.mul, 0), 0);
    expect(weight('att') + t.place.air.att).toBeCloseTo(m.A, 6);
    expect(weight('def') + t.place.air.def).toBeCloseTo(Math.max(1, m.D), 6);
  });

  it('sees the very actions the real close then fights, across computer-run months', () => {
    // The British month comes round with the Battle Fleet out on patrol where the Home Fleet is.
    const g = createGame(3, null);
    while (current(g) !== 'UK') stepAI(g);
    const home = fleet(g, 'Home Fleet'), battle = fleet(g, 'Battle Fleet');
    expect(sail(g, battle, home.zone, 'patrol')).toBe(true);
    let seen = 0;
    for (let i = 0; i < 6; i++) {
      const ahead = seaAhead(g, (c) => { stepAI(c); });
      const real: SeaMeeting[] = [];
      seaField.fought = (m) => { real.push(m); return undefined; };
      try { stepAI(g); } finally { seaField.fought = undefined; }
      expect(JSON.stringify(real), `month step ${i}`).toBe(JSON.stringify(ahead.map((a) => a.meeting)));
      seen += real.length;
    }
    expect(seen).toBeGreaterThan(0);
  });

  it('does not offer an action where one side has no gun ship: the staff settles it by the dice (b438)', () => {
    const { g, battle } = meet(5);
    battle.ships = { bb: 0, cv: 2, ca: 0, dd: 0, ss: battle.ships.ss };
    const met: SeaMeeting[] = [];
    seaField.fought = (m) => { met.push(m); return undefined; };
    try { seaBattles(loadOf(g), 'UK'); } finally { seaField.fought = undefined; }
    expect(met.length).toBe(1); // the rules still fight it
    expect(seaAhead(g, ukClose)).toEqual([]); // but the field is not offered it
    expect(fieldFights(plainTicket({ seed: 1, att: [{ name: 'A', nat: 'UK', ships: { cv: 2 } }], def: [{ name: 'B', nat: 'JAP', ships: { cv: 2 } }] }))).toBe(false);
    expect(fieldFights(plainTicket({ seed: 1, att: [{ name: 'A', nat: 'UK', ships: { cv: 2, dd: 1 } }], def: [{ name: 'B', nat: 'GER', ships: { bb: 1 } }] }))).toBe(true);
  });
});

describe('a played action stands in place of the dice', () => {
  it("the field's ratio and the ships it sank are what the rules book", () => {
    const { g, home, battle } = meet(5);
    battle.ships.ss = 6; // submarines take no part on the field: the dice settle them as before
    const [a] = seaAhead(g, ukClose);
    const sim = new SeaSim(a.ticket);
    sim.ai.att = true; sim.ai.def = true;
    runOut(sim);
    const fought = foughtOf(sim.over!);
    const x = sim.over!.extra as unknown as SeaLosses;
    const ships0 = { home: { ...home.ships }, battle: { ...battle.ships } };
    const played: SeaPlayed = new Map([[a.key, { meeting: a.meeting, fought }]]);
    closeWith(g, played, ukClose);
    const rep = g.seaReports[g.seaReports.length - 1];
    expect(rep.odds).toBe(sim.over!.R);
    expect(rep.won).toBe(sim.over!.winner === 'att');
    for (const c of SURFACE) {
      expect(rep.att[0].sunk[c], `Home Fleet ${c}`).toBe(x.sunk[home.id][c]);
      expect(rep.def[0].sunk[c], `Battle Fleet ${c}`).toBe(x.sunk[battle.id][c]);
      expect(home.ships[c]).toBe(ships0.home[c] - x.sunk[home.id][c]);
      expect(battle.ships[c]).toBe(ships0.battle[c] - x.sunk[battle.id][c]);
    }
    expect(battle.ships.ss + rep.def[0].sunk.ss).toBe(6);
    // The beaten side makes for port, as after any action.
    expect((rep.won ? battle : home).stance).toBe('port');
  });

  it('a chase let go (b458, 1) is booked as any other ending: the side that broke off beaten, the losses as they stood', () => {
    const { g, home, battle } = meet(5);
    const [a] = seaAhead(g, ukClose);
    const sim = new SeaSim(a.ticket);
    sim.ai.att = true; sim.ai.def = true;
    while (!sim.over && sim.brokenSide === null) sim.step();
    expect(sim.over).toBeNull();
    const chaser = sim.brokenSide === 'att' ? 'def' : 'att';
    const mine = sim.units.filter((u) => u.side === chaser && u.state === 'steady').map((u) => u.id);
    expect(sim.command({ do: 'hold', units: mine }, chaser)).toBe(true);
    expect(sim.over!.winner).toBe(chaser);
    const x = sim.over!.extra as unknown as SeaLosses;
    closeWith(g, new Map([[a.key, { meeting: a.meeting, fought: foughtOf(sim.over!) }]]), ukClose);
    const rep = g.seaReports[g.seaReports.length - 1];
    expect(rep.odds).toBe(sim.over!.R);
    expect(rep.won).toBe(chaser === 'att');
    for (const c of SURFACE) {
      expect(rep.att[0].sunk[c], `Home Fleet ${c}`).toBe(x.sunk[home.id][c]);
      expect(rep.def[0].sunk[c], `Battle Fleet ${c}`).toBe(x.sunk[battle.id][c]);
    }
  });

  it('handed to the dice part-way, the rules sink as at that ratio, and never fewer than had gone down', () => {
    for (let seed = 1; seed <= 6; seed++) {
      const { g, battle } = meet(seed);
      const [a] = seaAhead(g, ukClose);
      const [lo] = diceRange(a.ticket.reckoning.odds);
      const fought: SeaFought = { R: Math.max(1, lo), sunk: { [battle.id]: { dd: 3, ca: 1 } }, toDice: true };
      closeWith(g, new Map([[a.key, { meeting: a.meeting, fought }]]), ukClose);
      const rep = g.seaReports[g.seaReports.length - 1];
      expect(rep.odds).toBe(fought.R);
      expect(rep.def[0].sunk.dd).toBeGreaterThanOrEqual(3);
      expect(rep.def[0].sunk.ca).toBeGreaterThanOrEqual(1);
    }
  });

  it('fought to the end, exactly the ships lost on the field go down, no more', () => {
    const { g, home, battle } = meet(5);
    const [a] = seaAhead(g, ukClose);
    const fought: SeaFought = { R: 1.05, sunk: { [battle.id]: { dd: 3 }, [home.id]: { dd: 1 } } };
    closeWith(g, new Map([[a.key, { meeting: a.meeting, fought }]]), ukClose);
    const rep = g.seaReports[g.seaReports.length - 1];
    expect(shipCount(rep.def[0].sunk)).toBe(3);
    expect(rep.def[0].sunk.dd).toBe(3);
    expect(shipCount(rep.att[0].sunk)).toBe(1);
  });

  it('an action that came out with other fleets in it than the look-ahead saw is left to the dice', () => {
    const a = meet(5), b = meet(5);
    seaBattles(a.g, 'UK');
    const [ahead] = seaAhead(b.g, ukClose);
    const other: SeaMeeting = { ...ahead.meeting, met: [...ahead.meeting.met, 999] };
    closeWith(b.g, new Map([[meetingKey(other), { meeting: other, fought: { R: 3, sunk: {} } }]]), ukClose);
    expect(JSON.stringify(b.g.seaReports)).toBe(JSON.stringify(a.g.seaReports));
  });
});

describe('the hook left in place for a whole session (the 3D game)', () => {
  it('books a played action once, and with nothing played rolls the dice as before', () => {
    const a = meet(5), b = meet(5);
    const played: SeaPlayed = new Map();
    seaField.fought = playedHook(played);
    try {
      // Nothing played: the same as the dice alone.
      seaBattles(b.g, 'UK');
      seaBattles(a.g, 'UK');
    } finally { seaField.fought = undefined; }
    expect(saveGame(b.g)).toBe(saveGame(a.g));
    // Played: booked once, then forgotten.
    const c = meet(5);
    const [x] = seaAhead(c.g, ukClose);
    played.set(x.key, { meeting: x.meeting, fought: { R: 1.05, sunk: { [c.battle.id]: { dd: 2 } } } });
    seaField.fought = playedHook(played);
    try { seaBattles(c.g, 'UK'); } finally { seaField.fought = undefined; }
    expect(c.g.seaReports[0].odds).toBe(1.05);
    expect(c.g.seaReports[0].def[0].sunk.dd).toBe(2);
    expect(played.size).toBe(0);
  });
});
