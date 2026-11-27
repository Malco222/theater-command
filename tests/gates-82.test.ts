// The gates and what they say (Version 82): src/campaign/gates.ts, display only.
//
// Brian's ask of 2026-11-16: "make it so ships can go through the panama canal as long as you or your allies or a
// netutral country owns panama". The rule (canPass in navy.ts) already did, and is not changed: what was missing is
// that he could not see it. The design director's rulings 86 and 90(b):
//
// 1. A mark at each of the four gates: the holder's flag, and open or shut to his ships.
// 2. The words come from each gate's own rule. An ordinary gate is shut only to a country at war with its holder.
//    The Turkish straits are stricter, and shut there never says "you are at war with Turkey" to a player who is not.
// 3. A route refused at a gate says why in one line.
// 4. The mark stands on the water, at the gate.
// 5. Asking changes nothing.
import { describe, expect, it } from 'vitest';
import { createGame, setWar } from '../src/campaign/game';
import { gateBar, gateSpot, gateViews, onLand } from '../src/campaign/gates';
import { canPass, fleetReach, seaAt } from '../src/campaign/navy';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';

const game = (me: string | null): Game => startOpen(createGame(25, me));
const gate = (g: Game, name: string, viewer?: string | null) => gateViews(g, viewer === undefined ? g.human : viewer).find((v) => v.name === name)!;
const sea = (g: Game, name: string): number => g.seas.find((z) => z.name === name)!.id;

describe('the four gates and who holds them', () => {
  it('names each gate, its holder and whether the player may pass, and agrees with the rule', () => {
    const g = game('BRA');
    const views = gateViews(g);
    expect(views.map((v) => v.name).sort()).toEqual(['Danish straits', 'Panama Canal', 'Strait of Gibraltar', 'Turkish straits']);
    expect(views.map((v) => [v.name, v.holder]).sort()).toEqual([['Danish straits', 'DEN'], ['Panama Canal', 'USA'], ['Strait of Gibraltar', 'UK'], ['Turkish straits', 'TUR']]);
    g.gates.forEach((gt, i) => expect(views[i].open).toBe(canPass(g, gt.a, gt.b, 'BRA')));
    // Brazil is at peace: every gate is open, the Panama Canal in a neutral's hands too (Brian's rule).
    expect(views.every((v) => v.open === true && v.why === '')).toBe(true);
    expect(gate(g, 'Panama Canal').line).toBe('Held by the United States. Open to your ships.');
    expect(gate(g, 'Panama Canal').theName).toBe('The Panama Canal');
    expect(gate(g, 'Turkish straits').line).toBe('Held by Turkey. Open to your ships: you are at war with nobody.');
  });

  it('says a gate is his own, and says nothing of passing when nobody plays', () => {
    expect(gate(game('USA'), 'Panama Canal').line).toBe('You hold it. Open to your ships.');
    expect(gate(game('TUR'), 'Turkish straits').line).toBe('You hold it. Open to your ships.');
    const w = game(null);
    for (const v of gateViews(w)) { expect(v.open).toBeNull(); expect(v.line).toMatch(/^Held by .+\.$/); }
  });
});

describe('shut, in the words of each gate\'s own rule', () => {
  it('an ordinary gate is shut to a country at war with its holder, and says so', () => {
    const g = game('BRA');
    setWar(g, 'BRA', 'USA');
    const v = gate(g, 'Panama Canal');
    expect(v.open).toBe(false);
    expect(v.line).toBe('Shut to your ships: you are at war with the United States, who holds the Canal Zone.');
    expect(v.why).toBe('you are at war with the United States, who holds the Canal Zone');
    // At war with somebody else, the canal stays open: it shuts only to its holder's enemies.
    const h = game('BRA');
    setWar(h, 'BRA', 'GER');
    expect(gate(h, 'Panama Canal').open).toBe(true);
    // Whoever takes the Canal Zone holds the gate.
    h.regions[h.byName['Canal Zone'].id].ctrl = 'GER';
    expect(gate(h, 'Panama Canal').holder).toBe('GER');
    expect(gate(h, 'Panama Canal').line).toBe('Shut to your ships: you are at war with Germany, who holds the Canal Zone.');
  });

  it('the Turkish straits are shut in wartime to all but Turkey\'s allies, and never blame a war with Turkey that is not there', () => {
    const g = game('GER');
    expect(g.nations.GER.atWar.length).toBeGreaterThan(0);
    expect(g.nations.GER.atWar).not.toContain('TUR');
    const v = gate(g, 'Turkish straits');
    expect(v.open).toBe(false);
    expect(v.line).toBe("Shut to your ships: in wartime only Turkey's allies may pass.");
    expect(v.line).not.toMatch(/at war with Turkey/);
    // Gibraltar in the same month: Germany is at war with its holder.
    expect(gate(g, 'Strait of Gibraltar').line).toBe('Shut to your ships: you are at war with the United Kingdom, who holds Gibraltar.');
    // And the Danish straits are a neutral's: open.
    expect(gate(g, 'Danish straits').line).toBe('Held by Denmark. Open to your ships.');
  });
});

describe('a route refused at a gate says why', () => {
  it('names the shut gate that keeps a fleet from a sea it could otherwise reach', () => {
    const g = game('ITA');
    const iberia = sea(g, 'Iberian Coast');
    // A squadron that can sail out through Gibraltar while Italy is at peace with Britain: no gate is in its way.
    const f = g.fleets.find((x) => x.nat === 'ITA' && fleetReach(g, x).includes(iberia))!;
    expect(f).toBeTruthy();
    expect(gateBar(g, f, iberia)).toBeNull();
    setWar(g, 'ITA', 'UK');
    expect(fleetReach(g, f)).not.toContain(iberia);
    const bar = gateBar(g, f, iberia);
    expect(bar?.gate.name).toBe('Strait of Gibraltar');
    expect(bar?.line).toBe('The Strait of Gibraltar is closed to your ships: you are at war with the United Kingdom, who holds Gibraltar.');
    // A squadron for which that sea is too far in any case is told nothing of the gate.
    const far = g.fleets.find((x) => x.nat === 'ITA' && x.id !== f.id && gateBar(g, x, iberia) === null);
    expect(far).toBeTruthy();
    // A sea the fleet can reach has no gate in its way, and one too far for any reason but a gate has none either.
    for (const z of fleetReach(g, f)) expect(gateBar(g, f, z)).toBeNull();
    const farthest = sea(g, 'Gulf of Panama');
    expect(gateBar(g, f, farthest)).toBeNull();
  });

  it('says "are" of the straits', () => {
    const g = game('GER');
    const f = g.fleets.find((x) => x.nat === 'GER')!;
    const black = sea(g, 'Black Sea');
    const bar = gateBar(g, f, black);
    if (bar) expect(bar.line).toMatch(/^The (Turkish|Danish) straits are closed to your ships: |^The Strait of Gibraltar is closed to your ships: /);
  });
});

describe('where the mark stands, and that asking changes nothing', () => {
  it('stands on the water, near the ground that holds the gate', () => {
    const g = game('GER');
    for (const gt of g.gates) {
      const p = gateSpot(g, gt), r = g.regions[gt.region];
      expect([gt.a, gt.b]).toContain(seaAt(p.x, p.y, g));
      expect(onLand(g, p.x, p.y)).toBe(false);
      expect(Math.hypot(p.x - r.x, p.y - r.y)).toBeLessThan(40);
    }
    // The land test itself: a capital's own point is land, and the middle of a sea is not.
    expect(onLand(g, g.byName.Berlin.x, g.byName.Berlin.y)).toBe(true);
    const baltic = g.seas[sea(g, 'Baltic Sea')];
    expect(onLand(g, baltic.x, baltic.y)).toBe(false);
  });

  it('reads the game and writes nothing', () => {
    const g = game('GER');
    const before = saveGame(g);
    gateViews(g);
    for (const gt of g.gates) gateSpot(g, gt);
    for (const f of g.fleets.filter((x) => x.nat === 'GER')) for (const z of g.seas) gateBar(g, f, z.id);
    expect(saveGame(g)).toBe(before);
  });
});
