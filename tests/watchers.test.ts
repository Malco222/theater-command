// The enemy's watchers (Brian, 22 November 2026, release 2: on the card "Enemy spies are rare... Make them busier?"
// he picked "Busier, mostly watchers": about one enemy agent a season from each great power, most of them watching
// his front and giving their side's attacks there 8% more punch until he arrests them; sabotage at home about as
// often as before). Behind the switch secret.watchers ('sw:watch'), which needs the spy hunters on too.
import { afterEach, describe, expect, it } from 'vitest';
import { assess, createGame, setWar } from '../src/campaign/game';
import { INTEL, arrest, foesFound, intelAttack, intelTick, mostAgents, spyRoom, threat, watchRate } from '../src/campaign/intel';
import { huntersOn, setSecretWar, watchersOn } from '../src/campaign/secretwar';
import { startOpen } from '../src/campaign/staff';
import { loadGame, saveGame } from '../src/campaign/save';
import { setDesk } from '../src/campaign/depth';
import { beginHuman, endHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Agent, FoeAgent, Game, Region } from '../src/campaign/types';

const keep = structuredClone({ enemy: INTEL.enemy, secret: INTEL.secret });
afterEach(() => {
  Object.assign(INTEL.enemy, structuredClone(keep.enemy));
  Object.assign(INTEL.secret, structuredClone(keep.secret));
});
/** The watchers' numbers, as they stand now (the hook above puts back a fresh copy after each test). */
const W = new Proxy({} as typeof INTEL.secret.watchers, { get: (_, k) => (INTEL.secret.watchers as any)[k], set: (_, k, v) => ((INTEL.secret.watchers as any)[k] = v, true) });
const free = (seed = 7, human: string | null = 'FIN'): Game => startOpen(createGame(seed, human));
/** The player at war with the foe, the services past their quiet months, the plots quiet unless a test wants them. */
function scene(human = 'FIN', foe = 'SOV', seed = 7): Game {
  const g = free(seed, human);
  setWar(g, human, foe);
  INTEL.enemy.chance = 0;
  INTEL.enemy.quiet = 0;
  g.flags.spyfrom = 0;
  return g;
}
/** Every service at war with the player puts a watcher on his front every month it can. */
const busy = (): void => { W.chance = 1; };
const watchers = (g: Game, nat?: string): FoeAgent[] => (g.intel.foes ?? []).filter((f) => f.kind === 'watch' && (!nat || f.nat === nat));
/** A province the player holds beside ground held by the given side. */
const onFront = (g: Game, r: Region, nat: string, side: (x: string) => boolean): boolean => r.ctrl === nat && r.nb.some((j) => side(g.regions[j].ctrl));
const plant = (g: Game, reg: number, nat = 'SOV', strike = g.turn): FoeAgent => {
  const f: FoeAgent = { id: g.intel.foeNext ?? 1, nat, reg, kind: 'watch', strike };
  g.intel.foeNext = f.id + 1;
  (g.intel.foes ??= []).push(f);
  return f;
};
const hunter = (g: Game, reg: number, nat = 'FIN'): Agent => {
  const a: Agent = { id: g.intel.next++, nat, name: `Hunter ${g.intel.next}`, xp: 0, reg, ready: g.turn, mission: 'hunt', born: g.turn };
  g.intel.agents.push(a);
  return a;
};
/** A Soviet corps beside a Finnish province, and that province. */
function faceOff(g: Game): { r: Region; c: Game['corps'][number] } {
  for (const c of g.corps) {
    if (c.dead || c.nat !== 'SOV' || c.kind === 'fort') continue;
    const j = g.regions[c.reg].nb.find((k) => g.regions[k].ctrl === 'FIN');
    if (j !== undefined) return { r: g.regions[j], c };
  }
  throw new Error('no Soviet corps beside Finland');
}

describe('the switch', () => {
  it('is on for a new grand campaign and keeps with the save; it needs the hunters; a campaign begun with it off has none', () => {
    const g = free();
    expect(watchersOn(g)).toBe(true);
    expect(watchersOn(loadGame(saveGame(g))!)).toBe(true);
    // A watcher is one of the hidden agents the hunters search for: without them there are none.
    setSecretWar(g, 'hunt', false);
    expect(huntersOn(g)).toBe(false);
    expect(watchersOn(g)).toBe(false);
    W.on = false;
    expect(watchersOn(free())).toBe(false);
    expect(free().flags['sw:watch']).toBeUndefined();
    // A historical scenario never has it.
    const h = createGame(7, 'FIN');
    setSecretWar(h, 'hunt', true);
    setSecretWar(h, 'watch', true);
    expect(watchersOn(h)).toBe(false);
  });

  it('with it off no watcher ever lands, however busy the services', () => {
    const g = scene();
    setSecretWar(g, 'watch', false);
    busy();
    for (let k = 0; k < 6; k++) { intelTick(g, 'FIN'); g.turn++; }
    expect(watchers(g)).toEqual([]);
    expect(watchRate(g, 'FIN')).toEqual({});
    expect(spyRoom(g)!.watchHelp).toBeUndefined();
  });
});

describe('how often, and where', () => {
  it("each service's monthly chance is secret.watchers.chance times the level, a smaller country's at 'smaller' times it, nothing in the quiet months", () => {
    const g = scene();
    setWar(g, 'FIN', 'ROM');
    const rate = watchRate(g, 'FIN');
    const lv = INTEL.enemy.level[g.flags.level ?? 2] ?? 1;
    expect(rate.SOV).toBeCloseTo(W.chance * lv, 9);
    expect(rate.ROM).toBeCloseTo(W.chance * lv * INTEL.secret.everyone.smaller, 9);
    // With the plots' own 0.05 that is about one agent a season from a great power at the campaign's middle level.
    expect(1 / (W.chance + keep.enemy.chance)).toBeGreaterThan(2.5);
    expect(1 / (W.chance + keep.enemy.chance)).toBeLessThan(3.5);
    g.flags.spyfrom = g.turn;
    INTEL.enemy.quiet = 3;
    expect(watchRate(g, 'FIN')).toEqual({});
  });

  it('a new watcher lands hidden on the front facing its own side, at work a month later, and never two in one province', () => {
    const g = scene();
    busy();
    const before = g.headlines.length, start = g.turn;
    const front = g.regions.filter((r) => onFront(g, r, 'FIN', (x) => x === 'SOV')).length;
    for (let k = 0; k < front + 3; k++) { intelTick(g, 'FIN'); g.turn++; }
    const list = watchers(g, 'SOV');
    expect(list.length).toBe(Math.min(front, mostAgents('SOV', g) - W.reserve));
    for (const f of list) {
      expect(onFront(g, g.regions[f.reg], 'FIN', (x) => x === 'SOV')).toBe(true);
      expect(f.found).toBeUndefined();
    }
    expect(new Set(list.map((f) => f.reg)).size).toBe(list.length);
    // One a month while there is room, each at work 'ready' months after it lands.
    expect(list.map((f) => f.strike - W.ready).sort((x, y) => x - y)).toEqual(list.map((_, k) => start + k));
    // Hidden: nothing is said of them.
    expect(g.headlines.slice(before).some((h) => /agent/i.test(h.text))).toBe(false);
    expect(foesFound(g)).toEqual([]);
  });

  it("a watcher's first month: its side's attack is not yet helped; from the month after ('ready') it is", () => {
    const g = scene();
    const { r, c } = faceOff(g);
    const f = plant(g, r.id, 'SOV', g.turn + W.ready);
    expect(intelAttack(g, 'SOV', r)).toBeNull();
    g.turn = f.strike;
    expect(intelAttack(g, 'SOV', r)).toEqual({ mul: 1 + W.attack, agent: '', foe: 'SOV', found: false });
    void c;
  });

  it("a service keeps 'reserve' places free for its saboteurs: it lands watchers only up to its size less that", () => {
    const g = scene();
    busy();
    const most = mostAgents('SOV', g);
    // Fill the service with saboteurs but for the reserve: no watcher comes.
    for (let k = 0; k < most - W.reserve; k++) (g.intel.foes ??= []).push({ id: 100 + k, nat: 'SOV', reg: g.nations.FIN.capital, kind: 'factory', strike: g.turn + 50 });
    intelTick(g, 'FIN');
    expect(watchers(g)).toEqual([]);
  });

  it('a country with no front at all is watched on its home coast, on a sea the enemy has a coast on', () => {
    // Britain at war with Germany, France still standing between them: no land front.
    const g = scene('UK', 'GER');
    busy();
    expect(g.regions.some((r) => onFront(g, r, 'UK', (x) => !!x && g.nations.UK.atWar.includes(x)))).toBe(false);
    intelTick(g, 'UK');
    const list = watchers(g, 'GER');
    expect(list.length).toBe(1);
    const at = g.regions[list[0].reg];
    expect(at.owner).toBe('UK');
    expect(at.overseas).toBe(false);
    const enemySeas = new Set(g.regions.filter((r) => r.ctrl === 'GER').flatMap((r) => r.seas));
    expect(at.seas.some((z) => enemySeas.has(z))).toBe(true);
  });
});

describe('what a watcher does', () => {
  it('its side attacks the province 8% harder, an ally of its country too; the battle says the enemy knew the defences', () => {
    const g = scene();
    const { r, c } = faceOff(g);
    plant(g, r.id);
    const spy = intelAttack(g, 'SOV', r)!;
    expect(spy.mul).toBeCloseTo(1 + W.attack, 9);
    // Nobody else's attack: not a nation outside the war, and never his own side's.
    expect(intelAttack(g, 'FIN', r)).toBeNull();
    const with_ = assess(g, [c], r);
    const line = with_.lines.find((l) => l.label === 'Enemy intelligence')!;
    expect(line.value).toBe(`The Soviet army knew the defences of ${r.name}: one of its agents is at work there`);
    expect(line.effect).toBe(`Attack up ${Math.round(W.attack * 100)}%`);
    // Uncovered and still at large, it says so.
    g.intel.foes![0].found = g.turn;
    expect(assess(g, [c], r).lines.find((l) => l.label === 'Enemy intelligence')!.value).toBe(`The Soviet agent your hunters uncovered at ${r.name} has mapped its defences`);
    // Arrested: the edge is gone.
    expect(arrest(g, 'FIN', g.intel.foes![0].id)).toBe(true);
    expect(intelAttack(g, 'SOV', r)).toBeNull();
    const without = assess(g, [c], r);
    expect(without.lines.some((l) => l.label === 'Enemy intelligence')).toBe(false);
    expect(with_.A / without.A).toBeCloseTo(1 + W.attack, 3);
  });

  it('it never strikes: months go by and it stays, with no sabotage', () => {
    const g = scene();
    W.chance = 0;
    const { r } = faceOff(g);
    const f = plant(g, r.id);
    const before = g.headlines.length;
    for (let k = 0; k < 12; k++) { intelTick(g, 'FIN'); g.turn++; }
    expect(watchers(g).map((x) => x.id)).toEqual([f.id]);
    expect(g.headlines.slice(before).some((h) => /Saboteurs|Grounded/.test(h.text))).toBe(false);
  });

  it('a hunter uncovers it, says what it is doing, and ARREST ends it; the room lists it as a watcher', () => {
    const g = scene();
    const { r } = faceOff(g);
    plant(g, r.id);
    INTEL.secret.hunters.here = 1;
    const h = hunter(g, r.id);
    intelTick(g, 'FIN');
    const found = foesFound(g);
    expect(found.length).toBe(1);
    expect(g.headlines.some((x) => x.text === `Agent ${h.name} uncovers a Soviet agent watching your defences at ${r.name}. Arrest it: until you do, their attacks there go in harder.`)).toBe(true);
    const row = spyRoom(g)!.found![0];
    expect(row.watch).toBe(true);
    expect(row.text).toBe(`A Soviet agent watching your defences at ${r.name}, uncovered this month`);
    expect(row.then).toBe(`Until it is arrested, Soviet attacks there go in ${Math.round(W.attack * 100)}% harder.`);
    expect(arrest(g, 'FIN', found[0].id)).toBe(true);
    expect(watchers(g)).toEqual([]);
  });

  it('the room explains them while the switch is on', () => {
    const g = scene();
    INTEL.enemy.chance = keep.enemy.chance;
    const help = spyRoom(g)!.watchHelp!;
    expect(help).toMatch(/about one every 3 months from each great power at war with you/);
    expect(help).toMatch(new RegExp(`${Math.round(W.attack * 100)}% harder until it is arrested`));
    expect(help).toMatch(/Guards do not stop watchers/);
  });

  it('a double agent in its service warns of it the month it lands', () => {
    const g = scene();
    busy();
    INTEL.secret.doubles.blown = 0;
    g.intel.doubles = [{ id: 1, name: 'Nightjar', of: 'SOV', by: 'FIN', since: g.turn }];
    intelTick(g, 'FIN');
    const f = watchers(g, 'SOV')[0];
    expect(f.found).toBe(g.turn);
    expect(g.headlines.map((h) => h.text)).toContain(`Double agent Nightjar warns of a new Soviet agent watching your defences at ${g.regions[f.reg].name}.`);
  });
});

describe('when the front moves', () => {
  it('a watcher off its front goes to a new place on it, needs a month again, and is lost to the hunters', () => {
    const g = scene();
    const inland = g.regions.find((r) => r.ctrl === 'FIN' && !r.nb.some((j) => g.regions[j].ctrl !== 'FIN'))!;
    const f = plant(g, inland.id);
    f.found = g.turn - 1;
    intelTick(g, 'FIN');
    const now = watchers(g)[0];
    expect(now.id).toBe(f.id);
    expect(onFront(g, g.regions[now.reg], 'FIN', (x) => x === 'SOV')).toBe(true);
    expect(now.strike).toBe(g.turn + W.ready);
    expect(now.found).toBeUndefined();
    expect(g.intel.notes.some((n) => n.text === `The Soviet agent your hunters uncovered at ${inland.name} has slipped away to watch another part of your front.`)).toBe(true);
  });

  it('with no place left on the front it is recalled; at peace, or with its country fallen, it is gone', () => {
    const g = scene();
    const front = g.regions.filter((r) => onFront(g, r, 'FIN', (x) => x === 'SOV'));
    for (const r of front) plant(g, r.id);
    const inland = g.regions.find((r) => r.ctrl === 'FIN' && !r.nb.some((j) => g.regions[j].ctrl !== 'FIN'))!;
    const extra = plant(g, inland.id);
    intelTick(g, 'FIN');
    expect(watchers(g).some((x) => x.id === extra.id)).toBe(false);
    expect(watchers(g).length).toBe(front.length);
    g.nations.FIN.atWar = g.nations.FIN.atWar.filter((x) => x !== 'SOV');
    g.nations.SOV.atWar = g.nations.SOV.atWar.filter((x) => x !== 'FIN');
    intelTick(g, 'FIN');
    expect(watchers(g)).toEqual([]);
  });
});

describe('the plots and the sum', () => {
  it('the plots go on at their own rate beside the watchers: the threat the room shows is unchanged', () => {
    const g = scene();
    INTEL.enemy.chance = keep.enemy.chance;
    const on = threat(g, 'FIN');
    setSecretWar(g, 'watch', false);
    expect(threat(g, 'FIN')).toEqual(on);
  });
});

describe('the staff that runs the secret service', () => {
  it('keeps a hunter on the front once the service is big enough, and arrests every enemy agent the hunters uncover', () => {
    // Germany's player, at war from the first month, with the secret service handed to the staff.
    const g = startOpen(createGame(3, 'GER'));
    for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
    setDesk(g, 'spies', true);
    beginHuman(g);
    const foe = g.nations.GER.atWar[0];
    expect(foe).toBeDefined();
    let hunting = false, arrested = 0;
    for (let k = 0; k < 10; k++) {
      // An enemy agent uncovered on his ground each month: the staff arrests it.
      const r = g.regions[g.nations.GER.capital];
      if (g.nations[foe]?.alive && g.nations.GER.atWar.includes(foe)) (g.intel.foes ??= []).push({ id: 500 + k, nat: foe, reg: r.id, kind: 'factory', strike: g.turn + 50, found: g.turn });
      endHuman(g);
      for (let s = 0; !humanUp(g) && s < 200; s++) stepAI(g);
      beginHuman(g);
      if (g.intel.agents.some((a) => a.nat === 'GER' && a.mission === 'hunt')) hunting = true;
      arrested += g.headlines.filter((h) => h.desk === 'spies' && /arrested/.test(h.text)).length;
    }
    expect(hunting).toBe(true);
    expect(arrested).toBeGreaterThan(0);
  });
});
