// Indochina's ground under the demand: held by agreement (the design director's ruling of 21:08Z of 21 November
// 2026, form 2, on /mnt/project-files/pacific-r2/indies/PARAGRAPH-INDOCHINA-STATE.md; far.ts `stationed` and
// `stationBars`, pacific.ts `demand` and `coupMonth`).
//
// As first built the demand made the colony Japan's own ground, so every enemy of Japan had a front on it: in 9
// wars of 12 Britain or China, at peace with Vichy France throughout, took Hanoi and Laos from it, where in the
// record no Allied army crossed into the colony before 1945. Form 2: Japan is the holder, Vichy France stays the
// owner, and the ground carries the agreement's mark from the demand to the coup. A country at war with Japan and
// at peace with the owner forms no front over marked ground and orders no ground attack or landing into it; its
// aircraft raid Japan's troops there as anywhere. A country at war with the owner as well is not bound. Japan
// itself has the holder's footing. The mark ends at the coup at the book's stops (month 67), where the book holds
// it whatever becomes of Vichy (the director's 21:14Z); at stops 3 to 5 an owner at war with the holder ends it.
// Each test that shows a bar shows the same board as first built beside it, so that the mark is seen to make the
// difference. The states are set by hand; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { whyNotMission } from '../src/campaign/air';
import { stationBars, stationed, stationShut, unstation, whyStationBars } from '../src/campaign/far';
import { finishMonthEnd } from '../src/campaign/finish';
import { prizeOf } from '../src/campaign/towns';
import { FLAGS } from '../src/campaign/ui/flags';
import { holds } from '../src/campaign/diplomacy';
import { addCorps, atWar, capitulate, capture, createGame, declareWar, relocate } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { crossingsOf, whyNotLand } from '../src/campaign/navy';
import { agreedWords, demand, hopShores, pacificOpening } from '../src/campaign/pacific';
import { blocked, whyBlocked } from '../src/campaign/rivers';
import { fronts, startOpen, takenGround } from '../src/campaign/staff';
import { declarable, reach, whyNotAttack } from '../src/campaign/turn';
import '../src/campaign/far-rows';
import type { AirMission, Corps, Game, Region } from '../src/campaign/types';

/** France beaten by Germany in June 1940, at a stop of the dial, moved by hand to a month of the war. */
const france = (stop: number, turn: number, human: string | null = null): Game => {
  const g = startOpen(createGame(3, human));
  if (stop !== 3) setStop(g, stop, true);
  // (Hanoi's post is made from the 104th Division's fourth regiment, which stands at Canton from Japan's first month
  // (china-setup.json, `_canton`; the design director's 08:48Z of 22 November 2026): put there by hand, as the month is.)
  const regiment = addCorps(g, 'JAP', '21st Army (170th Infantry Regiment, of the 104th Division)', 'inf', 1, 'Canton', { xp: 0.35, est: 0.3 });
  regiment.str = 0.3;
  g.turn = 10;
  capitulate(g, 'FRA', 'GER');
  g.turn = turn;
  return g;
};
/** The colony's ground: what Vichy owns and Japan holds or owns after the demand. */
const colony = (g: Game): Region[] => ['Hanoi', 'Haiphong', 'Lang Son', 'Lao Kay', 'Vinh', 'Hue', 'Qui Nhon', 'Cam Ranh', 'Saigon', 'Phnom Penh', 'Battambang', 'Pakse', 'Vientiane', 'Luang Prabang', 'Kratie'].map((nm) => g.byName[nm]);
/** The demand made, with Britain at war with Japan as well as China. */
const marked = (stop = 3, turn = 21, human: string | null = null): Game => {
  const g = france(stop, turn, human);
  expect(demand(g)).toBe(true);
  declareWar(g, 'JAP', 'UK');
  return g;
};
/** The same board as it was first built: no mark, and the colony Japan's own. */
const asBuilt = (stop = 3, turn = 21, human: string | null = null): Game => {
  const g = marked(stop, turn, human);
  for (const r of colony(g)) { expect(unstation(g, r, 'JAP'), r.name).toBe(true); r.owner = 'JAP'; }
  return g;
};
const corps = (g: Game, name: string): Corps => g.corps.find((c) => c.name === name && !c.dead)!;
/** A Chinese corps stood beside Lao Kay, with its month before it. */
const yunnan = (g: Game): Corps => {
  const c = corps(g, 'Yunnan Army');
  relocate(g, c, g.byName['Mengtsz']);
  c.ap = c.apMax;
  return c;
};
const LINE = (place: string): string => `${place} belongs to Vichy France, and you are not at war with it: Japan's troops are there by agreement. Declare war on Vichy France first, or strike them from the air.`;

describe('the demand: Japan holds, Vichy France owns, and the ground is marked', () => {
  it("all fifteen provinces: Japan's to hold, Vichy's to own, and held by agreement; no war", () => {
    const g = france(3, 21);
    for (const r of colony(g)) expect([r.owner, r.ctrl, stationed(g, r)], r.name).toEqual(['VIC', 'VIC', false]);
    expect(demand(g)).toBe(true);
    for (const r of colony(g)) expect([r.owner, r.ctrl, stationed(g, r)], r.name).toEqual(['VIC', 'JAP', true]);
    expect(g.regions.filter((r) => stationed(g, r)).length).toBe(15);
    expect(atWar(g, 'JAP', 'VIC')).toBe(false);
  });

  it('ground Japan holds by war is not such ground: Canton is taken, not agreed', () => {
    const g = marked();
    const canton = g.byName['Canton'];
    expect([canton.owner, canton.ctrl, stationed(g, canton)]).toEqual(['CHN', 'JAP', false]);
    expect(stationBars(g, 'CHN', canton)).toBe(false);
  });

  it("a player's Japan gets the same on his own demand", () => {
    const g = france(1, 23, 'JAP');
    declareWar(g, 'JAP', 'VIC');
    expect(atWar(g, 'JAP', 'VIC')).toBe(false);
    for (const r of colony(g)) expect([r.owner, r.ctrl, stationed(g, r)], r.name).toEqual(['VIC', 'JAP', true]);
  });
});

describe('who is bound: by relation, never by nation', () => {
  it('at war with Japan and at peace with Vichy France: bound. Japan, Vichy, and a country at peace with Japan: not', () => {
    const g = france(3, 21);
    demand(g);
    const hanoi = g.byName['Hanoi'];
    expect(stationBars(g, 'CHN', hanoi)).toBe(true);
    expect(stationBars(g, 'UK', hanoi)).toBe(false);
    declareWar(g, 'JAP', 'UK');
    expect(stationBars(g, 'UK', hanoi)).toBe(true);
    for (const nat of ['JAP', 'VIC', 'THA', 'USA']) expect(stationBars(g, nat, hanoi), nat).toBe(false);
  });

  it('a country at war with the owner as well is not bound: Britain declares on Vichy France and may march, attack and land', () => {
    const g = marked();
    const saigon = g.byName['Saigon'], force = [corps(g, 'Malaya Command (Singapore garrison, the Malay Regiment and the Volunteers)')];
    expect(whyNotLand(g, force, saigon)).toBe(LINE('Saigon'));
    declareWar(g, 'UK', 'VIC');
    expect(atWar(g, 'UK', 'VIC')).toBe(true);
    expect(stationBars(g, 'UK', saigon)).toBe(false);
    expect(whyNotAttack(g, saigon, 'UK')).toBe('');
    expect(whyNotLand(g, force, saigon)).not.toBe(LINE('Saigon'));
    expect(fronts(g, 'UK').some((f) => colony(g).some((r) => f.depth[r.id] >= 1))).toBe(true);
    // China, still at peace with Vichy France, is bound as before.
    expect(stationBars(g, 'CHN', g.byName['Hanoi'])).toBe(true);
  });
});

describe('a bound country forms no front over marked ground', () => {
  it("China's fronts: none reaches into Tonkin; as first built one stood on Lao Kay and Lang Son", () => {
    const into = (g: Game, nat: string): string[] => colony(g).filter((r) => fronts(g, nat).some((f) => f.depth[r.id] >= 1 || f.edge.includes(r.id))).map((r) => r.name);
    const was = asBuilt();
    expect(into(was, 'CHN')).toEqual(expect.arrayContaining(['Lao Kay', 'Lang Son']));
    expect(into(was, 'UK').length).toBeGreaterThan(0);
    const g = marked();
    expect(into(g, 'CHN')).toEqual([]);
    expect(into(g, 'UK')).toEqual([]);
  });

  it("Japan's own fronts stand as they did: the holder's footing, out of Tonkin into Yunnan and Kwangsi", () => {
    const facing = (g: Game): number[] => fronts(g, 'JAP').flatMap((f) => f.ours).filter((id) => colony(g).some((r) => r.id === id)).sort((a, b) => a - b);
    const was = facing(asBuilt());
    expect(was.length).toBeGreaterThan(0);
    expect(facing(marked())).toEqual(was);
  });
});

describe('a bound country orders no ground attack into marked ground', () => {
  it('the way is shut to its corps, with the reason; the map shows no attack; as first built both were open', () => {
    const was = asBuilt(), old = yunnan(was), there = was.byName['Lao Kay'];
    expect(blocked(was, old, there)).toBe(false);
    expect(reach(was, old).get(there.id)?.attack).toBe(true);
    expect(whyNotAttack(was, there, 'CHN')).toBe('');

    const g = marked(), c = yunnan(g), laoKay = g.byName['Lao Kay'];
    expect(blocked(g, c, laoKay)).toBe(true);
    expect(whyBlocked(g, c, laoKay)).toBe(LINE('Lao Kay'));
    expect(reach(g, c).has(laoKay.id)).toBe(false);
    expect(whyNotAttack(g, laoKay, 'CHN')).toBe(LINE('Lao Kay'));
    expect(whyStationBars(g, 'CHN', laoKay)).toBe(LINE('Lao Kay'));
  });

  it("Japan's corps march in and out of it as they did", () => {
    const g = marked();
    const own = corps(g, 'Indochina Expeditionary Force (Hanoi and Gia Lam)');
    expect(blocked(g, own, g.byName['Lang Son'])).toBe(false);
    expect(whyNotAttack(g, g.byName['Hanoi'], 'JAP')).toBe('');
    // (Out of Tonkin into a war Vichy France is not in: the record's own use of the colony. No twin of the clients' rider.)
    const guard = corps(g, 'Lao Kay airfield guard (Indochina Expeditionary Force)');
    expect(blocked(g, guard, g.byName['Mengtsz'])).toBe(false);
  });
});

describe('a bound country lands nobody on marked ground', () => {
  it('a landing is refused in the same line; the shores are on no list of hops or crossings', () => {
    const shores = (g: Game): string[] => colony(g).filter((r) => r.seas.length > 0).map((r) => r.name);
    const hops = (g: Game, nat: string): string[] => (hopShores(g, nat)?.open ?? []).map((r) => r.name).filter((nm) => shores(g).includes(nm));
    const across = (g: Game, nat: string): string[] => crossingsOf(g, nat).map((x) => x.to.name).filter((nm) => shores(g).includes(nm));
    const force = (g: Game): Corps[] => [corps(g, 'Malaya Command (Singapore garrison, the Malay Regiment and the Volunteers)')];

    // (The United States at war with Japan too: its army reaches no part of the colony by land, so its staff looks across the water.)
    const was = asBuilt();
    declareWar(was, 'JAP', 'USA');
    expect(whyNotLand(was, force(was), was.byName['Saigon'])).not.toBe(LINE('Saigon'));
    for (const nat of ['UK', 'USA', 'CHN']) expect(hops(was, nat), nat).toEqual(expect.arrayContaining(['Saigon', 'Haiphong', 'Cam Ranh']));
    expect(across(was, 'USA')).toContain('Saigon');

    const g = marked();
    declareWar(g, 'JAP', 'USA');
    expect(whyNotLand(g, force(g), g.byName['Saigon'])).toBe(LINE('Saigon'));
    for (const nat of ['UK', 'USA', 'CHN']) expect(hops(g, nat), nat).toEqual([]);
    for (const nat of ['UK', 'USA', 'CHN']) expect(across(g, nat), nat).toEqual([]);
    // (Other shores Japan holds stay on the lists.)
    expect((hopShores(g, 'USA')?.open ?? []).length).toBeGreaterThan(0);
    expect(crossingsOf(g, 'USA').length).toBeGreaterThan(0);
  });
});

describe('air raids go on', () => {
  it("no mission of a British or Chinese air command against the colony reads differently for the mark", () => {
    const was = asBuilt(), g = marked();
    const MISSIONS: AirMission[] = ['ground', 'support', 'superiority', 'bomb', 'harbour'];
    let asked = 0, open = 0;
    for (const f of g.airFleets) {
      if (f.nat !== 'UK' && f.nat !== 'CHN') continue;
      const twin = was.airFleets.find((x) => x.id === f.id)!;
      for (const m of MISSIONS) for (const r of colony(g)) {
        const why = whyNotMission(g, f, m, r.id);
        expect(why, `${f.name} ${m} ${r.name}`).toBe(whyNotMission(was, twin, m, r.id));
        asked++;
        if (why === null) open++;
      }
    }
    expect(asked).toBeGreaterThan(0);
    expect(open).toBeGreaterThan(0);
  });
});

describe('a player is told what frees him, and can do it', () => {
  it("a player's China may declare war on Vichy France, the owner of the ground shut to him; as first built Vichy was out of his reach", () => {
    const was = asBuilt(3, 21, 'CHN');
    expect(declarable(was)).not.toContain('VIC');
    const g = marked(3, 21, 'CHN');
    expect(declarable(g)).toContain('VIC');
    expect(whyNotAttack(g, g.byName['Lang Son'])).toBe(LINE('Lang Son'));
  });

  it('where war on the owner cannot be declared the line does not send him to declare it', () => {
    // France free again in Paris, and Vichy's ground France's: Britain stands beside France.
    const g = marked(3, 40);
    const F = g.nations.FRA;
    F.alive = true;
    F.freedOn = { turn: 40, week: 1, by: 'UK' } as Game['nations'][string]['freedOn'];
    g.regions[F.capital].ctrl = 'FRA';
    finishMonthEnd(g);
    expect(g.nations.VIC.alive).toBe(false);
    const saigon = g.byName['Saigon'];
    expect([saigon.owner, saigon.ctrl, stationed(g, saigon)]).toEqual(['FRA', 'JAP', true]);
    expect(stationShut(g, 'UK', saigon)).toBe(true);
    expect(whyStationBars(g, 'UK', saigon)).toBe("Saigon belongs to France, and Japan's troops are there by agreement: no army of yours may go in while the agreement stands. Strike them from the air.");
    expect(agreedWords(g, saigon, 'UK')?.own).toBe("You may strike Japan's troops, works and ships here from the air, but not attack or land while the agreement stands.");
    // (While Vichy stood, war on it would have opened the ground: shut, but not for good.)
    const open = marked(3, 40);
    expect([stationBars(open, 'UK', open.byName['Saigon']), stationShut(open, 'UK', open.byName['Saigon'])]).toEqual([true, false]);
  });
});

describe("the province's card says what the ground is (the UI director's words of 20:58Z)", () => {
  const ALL = "Belongs to Vichy France. Japan's troops are stationed here by agreement, and the French garrison keeps its posts.";

  it('one line for everyone, and one by who is reading: the country shut out, the holder, and nobody else', () => {
    const g = marked();
    const hanoi = g.byName['Hanoi'];
    expect(agreedWords(g, hanoi, null)).toEqual({ all: ALL, own: null });
    expect(agreedWords(g, hanoi, 'CHN')).toEqual({ all: ALL, own: "You may strike Japan's troops, works and ships here from the air, but not attack or land without war on Vichy France." });
    // (Japan's own player is "you" in the one line and has no second: the rider of 21:27Z.)
    expect(agreedWords(g, hanoi, 'JAP')).toEqual({ all: 'Belongs to Vichy France. Your troops are stationed here by agreement, and the French garrison keeps its posts under its own officers.', own: null });
    expect(agreedWords(g, hanoi, 'USA')).toEqual({ all: ALL, own: null });
    expect(agreedWords(g, hanoi, 'VIC')).toEqual({ all: ALL, own: null });
  });

  it("Japan's money line calls it ground held by agreement, never conquered ground (the rider of 21:27Z)", () => {
    const g = marked();
    const money = (r: Game['regions'][number]): string => prizeOf(g, r.id, 'JAP')?.items.find((it) => it.key === 'income')?.text ?? '';
    expect(money(g.byName['Saigon'])).toContain('Ground held by agreement pays less than it pays its own country');
    expect(money(g.byName['Canton'])).toContain('Conquered ground pays less than it pays its own country');
  });

  it("Vichy France flies the plain tricolour, with no regime's emblem (the art director, 21 November 2026)", () => {
    expect(FLAGS.VIC).toBe(FLAGS.FRA);
  });

  it('no such lines on any other ground, nor once the mark is struck', () => {
    const g = marked();
    expect(agreedWords(g, g.byName['Canton'], 'CHN')).toBeNull();
    expect(agreedWords(g, g.byName['Bangkok'], 'CHN')).toBeNull();
    expect(agreedWords(asBuilt(), asBuilt().byName['Hanoi'], 'CHN')).toBeNull();
  });
});

describe("the book and the staff read Japan as in Indochina, by agreement", () => {
  it("the book's test of Saigon holds from the demand, and to the coup and after; it never read true of ground taken in war", () => {
    const g = france(1, 22);
    pacificOpening(g);
    expect(holds(g, 'holds:JAP,Saigon')).toBe(false);
    g.turn = 23;
    pacificOpening(g);
    expect([g.byName['Saigon'].owner, g.byName['Saigon'].ctrl]).toEqual(['VIC', 'JAP']);
    expect(holds(g, 'holds:JAP,Saigon')).toBe(true);
    expect(holds(g, 'holds:VIC,Saigon')).toBe(false);
    // (Canton, taken from China in war, is not Japan's by the test's old meaning: its own and in its hands.)
    expect(holds(g, 'holds:JAP,Canton')).toBe(false);
    g.turn = 67;
    pacificOpening(g);
    expect([g.byName['Saigon'].owner, holds(g, 'holds:JAP,Saigon')]).toEqual(['JAP', true]);
  });

  it("Japan's staff names no garrison of its own for the colony: it is not taken ground; as first built it was Japan's own and asked none either", () => {
    const inColony = (g: Game): number => takenGround(g, 'JAP').filter((t) => t.ids.some((id) => colony(g).some((r) => r.id === id))).length;
    expect(inColony(asBuilt())).toBe(0);
    expect(inColony(marked())).toBe(0);
    // (What Japan has taken in war is asked for as it was: the same pieces of ground, the same asks.)
    const pieces = (g: Game): string[] => takenGround(g, 'JAP').map((t) => `${[...t.ids].sort((a, b) => a - b).join(',')}:${t.ask}`).sort();
    expect(pieces(marked())).toEqual(pieces(asBuilt()));
    expect(pieces(marked()).length).toBeGreaterThan(0);
  });
});

describe("the mark's end", () => {
  it("at the book's stops: the coup of month 67 strikes the mark, and the colony is Japan's own from that month", () => {
    for (const stop of [1, 2]) {
      const g = france(stop, 23);
      pacificOpening(g);
      g.turn = 66;
      pacificOpening(g);
      for (const r of colony(g)) expect([r.owner, r.ctrl, stationed(g, r)], `stop ${stop} ${r.name}`).toEqual(['VIC', 'JAP', true]);
      g.turn = 67;
      pacificOpening(g);
      expect(g.flags['pac:coup']).toBe(67);
      for (const r of colony(g)) expect([r.owner, r.ctrl, stationed(g, r)], `stop ${stop} ${r.name}`).toEqual(['JAP', 'JAP', false]);
      expect(g.regions.some((r) => stationed(g, r))).toBe(false);
      expect(stationBars(g, 'CHN', g.byName['Hanoi'])).toBe(false);
    }
  });

  it("at the book's stops the book holds it whatever becomes of Vichy: France freed, the colony stays Vichy's and marked to the coup", () => {
    const g = france(1, 23);
    pacificOpening(g);
    declareWar(g, 'JAP', 'UK');
    g.turn = 60;
    const F = g.nations.FRA;
    F.alive = true;
    F.freedOn = { turn: 60, week: 1, by: 'UK' } as Game['nations'][string]['freedOn'];
    g.regions[F.capital].ctrl = 'FRA';
    finishMonthEnd(g);
    expect(g.nations.VIC.alive).toBe(false);
    for (const r of colony(g)) expect([r.owner, r.ctrl, stationed(g, r)], r.name).toEqual(['VIC', 'JAP', true]);
    expect(stationBars(g, 'UK', g.byName['Saigon'])).toBe(true);
    expect(stationBars(g, 'CHN', g.byName['Hanoi'])).toBe(true);
    // (Vichy's ground elsewhere is France's, as it always was.)
    expect(g.regions.some((r) => r.owner === 'VIC' && !stationed(g, r))).toBe(false);
    g.turn = 67;
    pacificOpening(g);
    for (const r of colony(g)) expect([r.owner, stationed(g, r)], r.name).toEqual(['JAP', false]);
  });

  it('at stops 3 to 5 no coup comes by the date; an owner at war with the holder leaves no agreement', () => {
    const g = marked(3, 21);
    g.turn = 67;
    pacificOpening(g);
    g.turn = 72;
    pacificOpening(g);
    expect(g.flags['pac:coup']).toBeUndefined();
    for (const r of colony(g)) expect([r.owner, stationed(g, r)], r.name).toEqual(['VIC', true]);
    // France free, the colony France's by name, and then France at war with Japan: the mark reads no more.
    const F = g.nations.FRA;
    F.alive = true;
    F.freedOn = { turn: 72, week: 1, by: 'UK' } as Game['nations'][string]['freedOn'];
    g.regions[F.capital].ctrl = 'FRA';
    finishMonthEnd(g);
    const hanoi = g.byName['Hanoi'];
    expect([hanoi.owner, stationed(g, hanoi), stationBars(g, 'CHN', hanoi)]).toEqual(['FRA', true, true]);
    declareWar(g, 'FRA', 'JAP');
    expect(atWar(g, 'FRA', 'JAP')).toBe(true);
    expect([stationed(g, hanoi), stationBars(g, 'CHN', hanoi), stationBars(g, 'UK', hanoi)]).toEqual([false, false, false]);
  });

  it("ground Japan loses to a country that is not bound is marked no more; taken back, it goes to Vichy France itself, Japan's ally by then", () => {
    const g = marked();
    // (War made on Vichy France brings it to the Axis: from then Japan frees its ground as any ally's, game.ts capture.)
    declareWar(g, 'UK', 'VIC');
    const vinh = g.byName['Vinh'];
    capture(g, vinh, 'UK');
    expect([vinh.owner, vinh.ctrl, stationed(g, vinh)]).toEqual(['VIC', 'UK', false]);
    capture(g, vinh, 'JAP');
    expect([vinh.owner, vinh.ctrl, stationed(g, vinh)]).toEqual(['VIC', 'VIC', false]);
    // The rest of the colony, never lost, is held by agreement as before.
    expect(stationed(g, g.byName['Saigon'])).toBe(true);
  });
});
