// An army with a move left is shown its attack (Version 84.2.3; Brian's find of 2026-11-17, "if an enemy army is out
// of a city i couldnt attack them", traced by the code checker). An attack on the province next door takes one move
// of the month, whatever the ground: the battle charges one, and the attack's card and its order ask only for a
// move left. reach() used to gate the attack by what a march onto that ground would cost, so an army that had come
// up with a move in hand had no red on the map and no word of an attack under the pointer (and the 3D game, which
// reads reach(), refused it outright). Now reach() agrees with candidates() and orderAttack(). A march keeps its
// gate. No war nobody plays asks reach() for an attack, so the measured wars stand byte for byte.
import { describe, expect, it } from 'vitest';
import { allied, atWar, createGame, hasRoom, hostileUnits, moveCost } from '../src/campaign/game';
import { blocked } from '../src/campaign/rivers';
import { garrison, garrisonOf } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, candidates, humanUp, myGarrison, orderAttack, orderSally, previewAttack, reach, sallyOutlook, stepAI, whyNotSallyOut } from '../src/campaign/turn';
import { relocate } from '../src/campaign/game';
import type { Corps, Game, Region } from '../src/campaign/types';

function playing(human = 'GER', seed = 3): Game {
  const g = startOpen(createGame(seed, human));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
const field = (g: Game): Corps[] => g.corps.filter((c) => c.nat === 'GER' && !c.dead && c.kind !== 'fort' && c.kind !== 'eng' && !c.town);
/** Every German corps with an enemy army in the field next door that its arm can go at. */
const facing = (g: Game): { c: Corps; r: Region }[] =>
  field(g).flatMap((c) => g.regions[c.reg].nb.map((j) => g.regions[j]).filter((r) => hostileUnits(g, r, 'GER').length > 0 && !blocked(g, c, r, c.reg)).map((r) => ({ c, r })));

describe('an army with a move left is shown its attack (Version 84.2.3)', () => {
  it('reach() marks the enemy army next door for attack whenever the attack itself is allowed', () => {
    const g = playing();
    const pairs = facing(g);
    expect(pairs.length).toBeGreaterThan(10);
    let hidden = 0;
    for (const { c, r } of pairs) {
      const had = c.ap;
      for (const ap of [1, 2, had]) {
        c.ap = ap;
        expect(candidates(g, r), `${c.name}, ${ap} left, at ${r.name}`).toContain(c);
        expect(reach(g, c).get(r.id), `${c.name}, ${ap} left, to ${r.name}`).toEqual({ cost: 1, prev: c.reg, attack: true });
        // What the old gate hid: a march onto that ground would have cost more than the moves in hand.
        if (ap < Math.min(moveCost(g, c, r, c.reg), 4)) hidden++;
      }
      c.ap = had;
    }
    expect(hidden).toBeGreaterThan(0);
  });

  it('and the order goes in on one move, which the battle takes', () => {
    const g = playing();
    const { c, r } = facing(g).find((p) => moveCost(g, p.c, p.r, p.c.reg) >= 2)!;
    expect(c).toBeDefined();
    c.ap = 1;
    expect(reach(g, c).get(r.id)?.attack).toBe(true);
    expect(previewAttack(g, [c], r)).not.toBeNull();
    const from = c.reg;
    const rep = orderAttack(g, [c], r);
    expect(rep).not.toBeNull();
    // Thrown back it has spent its move; through, it stands on the ground it won.
    expect(c.dead || c.ap === 0 || c.reg !== from).toBe(true);
  });

  it('keeps the gate for a march: ground that costs more than the moves in hand is not reached', () => {
    const g = playing();
    let dear = 0;
    for (const c of field(g)) {
      const had = c.ap;
      for (const j of g.regions[c.reg].nb) {
        const r = g.regions[j];
        if (hostileUnits(g, r, 'GER').length || blocked(g, c, r, c.reg) || !(allied(g, 'GER', r.ctrl) || atWar(g, 'GER', r.ctrl)) || !hasRoom(g, r, c)) continue;
        const step = moveCost(g, c, r, c.reg);
        if (step < 2) continue;
        dear++;
        c.ap = 1;
        expect(reach(g, c).has(j), `${c.name}, 1 left, to ${r.name} at ${step}`).toBe(false);
        c.ap = Math.max(step, had);
        expect(reach(g, c).get(j), `${c.name}, ${c.ap} left, to ${r.name} at ${step}`).toMatchObject({ attack: false });
      }
      c.ap = had;
    }
    expect(dear).toBeGreaterThan(0);
  });

  it('shows nothing to a corps with no move left, and none to fortress troops', () => {
    const g = playing();
    const { c } = facing(g)[0];
    c.ap = 0;
    expect(reach(g, c).size).toBe(0);
    const fort = g.corps.find((x) => x.nat === 'GER' && x.kind === 'fort' && !x.dead);
    if (fort) expect(reach(g, fort).size).toBe(0);
  });

  it('a garrison with the enemy round its town has its sortie: the order the page now opens from a press on that army', () => {
    const g = playing();
    // A German province on the Polish border: its corps go inside, and a Polish army is put in the country round the town.
    const { c, r: from } = facing(g).find((p) => p.r.ctrl === 'POL' && g.regions[p.c.reg].ctrl === 'GER')!;
    const home = g.regions[c.reg];
    for (const u of [...home.units]) if (u.nat === 'GER' && u.kind !== 'fort' && u.kind !== 'eng') expect(garrison(g, u), u.name).toBe(true);
    expect(myGarrison(g, home.id).length).toBeGreaterThan(0);
    for (const u of [...home.units]) relocate(g, u, g.regions.find((x) => x.ctrl === 'GER' && x.id !== home.id && !home.nb.includes(x.id))!);
    expect(whyNotSallyOut(g, home.id)).toMatch(/is not under siege\.$/);
    for (const u of [...from.units]) relocate(g, u, home);
    expect(hostileUnits(g, home, 'GER').length).toBeGreaterThan(0);
    expect(garrisonOf(home).some((u) => u.nat === 'GER')).toBe(true);
    expect(whyNotSallyOut(g, home.id)).toBeNull();
    expect(sallyOutlook(g, home.id)).not.toBeNull();
    const rep = orderSally(g, home.id);
    expect(rep).not.toBeNull();
    expect(rep!.sally).toBe(true);
    // One sortie a month from each town: the second press is told so.
    expect(whyNotSallyOut(g, home.id) === null || /has made its sortie this month\.$/.test(whyNotSallyOut(g, home.id)!) || /is not under siege\.$/.test(whyNotSallyOut(g, home.id)!)).toBe(true);
    if (hostileUnits(g, home, 'GER').length) expect(whyNotSallyOut(g, home.id)).toMatch(/has made its sortie this month\.$/);
  });
});
