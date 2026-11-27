// Ruling 240 on the whole world's chart (the far side's switch, on for this file alone; tests/ship-aa-240.test.ts and
// tests/damage-control-240.test.ts hold the same on the chart as shipped). Every nation alike: the three rules read
// research only, so the nations that live only out here (Japan, China, Australia, New Zealand, South Africa and the
// rest) must read exactly as Germany does: the kit, the guns' fire and its line on the raid card, and how hard a ship
// is to sink. The switches are flipped in memory and put back after every test.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as { TC_FAR?: unknown }).TC_FAR = 1; });
import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import { Rng } from '../src/core/rng';
import { CARRIERS, assessAir, shipHits, type AirAssess } from '../src/campaign/air';
import { syncWings, wingsOf } from '../src/campaign/carriers';
import { createGame, setWar } from '../src/campaign/game';
import { NAVY, atSea, lossOf, sinkShips, toughOf, zoneOf } from '../src/campaign/navy';
import { PLANS } from '../src/campaign/plans';
import { saveGame } from '../src/campaign/save';
import { FAR_STAGE } from '../src/campaign/stage';
import { startOpen } from '../src/campaign/staff';
import { SHIP_RESEARCH, dcOf, flakMul, shipKit, techOf } from '../src/campaign/tech';
import type { AirFleet, Fleet, Game, ShipClass, Ships } from '../src/campaign/types';

const R = SHIP_RESEARCH()!;
let was = { stage: false, vt: false, dc: false };
beforeEach(() => { was = { stage: R.stage.on, vt: R.vt.on, dc: R.dc.on }; });
afterEach(() => { R.stage.on = was.stage; R.vt.on = was.vt; R.dc.on = was.dc; });
const set = (stage: boolean, vt: boolean, dc: boolean): void => { R.stage.on = stage; R.vt.on = vt; R.dc.on = dc; };

const FOUR = ['firecontrol', 'taskforces', 'proxfuze', 'damagecontrol'];
const CLASSES: ShipClass[] = ['bb', 'cv', 'ca', 'dd', 'ss'];
const DC = CARRIERS.strike.dc;
const noShips = (): Ships => ({ bb: 0, cv: 0, ca: 0, dd: 0, ss: 0 });
/** Puts researches straight onto a nation's list, and takes all four off it again. */
const give = (g: Game, nat: string, ...ids: string[]): void => { const s = techOf(g, nat)!; for (const id of ids) if (!s.done.includes(id)) s.done.push(id); };
const strip = (g: Game, nat: string): void => { const s = techOf(g, nat)!; s.done = s.done.filter((id) => !FOUR.includes(id)); };
const named = (g: Game, name: string): Fleet => g.fleets.find((f) => f.name === name)!;
const gunsLine = (a: AirAssess): { label: string; value: string; effect?: string } | undefined => a.lines.find((l) => l.label === "Ships' guns");

/** The grand campaign on the whole world's chart, nobody at the table, with none of the four researches on any list. */
function world(): { g: Game; nats: string[] } {
  const g = startOpen(createGame(3, null));
  const nats = Object.keys(g.nations);
  for (const nat of nats) for (const id of FOUR) expect(techOf(g, nat)!.done, `${nat} begins with ${id}`).not.toContain(id);
  return { g, nats };
}
/** Britain at war with Germany, the Home Fleet with six carriers and the German Battle Fleet with it in the open Atlantic. */
function duel(): { g: Game; nats: string[]; foe: Fleet; strike: AirFleet } {
  const { g, nats } = world();
  setWar(g, 'UK', 'GER');
  const home = named(g, 'Home Fleet'), foe = named(g, 'Battle Fleet');
  expect([home.nat, foe.nat]).toEqual(['UK', 'GER']);
  home.ships.cv = 6;
  for (const f of [home, foe]) { f.zone = zoneOf(g, 'natl'); f.stance = 'patrol'; f.lane = undefined; expect(atSea(f)).toBe(true); }
  syncWings(g);
  return { g, nats, foe, strike: wingsOf(g, home.id).find((w) => w.kind === 'bmr')! };
}

describe('ruling 240 on the whole world\'s chart: every nation alike', () => {
  it('is the far chart, with the nations that live only there and Britain\'s kin among them', () => {
    expect(FAR_STAGE).toBe(true);
    const { g, nats } = world();
    expect(nats.length).toBeGreaterThan(60);
    for (const nat of ['JAP', 'CHN', 'AUS', 'NZL', 'SAF', 'THA']) expect(nats, nat).toContain(nat);
    expect(nats.filter((k) => PLANS.kin[k] === 'UK').sort()).toEqual(['AUS', 'CAN', 'NZL', 'SAF']);
    expect(g.fleets.some((f) => f.nat === 'JAP')).toBe(true);
  });

  it('reads a navy\'s kit from its research alone, for every nation on it', () => {
    set(true, true, true);
    const { g, nats } = world();
    const STEPS: [string, [unknown, number, number]][] = [
      ['', [{ stage: 'early', vt: false, dc: false }, 0.6, 1]],
      ['firecontrol', [{ stage: 'mid', vt: false, dc: false }, 1, 1]],
      ['proxfuze', [{ stage: 'mid', vt: true, dc: false }, 1.3, 1]],
      ['taskforces', [{ stage: 'late', vt: true, dc: false }, 1.5 * 1.3, 1]],
      ['damagecontrol', [{ stage: 'late', vt: true, dc: true }, 1.5 * 1.3, 1.3]],
    ];
    const NONE = STEPS[0][1], ALL = STEPS[4][1];
    const look = (nat: string): unknown[] => [shipKit(g, nat), flakMul(g, nat), dcOf(g, nat)];
    for (const nat of nats) {
      for (const [id, want] of STEPS) {
        if (id) give(g, nat, id);
        expect(look(nat), `${nat} with ${id || 'none'}`).toEqual(want);
      }
      // Nobody else's ships are the better for it, but its kin's, who have all of it.
      for (const other of nats) if (other !== nat) expect(look(other), `${other} beside ${nat}`).toEqual(PLANS.kin[other] === nat ? ALL : NONE);
      strip(g, nat);
      // Each part by itself: damage control on early-war guns, the shells on early-war guns.
      give(g, nat, 'damagecontrol');
      expect(look(nat), nat).toEqual([{ stage: 'early', vt: false, dc: true }, 0.6, 1.3]);
      strip(g, nat);
      give(g, nat, 'proxfuze');
      expect(look(nat), nat).toEqual([{ stage: 'early', vt: true, dc: false }, 0.6 * 1.3, 1]);
      strip(g, nat);
      expect(look(nat), nat).toEqual(NONE);
    }
    // With the switches off nobody reads anything, whatever its list holds.
    for (const nat of nats) give(g, nat, ...FOUR);
    set(false, false, false);
    for (const nat of nats) expect(look(nat), nat).toEqual([{ stage: null, vt: false, dc: false }, 1, 1]);
  });

  it('gives the same fire, the same line on the raid card and the same sinkings for the same ships under every flag', () => {
    const { g, nats, foe, strike } = duel();
    const under = (nat: string): unknown[] => {
      foe.nat = nat;
      const out: unknown[] = [];
      const look = (): void => {
        const a = assessAir(g, [strike], 'fleet', foe.id);
        expect(a.why, nat).toBeNull();
        out.push([a.flak, a.bomberLoss, gunsLine(a), a.tough, shipHits(a, 'fleet', 120)]);
      };
      // The switches as shipped, then all three on: no research, mid-war guns, the shells, late-war guns, damage control.
      set(false, false, false);
      look();
      set(true, true, true);
      look();
      for (const id of ['firecontrol', 'proxfuze', 'taskforces', 'damagecontrol']) { give(g, nat, id); look(); }
      // The shells while no stage is in force.
      set(false, true, false);
      look();
      strip(g, nat);
      return out;
    };
    const ger = under('GER') as [number, number, { value: string; effect: string }, number[] | undefined, { sunk: Ships }][];
    expect(ger.map((x) => x[0])).toEqual([0.042, expect.closeTo(0.0252, 12), 0.042, expect.closeTo(0.0546, 12), expect.closeTo(0.0819, 12), expect.closeTo(0.0819, 12), expect.closeTo(0.0546, 12)]);
    expect(ger.map((x) => x[2].value)).toEqual([
      'Weight of anti-aircraft fire 28', 'Weight of anti-aircraft fire 17, early-war guns', 'Weight of anti-aircraft fire 28, mid-war guns',
      'Weight of anti-aircraft fire 36, mid-war guns with proximity shells', 'Weight of anti-aircraft fire 55, late-war guns with proximity shells',
      'Weight of anti-aircraft fire 55, late-war guns with proximity shells', 'Weight of anti-aircraft fire 36, with proximity shells',
    ]);
    expect(ger[0][2].effect).toBe('About 3% of the bombers brought down: battleships and carriers carry the most guns');
    expect(ger[1][2].effect).toBe('They may bring down about 2% of the bombers: battleships and carriers carry the most guns');
    // Damage control: the holder's surface ships sink 1/dc as often, on the same hits.
    expect(ger.map((x) => x[3])).toEqual([undefined, undefined, undefined, undefined, undefined, [DC], undefined]);
    expect(ger[5][4].sunk.dd).toBeCloseTo(ger[4][4].sunk.dd / DC, 12);
    for (const nat of nats) expect(under(nat), nat).toEqual(ger);
  });

  it('makes the same ships as hard to sink under every flag: toughOf, the fight\'s figure and the ships that go down', () => {
    set(false, false, true);
    const { g, nats } = world();
    const fleet = named(g, 'Battle Fleet');
    fleet.ships.ss = 9;
    const under = (flag: string, holder: string): unknown[] => {
      const out: unknown[] = [];
      fleet.nat = flag;
      for (const holds of [false, true]) {
        if (holds) give(g, holder, 'damagecontrol');
        expect(dcOf(g, flag), `${flag} by ${holder} ${holds}`).toBe(holds ? DC : 1);
        out.push(CLASSES.map((c) => toughOf(g, fleet, c)));
        for (let k = 1; k <= 4; k++) {
          const f: Fleet = { ...fleet, ships: { ...fleet.ships }, lost: { ...fleet.lost } };
          const rng = new Rng(100 + k), kept = noShips();
          out.push([CLASSES.map((c) => lossOf(g, f, c, 0.1 * k)), sinkShips(f, 0.1 * k, rng, undefined, dcOf(g, flag), kept), kept, rng.state]);
        }
      }
      strip(g, holder);
      return out;
    };
    const ger = under('GER', 'GER');
    expect(ger[0]).toEqual(CLASSES.map((c) => NAVY.classes[c].tough));
    expect(ger[5]).toEqual(CLASSES.map((c) => (c === 'ss' ? NAVY.classes[c].tough : NAVY.classes[c].tough / DC)));
    for (const flag of nats) expect(under(flag, flag), flag).toEqual(ger);
    // A kin nation's ships by its senior partner's research (the dominions Britain's).
    const kin = nats.filter((k) => PLANS.kin[k] !== undefined && nats.includes(PLANS.kin[k]));
    expect(kin).toEqual(expect.arrayContaining(['AUS', 'NZL', 'SAF', 'CAN']));
    for (const k of kin) expect(under(k, PLANS.kin[k]), `${k} by ${PLANS.kin[k]}`).toEqual(ger);
  });

  it('reaches no history campaign there either, whatever the switches say', () => {
    set(true, true, true);
    const g = createGame(3, null);
    const before = saveGame(g);
    for (const nat of Object.keys(g.nations)) {
      expect(shipKit(g, nat), nat).toEqual({ stage: null, vt: false, dc: false });
      expect(flakMul(g, nat), nat).toBe(1);
      expect(dcOf(g, nat), nat).toBe(1);
    }
    for (const f of g.fleets) for (const c of CLASSES) expect(toughOf(g, f, c), `${f.name} ${c}`).toBe(NAVY.classes[c].tough);
    expect(saveGame(g) === before).toBe(true);
  });
});
