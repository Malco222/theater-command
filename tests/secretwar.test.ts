// The secret war (rulings 151(c), 135 and 153), each half behind its own switch: spy hunters, who uncover the other
// side's real agents on the player's ground so that they can be arrested before they strike; and stolen orders, an
// enemy field's bombing order for next month read by the player's agent, flown as written, met in full.
import { afterEach, describe, expect, it } from 'vitest';
import { airAI, assessAir, fieldOrder } from '../src/campaign/air';
import { createGame, setWar } from '../src/campaign/game';
import { INTEL, foesFound, intelTick, mostAgents, setMission, spyRoom, threat, whyNotArrest, whyNotMission, arrest } from '../src/campaign/intel';
import { levelIndex } from '../src/campaign/difficulty';
import { loadGame, saveGame } from '../src/campaign/save';
import { setAirPiece } from '../src/campaign/airversion';
import { doublesOn, everyoneOn, flyingOrder, huntersOn, ordersDue, ordersOn, setSecretWar, watchersOn } from '../src/campaign/secretwar';
import { startOpen } from '../src/campaign/staff';
import type { Agent, Game, Mission } from '../src/campaign/types';

const free = (seed = 7, human: string | null = 'UK'): Game => startOpen(createGame(seed, human));
const keep = structuredClone({ enemy: INTEL.enemy, missions: INTEL.missions, secret: INTEL.secret });
afterEach(() => {
  Object.assign(INTEL.enemy, structuredClone(keep.enemy));
  Object.assign(INTEL.missions, structuredClone(keep.missions));
  Object.assign(INTEL.secret, structuredClone(keep.secret));
});
const plant = (g: Game, at: string, mission: Mission, nat = 'UK'): Agent => {
  const a: Agent = { id: g.intel.next++, nat, name: `Test ${g.intel.next}`, xp: 0, reg: g.byName[at].id, ready: g.turn, mission, born: g.turn };
  g.intel.agents.push(a);
  return a;
};
/** Every month the other side's services plot, at once. */
const busy = (g: Game): void => { INTEL.enemy.chance = 1; INTEL.enemy.quiet = 0; g.flags.spyfrom = 0; };

describe('the switches', () => {
  it('are on for a new campaign (Brian, 22 November, release 2) and keep with the save; off, nothing new is offered or kept', () => {
    const g = free();
    expect(huntersOn(g)).toBe(true);
    expect(ordersOn(g)).toBe(true);
    expect(everyoneOn(g)).toBe(true);
    expect(doublesOn(g)).toBe(true);
    // Switched off, the campaign plays the old way.
    setSecretWar(g, 'hunt', false);
    setSecretWar(g, 'orders', false);
    setSecretWar(g, 'all', false);
    setSecretWar(g, 'double', false);
    setAirPiece(g, 'field', false);
    setWar(g, 'UK', 'GER');
    const home = plant(g, 'London', 'guard');
    const away = plant(g, 'Ruhr', 'watch');
    const room = spyRoom(g)!;
    expect(room.agents.find((a) => a.id === home.id)!.missions.map((m) => m.id)).toEqual(['guard']);
    expect(room.agents.find((a) => a.id === away.id)!.missions.map((m) => m.id)).toEqual(['watch', 'books', 'factory', 'rail', 'oil', 'bbPlans', 'cvPlans', 'bomberPlans', 'jetPlans', 'rocketPlans', 'guidedPlans']);
    expect(room.found).toBeUndefined();
    expect(room.orders).toBeUndefined();
    busy(g);
    intelTick(g, 'UK');
    expect(g.intel.foes).toBeUndefined();
    setSecretWar(g, 'hunt', true);
    setSecretWar(g, 'orders', true);
    const back = loadGame(saveGame(g))!;
    expect(huntersOn(back) && ordersOn(back)).toBe(true);
    expect(everyoneOn(back)).toBe(false);
  });

  it('a campaign begun with the data switches off takes none of them, and a historical scenario never has them', () => {
    INTEL.secret.hunters.on = false;
    INTEL.secret.orders.on = false;
    INTEL.secret.everyone.on = false;
    INTEL.secret.doubles.on = false;
    INTEL.secret.watchers.on = false;
    // (and the network the service opens with, the spies thread of 24 November 2026: tests/spies-network-r2.test.ts)
    INTEL.secret.network.on = false;
    const g = free();
    expect(huntersOn(g) || ordersOn(g) || everyoneOn(g) || doublesOn(g) || watchersOn(g)).toBe(false);
    expect(Object.keys(g.flags).some((k) => k.startsWith('sw:'))).toBe(false);
    const h = createGame(7, 'UK');
    setSecretWar(h, 'all', true);
    expect(everyoneOn(h)).toBe(false);
  });
});

describe('spies who hunt spies (ruling 151(c))', () => {
  it("the other side's plots become hidden agents, who strike after 'lead' months; the rate in sum is the same", () => {
    const g = free();
    setWar(g, 'UK', 'GER');
    setSecretWar(g, 'hunt', true);
    // (The watchers, release 2, have tests of their own: tests/watchers.test.ts.)
    setSecretWar(g, 'watch', false);
    busy(g);
    const before = g.headlines.length;
    intelTick(g, 'UK');
    const foes = g.intel.foes ?? [];
    expect(foes.length).toBeGreaterThan(0);
    expect(foes.every((f) => f.strike === g.turn + INTEL.secret.hunters.lead && f.found === undefined)).toBe(true);
    // Hidden: nothing is said of them, and the player sees none.
    expect(g.headlines.slice(before).some((h) => /Saboteurs|sabotage/.test(h.text))).toBe(false);
    expect(foesFound(g)).toEqual([]);
    INTEL.enemy.chance = 0;
    g.turn += INTEL.secret.hunters.lead;
    intelTick(g, 'UK');
    expect(g.headlines.slice(before).some((h) => /^Saboteurs|^Grounded this month: saboteurs/.test(h.text))).toBe(true);
    expect(g.intel.foes!.length).toBe(0);
  });

  it('a hunter uncovers an agent in its province or beside it; ARREST ends it before it strikes; nothing more is shown', () => {
    const g = free();
    setWar(g, 'UK', 'GER');
    setSecretWar(g, 'hunt', true);
    INTEL.secret.hunters.here = INTEL.secret.hunters.near = 1;
    g.intel.foes = [{ id: 1, nat: 'GER', reg: g.byName.London.id, kind: 'factory', strike: g.turn + 2 }];
    const hunter = plant(g, 'London', 'guard');
    expect(whyNotMission(g, 'UK', hunter.id, 'hunt')).toBeNull();
    expect(setMission(g, 'UK', hunter.id, 'hunt')).toBe(true);
    expect(whyNotArrest(g, 'UK', 1)).toBe('Nobody knows where that agent is.');
    intelTick(g, 'UK');
    expect(hunter.mission).toBe('hunt');
    expect(foesFound(g).map((f) => f.id)).toEqual([1]);
    expect(g.headlines.some((h) => /^Agent .+ uncovers a German agent at London\. Arrest the agent before they strike\.$/.test(h.text))).toBe(true);
    expect(spyRoom(g)!.found![0].text).toMatch(/^A German agent at London, uncovered this month$/);
    expect(arrest(g, 'UK', 1)).toBe(true);
    expect(g.intel.foes).toEqual([]);
    expect(g.headlines.at(-1)!.text).toBe('A German agent is arrested at London.');
    // Abroad no agent hunts.
    const away = plant(g, 'Ruhr', 'watch');
    expect(whyNotMission(g, 'UK', away.id, 'hunt')).toBe('An agent hunts enemy agents only on friendly ground.');
  });

  it('an agent left at large strikes in its month, unless a guard stops it', () => {
    const g = free();
    setWar(g, 'UK', 'GER');
    setSecretWar(g, 'hunt', true);
    const at = g.regions.find((r) => r.ctrl === 'UK' && r.owner === 'UK' && r.fac > 1)!;
    g.intel.foes = [{ id: 1, nat: 'GER', reg: at.id, kind: 'factory', strike: g.turn, found: g.turn - 1 }];
    intelTick(g, 'UK');
    expect(g.headlines.some((h) => h.text.startsWith(`Saboteurs wreck a factory at ${at.name}.`))).toBe(true);
  });

  it('an uncovered agent whose country has made peace or fallen is no longer listed: it will not strike', () => {
    for (const end of ['peace', 'fall'] as const) {
      const g = free();
      setWar(g, 'UK', 'GER');
      setWar(g, 'UK', 'HUN');
      setSecretWar(g, 'hunt', true);
      const London = g.byName.London.id;
      g.intel.foes = [
        { id: 1, nat: 'HUN', reg: London, kind: 'factory', strike: g.turn + 2, found: g.turn },
        { id: 2, nat: 'GER', reg: London, kind: 'rail', strike: g.turn + 2, found: g.turn },
      ];
      expect(foesFound(g).map((f) => f.id)).toEqual([1, 2]);
      if (end === 'peace') {
        g.nations.UK.atWar = g.nations.UK.atWar.filter((x) => x !== 'HUN');
        g.nations.HUN.atWar = g.nations.HUN.atWar.filter((x) => x !== 'UK');
      } else g.nations.HUN.alive = false;
      expect(foesFound(g).map((f) => f.id)).toEqual([2]);
      expect(spyRoom(g)!.found!.map((f) => f.id)).toEqual([2]);
      // Only the list: the agent itself is kept as before, and listed again if the war comes back.
      expect(g.intel.foes.map((f) => f.id)).toEqual([1, 2]);
      if (end === 'peace') { setWar(g, 'UK', 'HUN'); expect(foesFound(g).map((f) => f.id)).toEqual([1, 2]); }
    }
  });
});

describe('stealing the orders (ruling 135)', () => {
  /** Britain's player at war with Germany, a German bomber group on a field in reach of England, and an agent there. */
  function scene(): { g: Game; a: Agent; field: number } {
    const g = free(7, 'UK');
    g.turn = 12;
    setWar(g, 'UK', 'GER');
    setSecretWar(g, 'orders', true);
    const field = g.byName.Ruhr.id;
    const b = g.airFleets.filter((f) => f.nat === 'GER' && f.bmr > 0 && f.ship === undefined).sort((x, y) => y.bmr - x.bmr)[0];
    b.base = field;
    b.bmr = Math.max(b.bmr, 300);
    b.ready = 1;
    return { g, a: plant(g, 'Ruhr', 'watch'), field };
  }

  it('is offered abroad with its switch where an enemy bomber group is based, and says why not', () => {
    const { g, a } = scene();
    const offer = spyRoom(g)!.agents.find((x) => x.id === a.id)!.missions.find((m) => m.id === 'orders')!;
    expect(offer.label).toBe('Steal the orders');
    expect(offer.success).toBeNull();
    const none = plant(g, 'Hanover', 'watch');
    for (const f of g.airFleets) if (f.base === none.reg) f.base = g.byName.Ruhr.id;
    expect(whyNotMission(g, 'UK', none.id, 'orders')).toBe('No enemy bomber group flies from Hanover.');
  });

  it('on a success the order is written now and read; next month the staff flies exactly that, and fighters meet it in full', () => {
    const { g, a, field } = scene();
    INTEL.missions.orders.success = 1;
    INTEL.missions.orders.caught = 0;
    const want = fieldOrder(g, 'GER', field);
    expect(setMission(g, 'UK', a.id, 'orders')).toBe(true);
    intelTick(g, 'UK');
    const book = g.intel.orders ?? [];
    if (!want) {
      expect(book.length).toBe(0);
      expect(g.headlines.at(-1)!.text).toMatch(/no German raid is ordered from the field for next month\.$/);
      return;
    }
    expect(book.length).toBe(1);
    const o = book[0];
    expect(o).toMatchObject({ nat: 'GER', field, target: want.target, mission: want.mission, turn: g.turn + 1, agent: a.name, by: 'UK' });
    expect(g.headlines.some((h) => h.text.startsWith(`Agent ${a.name}, the Ruhr: `) && h.text.includes(`against ${g.regions[o.target].name} next month, ${o.mission === 'night' ? 'by night' : 'by day'}, about ${o.bombers.toLocaleString('en-US')} bombers.`))).toBe(true);
    expect(spyRoom(g)!.orders!.length).toBe(1);
    // Fighters that meet a read raid meet it in full: never fewer than without the agent's report.
    const fleets = g.airFleets.filter((f) => o.groups.includes(f.id));
    const plain = assessAir(g, fleets, o.mission, o.target);
    g.turn++;
    expect(ordersDue(g, 'GER').length).toBe(1);
    flyingOrder(g, o.target);
    o.turn = g.turn;
    const met = assessAir(g, fleets, o.mission, o.target);
    flyingOrder(g, null);
    expect(met.D).toBeGreaterThanOrEqual(plain.D);
    if (met.D > 0) expect(met.lines.some((l) => l.label === 'Intelligence' && l.value === `Met by fighters posted on Agent ${a.name}'s report`)).toBe(true);
    // The staff's month: the order is flown first, as written.
    const reports = g.airReports.length;
    airAI(g, 'GER');
    expect(o.done).toBe('flown');
    const flown = g.airReports.slice(reports).find((r) => r.attNat === 'GER' && r.region === o.target);
    expect(flown?.mission).toBe(o.mission);
  });

  it('it does not always work: nothing found; caught, and the orders change; or the order lapses', () => {
    const { g, a } = scene();
    INTEL.missions.orders.success = 0;
    setMission(g, 'UK', a.id, 'orders');
    intelTick(g, 'UK');
    expect(g.intel.notes.at(-1)!.text).toMatch(/finds nothing at Ruhr this month/);
    // Lapsed: the group has gone before its month.
    const s = scene();
    INTEL.missions.orders.success = 1;
    INTEL.missions.orders.caught = 0;
    setMission(s.g, 'UK', s.a.id, 'orders');
    intelTick(s.g, 'UK');
    const o = s.g.intel.orders?.[0];
    if (o) {
      s.g.turn++;
      for (const f of s.g.airFleets) if (o.groups.includes(f.id)) f.base = s.g.byName.Berlin.id;
      airAI(s.g, 'GER');
      expect(o.done).toBe('lapsed');
      expect(s.g.headlines.some((h) => h.text.startsWith(`The order against ${s.g.regions[o.target].name} was cancelled: `))).toBe(true);
    }
    // Caught at the field: the orders it read are changed.
    const c = scene();
    INTEL.missions.orders.success = 1;
    INTEL.missions.orders.caught = 0.95;
    setMission(c.g, 'UK', c.a.id, 'orders');
    let caught = false;
    for (let seed = 1; seed < 30 && !caught; seed++) {
      const t = scene();
      t.g.seed = seed;
      setMission(t.g, 'UK', t.a.id, 'orders');
      intelTick(t.g, 'UK');
      if (!t.g.intel.agents.includes(t.a)) {
        caught = true;
        if (t.g.intel.orders?.length) {
          expect(t.g.intel.orders.every((x) => x.done === 'lapsed')).toBe(true);
          expect(t.g.headlines.some((h) => /The enemy changes the orders/.test(h.text))).toBe(true);
        }
      }
    }
    expect(caught).toBe(true);
  });
});

describe('every country spies on the player (Brian, 22 November, release 2)', () => {
  const GREAT = (): string[] => INTEL.enemy.services;

  it('a small nation at war with the player has a service, at the smaller rate; a great power keeps its rate exactly', () => {
    const g = free();
    setWar(g, 'UK', 'GER');
    setWar(g, 'UK', 'HUN');
    g.flags.spyfrom = -99;
    const T = threat(g, 'UK');
    expect(T.foes).toEqual(expect.arrayContaining(['GER', 'HUN']));
    expect(T.each).toBeCloseTo(INTEL.enemy.chance * INTEL.enemy.level[levelIndex(g)], 12);
    expect(T.rate.GER).toBe(T.each);
    expect(T.rate.HUN).toBe(T.each * INTEL.secret.everyone.smaller);
    expect(T.chance).toBeCloseTo(1 - T.foes.reduce((p, f) => p * (1 - T.rate[f]), 1), 12);
    // The great powers' rate is the same with the switch off: only the small nations go.
    const off = free();
    setWar(off, 'UK', 'GER');
    setWar(off, 'UK', 'HUN');
    off.flags.spyfrom = -99;
    setSecretWar(off, 'all', false);
    const O = threat(off, 'UK');
    expect(O.foes).not.toContain('HUN');
    expect(O.each).toBe(T.each);
    for (const f of O.foes) expect(O.rate[f]).toBe(T.rate[f]);
    // The room says each foe's chance.
    expect(spyRoom(g)!.threat.rate).toEqual(T.rate);
  });

  it('with spy hunters on, a small nation puts its own hidden agents on the player\'s ground', () => {
    const g = free();
    setWar(g, 'UK', 'HUN');
    busy(g);
    INTEL.secret.everyone.smaller = 1;
    const before = g.headlines.length;
    intelTick(g, 'UK');
    const theirs = (g.intel.foes ?? []).filter((f) => f.nat === 'HUN');
    expect(theirs.length).toBe(1);
    expect(theirs[0].found).toBeUndefined();
    expect(theirs[0].strike).toBe(g.turn + INTEL.secret.hunters.lead);
    expect(g.regions[theirs[0].reg].ctrl).toBe('UK');
    // Hidden until it strikes; then the police blame Hungarian agents.
    expect(g.headlines.slice(before).some((h) => /Hungarian/.test(h.text))).toBe(false);
    INTEL.enemy.chance = 0;
    g.turn += INTEL.secret.hunters.lead;
    intelTick(g, 'UK');
    expect(g.headlines.slice(before).some((h) => /Your police blame Hungarian agents\./.test(h.text))).toBe(true);
  });

  it('works for a small nation\'s player too: every nation at war with him plots', () => {
    const g = free(7, 'NOR');
    setWar(g, 'NOR', 'GER');
    setWar(g, 'NOR', 'FIN');
    g.flags.spyfrom = -99;
    const T = threat(g, 'NOR');
    expect(T.foes).toEqual(expect.arrayContaining(['GER', 'FIN']));
    expect(T.rate.GER).toBe(T.each);
    expect(T.rate.FIN).toBe(T.each * INTEL.secret.everyone.smaller);
  });

  it('a service never runs more hidden agents on the player\'s ground at once than it could run at all', () => {
    const g = free();
    setWar(g, 'UK', 'GER');
    setWar(g, 'UK', 'HUN');
    busy(g);
    INTEL.secret.everyone.smaller = 1;
    INTEL.secret.hunters.lead = 100;
    for (let k = 0; k < 20; k++) {
      intelTick(g, 'UK');
      for (const nat of ['GER', 'HUN']) expect((g.intel.foes ?? []).filter((f) => f.nat === nat).length).toBeLessThanOrEqual(mostAgents(nat, g));
      g.turn++;
    }
    expect((g.intel.foes ?? []).filter((f) => f.nat === 'HUN').length).toBe(mostAgents('HUN', g));
    expect((g.intel.foes ?? []).filter((f) => f.nat === 'GER').length).toBe(mostAgents('GER', g));
  });

  it('with the switch off, only the great powers work against him, and the month goes as before', () => {
    // A German player in 1939 is at war with Poland and Canada as well as the great powers.
    const g = startOpen(createGame(3, 'GER'));
    setSecretWar(g, 'all', false);
    g.flags.spyfrom = -99;
    const smallFoes = g.nations.GER.atWar.filter((f) => !GREAT().includes(f));
    expect(smallFoes.length).toBeGreaterThan(0);
    const T = threat(g, 'GER');
    expect(T.foes.every((f) => GREAT().includes(f))).toBe(true);
    expect(T.foes).toEqual(g.order.filter((f) => GREAT().includes(f) && g.nations[f].alive && g.nations.GER.atWar.includes(f)));
    // The old sum, to the last bit.
    expect(T.chance).toBe(1 - Math.pow(1 - T.each, T.foes.length));
    for (const f of T.foes) expect(T.rate[f]).toBe(T.each);
    // A year of the secret war: nothing comes from a small country's service.
    busy(g);
    INTEL.enemy.chance = 0.5;
    for (let k = 0; k < 12; k++) {
      intelTick(g, 'GER');
      expect((g.intel.foes ?? []).every((f) => GREAT().includes(f.nat))).toBe(true);
      g.turn++;
    }
    const adj = smallFoes.map((f) => g.nations[f].adj);
    expect(g.headlines.some((h) => adj.some((a) => h.text.includes(`${a} agent`)))).toBe(false);
  });
});
