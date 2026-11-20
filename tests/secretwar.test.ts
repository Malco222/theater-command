// The secret war (rulings 151(c), 135 and 153), each half behind its own switch: spy hunters, who uncover the other
// side's real agents on the player's ground so that they can be arrested before they strike; and stolen orders, an
// enemy field's bombing order for next month read by the player's agent, flown as written, met in full.
import { afterEach, describe, expect, it } from 'vitest';
import { airAI, assessAir, fieldOrder } from '../src/campaign/air';
import { createGame, setWar } from '../src/campaign/game';
import { INTEL, foesFound, intelTick, setMission, spyRoom, whyNotArrest, whyNotMission, arrest } from '../src/campaign/intel';
import { loadGame, saveGame } from '../src/campaign/save';
import { flyingOrder, huntersOn, ordersDue, ordersOn, setSecretWar } from '../src/campaign/secretwar';
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
  it('are off for a new campaign and keep with the save; off, nothing new is offered or kept', () => {
    const g = free();
    expect(huntersOn(g)).toBe(false);
    expect(ordersOn(g)).toBe(false);
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
  });
});

describe('spies who hunt spies (ruling 151(c))', () => {
  it("the other side's plots become hidden agents, who strike after 'lead' months; the rate in sum is the same", () => {
    const g = free();
    setWar(g, 'UK', 'GER');
    setSecretWar(g, 'hunt', true);
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
    expect(g.headlines.some((h) => /^Agent .+ uncovers a German agent at London\. Arrest it before it strikes\.$/.test(h.text))).toBe(true);
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
