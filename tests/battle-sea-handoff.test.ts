// The hand-back between the campaign and the field for fleet actions (src/battle/sea/handoff.ts, navy.ts seaField).
// With the seam unset, or set and handing nothing back, the rules fight every action exactly as before; a played
// action's ratio and ships lost stand in place of the dice, and nothing else changes.
import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import { runOut } from '../src/battle/core/clock';
import { closeWith, foughtOf, meetingKey, playedHook, seaAhead, type SeaPlayed } from '../src/battle/sea/handoff';
import { RULES_CLASSES, SEA } from '../src/battle/sea/numbers';
import { diceRange, sunkCaps } from '../src/battle/sea/result';
import { SeaSim, type SeaLosses } from '../src/battle/sea/sim';
import { createGame } from '../src/campaign/game';
import { afloatWatch, sail, seaBattles, seaField, shipCount, toughOf, zoneOf, type AfloatTold, type SeaFought, type SeaMeeting } from '../src/campaign/navy';
import { CARRIERS } from '../src/campaign/air';
import { SHIP_RESEARCH, dcOf } from '../src/campaign/tech';
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

describe("a navy's carriers grow by its research (sea.json carriers; the design director, 20:56Z)", () => {
  it("the sea ticket carries Carrier task forces off the nation's record where the campaign keeps research, and no other research; none where it keeps none", () => {
    const { g } = meet(5);
    const sides = (): { att?: string[]; def?: string[]; nats: string[]; way: unknown } => {
      const t = seaAhead(g, ukClose)[0].ticket, s = new SeaSim(t) as any;
      return { att: t.att.doctrine, def: t.def.doctrine, nats: [t.att.nat, t.def.nat], way: s.carrierWay('att') };
    };
    expect(sides().nats).toEqual(['UK', 'GER']);
    g.flags.open = 1;
    g.nations.UK.tech = { done: ['carriers', 'radar'], work: [] };
    expect(sides().att).toBeUndefined();
    g.nations.UK.tech = { done: ['carriers', 'taskforces', 'radar'], work: [] };
    const held = sides();
    expect(held.att).toEqual(['taskforces']);
    expect(held.def).toBeUndefined();
    expect(held.way).toEqual(SEA.carriers.path.UK[1]);
    // Where the campaign keeps no research, none is read, whatever the record holds.
    g.flags.open = 0;
    expect(sides().att).toBeUndefined();
    expect(sides().way).toEqual(SEA.carriers.path.UK[0]);
  });
});

describe("damage control on the field (ruling 240, C3: the hits to sink a fleet's surface ships times strike.dc, by research)", () => {
  // The switch is flipped in memory and put back after every test, as tests/damage-control-240.test.ts does.
  const R = SHIP_RESEARCH()!;
  let was = false;
  beforeEach(() => { was = R.dc.on; });
  afterEach(() => { R.dc.on = was; });
  const DC = CARRIERS.strike.dc;
  /** The Norwegian Sea action of meet(5) in a grand campaign, the British holding the research or not. */
  const grand = (on: boolean, held: boolean): { g: Game; home: Fleet; battle: Fleet } => {
    R.dc.on = on;
    const m = meet(5);
    m.g.flags.open = 1;
    m.g.nations.UK.tech = { done: held ? ['damagecontrol'] : [], work: [] };
    return m;
  };

  it("with the switch off, or without the research, a fleet's ticket carries no toughness and no kit, and the field's limit is navy.json's: the ticket is the same to the byte", () => {
    // (The switch is read when it is asked, so each ticket is written with the switch as that case has it.)
    const off = grand(false, true);
    expect(dcOf(off.g, 'UK')).toBe(1);
    const t = seaAhead(off.g, ukClose)[0].ticket;
    const none = grand(true, false);
    expect(dcOf(none.g, 'UK')).toBe(1);
    const u = seaAhead(none.g, ukClose)[0].ticket;
    for (const f of [...t.place.fleets, ...u.place.fleets]) { expect(f.tough).toBeUndefined(); expect(f.kit).toBeUndefined(); }
    expect(JSON.stringify(u)).toBe(JSON.stringify(t));
    expect(sunkCaps(t.place.fleets, t.reckoning.odds)).toEqual(sunkCaps(t.place.fleets, t.reckoning.odds, true));
  });

  it("with it on and the research held, the fleet's ticket carries its own toughness by class (navy.json's over strike.dc) and its kit; the field's limit by class is the dice's at that toughness, the enemy's untouched", () => {
    const { g, home } = grand(true, true);
    expect(dcOf(g, 'UK')).toBe(DC);
    const t = seaAhead(g, ukClose)[0].ticket;
    const uk = t.place.fleets.find((f) => f.nat === 'UK')!, ger = t.place.fleets.find((f) => f.nat === 'GER')!;
    expect(uk.side).toBe('att');
    for (const c of SURFACE) {
      expect(uk.tough![c], c).toBe(toughOf(g, home, c));
      expect(uk.tough![c], c).toBeCloseTo(RULES_CLASSES[c].tough / DC, 12);
    }
    expect(uk.kit).toEqual({ stage: null, vt: false, dc: true });
    expect(ger.tough).toBeUndefined();
    expect(ger.kit).toBeUndefined();
    const caps = sunkCaps(t.place.fleets, t.reckoning.odds), plain = sunkCaps(t.place.fleets, t.reckoning.odds, true);
    for (const c of SURFACE) expect(caps.att[c], c).toBeLessThanOrEqual(plain.att[c]);
    expect(SURFACE.some((c) => caps.att[c] < plain.att[c])).toBe(true);
    expect(caps.def).toEqual(plain.def);
  });

  it("on the field a ship that goes out burning where navy.json's limit would have sunk her is one its damage control kept afloat; the campaign books her afloat, its watcher hears of her, and the battle's news tells her", () => {
    const { g, home, battle } = grand(true, true);
    const [a] = seaAhead(g, ukClose);
    const t = a.ticket;
    const caps = sunkCaps(t.place.fleets, t.reckoning.odds), plain = sunkCaps(t.place.fleets, t.reckoning.odds, true);
    const sim = new SeaSim(t) as any;
    const c = SURFACE.find((k) => caps.att[k] < plain.att[k])!;
    // Every British ship of that class goes, one by one: the first go down to the limit, the next burn; of those that
    // burn, as many as navy.json's limit would still have sunk are the ones damage control kept afloat.
    let n = 0;
    for (const u of sim.units) if (u.side === 'att' && u.extra.cls === c) while (u.extra.ships > 0) { sim.loseShip(u); n++; }
    expect(n).toBeGreaterThanOrEqual(plain.att[c]);
    const x0 = sim.fleetLoss;
    expect(x0.sunk[home.id][c]).toBe(Math.min(n, caps.att[c]));
    expect(x0.burnt[home.id][c]).toBe(n - Math.min(n, caps.att[c]));
    expect(sim.kept[home.id][c]).toBe(Math.min(n, plain.att[c]) - Math.min(n, caps.att[c]));
    for (const k of SURFACE) if (k !== c) expect(sim.kept[home.id][k]).toBe(0);
    expect(sim.kept[battle.id]).toBeUndefined();
    sim.finish('att', 'rout');
    const x = sim.over!.extra as unknown as SeaLosses;
    expect(x.kept).toEqual({ [home.id]: { ...{ bb: 0, cv: 0, ca: 0, dd: 0 }, [c]: sim.kept[home.id][c] } });
    const fought = foughtOf(sim.over!);
    expect(fought.kept).toEqual(x.kept);
    const told: AfloatTold[] = [];
    const tell = afloatWatch.tell;
    afloatWatch.tell = (w) => { told.push(w); };
    try { closeWith(g, new Map([[a.key, { meeting: a.meeting, fought }]]), ukClose); } finally { afloatWatch.tell = tell; }
    const rep = g.seaReports[g.seaReports.length - 1];
    for (const k of SURFACE) expect(rep.att[0].sunk[k], k).toBe(x.sunk[home.id][k]);
    const mine = told.find((w) => w.route === 'action' && w.nat === 'UK')!;
    expect(mine.saved[c]).toBe(sim.kept[home.id][c]);
    expect(told.find((w) => w.route === 'action' && w.nat === 'GER')!.saved).toEqual({ bb: 0, cv: 0, ca: 0, dd: 0, ss: 0 });
    const news = g.headlines.filter((h) => h.turn === g.turn && h.kind === 'battle').map((h) => h.text).join(' ');
    expect(news).toMatch(/of the Home Fleet (is|are) badly hit, but the repair parties keep the ships? afloat\./);
  });

  it("handed back to the dice, the dice keep ships afloat as an action they fight does, and the field's own count is not added on top", () => {
    const { g, home } = grand(true, true);
    const [a] = seaAhead(g, ukClose);
    const told: AfloatTold[] = [];
    const tell = afloatWatch.tell;
    afloatWatch.tell = (w) => { told.push(w); };
    const fought: SeaFought = { R: 0.6, sunk: {}, toDice: true, kept: { [home.id]: { dd: 50 } } };
    try { closeWith(g, new Map([[a.key, { meeting: a.meeting, fought }]]), ukClose); } finally { afloatWatch.tell = tell; }
    expect(told.find((w) => w.route === 'action' && w.nat === 'UK')!.saved.dd).toBeLessThan(50);
  });
});
