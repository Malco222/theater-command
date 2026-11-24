// A fleet sent straight at an enemy fleet (release 2; Brian, 23 November 2026 22:23Z: his German fleet sent at the
// British produced no battle). Behind its own switch (stage.json 'fleetAttack'): off, no campaign carries any of it.
import { describe, expect, it } from 'vitest';
import stage from '../data/campaign/stage.json';
import { createGame } from '../src/campaign/game';
import { setFog } from '../src/campaign/intel';
import { detach } from '../src/campaign/fleets';
import { FLEET_ATTACK, fleetAttackOn, orderFleetAttack, planFleetAttack, setFleetAttack, whyNotFleetAttack } from '../src/campaign/fleetattack';
import { NAVY, fleetPower, sail, shipCount, zoneOf } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { FLEETATTACK_STAGE } from '../src/campaign/stage';
import { startOpen } from '../src/campaign/staff';
import { fleetOptions, humanUp, orderFleet, stepAI } from '../src/campaign/turn';
import type { Fleet, Game } from '../src/campaign/types';

const fleet = (g: Game, name: string): Fleet => g.fleets.find((f) => f.name === name)!;

/** A grand campaign with Germany at the table in its own month, the switch as given, and the fog lifted unless asked. */
function germany(on = true, seed = 3, fog = false): { g: Game; battle: Fleet; home: Fleet } {
  const g = startOpen(createGame(seed, 'GER'));
  let guard = 0;
  while (!humanUp(g) && guard++ < 50) stepAI(g);
  expect(humanUp(g)).toBe(true);
  setFleetAttack(g, on); // (on unless switched off since the 1.5.1 patch shipped it on)
  if (!fog) setFog(g, false);
  return { g, battle: fleet(g, 'Battle Fleet'), home: fleet(g, 'Home Fleet') };
}

describe('the switch', () => {
  it('switched off, nothing of it is read (the 1.5.1 patch ships it on: the flag 0 turns it off)', () => {
    expect((stage as { fleetAttack?: boolean }).fleetAttack).toBe(false);
    expect(FLEETATTACK_STAGE).toBe(false);
    const { g, battle, home } = germany(false);
    expect(fleetAttackOn(g)).toBe(false);
    expect(g.flags.fleetAttack).toBe(0);
    expect(whyNotFleetAttack(g, battle.id, home.id)).toMatch(/switched on/);
    const before = saveGame(g);
    expect(orderFleetAttack(g, battle.id, home.id)).toBeNull();
    expect(saveGame(g)).toBe(before);
    const opts = fleetOptions(g, battle.id)!;
    expect(opts.attack).toBeUndefined();
    expect(opts.held).toBeUndefined();
  });
  it('is the grand campaign only, and can be turned on and off in a campaign under way', () => {
    const g = createGame(3, 'GER');
    setFleetAttack(g, true);
    expect(fleetAttackOn(g)).toBe(false);
    const { g: o } = germany(true);
    expect(fleetAttackOn(o)).toBe(true);
    setFleetAttack(o, false);
    expect(fleetAttackOn(o)).toBe(false);
  });
});

describe('the attack at sea', () => {
  it('is fought at once, at odds the admiral would never accept, and Brian sees a battle', () => {
    const { g, battle, home } = germany();
    // The Home Fleet patrols the North Sea, as it does on the first day; the Battle Fleet lies in Wilhelmshaven.
    expect(home.stance).toBe('patrol');
    const A = fleetPower(g, battle), D = fleetPower(g, home);
    expect(A / D).toBeLessThan(NAVY.battle.seek);
    const plan = planFleetAttack(g, battle.id, home.id)!;
    expect(plan.why).toBeNull();
    expect(plan.where).toBe('sea');
    expect(plan.enough).toBe(false);
    expect(plan.notes[0]).toMatch(/would not offer battle/);
    const reports = g.seaReports.length;
    const out = orderFleetAttack(g, battle.id, home.id)!;
    expect(out.outcome).toBe('fought');
    expect(g.seaReports.length).toBe(reports + 1);
    const rep = out.report!;
    expect(rep.attNat).toBe('GER');
    expect(rep.defNat).toBe('UK');
    expect(rep.port).toBeUndefined();
    expect(out.text).toMatch(/North Sea/);
    expect(battle.attacked).toBe(g.turn);
  });

  it('is fought on the figures its forecast gives, and ends inside the forecast band', () => {
    for (let seed = 1; seed <= 12; seed++) {
      const { g, battle, home } = germany(true, seed);
      const plan = planFleetAttack(g, battle.id, home.id, null)!;
      const out = orderFleetAttack(g, battle.id, home.id)!;
      if (out.outcome !== 'fought') continue;
      const r = out.report!;
      expect(plan.ours.total).toBeCloseTo(r.aPower, 9);
      expect(plan.theirs.total).toBeCloseTo(r.dPower, 9);
      expect(r.odds).toBeGreaterThanOrEqual(plan.odds!.low - 1e-9);
      expect(r.odds).toBeLessThanOrEqual(plan.odds!.high + 1e-9);
    }
  });

  it('the forecast changes nothing: no dice, no ship, no flag, and the save is the same', () => {
    const { g, battle, home } = germany();
    const before = saveGame(g), dice = g.rng.state;
    for (const viewer of [null, 'GER', 'UK']) planFleetAttack(g, battle.id, home.id, viewer);
    whyNotFleetAttack(g, battle.id, home.id);
    fleetOptions(g, battle.id);
    expect(saveGame(g)).toBe(before);
    expect(g.rng.state).toBe(dice);
  });

  it('the same order on the same game is the same battle, and draws nothing from the game\'s own dice', () => {
    const a = germany(), b = germany();
    const dice = a.g.rng.state;
    const ra = orderFleetAttack(a.g, a.battle.id, a.home.id), rb = orderFleetAttack(b.g, b.battle.id, b.home.id);
    expect(JSON.stringify(ra)).toBe(JSON.stringify(rb));
    expect(a.g.rng.state).toBe(dice);
  });

  it('a fleet attacks once a month, and then stays where the battle left it', () => {
    const { g, battle, home } = germany();
    orderFleetAttack(g, battle.id, home.id);
    expect(whyNotFleetAttack(g, battle.id, home.id)).toMatch(/already been sent/);
    expect(orderFleetAttack(g, battle.id, home.id)).toBeNull();
    const opts = fleetOptions(g, battle.id)!;
    expect(opts.held).toMatch(/stays where the battle left it/);
    expect(opts.seas).toEqual([]);
    expect(opts.ports).toEqual([]);
    expect(orderFleet(g, battle.id, zoneOf(g, 'skag'), 'patrol')).toBe(false);
    // A squadron split off from it has attacked with it.
    if (shipCount(battle.ships) > 1) {
      const sq = detach(g, battle, { dd: Math.min(1, battle.ships.dd) || 0, ca: battle.ships.dd ? 0 : 1 });
      if (sq) expect(sq.attacked).toBe(g.turn);
    }
    // And it is saved with the game.
    const back = loadGame(saveGame(g))!;
    expect(back.fleets.find((f) => f.id === battle.id)!.attacked).toBe(g.turn);
  });

  it('ships not patrolling have to be found first, and some months are not', () => {
    let missed = 0, fought = 0;
    for (let seed = 1; seed <= 30; seed++) {
      const { g, battle, home } = germany(true, seed);
      sail(g, home, zoneOf(g, 'nsea'), 'raid');
      const plan = planFleetAttack(g, battle.id, home.id)!;
      expect(plan.found).toBe(NAVY.battle.catchRaider);
      const out = orderFleetAttack(g, battle.id, home.id)!;
      if (out.outcome === 'missed') { missed++; expect(out.report).toBeNull(); expect(battle.zone).toBe(zoneOf(g, 'nsea')); expect(battle.stance).toBe('patrol'); }
      if (out.outcome === 'fought') fought++;
    }
    expect(missed).toBeGreaterThan(0);
    expect(fought).toBeGreaterThan(0);
  });

  it('a heavily outweighed enemy may make for port, and the attacker holds the sea', () => {
    let declined = 0;
    for (let seed = 1; seed <= 30; seed++) {
      const { g, battle } = germany(true, seed);
      // A small British squadron in the North Sea, split off from the Home Fleet.
      const home = fleet(g, 'Home Fleet');
      const sq = detach(g, home, { dd: 2 })!;
      sail(g, home, home.home, 'port');
      const plan = planFleetAttack(g, battle.id, sq.id)!;
      expect(plan.slips).toBe(NAVY.battle.evade);
      const out = orderFleetAttack(g, battle.id, sq.id)!;
      if (out.outcome === 'declined') { declined++; expect(sq.stance).toBe('port'); }
    }
    expect(declined).toBeGreaterThan(0);
  });
});

describe('the attack in harbour', () => {
  it('fights every fleet of his side in that port, at anchor, under the guns of its fortress', () => {
    const { g, battle, home } = germany();
    sail(g, home, home.home, 'port');
    const scapa = g.regions[home.base];
    scapa.fort = { lv: 2, vs: [] };
    const plan = planFleetAttack(g, battle.id, home.id, null)!;
    expect(plan.why).toBeNull();
    expect(plan.where).toBe('harbour');
    expect(plan.port).toBe('Scapa Flow');
    expect(plan.anchor).toBe(FLEET_ATTACK.atAnchor);
    expect(plan.shore).toBe(2 * FLEET_ATTACK.gunsPerLevel);
    expect(plan.slips).toBe(0);
    expect(plan.found).toBe(1);
    expect(plan.theirs.total).toBeCloseTo(fleetPower(g, home) * FLEET_ATTACK.atAnchor + plan.shore + plan.theirs.air, 9);
    expect(plan.lines.map((l) => l.label)).toEqual(expect.arrayContaining(['In harbour', 'Shore batteries']));
    const out = orderFleetAttack(g, battle.id, home.id)!;
    expect(out.outcome).toBe('fought');
    expect(out.report!.port).toBe('Scapa Flow');
    expect(out.report!.dPower).toBeCloseTo(plan.theirs.total, 9);
    expect(out.report!.lines.map((l) => l.label)).toEqual(expect.arrayContaining(['In harbour', 'Shore batteries']));
    expect(out.text).toMatch(/at Scapa Flow/);
  });

  it('works dug by the enemy do not fire for the side that took them', () => {
    const { g, battle, home } = germany();
    sail(g, home, home.home, 'port');
    g.regions[home.base].fort = { lv: 3, vs: [], by: 'GER' };
    expect(planFleetAttack(g, battle.id, home.id, null)!.shore).toBe(0);
  });
});

describe('what cannot be ordered', () => {
  it('says why, in a sentence', () => {
    const { g, battle, home } = germany();
    const subs = fleet(g, 'U-boat Fleet');
    expect(whyNotFleetAttack(g, subs.id, home.id)).toMatch(/Submarines fight no fleet action/);
    expect(whyNotFleetAttack(g, home.id, battle.id)).toMatch(/your own fleet/);
    const med = fleet(g, 'Mediterranean Fleet');
    expect(whyNotFleetAttack(g, battle.id, med.id)).toMatch(/out of reach/);
    expect(orderFleetAttack(g, battle.id, med.id)).toBeNull();
    expect(whyNotFleetAttack(g, battle.id, subs.id)).toMatch(/not at war/);
    expect(whyNotFleetAttack(g, battle.id, 99999)).toMatch(/No such enemy fleet/);
  });

  it('lists the fleets it can be sent at, for the map to light up', () => {
    const { g, battle, home } = germany();
    const opts = fleetOptions(g, battle.id)!;
    expect(opts.attack).toContain(home.id);
    expect(opts.attack).not.toContain(fleet(g, 'Mediterranean Fleet').id);
  });

  it('a fleet nobody has found cannot be sent at; one only reported is fought on what is really there', () => {
    const { g, battle, home } = germany(true, 3, true);
    const plan = planFleetAttack(g, battle.id, home.id);
    if (plan && plan.why === null && plan.theirs.fleets.every((x) => !x.counted)) {
      expect(plan.odds).toBeNull();
      expect(plan.notes[0]).toMatch(/not identified/);
    }
  });
});
