// (Release 2; Brian, 26 November 2026 03:51Z: "i can't drag my fleet on top of another fleet to start a battle".) A fleet
// of a country at peace with the player was plain sea to the 3D: the drop sailed his fleet there and said nothing. The
// fleet in hand now lists the fleets of countries at peace that he could declare war on this month, each with why it
// could not be sent at them even at war (empty where it could), so the 3D asks the question of war only where the
// attack card could then be fought. whyNotFleetAttack's `asAtWar` reads exactly as the war itself would.
import { describe, expect, it } from 'vitest';
import { allied, atWar, createGame, setWar } from '../src/campaign/game';
import { setFog } from '../src/campaign/intel';
import { setFleetAttack, whyNotFleetAttack } from '../src/campaign/fleetattack';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, declarable, humanUp, stepAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
type Hand = { fleetAttack: boolean; attack: number[]; peace: { id: number; why: string }[] };

/** Germany at the table in its first month of orders, the fog lifted, fleet attacks switched as asked. */
function germany(on: boolean): Game {
  const g = startOpen(createGame(3, 'GER'));
  let guard = 0;
  while (!humanUp(g) && guard++ < 200) stepAI(g);
  beginHuman(g);
  setFleetAttack(g, on);
  setFog(g, false);
  return g;
}
const fleet = (g: Game, nat: string, name: string): number => g.fleets.find((f) => f.nat === nat && f.name === name)!.id;

describe('the fleet in hand and the fleets of countries at peace', () => {
  it('lists only fleets of countries at peace he could declare war on, with the rules\' reason where even war would not bring them to battle', () => {
    const g = germany(true);
    TC().load(saveGame(g));
    const battle = fleet(g, 'GER', 'Battle Fleet');
    const hand = TC().fleetInHand(battle) as Hand;
    expect(hand.fleetAttack).toBe(true);
    const can = new Set(declarable(g));
    expect(hand.peace.length).toBeGreaterThan(0);
    for (const p of hand.peace) {
      const t = g.fleets.find((x) => x.id === p.id)!;
      expect(t.nat).not.toBe('GER');
      expect(can.has(t.nat)).toBe(true);
      expect(atWar(g, 'GER', t.nat)).toBe(false);
      expect(allied(g, 'GER', t.nat)).toBe(false);
    }
    // The enemy's fleets are the attack card's, never the question of war's.
    for (const id of hand.attack) expect(hand.peace.some((p) => p.id === id)).toBe(false);
    // The Soviet Northern Fleet is in the Battle Fleet's reach: war on the Soviet Union would bring it to battle. The
    // Baltic Fleet at Kronstadt is not, and the reason is the rules' own sentence.
    expect(hand.peace.find((p) => p.id === fleet(g, 'SOV', 'Northern Fleet'))?.why).toBe('');
    expect(hand.peace.find((p) => p.id === fleet(g, 'SOV', 'Baltic Fleet'))?.why).toMatch(/Kronstadt is out of reach of the Battle Fleet/);
    // A submarine fleet in hand: even at war it fights no fleet action.
    const boats = TC().fleetInHand(fleet(g, 'GER', 'U-boat Fleet')) as Hand;
    expect(boats.peace.length).toBeGreaterThan(0);
    expect(boats.peace.every((p) => /Submarines fight no fleet action/.test(p.why))).toBe(true);
  });

  it('asAtWar reads exactly as the war itself: for every listed fleet, war declared gives the same answer', () => {
    const g = germany(true);
    for (const name of ['Battle Fleet', 'Swinemunde Fleet', 'U-boat Fleet']) {
      const id = fleet(g, 'GER', name);
      TC().load(saveGame(g));
      for (const p of (TC().fleetInHand(id) as Hand).peace) {
        const t = g.fleets.find((x) => x.id === p.id)!;
        const war = loadGame(saveGame(g))!;
        setWar(war, 'GER', t.nat);
        expect(whyNotFleetAttack(war, id, p.id) ?? '').toBe(p.why);
        expect(whyNotFleetAttack(g, id, p.id, true) ?? '').toBe(p.why);
        // and at peace the plain question still says so (a submarine fleet is refused before the war is asked about)
        if (name !== 'U-boat Fleet') expect(whyNotFleetAttack(g, id, p.id)).toMatch(/is not at war with Germany/);
      }
    }
  });

  it('nothing is listed where fleet attacks are switched off', () => {
    const g = germany(false);
    TC().load(saveGame(g));
    const hand = TC().fleetInHand(fleet(g, 'GER', 'Battle Fleet')) as Hand;
    expect(hand.fleetAttack).toBe(false);
    expect(hand.peace).toEqual([]);
  });

  it('a step names another country\'s fleet by its name on the table; his own fleets and places answer first, as before', () => {
    const g = germany(true);
    TC().load(saveGame(g));
    const find = (name: string) => TC().find(name) as { fleet: number; corps: number; sea: number; region: number };
    expect(find('Northern Fleet').fleet).toBe(fleet(g, 'SOV', 'Northern Fleet'));
    expect(find('Battle Fleet').fleet).toBe(fleet(g, 'GER', 'Battle Fleet'));
    expect(find('North Sea').fleet).toBe(-1);
    expect(find('North Sea').sea).toBeGreaterThanOrEqual(0);
    expect(find('No Such Fleet Anywhere').fleet).toBe(-1);
  });
});
