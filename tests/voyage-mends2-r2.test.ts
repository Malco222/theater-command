// The second review's mends to the far chart's voyages and fleets struck at anchor (release 2; the design director's
// ruling of 04:33Z, 25 November 2026, 1 and 2). A fleet in passage whose ways on and back are both shut, the harbour it
// left lost, makes for a friendly harbour, an ally's too, rather than go down; one that comes into a harbour within its
// reach by a turn the month forces on it lies there that month as a fleet that comes in does. On its way it is an enemy
// at sea to be fought, struck or searched for, though it holds no sea; it covers no crossing; it sweeps no minefield,
// and its panel names a field only on the sea it is on. A fleet's book of strikes at anchor, split and joined again or
// joined with another struck in the same raid, keeps one entry for one raid and the same share lost. The whole-world chart.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
import { describe, expect, it } from 'vitest';
import { detach, merge } from '../src/campaign/fleets';
import { allied, atWar, createGame, declareWar } from '../src/campaign/game';
import { MINES, mineLine, minesMonth, setSeaMines } from '../src/campaign/mines';
import {
  anchorLoss, besideIn, cameIn, crossingAccount, crossingsOf, fleetReach, inPassage, navalUpkeep, rebase, sail, setGate, shipCount, surfaceShips,
} from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import type { Fleet, Game } from '../src/campaign/types';

const fleet = (g: Game, nat: string, name: string): Fleet => g.fleets.find((f) => f.nat === nat && f.name === name)!;
const said = (g: Game, from: number, text: string): boolean => g.headlines.slice(from).some((h) => h.text === text);
const gateOf = (g: Game, name: string): number => g.gates.findIndex((x) => (x as { name?: string }).name === name);
/** The next month at sea for a nation: its fleets in passage steam on, and those due come in. */
const nextMonth = (g: Game, nat = 'JAP'): void => { g.turn++; navalUpkeep(g, nat); };

/** The computer's war at its opening, Konigsberg made Japan's, and the Combined Fleet sent there. */
function voyage(): { g: Game; f: Fleet } {
  const g = startOpen(createGame(3, null));
  const f = fleet(g, 'JAP', 'Combined Fleet'), kon = g.byName['Konigsberg'];
  kon.ctrl = 'JAP';
  expect(rebase(g, f, kon)).toBe(true);
  expect(inPassage(f)).toBe(true);
  return { g, f };
}

describe('a fleet in passage with its ways on and back shut', () => {
  it('the harbour it left lost and none of its own in reach, it makes for a friendly harbour, an ally\'s too', () => {
    const { g, f } = voyage();
    const kon = g.byName['Konigsberg'], to = f.port, from = f.passage!.fromPort, left = g.regions[f.passage!.from];
    for (let k = 0; k < 12 && g.turn < f.passage!.due - 1; k++) nextMonth(g);
    setGate(g, gateOf(g, 'the Danish straits'), true);
    for (const r of g.regions) if (r.port && r.ctrl === 'JAP' && r !== kon) r.ctrl = 'USA';
    expect(left.ctrl).toBe('USA');
    const news = g.headlines.length;
    nextMonth(g);
    expect(g.fleets.includes(f)).toBe(true);
    const base = g.regions[f.base];
    expect(base.ctrl).not.toBe('JAP');
    expect(allied(g, 'JAP', base.ctrl)).toBe(true);
    expect(said(g, news, `With its way to ${to} shut, and ${from} lost, the Combined Fleet makes for ${base.name} instead.`)).toBe(true);
  });

  it('turned back to a harbour within its reach, it lies in harbour this month as a fleet that comes in does', () => {
    const g = startOpen(createGame(3, null));
    const f = fleet(g, 'JAP', '2nd Fleet'), bx = g.byName['Bordeaux'], kon = g.byName['Konigsberg'];
    bx.ctrl = 'JAP';
    kon.ctrl = 'JAP';
    // (Based at Bordeaux already: the reckoning under test is the turn back, not the voyage there.)
    Object.assign(f, { base: bx.id, port: bx.name, home: bx.seas[0], zone: bx.seas[0], station: bx.seas[0], stance: 'port' });
    expect(rebase(g, f, kon)).toBe(true);
    expect(inPassage(f)).toBe(true);
    for (let k = 0; k < 12 && g.turn < f.passage!.due - 1; k++) nextMonth(g);
    setGate(g, gateOf(g, 'the Danish straits'), true);
    const news = g.headlines.length;
    nextMonth(g);
    expect(f.base).toBe(bx.id);
    expect(f.stance).toBe('port');
    expect(said(g, news, 'The 2nd Fleet turns back, and is in harbour at Bordeaux again.')).toBe(true);
    expect(cameIn(g, f)).toBe(true);
  });
});

describe('a fleet in passage, on its way', () => {
  it('holds no sea in a sea\'s read, and is an enemy at sea to be fought, struck or searched for', () => {
    const { g, f } = voyage();
    declareWar(g, 'JAP', 'USA');
    // (A sea on its way with no coast and no enemy but the fleet itself: the fleet is all that could make a foe there.)
    for (let k = 0; k < 12 && inPassage(f) && g.seas[f.zone].coast.length; k++) nextMonth(g);
    expect(inPassage(f)).toBe(true);
    expect(g.seas[f.zone].coast.length).toBe(0);
    const foe = besideIn(g, f.zone, 'USA');
    expect(foe).not.toBeNull();
    expect(foe!('USA')).toBe(true);
    expect(besideIn(g, f.zone, 'USA', true, true)).toBeNull();
    f.stance = 'patrol';
    expect(besideIn(g, f.zone, 'USA', true, true)).not.toBeNull();
    f.stance = 'passage';
  });

  it('covers no crossing: the help to send a fleet off the beach does not count one on its way home', () => {
    const g = startOpen(createGame(3, null));
    for (const f of g.fleets) if (atWar(g, f.nat, 'GER') || f.nat === 'GER') sail(g, f, f.home, 'port');
    const x = crossingsOf(g, 'GER').find((c) => c.owner === 'UK' && c.home)!;
    expect(x).toBeTruthy();
    const worst = x.account.worst;
    const cover = g.fleets.filter((f) => f.nat === 'GER' && surfaceShips(f) > 0 && fleetReach(g, f).includes(worst));
    expect(cover.length).toBeGreaterThan(0);
    const help = (): string | undefined => crossingAccount(g, 'GER', x.path, x.to).helps.find((h) => h.id === 'sea')?.text;
    expect(help()).toBe(`Send your ${cover.length} fleets in reach to patrol the North Sea`);
    // Every fleet that could cover it far off across the ocean and on its way home: in passage, none covers it, though
    // its home harbour's reach takes in the sea.
    const far = g.byName['Norfolk'];
    far.ctrl = 'GER';
    for (const f of cover) {
      const home = g.regions[f.base];
      Object.assign(f, { base: far.id, port: far.name, home: far.seas[0], zone: far.seas[0], station: far.seas[0], stance: 'port' });
      expect(rebase(g, f, home)).toBe(true);
      expect(inPassage(f)).toBe(true);
      expect(fleetReach(g, f)).toContain(worst);
      expect(f.zone).not.toBe(worst);
    }
    expect(help()).toBeUndefined();
  });

  it('sweeps no minefield on its way, and learns of none', () => {
    const { g, f } = voyage();
    declareWar(g, 'JAP', 'USA');
    setSeaMines(g, true);
    f.ships.dd = Math.max(f.ships.dd, 4);
    expect(g.fleets.filter((x) => x !== f && x.zone === f.zone && atWar(g, x.nat, 'USA')).length).toBe(0);
    g.mines = [{ z: f.zone, nat: 'USA', s: 5, t: g.turn }];
    minesMonth(g);
    expect(g.mines[0].s).toBeCloseTo(5 - MINES.decay, 9);
    expect(g.mines[0].known ?? []).not.toContain('JAP');
    f.stance = 'patrol';
    minesMonth(g);
    expect(g.mines[0].s).toBeCloseTo(5 - 2 * MINES.decay - MINES.sweep, 9);
    expect(g.mines[0].known).toContain('JAP');
    f.stance = 'passage';
  });

  it('its panel names a field only on the sea it is on, not off the harbour it has yet to reach', () => {
    const { g, f } = voyage();
    declareWar(g, 'JAP', 'USA');
    setSeaMines(g, true);
    expect(f.zone).not.toBe(f.home);
    g.mines = [{ z: f.home, nat: 'USA', s: 1, t: g.turn, known: ['JAP'] }];
    expect(mineLine(g, f)).toBe('');
    f.stance = 'patrol';
    expect(mineLine(g, f)).not.toBe('');
    f.stance = 'passage';
    g.mines = [{ z: f.zone, nat: 'USA', s: 1, t: g.turn, known: ['JAP'] }];
    expect(mineLine(g, f)).not.toBe('');
  });
});

/** America at war with Japan in its first month, its Pacific Fleet at anchor at its base. */
function anchored(): { g: Game; f: Fleet } {
  const g = startOpen(createGame(3, null));
  if (!atWar(g, 'JAP', 'USA')) declareWar(g, 'JAP', 'USA');
  const f = fleet(g, 'USA', 'Pacific Fleet');
  expect(f.stance).toBe('port');
  return { g, f };
}
const RAID = (f: Fleet): { sea: number; field: number; km: number }[] => [{ sea: f.home, field: -1, km: 600 }];
const surface = (f: Fleet): number => shipCount(f.ships) - f.ships.ss;
function strike(g: Game, f: Fleet, lost: number): void {
  (f.anchor ??= []).push({ t: g.turn, at: f.base, n: Math.round(lost * surface(f)), lost, w: 300, from: RAID(f) });
}

describe("a fleet's book of strikes at anchor", () => {
  it('split and joined again: one entry for the raid, the same share lost, the ships sunk counted once', () => {
    const { g, f } = anchored();
    strike(g, f, 0.4);
    const n = f.anchor![0].n;
    const sq = detach(g, f, { dd: Math.min(2, f.ships.dd) }, 'Screen')!;
    expect(sq).toBeTruthy();
    expect(sq.anchor!.map((e) => [e.n, e.lost])).toEqual([[0, 0.4]]);
    expect(merge(g, sq, f)).toBe(true);
    expect(f.anchor!.length).toBe(1);
    expect(f.anchor![0].n).toBe(n);
    expect(anchorLoss(g, f)).toBeCloseTo(0.4, 9);
  });

  it('two fleets struck in one raid and joined: one entry, each loss a share of the whole', () => {
    const { g, f } = anchored();
    const other: Fleet = { ...f, id: g.nextFleet++, name: 'Second Fleet', ships: { ...f.ships }, lost: { ...f.lost }, anchor: undefined };
    g.fleets.push(other);
    strike(g, f, 0.4);
    strike(g, other, 0.2);
    const na = f.anchor![0].n, nb = other.anchor![0].n, sa = surface(f), sb = surface(other);
    expect(merge(g, f, other)).toBe(true);
    expect(other.anchor!.length).toBe(1);
    expect(other.anchor![0].n).toBe(na + nb);
    const ha = sa + na, hb = sb + nb;
    expect(anchorLoss(g, other)).toBeCloseTo((0.4 * ha + 0.2 * hb) / (ha + hb), 9);
  });
});
