// The cards' lines for the faulty torpedoes and the fleet's fuel (the UI director's sheet 24, boxes 3 and 4; rulings
// 194(f) and 194(g)). Display only: each reads a rule that stands and moves no figure. Every state is set by hand.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { createGame, declareWar } from '../src/campaign/game';
import { fleetCard } from '../src/campaign/fleets';
import { setStop } from '../src/campaign/history';
import { NAVY, faultyRaiders, fuelLine, subsWorth, torpedoFault, torpedoWords, zoneOf } from '../src/campaign/navy';
import { RES, storesOf } from '../src/campaign/resources';
import { startOpen } from '../src/campaign/staff';
import type { Fleet, Game } from '../src/campaign/types';

const grand = (): Game => startOpen(createGame(3, null));
const named = (g: Game, nat: string, name: string): Fleet => g.fleets.find((x) => x.nat === nat && x.name === name)!;
/** A fleet of submarines alone, raiding in one sea, set by hand. */
const raiders = (g: Game, f: Fleet, sea: string): Fleet => { f.ships = { bb: 0, cv: 0, ca: 0, dd: 0, ss: 12 }; f.zone = zoneOf(g, sea); f.stance = 'raid'; f.ready = 1; return f; };

describe('faulty torpedoes, as the cards say them (sheet 24, box 3)', () => {
  it('by the book the month is September 1943, and the sentence goes the month they are mended', () => {
    const g = grand();
    setStop(g, 1, true);
    const S = NAVY.subs!.USA;
    expect(torpedoFault(g, 'USA')).toEqual({ share: 0.25, until: 49 });
    expect(torpedoWords(g, 'USA', 'your submarines')).toBe('Faulty torpedoes: your submarines raid at a quarter of their worth until September 1943.');
    expect(torpedoWords(g, 'USA', 'these boats')).toBe('Faulty torpedoes: these boats raid at a quarter of their worth until September 1943.');
    expect(torpedoWords(g, 'USA', 'your boats', false)).toBe('Faulty torpedoes: your boats raid at a quarter of their worth.');
    g.turn = S.book;
    expect(torpedoFault(g, 'USA')).toBeNull();
    expect(torpedoWords(g, 'USA', 'your submarines')).toBeNull();
    // No other navy's torpedoes are faulty.
    g.turn = 30;
    expect(torpedoFault(g, 'JAP')).toBeNull();
  });

  it('off the book the month is counted from the start of America\'s war with Japan, and before that war none is named', () => {
    const g = grand();
    const S = NAVY.subs!.USA;
    expect(torpedoFault(g, 'USA')).toEqual({ share: 0.25, until: null });
    expect(torpedoWords(g, 'USA', 'your submarines')).toBe('Faulty torpedoes: your submarines raid at a quarter of their worth until the fault is found.');
    g.turn = 30;
    declareWar(g, 'JAP', 'USA');
    const until = 30 + S.month - 1;
    expect(torpedoFault(g, 'USA')).toEqual({ share: 0.25, until });
    // The card's month is the rule's month: a quarter the month before, the whole of their worth from that month.
    const f = raiders(g, named(g, 'USA', 'Pacific Submarine Fleet'), 'honshu');
    g.turn = until - 1;
    expect(subsWorth(g, f)).toBe(0.25);
    expect(torpedoFault(g, 'USA')).not.toBeNull();
    g.turn = until;
    expect(subsWorth(g, f)).toBe(1);
    expect(torpedoFault(g, 'USA')).toBeNull();
  });

  it('the line stands only where the navy\'s own boats are among the raiders, and on its own fleet\'s card', () => {
    const g = grand();
    g.turn = 30;
    declareWar(g, 'JAP', 'USA');
    const f = raiders(g, named(g, 'USA', 'Pacific Submarine Fleet'), 'honshu');
    const honshu = zoneOf(g, 'honshu'), kyushu = zoneOf(g, 'kyushu');
    expect(faultyRaiders(g, 'USA', [honshu])).toBe(true);
    expect(faultyRaiders(g, 'USA', [kyushu])).toBe(false);
    expect(faultyRaiders(g, 'JAP', [honshu])).toBe(false);
    expect(fleetCard(g, f).torpedoes).toMatch(/^Faulty torpedoes: these boats raid at a quarter of their worth until \w+ 194\d\.$/);
    // In harbour the boats raid nothing.
    f.stance = 'port';
    expect(faultyRaiders(g, 'USA', [honshu])).toBe(false);
    // Japan's own boats: no line.
    const j = raiders(g, g.fleets.find((x) => x.nat === 'JAP' && x.ships.ss > 0)!, 'hawaii');
    expect(fleetCard(g, j).torpedoes).toBeUndefined();
  });
});

describe('the fleet\'s fuel line (sheet 24, box 4)', () => {
  const fleet = (g: Game): Fleet => g.fleets.filter((f) => f.nat === 'JAP' && f.base >= 0).sort((a, b) => b.ships.bb - a.ships.bb)[0];

  it('no line while there is oil; short of fuel by the Oil page\'s own figure; fuelled from the wells names the field, and never both', () => {
    const g = grand();
    const f = fleet(g);
    expect(fuelLine(g, f)).toBeNull();
    expect(fleetCard(g, f).fuel).toBeUndefined();
    storesOf(g, 'JAP')!.oilMet = 0;
    expect(fuelLine(g, f)).toEqual({ kind: 'short', less: Math.round(RES.oil.short.fleet * 100) });
    expect(fleetCard(g, f).fuel).toEqual({ kind: 'short', less: 40 });
    // Based at Brunei beside its oil: the wells' line, short month or not.
    g.byName['Brunei'].ctrl = 'JAP';
    f.base = g.byName['Brunei'].id;
    expect(fuelLine(g, f)).toEqual({ kind: 'wells', field: 'Brunei' });
    storesOf(g, 'JAP')!.oilMet = 1;
    expect(fuelLine(g, f)).toEqual({ kind: 'wells', field: 'Brunei' });
  });
});
