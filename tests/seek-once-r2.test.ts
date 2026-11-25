// One battle sought a month (release 2, navy.ts; the design director's ruling of 25 November 2026, 02:38Z). seaBattles
// runs at the close of every nation's month and takes onto the seeking side every allied fleet in the sea at war with a
// foe there, so one fleet could seek battle in its ally's meeting and again in its own; and a fleet sent at the enemy by
// order could seek again at its month's end where the order missed. With the switch on, the seeking side of every
// action is marked with the month and seeks no more that month; an order counts whether it finds or misses; defenders
// are never marked. Behind its own switch (stage.json 'seekOnce'): off, nothing is written.
import { describe, expect, it } from 'vitest';
import stage from '../data/campaign/stage.json';
import { createGame } from '../src/campaign/game';
import { setFog } from '../src/campaign/intel';
import { orderFleetAttack, planFleetAttack, setFleetAttack, whyNotFleetAttack } from '../src/campaign/fleetattack';
import { NAVY, hasSought, seaBattles, seekOnceOn, setSeekOnce, zoneOf } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { SEEKONCE_STAGE } from '../src/campaign/stage';
import { startOpen } from '../src/campaign/staff';
import { humanUp, stepAI } from '../src/campaign/turn';
import type { Fleet, Game, SeaReport } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

const NAT: Record<string, string> = { 'Home Fleet': 'UK', 'Atlantic Fleet': 'FRA', 'Battle Fleet': 'GER' };
const fleet = (g: Game, name: string): Fleet => g.fleets.find((f) => f.name === name && f.nat === NAT[name])!;
const seekers = (r: SeaReport): string[] => r.att.map((s) => s.fleet);

/**
 * A grand campaign of the computer's, with the Home Fleet and the French Atlantic Fleet patrolling the Norwegian Sea and
 * the German Battle Fleet in it too: Britain's meeting finds it.
 */
function northSea(on: boolean, seed = 5): Game {
  const g = startOpen(createGame(seed, null));
  if (on) setSeekOnce(g, true);
  const norw = zoneOf(g, 'norw');
  for (const name of ['Home Fleet', 'Atlantic Fleet', 'Battle Fleet']) {
    const f = fleet(g, name);
    f.zone = norw;
    f.stance = 'patrol';
  }
  return g;
}

/** The German Battle Fleet put back to sea, whole, as if it had sailed in after Britain's meeting. */
function backAtSea(g: Game, whole: Fleet['ships']): void {
  const b = fleet(g, 'Battle Fleet');
  b.ships = { ...whole };
  b.ready = 1;
  b.zone = zoneOf(g, 'norw');
  b.stance = 'patrol';
}

/** A grand campaign with Britain at the table in its own month, fleet attacks on and the fog lifted. */
function britain(on: boolean, seed = 3): Game {
  const g = startOpen(createGame(seed, 'UK'));
  let guard = 0;
  while (!humanUp(g) && guard++ < 50) stepAI(g);
  expect(humanUp(g)).toBe(true);
  setFleetAttack(g, true);
  if (on) setSeekOnce(g, true);
  setFog(g, false);
  return g;
}

describe('the switch', () => {
  it('ships off, and off no fleet is marked and every meeting reads as before', () => {
    expect((stage as { seekOnce?: boolean }).seekOnce).toBe(false);
    expect(SEEKONCE_STAGE).toBe(false);
    const g = northSea(false);
    expect(seekOnceOn(g)).toBe(false);
    expect(g.flags.seekOnce).toBeUndefined();
    const reps = seaBattles(g, 'UK');
    expect(reps.length).toBe(1);
    expect(g.fleets.every((f) => f.sought === undefined)).toBe(true);
    expect(g.fleets.every((f) => !hasSought(g, f))).toBe(true);
  });

  it('is the grand campaign only, and can be turned on and off in a campaign under way', () => {
    const g = createGame(5, null);
    setSeekOnce(g, true);
    expect(seekOnceOn(g)).toBe(false);
    const o = northSea(true);
    expect(seekOnceOn(o)).toBe(true);
    setSeekOnce(o, false);
    expect(seekOnceOn(o)).toBe(false);
    expect(o.flags.seekOnce).toBeUndefined();
  });

  it('the same battle is fought with it on or off: only the mark differs', () => {
    const a = northSea(false), b = northSea(true);
    const ra = seaBattles(a, 'UK'), rb = seaBattles(b, 'UK');
    expect(JSON.stringify(rb)).toBe(JSON.stringify(ra));
  });
});

describe("an ally's meeting", () => {
  it('off, the Home Fleet seeks battle in Britain\'s meeting and again in France\'s; on, once', () => {
    for (const on of [false, true]) {
      const g = northSea(on);
      const whole = { ...fleet(g, 'Battle Fleet').ships };
      const first = seaBattles(g, 'UK');
      expect(first.length).toBe(1);
      expect(seekers(first[0])).toEqual(expect.arrayContaining(['Home Fleet', 'Atlantic Fleet']));
      backAtSea(g, whole);
      const second = seaBattles(g, 'FRA');
      if (on) {
        expect(second).toEqual([]);
        expect(fleet(g, 'Home Fleet').sought).toBe(g.turn);
        expect(fleet(g, 'Atlantic Fleet').sought).toBe(g.turn);
      } else {
        expect(second.length).toBe(1);
        expect(seekers(second[0])).toContain('Home Fleet');
      }
    }
  });

  it('defenders are never marked: a fleet that was attacked can be attacked again', () => {
    const g = northSea(true);
    const whole = { ...fleet(g, 'Battle Fleet').ships };
    seaBattles(g, 'UK');
    expect(fleet(g, 'Battle Fleet').sought).toBeUndefined();
    // A fresh British fleet that has not sought battle finds the Germans back at sea, and they fight it.
    const h = fleet(g, 'Home Fleet');
    h.sought = undefined;
    fleet(g, 'Atlantic Fleet').stance = 'port';
    backAtSea(g, whole);
    const again = seaBattles(g, 'UK');
    expect(again.length).toBe(1);
    expect(again[0].def.map((s) => s.fleet)).toContain('Battle Fleet');
  });

  it('next month the mark is spent', () => {
    const g = northSea(true);
    seaBattles(g, 'UK');
    const h = fleet(g, 'Home Fleet');
    expect(hasSought(g, h)).toBe(true);
    g.turn++;
    expect(hasSought(g, h)).toBe(false);
  });

  it('the mark keeps through a save', () => {
    const g = northSea(true);
    seaBattles(g, 'UK');
    const back = loadGame(saveGame(g))!;
    expect(hasSought(back, fleet(back, 'Home Fleet'))).toBe(true);
  });
});

describe('the order', () => {
  it('counts as the month\'s seek though it missed: the month-end meeting does not try again', () => {
    const was = NAVY.battle.catchRaider;
    try {
      for (const on of [false, true]) {
        const g = britain(on);
        const home = fleet(g, 'Home Fleet'), battle = fleet(g, 'Battle Fleet');
        // The Battle Fleet raids the North Sea, and the Home Fleet's search misses it.
        battle.zone = zoneOf(g, 'nsea');
        battle.stance = 'raid';
        NAVY.battle.catchRaider = 0;
        const out = orderFleetAttack(g, home.id, battle.id)!;
        NAVY.battle.catchRaider = was;
        expect(out.outcome).toBe('missed');
        expect(home.stance).toBe('patrol');
        expect(home.zone).toBe(battle.zone);
        expect(home.sought).toBe(on ? g.turn : undefined);
        // At the month's end the German stands to fight in the same sea.
        battle.stance = 'patrol';
        const reps = seaBattles(g, 'UK').filter((r) => seekers(r).includes('Home Fleet'));
        expect(reps.length).toBe(on ? 0 : 1);
      }
    } finally {
      NAVY.battle.catchRaider = was;
    }
  });

  it('a fleet that sought battle in an earlier meeting is refused, with the reason on the card', () => {
    const g = britain(true);
    const home = fleet(g, 'Home Fleet'), battle = fleet(g, 'Battle Fleet');
    expect(whyNotFleetAttack(g, home.id, battle.id)).toBeNull();
    home.sought = g.turn;
    const why = whyNotFleetAttack(g, home.id, battle.id);
    expect(why).toMatch(/already sought battle this month: a fleet seeks battle once a month/);
    expect(planFleetAttack(g, home.id, battle.id)!.why).toBe(why);
    expect(orderFleetAttack(g, home.id, battle.id)).toBeNull();
    // Off, the same fleet can be sent.
    setSeekOnce(g, false);
    expect(whyNotFleetAttack(g, home.id, battle.id)).toBeNull();
  });

  it('a fleet of his in that sea that has already sought battle does not join the attack', () => {
    const g = britain(true);
    const home = fleet(g, 'Home Fleet'), battle = fleet(g, 'Battle Fleet');
    battle.zone = zoneOf(g, 'nsea');
    battle.stance = 'patrol';
    const other = g.fleets.find((f) => f.nat === 'UK' && f.id !== home.id && f.stance !== 'port' && f.ships.bb + f.ships.ca + f.ships.dd > 0)!;
    other.zone = battle.zone;
    other.stance = 'patrol';
    other.sought = g.turn;
    const out = orderFleetAttack(g, home.id, battle.id)!;
    expect(out.outcome).not.toBe('missed');
    if (out.report) expect(seekers(out.report)).not.toContain(other.name);
    expect(home.sought).toBe(g.turn);
    expect(battle.sought).toBeUndefined();
  });
});

describe('the fleets that join an attack (1.5.1 fleet attack once a month, the design director\'s ruling A of 02:34Z)', () => {
  for (const on of [true, false]) {
    it(`${on ? 'on' : 'off'}: a fleet that joined ${on ? 'has sought battle this month and does not seek again at the meeting' : 'is marked as attacked only'}`, () => {
      const g = britain(on);
      const home = fleet(g, 'Home Fleet'), battle = fleet(g, 'Battle Fleet');
      battle.zone = zoneOf(g, 'nsea');
      battle.stance = 'patrol';
      const other = g.fleets.find((f) => f.nat === 'UK' && f.id !== home.id && f.stance !== 'port' && f.ships.bb + f.ships.ca + f.ships.dd > 0)!;
      other.zone = battle.zone;
      other.stance = 'patrol';
      const out = orderFleetAttack(g, home.id, battle.id)!;
      expect(out.outcome).not.toBe('missed');
      expect(seekers(out.report!)).toContain(other.name);
      expect(other.attacked).toBe(g.turn);
      expect(other.sought === g.turn).toBe(on);
      expect(hasSought(g, other)).toBe(on);
      // The month's meeting, the German fleet back at sea whole: off the joiner seeks battle again; on it does not.
      backAtSea(g, { bb: 2, cv: 0, ca: 2, dd: 6, ss: 0 });
      battle.zone = other.zone;
      other.stance = 'patrol';
      const again = seaBattles(g, 'UK').some((r) => seekers(r).includes(other.name));
      expect(again).toBe(!on);
    });
  }
});

describe('through the bridge', () => {
  it('the view carries the switch, act and TC.seekOnce switch it', () => {
    const g = britain(false);
    type Call = (...a: unknown[]) => unknown;
    const TC = (globalThis as unknown as { TC: Record<string, Call> }).TC;
    TC.load(saveGame(g));
    expect((TC.view() as { seekOnce: boolean }).seekOnce).toBe(false);
    const out = TC.act('seekoncerule:on') as { ok: boolean; note: string };
    expect(out.ok).toBe(true);
    expect(out.note).toMatch(/One battle sought a month is on/);
    expect((TC.view() as { seekOnce: boolean }).seekOnce).toBe(true);
    expect((TC.seekOnce(false) as { on: boolean }).on).toBe(false);
    expect((TC.view() as { seekOnce: boolean }).seekOnce).toBe(false);
  });
});
