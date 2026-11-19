// A player whose staff runs his navy (the Pacific playtester's finding 1, 19 November 2026).
//
// The far side's dated fleet moves and the plan's strike on a harbour were the computer's alone: the rules asked who
// the player was, not whether he ran the navy himself. So a player's America never had its fleet at Pearl Harbor,
// and a player's Japan never struck it, though his admirals ran everything else at sea. Both now ask the question
// every other staff routine asks (depth.ts, `byHand`): where the player has handed the navy to his staff, the staff
// keeps the dates and flies the plan's strike in the month he declares; where he runs it himself, nothing is done
// for him. Ruling 200(c) stands: a fleet that is not there is not struck there, and nothing is moved in to be hit.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { bookMonth } from '../src/campaign/claims';
import { setDesk } from '../src/campaign/depth';
import { monthlyDiplomacy } from '../src/campaign/diplomacy';
import { atWar, capitulate, createGame, declareWar, startPhase } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { navalUpkeep } from '../src/campaign/navy';
import { ops, startOpen } from '../src/campaign/staff';
import { declarable, orderDeclare } from '../src/campaign/turn';
import type { Fleet, Game } from '../src/campaign/types';

const war = (stop: number, turn: number, human: string | null, seed = 17): Game => {
  const g = startOpen(createGame(seed, human));
  if (stop !== 3) setStop(g, stop, true);
  g.turn = turn;
  return g;
};
const fleet = (g: Game, nat: string, name: string): Fleet => g.fleets.find((f) => f.nat === nat && f.name === name)!;
const base = (g: Game, f: Fleet): string => g.regions[f.base].name;
const news = (g: Game, turn: number, part: string): boolean => g.headlines.some((h) => h.turn === turn && h.text.includes(part));
const hulls = (f: Fleet): number => f.ships.bb + f.ships.cv + f.ships.ca + f.ships.dd;

/**
 * A made-up December 1941 with a player on Japan's seat, as far-pacific-south.test.ts makes it for the computer: the
 * Netherlands and France struck down in their months, the opening read from June 1941, Japan's dated rows taken in
 * its own months, America's fleets at Pearl Harbor as its row of May 1940 puts them. It stops as Japan's December opens.
 */
const december = (stop: number, staff: boolean): Game => {
  const g = war(stop, 9, 'JAP');
  setDesk(g, 'navy', staff);
  declareWar(g, 'GER', 'NED');
  capitulate(g, 'NED', 'GER');
  g.turn = 10;
  capitulate(g, 'FRA', 'GER');
  for (const t of [22, 23, 24, 25, 27]) { g.turn = t; bookMonth(g); monthlyDiplomacy(g); if (t === 24 || t === 27) startPhase(g, 'JAP'); }
  const sea = g.seas.findIndex((z) => z.key === 'hawaii');
  for (const f of g.fleets.filter((x) => x.nat === 'USA' && (x.name === 'Pacific Fleet' || x.name === 'Pacific Carrier Fleet'))) {
    f.base = g.byName['Pearl Harbor'].id; f.port = 'Pearl Harbor'; f.home = sea; f.zone = sea; f.station = sea; f.stance = 'port';
  }
  g.turn = 28;
  bookMonth(g);
  monthlyDiplomacy(g);
  navalUpkeep(g, 'USA');
  g.phase = g.order.indexOf('JAP');
  startPhase(g, 'JAP');
  return g;
};

describe("a player's America: the Pacific Fleet's base by date", () => {
  it('moves to Pearl Harbor in May 1940 where his admirals run the navy', () => {
    const g = war(3, 9, 'USA');
    setDesk(g, 'navy', true);
    expect(base(g, fleet(g, 'USA', 'Pacific Fleet'))).not.toBe('Pearl Harbor');
    navalUpkeep(g, 'USA');
    expect(base(g, fleet(g, 'USA', 'Pacific Fleet'))).toBe('Pearl Harbor');
    expect(base(g, fleet(g, 'USA', 'Pacific Carrier Fleet'))).toBe('Pearl Harbor');
    expect(news(g, 9, 'The Pacific Fleet is ordered to stay at Pearl Harbor')).toBe(true);
  });

  it('stays where he keeps it where he runs the navy himself', () => {
    const g = war(3, 9, 'USA');
    setDesk(g, 'navy', false);
    const was = base(g, fleet(g, 'USA', 'Pacific Fleet'));
    navalUpkeep(g, 'USA');
    expect(base(g, fleet(g, 'USA', 'Pacific Fleet'))).toBe(was);
    expect(news(g, 9, 'The Pacific Fleet is ordered to stay at Pearl Harbor')).toBe(false);
  });

  it("and the computer's America moves it as before", () => {
    const g = war(3, 9, null);
    navalUpkeep(g, 'USA');
    expect(base(g, fleet(g, 'USA', 'Pacific Fleet'))).toBe('Pearl Harbor');
  });
});

describe("a player's Japan: the strike on Pearl Harbor", () => {
  it('by the book, his admirals gather the striking force in the Kuriles in November and strike as he declares', () => {
    const g = december(1, true);
    const force = fleet(g, 'JAP', 'Mobile Fleet');
    expect(base(g, force)).toBe('Etorofu');
    const pacific = fleet(g, 'USA', 'Pacific Fleet');
    const afloat = hulls(pacific);
    expect(declarable(g)).toContain('USA');
    expect(orderDeclare(g, 'USA')).toBe(true);
    expect(atWar(g, 'JAP', 'USA')).toBe(true);
    expect(news(g, 28, 'raid Pearl Harbor without warning')).toBe(true);
    expect(hulls(pacific)).toBeLessThan(afloat);
    // The strike flown, the force is ordered home the same month, as the computer's is.
    expect(base(g, force)).toBe('Hiroshima');
    expect(force.port).toBe('Kure');
  });

  it('where he runs the navy himself nothing is moved and nothing is flown for him', () => {
    const g = december(1, false);
    const force = fleet(g, 'JAP', 'Mobile Fleet');
    expect(base(g, force)).not.toBe('Etorofu');
    const pacific = fleet(g, 'USA', 'Pacific Fleet');
    const afloat = hulls(pacific);
    expect(orderDeclare(g, 'USA')).toBe(true);
    expect(news(g, 28, 'raid Pearl Harbor')).toBe(false);
    expect(hulls(pacific)).toBe(afloat);
  });

  it('a declaration on a country whose harbour the plan does not name flies nothing', () => {
    const g = december(1, true);
    const pacific = fleet(g, 'USA', 'Pacific Fleet');
    const afloat = hulls(pacific);
    expect(ops('JAP').flatMap((o) => o.strikes ?? []).map((s) => s.harbour)).toEqual(['Pearl Harbor']);
    expect(orderDeclare(g, 'UK')).toBe(true);
    expect(atWar(g, 'JAP', 'USA')).toBe(false);
    expect(news(g, 28, 'raid Pearl Harbor')).toBe(false);
    expect(hulls(pacific)).toBe(afloat);
  });

  it('a fleet that is not in reach strikes nothing: where history is loose the force lies at Kure (ruling 200(c))', () => {
    const g = december(3, true);
    const force = fleet(g, 'JAP', 'Mobile Fleet');
    expect(base(g, force)).not.toBe('Etorofu');
    const pacific = fleet(g, 'USA', 'Pacific Fleet');
    const afloat = hulls(pacific);
    expect(orderDeclare(g, 'USA')).toBe(true);
    expect(news(g, 28, 'raid Pearl Harbor')).toBe(false);
    expect(hulls(pacific)).toBe(afloat);
  });
});
