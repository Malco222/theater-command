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
import { addCorps, atWar, capitulate, createGame, declareWar, removeCorps, startPhase } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { staffMonth } from '../src/campaign/desks';
import { navalUpkeep, planCrossing } from '../src/campaign/navy';
import { gatherMonth } from '../src/campaign/readiness';
import { ops, startOpen } from '../src/campaign/staff';
import { declarable, orderDeclare } from '../src/campaign/turn';
import type { Fleet, Game, Region } from '../src/campaign/types';

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
    const g = war(2, 9, 'USA');
    setDesk(g, 'navy', true);
    expect(base(g, fleet(g, 'USA', 'Pacific Fleet'))).not.toBe('Pearl Harbor');
    navalUpkeep(g, 'USA');
    expect(base(g, fleet(g, 'USA', 'Pacific Fleet'))).toBe('Pearl Harbor');
    expect(base(g, fleet(g, 'USA', 'Pacific Carrier Fleet'))).toBe('Pearl Harbor');
    expect(news(g, 9, 'The Pacific Fleet is ordered to stay at Pearl Harbor')).toBe(true);
  });

  it('stays where he keeps it where he runs the navy himself', () => {
    const g = war(2, 9, 'USA');
    setDesk(g, 'navy', false);
    const was = base(g, fleet(g, 'USA', 'Pacific Fleet'));
    navalUpkeep(g, 'USA');
    expect(base(g, fleet(g, 'USA', 'Pacific Fleet'))).toBe(was);
    expect(g.headlines.some((h) => h.turn === 9 && h.text === 'The Pacific Fleet is ordered to stay at Pearl Harbor.')).toBe(false);
    // (The move waits for him, and the news tells him so: the navy thread, on the audit's re-check of 25 November 2026.)
    expect(news(g, 9, `by the staff's plan. You run the navy yourself: the Pacific Fleet stays at ${was} until you move its base to Pearl Harbor`)).toBe(true);
  });

  it("and the computer's America moves it as before", () => {
    const g = war(2, 9, null);
    navalUpkeep(g, 'USA');
    expect(base(g, fleet(g, 'USA', 'Pacific Fleet'))).toBe('Pearl Harbor');
  });

  it('at stops 3 to 5 the book dates no move: the fleets stay where their staff keeps them (Units decide audit item 42)', () => {
    for (const stop of [3, 4, 5]) {
      const g = war(stop, 9, null);
      const was = base(g, fleet(g, 'USA', 'Pacific Fleet'));
      navalUpkeep(g, 'USA');
      expect(base(g, fleet(g, 'USA', 'Pacific Fleet')), `stop ${stop}`).toBe(was);
      expect(base(g, fleet(g, 'USA', 'Pacific Carrier Fleet')), `stop ${stop}`).not.toBe('Pearl Harbor');
      expect(news(g, 9, 'ordered to stay at Pearl Harbor'), `stop ${stop}`).toBe(false);
    }
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
    // The raid is remembered where it was flown, for the news that names the harbour (rider b641 (e)).
    expect(g.flags['harbour:JAP:Pearl Harbor']).toBe(28);
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
    expect(g.flags['harbour:JAP:Pearl Harbor']).toBeUndefined();
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

describe("a player's Japan where no book dates the strike: his admirals gather for the crossing he has named (rider b641 (b) to ruling 234; the ruling of 17:09Z, 25 November 2026)", () => {
  /** Puts so many corps of his on a quay, none else of his there. */
  const onQuay = (g: Game, from: Region, corps: number): void => {
    for (const c of g.corps.filter((u) => u.nat === 'JAP' && u.reg === from.id)) removeCorps(g, c);
    for (let k = 0; k < corps; k++) addCorps(g, 'JAP', `Quay Corps ${k}`, 'inf', 1, from.id, {});
  };
  /** A crossing he names as his plan, from a home port of his to a shore of a country he is at war with, and so many of his corps on its quay. */
  function named(g: Game, corps = 1): { from: Region; to: Region } {
    for (const from of g.regions.filter((r) => r.port && r.ctrl === 'JAP' && r.owner === 'JAP')) {
      for (const to of g.regions.filter((r) => r.seas.length > 0 && atWar(g, 'JAP', r.ctrl))) {
        if (!planCrossing(g, 'JAP', from, to)) continue;
        onQuay(g, from, corps);
        return { from, to };
      }
    }
    throw new Error('no crossing to name');
  }
  /** Japan's seat at a loose stop of the dial, the freeze come in July 1941 (turn 23). */
  const frozen = (staff: boolean, human: string | null = 'JAP', stop = 3): Game => {
    const g = war(stop, 23, human);
    if (human) setDesk(g, 'navy', staff);
    g.flags['pac:freeze'] = 23;
    return g;
  };
  const month = (g: Game, t: number): void => { g.turn = t; gatherMonth(g, 'JAP'); };

  it("with no crossing named nothing is gathered; named, from the month his corps stand on its quay the striking force lies at Hitokappu Bay, and it stands down when the window's sixth month has passed", () => {
    const g = frozen(true);
    const force = fleet(g, 'JAP', 'Mobile Fleet'), was = base(g, force);
    expect(was).not.toBe('Etorofu');
    // No crossing named: his navy waits on his plan, whatever the calendar says.
    for (const t of [24, 25]) { month(g, t); expect(base(g, force), `turn ${t}`).toBe(was); }
    // Named, with none of his corps on its quay: still nothing.
    const x = named(g, 0);
    month(g, 26);
    expect(base(g, force)).toBe(was);
    expect(g.flags['pac:gathered']).toBeUndefined();
    // His corps on the quay: the admirals gather that month.
    onQuay(g, x.from, 1);
    month(g, 27);
    expect([base(g, force), force.port]).toEqual(['Etorofu', 'Hitokappu Bay']);
    expect(news(g, 27, 'gathers at Hitokappu Bay')).toBe(true);
    // It lies there through the sixth month after the freeze, the last the computer's own staff could wait.
    for (const t of [28, 29]) { month(g, t); expect(base(g, force), `turn ${t}`).toBe('Etorofu'); }
    // No war on America by then: the admirals stand down, and the force comes home as it would from the strike.
    month(g, 30);
    expect([base(g, force), force.port]).toEqual(['Hiroshima', 'Kure']);
    expect(news(g, 30, 'comes home to Kure')).toBe(true);
    // And it does not gather again.
    for (const t of [31, 32]) { month(g, t); expect(base(g, force)).toBe('Hiroshima'); }
  });

  it('gathered, the plan\'s strike is flown as he declares, and the gathering is over', () => {
    const g = december(3, true);
    g.flags['pac:freeze'] = 24;
    named(g);
    const force = fleet(g, 'JAP', 'Mobile Fleet');
    gatherMonth(g, 'JAP');
    expect(base(g, force)).toBe('Etorofu');
    const pacific = fleet(g, 'USA', 'Pacific Fleet');
    const afloat = hulls(pacific);
    expect(orderDeclare(g, 'USA')).toBe(true);
    expect(news(g, 28, 'raid Pearl Harbor without warning')).toBe(true);
    expect(hulls(pacific)).toBeLessThan(afloat);
    expect(base(g, force)).toBe('Hiroshima');
    // The war come, nothing more is gathered or stood down.
    for (const t of [29, 30, 31]) { month(g, t); expect(base(g, force)).toBe('Hiroshima'); }
  });

  it("his own hand on the navy's desk ends it at once: the fleet is his where it lies, and it is not gathered or brought home again", () => {
    const g = frozen(true);
    named(g);
    const force = fleet(g, 'JAP', 'Mobile Fleet');
    month(g, 25);
    expect(base(g, force)).toBe('Etorofu');
    setDesk(g, 'navy', false);
    month(g, 26);
    setDesk(g, 'navy', true);
    for (const t of [27, 28, 29, 30, 31]) { month(g, t); expect(base(g, force), `turn ${t}`).toBe('Etorofu'); }
  });

  it('where he runs the navy himself nothing is moved; handed to his staff inside the window, they gather then', () => {
    const g = frozen(false);
    named(g);
    const force = fleet(g, 'JAP', 'Mobile Fleet'), was = base(g, force);
    for (const t of [24, 25, 26]) { month(g, t); expect(base(g, force)).toBe(was); }
    setDesk(g, 'navy', true);
    month(g, 27);
    expect(base(g, force)).toBe('Etorofu');
    // Handed over only after the window, nothing is gathered.
    const late = frozen(false);
    named(late);
    const other = fleet(late, 'JAP', 'Mobile Fleet'), lay = base(late, other);
    setDesk(late, 'navy', true);
    for (const t of [30, 31]) { month(late, t); expect(base(late, other)).toBe(lay); }
  });

  it("it is read as his staff's month opens, and for nobody else: not the computer's Japan, another player's seat, or where the book dates the strike", () => {
    const g = frozen(true);
    named(g);
    const force = fleet(g, 'JAP', 'Mobile Fleet');
    g.turn = 25;
    staffMonth(g);
    expect(base(g, force)).toBe('Etorofu');
    for (const other of [frozen(true, null), frozen(true, 'USA'), frozen(true, 'JAP', 1)]) {
      named(other);
      const f = fleet(other, 'JAP', 'Mobile Fleet'), was = base(other, f);
      month(other, 25);
      expect(base(other, f)).toBe(was);
      expect(other.flags['pac:gathered']).toBeUndefined();
    }
  });
});
