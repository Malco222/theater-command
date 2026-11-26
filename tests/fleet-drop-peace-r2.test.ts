// (Release 2; Brian, 26 November 2026 03:51Z: "i can't drag my fleet on top of another fleet to start a battle".) A fleet
// of a country at peace with the player was plain sea to the 3D: the drop sailed his fleet there and said nothing. The
// fleet in hand now lists the fleets of countries at peace that he could declare war on this month, each with the
// sights' quick reading of why it could not be sent at them even at war (whyNotFleetAttack's `asAtWar`, read at peace).
// The drop asks warFleetAttack, which reads the war declared on a copy (DECLARE WAR's own orderDeclare, then the tray's
// left-behind ships split off as the card splits them), so war is put only where the attack card could then be fought:
// a declaration moves the reach too (Italy at war with nobody passes the closed Turkish straits; at war, not).
import { describe, expect, it } from 'vitest';
import { allied, atWar, createGame, setWar } from '../src/campaign/game';
import { setFog } from '../src/campaign/intel';
import { setFleetAttack, whyNotFleetAttack } from '../src/campaign/fleetattack';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, declarable, humanUp, orderDeclare, stepAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
type Hand = { fleetAttack: boolean; attack: number[]; peace: { id: number; why: string }[]; cards: { cls: string; count: number }[] };
type WarRead = { ok: boolean; declare: boolean; why: string };

/** A seat at the table in its first month of orders, the fog lifted, fleet attacks switched as asked. */
function seat(nat: string, on: boolean): Game {
  const g = startOpen(createGame(3, nat));
  let guard = 0;
  while (!humanUp(g) && guard++ < 200) stepAI(g);
  beginHuman(g);
  setFleetAttack(g, on);
  setFog(g, false);
  return g;
}
const germany = (on: boolean): Game => seat('GER', on);
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

  it('the drop\'s answer is the war itself: for every listed fleet of six seats, war declared and the card agree with it', () => {
    let seen = 0, moved = 0;
    for (const nat of ['GER', 'ITA', 'UK', 'FRA', 'USA', 'SOV']) {
      const g = seat(nat, true);
      const text = saveGame(g);
      for (const f of g.fleets.filter((x) => x.nat === nat)) {
        TC().load(text);
        const hand = TC().fleetInHand(f.id) as Hand | null;
        if (!hand) continue;
        for (const p of hand.peace) {
          const t = g.fleets.find((x) => x.id === p.id)!;
          TC().load(text);
          const read = TC().warFleetAttack(f.id, p.id, '') as WarRead;
          expect(read.ok).toBe(true);
          expect(read.declare).toBe(true);
          // the war itself, as DECLARE WAR makes it in the 3D: TC.declare, then the fleet in hand read again
          TC().load(text);
          expect((TC().declare(t.nat) as { ok: boolean }).ok).toBe(true);
          const war = loadGame((TC().save() as { text: string }).text)!;
          expect(whyNotFleetAttack(war, f.id, p.id) ?? '').toBe(read.why);
          expect(((TC().fleetInHand(f.id) as Hand).attack).includes(p.id)).toBe(read.why === '');
          // the sights' quick reading is whyNotFleetAttack at peace with the war line skipped, and no more
          expect(whyNotFleetAttack(g, f.id, p.id, true) ?? '').toBe(p.why);
          seen++;
          if (p.why !== read.why) moved++;
        }
      }
    }
    expect(seen).toBeGreaterThan(50);
    // (the reach a declaration moves is found: at least Italy's 1st Fleet at the Soviet Black Sea Fleet, below)
    expect(moved).toBeGreaterThan(0);
  }, 300000);

  it('Italy, at war with nobody, passes the closed straits only until it declares: war is not offered for the Black Sea Fleet', () => {
    const g = seat('ITA', true);
    const text = saveGame(g);
    TC().load(text);
    const first = fleet(g, 'ITA', '1st Fleet'), black = fleet(g, 'SOV', 'Black Sea Fleet');
    const hint = (TC().fleetInHand(first) as Hand).peace.find((p) => p.id === black);
    expect(hint?.why).toBe('');
    const read = TC().warFleetAttack(first, black, '') as WarRead;
    expect(read.declare).toBe(true);
    expect(read.why).toMatch(/Sevastopol is out of reach of the 1st Fleet/);
    // and the war itself says the same
    const war = loadGame(text)!;
    expect(orderDeclare(war, 'SOV')).toBe(true);
    expect(whyNotFleetAttack(war, first, black)).toBe(read.why);
  });

  it('reading on a copy changes nothing in the war, and a country war cannot be declared on is answered so', () => {
    const g = germany(true);
    TC().load(saveGame(g));
    // (the war as the bridge holds it once loaded: loading switches on the campaign's own rules, so not saveGame(g))
    const before = (TC().save() as { text: string }).text;
    const battle = fleet(g, 'GER', 'Battle Fleet');
    for (const p of (TC().fleetInHand(battle) as Hand).peace) TC().warFleetAttack(battle, p.id, '');
    expect((TC().save() as { text: string }).text).toBe(before);
    // an enemy's fleet: no question of war, the card's own reason
    const enemy = g.fleets.find((x) => x.nat === 'UK')!;
    const atw = TC().warFleetAttack(battle, enemy.id, '') as WarRead;
    expect(atw.declare).toBe(false);
    expect(atw.why).toBe(whyNotFleetAttack(g, battle, enemy.id) ?? '');
    // a country not declarable this month (none of its fleets is listed)
    const can = new Set(declarable(g));
    const far = g.fleets.find((x) => x.nat !== 'GER' && !can.has(x.nat) && !atWar(g, 'GER', x.nat) && !allied(g, 'GER', x.nat) && x.ships && Object.values(x.ships).some((n) => n > 0));
    if (far) expect((TC().warFleetAttack(battle, far.id, '') as WarRead).declare).toBe(false);
  });

  it('with ships left behind in the tray, the answer is the split fleet\'s, as DECLARE WAR, the split and the card give it', () => {
    let tried = 0;
    for (const nat of ['GER', 'UK', 'ITA']) {
      const g = seat(nat, true);
      const text = saveGame(g);
      for (const f of g.fleets.filter((x) => x.nat === nat)) {
        TC().load(text);
        const hand = TC().fleetInHand(f.id) as Hand | null;
        if (!hand || !hand.peace.length) continue;
        const total = hand.cards.reduce((a, c) => a + c.count, 0);
        // leave behind every surface ship but the submarines where it has both (the lit ships then fight no fleet action),
        // or else one ship of its first class
        const subs = hand.cards.filter((c) => c.cls === 'ss').reduce((a, c) => a + c.count, 0);
        const left = subs > 0 && subs < total
          ? hand.cards.filter((c) => c.cls !== 'ss' && c.count > 0).map((c) => `${c.cls}.${c.count}`).join(',')
          : total > 1 ? `${hand.cards.find((c) => c.count > 0)!.cls}.1` : '';
        if (!left) continue;
        const p = hand.peace[0];
        const t = g.fleets.find((x) => x.id === p.id)!;
        TC().load(text);
        const read = TC().warFleetAttack(f.id, p.id, left) as WarRead;
        TC().load(text);
        expect((TC().declare(t.nat) as { ok: boolean }).ok).toBe(true);
        const split = TC().act(`fleetsplitmany:${f.id}:${left}`) as { ok: boolean };
        const war = loadGame((TC().save() as { text: string }).text)!;
        expect(split.ok ? whyNotFleetAttack(war, f.id, p.id) ?? '' : read.why).toBe(read.why);
        if (subs > 0 && subs < total) expect(read.why).toMatch(/Submarines fight no fleet action/);
        tried++;
      }
    }
    expect(tried).toBeGreaterThan(3);
  }, 300000);

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
