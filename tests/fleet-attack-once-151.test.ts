// A fleet attacks once a month, the fleets that join it too (1.5.1; the playtester, 25 November 2026: five destroyers
// split off the Home Fleet each sent at the German Battle Fleet, and the Home Fleet joined all five; the design
// director's ruling A, 02:34Z: every fleet that fights in an attack has attacked this month and stays where the battle
// left it; fleets that fight only as defenders are not marked).
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { setFog } from '../src/campaign/intel';
import { detach } from '../src/campaign/fleets';
import { orderFleetAttack, planFleetAttack, setFleetAttack, whyNotFleetAttack } from '../src/campaign/fleetattack';
import { sail, shipCount } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import { fleetOptions, humanUp, stepAI } from '../src/campaign/turn';
import type { Fleet, Game } from '../src/campaign/types';

const fleet = (g: Game, name: string): Fleet => g.fleets.find((f) => f.name === name)!;

/** A grand campaign with Britain at the table in its own month, fleets sent at fleets, the fog lifted. */
function britain(seed = 5): { g: Game; home: Fleet; battle: Fleet } {
  const g = startOpen(createGame(seed, 'UK'));
  let guard = 0;
  while (!humanUp(g) && guard++ < 50) stepAI(g);
  expect(humanUp(g)).toBe(true);
  setFleetAttack(g, true);
  setFog(g, false);
  const home = fleet(g, 'Home Fleet'), battle = fleet(g, 'Battle Fleet');
  // The Home Fleet on patrol in the sea the attack on Wilhelmshaven goes in from, as the playtester found it in 1940.
  const zone = planFleetAttack(g, home.id, battle.id)!.zone;
  if (home.zone !== zone || home.stance !== 'patrol') expect(sail(g, home, zone, 'patrol')).toBe(true);
  return { g, home, battle };
}

describe('a fleet attacks once a month, and so does every fleet that joins it', () => {
  it('five destroyers split off the Home Fleet fight the German Battle Fleet once, not five times', () => {
    const { g, home, battle } = britain();
    expect(home.stance).toBe('patrol');
    const squadrons: Fleet[] = [];
    for (let k = 0; k < 5; k++) {
      const sq = detach(g, home, { dd: 1 });
      if (sq) squadrons.push(sq);
    }
    expect(squadrons.length).toBe(5);
    const reports = g.seaReports.length;
    let fought = 0;
    for (const sq of squadrons) {
      const out = orderFleetAttack(g, sq.id, battle.id);
      if (out?.outcome === 'fought') fought++;
    }
    expect(fought).toBe(1);
    expect(g.seaReports.length).toBe(reports + 1);
    // Every one of them fought in it, the Home Fleet too: each has attacked, and each still afloat is refused and stays
    // where it is (a destroyer sunk in it has no ships to send).
    for (const x of [home, ...squadrons]) {
      expect(x.attacked).toBe(g.turn);
      if (!shipCount(x.ships) || !g.fleets.includes(x)) continue;
      expect(whyNotFleetAttack(g, x.id, battle.id)).toMatch(/already been sent/);
      expect(fleetOptions(g, x.id)!.held).toMatch(/stays where the battle left it/);
    }
  });

  it('the card counts only the fleets that would really join: one that has fought this month is not among them', () => {
    const { g, home, battle } = britain();
    // A squadron off the Home Fleet goes into harbour, so it does not join the Home Fleet's own attack.
    const sq = detach(g, home, { dd: 1 })!;
    expect(sq).not.toBeNull();
    sail(g, sq, sq.home, 'port');
    expect(sq.stance).toBe('port');
    const before = planFleetAttack(g, sq.id, battle.id)!;
    expect(before.ours.fleets.map((x) => x.id)).toContain(home.id);
    expect(orderFleetAttack(g, home.id, battle.id)).not.toBeNull();
    expect(sq.attacked).toBeUndefined();
    const after = planFleetAttack(g, sq.id, battle.id)!;
    expect(after.ours.fleets.map((x) => x.id)).not.toContain(home.id);
  });

  it('the enemy fleets that fight only as defenders are not marked', () => {
    const { g, home, battle } = britain();
    const out = orderFleetAttack(g, home.id, battle.id)!;
    expect(out).not.toBeNull();
    expect(battle.attacked).toBeUndefined();
  });
});
