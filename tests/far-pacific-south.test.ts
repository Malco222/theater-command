// Japan's opening with the far side's switch on (ruling 200, the staffs' plans: the Pacific planner's papers 14 and
// 18; and ruling 199, a fallen country's far ground: paper 07). The great choice 'south' and what it carries (the ten
// landings, the quays, the strike on Pearl Harbor), the demand on Indochina, Siam's entry, the keeping of the Dutch
// and French far ground, and the Pacific's page of history's book. The states are set by hand on the first day's
// chart and the month's readings are called as the game calls them; one month of Japan's own is played, a made-up
// December 1941, to see the blow fall. No war is played here.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { wantsOut } from '../src/campaign/ai';
import { plannedStrike } from '../src/campaign/carriers';
import { bookMonth } from '../src/campaign/claims';
import { DIPLOMACY, holds, monthlyDiplomacy, step } from '../src/campaign/diplomacy';
import { allied, atWar, bloc, capitulate, createGame, declareWar, startPhase } from '../src/campaign/game';
import { HISTORY, bookExpects, onBook, setStop } from '../src/campaign/history';
import { NAVY, assaultStrength, navalUpkeep } from '../src/campaign/navy';
import { PACIFIC, demand, freezeAt, keepersOf, mayDemand, pacificOpening } from '../src/campaign/pacific';
import { FAR_FIRST, WORLD_FIRST, WORLD_MAP, protectorOf } from '../src/campaign/world';
import { ops, startOpen } from '../src/campaign/staff';
import { broughtWatch, stations } from '../src/campaign/stations';
import { runTurnAI } from '../src/campaign/turn';
import type { Corps, Game } from '../src/campaign/types';

/** A grand campaign on the whole world at a stop of the dial, moved by hand to a month of the war. */
const war = (stop: number, turn: number, human: string | null = null, seed = 3): Game => {
  const g = startOpen(createGame(seed, human));
  if (stop !== 3) setStop(g, stop, true);
  g.turn = turn;
  return g;
};
const news = (g: Game, turn: number, part: string): boolean => g.headlines.some((h) => h.turn === turn && h.text.includes(part));
const fell = (g: Game, id: string, turn: number, to: string): void => {
  const n = g.nations[id];
  n.alive = false;
  n.fellOn = { turn, week: 1, to };
  for (const x of Object.values(g.nations)) x.atWar = x.atWar.filter((e) => e !== id);
  n.atWar = [];
};
const corps = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.name === name && !c.dead);
const at = (g: Game, name: string): string | undefined => { const c = corps(g, name); return c && g.regions[c.reg].name; };
const SOUTH = () => ops('JAP').find((op) => op.id === 'south')!;
/** France beaten by Germany in a month of the war (June 1940 unless told). */
const france = (stop: number, turn = 10, human: string | null = null): Game => {
  const g = war(stop, turn, human);
  capitulate(g, 'FRA', 'GER');
  return g;
};

describe("Japan's opening (ruling 200): the rows", () => {
  it('gives Japan the strike south first in its list: December 1941 by the book, ten landings (Hong Kong goes by the overland road), three quays, the strike on Pearl Harbor', () => {
    const list = ops('JAP');
    expect(list[0].id).toBe('south');
    const op = SOUTH();
    expect(op.declare).toEqual(['USA', 'UK', 'NED']);
    expect(op.side).toBe(true);
    expect(op.if).toEqual(['flag:pac:strike']);
    // (Hong Kong has left the landing rows for the overland road, rider b572 to ruling 234: far-pacific-hops.test.ts.)
    expect(op.landings!.length).toBe(10);
    expect(op.landings!.map((l) => l.to[0])).toEqual(['Singora', 'Kota Bharu', 'Guam', 'Aparri', 'Legaspi', 'Wake', 'Brunei', 'Davao', 'Lingayen', 'Jolo']);
    expect(op.quays).toEqual(['Hainan', 'Cam Ranh', 'Canton']);
    expect(op.strikes).toEqual([{ fleet: 'Mobile Fleet', harbour: 'Pearl Harbor', home: { base: 'Hiroshima', port: 'Kure', home: 'inland', words: 'The carrier striking force comes home to Kure' } }]);
    expect(HISTORY.dates.JAP?.south).toBe(28);
    expect(HISTORY.theatre['JAP:south']).toBe('pacific');
    expect(HISTORY.theatres.pacific.after).toEqual(['west']);
    expect(HISTORY.bookIf?.['JAP:south']).toEqual(['alive:JAP']);
    expect(HISTORY.joined.axis).toContain('THA');
    expect(HISTORY.marks.filter((m) => m.theatre === 'pacific').map((m) => m.why)).toEqual([
      'war came there before its time', 'Japan has stayed out of Indochina', "Japan did not strike when history's Japan did",
      'Singapore and Manila still stand', 'Singapore still stands', 'Manila still stands', 'Pearl Harbor has fallen', 'Brisbane has fallen',
    ]);
    // Every beach and quay is on the chart, on a sea.
    const g = war(1, 1);
    for (const name of [...op.landings!.flatMap((l) => l.to), ...op.quays!, 'Pearl Harbor']) { expect(g.byName[name], name).toBeDefined(); expect(g.byName[name].seas.length, name).toBeGreaterThan(0); }
  });

  it("reads Siam's row at a declaration of war, and moves the carrier striking force to Etorofu and home again by date", () => {
    const siam = DIPLOMACY.events.find((e) => e.id === 'siam-beside-japan')!;
    expect(siam.when).toBe('war');
    // (At stops 1 to 3 of the dial, not the book's stops alone: riders b591 and b593 to ruling 234. A second row is read at stops 4 and 5.)
    expect(siam.if).toContain('stop:1,2,3');
    expect(siam.if).not.toContain('book');
    const own = DIPLOMACY.events.find((e) => e.id === 'siam-chooses-japan')!;
    expect(own.when).toBe('war');
    expect(own.if).toEqual(siam.if!.map((c) => (c === 'stop:1,2,3' ? 'stop:4,5' : c)).flatMap((c) => (c === 'stop:4,5' ? [c, 'step>=:THA,1'] : [c])));
    expect([own.enter, own.war, own.text]).toEqual([siam.enter, siam.war, siam.text]);
    const moves = (NAVY.moves ?? []).filter((m) => m.nat === 'JAP' && m.fleet === 'Mobile Fleet');
    expect(moves.map((m) => [m.turn, m.base, m.home])).toEqual([[27, 'Etorofu', 'kurile'], [29, 'Hiroshima', 'inland']]);
    expect(moves.every((m) => JSON.stringify(m.stops) === '[1,2]')).toBe(true);
  });

  it("gives Japan landing craft from the first month, a row beside the Allies' of June 1943 (ruling 234(b)2)", () => {
    expect(NAVY.landing.craft).toEqual([[46, ['UK', 'USA'], 0.8], [1, ['JAP'], 0.8]]);
    const g = war(1, 1);
    // A force of Japan's keeps 0.8 of its strength fighting its way off a beach; Britain's and America's 0.6 until June 1943. It never grows.
    expect(assaultStrength(g, 'JAP')).toBe(0.8);
    expect([assaultStrength(g, 'UK'), assaultStrength(g, 'USA'), assaultStrength(g, 'GER')]).toEqual([NAVY.landing.assault, NAVY.landing.assault, NAVY.landing.assault]);
    g.turn = 46;
    expect([assaultStrength(g, 'JAP'), assaultStrength(g, 'UK'), assaultStrength(g, 'USA'), assaultStrength(g, 'GER')]).toEqual([0.8, 0.8, 0.8, NAVY.landing.assault]);
    g.turn = 70;
    expect(assaultStrength(g, 'JAP')).toBe(0.8);
  });
});

describe("a fallen country's far ground (ruling 199)", () => {
  it("names Britain, then Australia, then America for the far side's ground, and leaves the near side's keeper as it was", () => {
    const g = war(1, 9);
    expect(PACIFIC?.keepers).toEqual(['UK', 'AUS', 'USA']);
    expect(keepersOf(g, g.byName['Batavia'])).toEqual(['UK', 'AUS', 'USA']);
    expect(keepersOf(g, g.byName['Curacao'])).toBeNull();
  });

  it("the list is Asia's and the Pacific's: Africa's far ground keeps the world's own order, Britain and then America (ruling 234(e)1)", () => {
    const g = war(1, 9);
    expect(PACIFIC?.africa).toEqual([478, 678]);
    // The pair is the chart's: El Aaiun to Sao Tome, the provinces of Africa beyond the old chart with its islands.
    const first = g.regions.find((r) => r.id >= FAR_FIRST)!.id;
    expect([g.regions[first + 478].name, g.regions[first + 678].name]).toEqual(['El Aaiun', 'Sao Tome']);
    for (const name of ['Dakar', 'Djibouti', 'Brazzaville', 'Tananarive', 'Luanda', 'Massawa', 'Windhoek', 'Reunion', 'Sao Tome', 'El Aaiun']) expect(keepersOf(g, g.byName[name]), name).toEqual(['UK', 'USA']);
    // (Rider b534(5): Africa is the whole continent and its islands. The pair covers all of it on the far chart, the east
    // coast, Madagascar and the islands of both oceans; the provinces before it in that part of the world are Persia's
    // and Arabia's, which are Asia.)
    for (const name of ['Mombasa', 'Dar es Salaam', 'Mogadishu', 'Zanzibar', 'Nairobi', 'Addis Ababa', 'Khartoum', 'Lourenco Marques', 'Cape Town', 'Diego Suarez', 'Mauritius', 'Seychelles', 'Cape Verde', 'Walvis Bay', 'Leopoldville', 'Lagos', 'Monrovia']) {
      expect(g.byName[name], name).toBeDefined();
      expect(keepersOf(g, g.byName[name]), name).toEqual(['UK', 'USA']);
    }
    for (const name of ['Muscat', 'Mukalla', 'Sanaa', 'Jeddah', 'Basra', 'Tehran']) expect(keepersOf(g, g.byName[name]), name).toEqual(['UK', 'AUS', 'USA']);
    // (A town cut out of an African province reads as its province does.)
    const cut = g.regions.find((r) => { const of = (WORLD_MAP.regions[r.id - WORLD_FIRST] as { of?: number } | undefined)?.of; return of !== undefined && of >= first + 478 && of <= first + 678; })!;
    expect(cut).toBeDefined();
    expect(keepersOf(g, cut), cut.name).toEqual(['UK', 'USA']);
    // Asia and the Pacific keep ruling 199's list, to the last province before Africa and the first after it.
    for (const name of ['Aden', 'Goa', 'Addu Atoll', 'Saigon', 'Noumea', 'Dili', 'Macau', 'Batavia']) if (g.byName[name]) expect(keepersOf(g, g.byName[name]), name).toEqual(['UK', 'AUS', 'USA']);
    expect(keepersOf(g, g.regions[first + 477])).toEqual(['UK', 'AUS', 'USA']);
    expect(keepersOf(g, g.regions[first + 679])).toEqual(['UK', 'AUS', 'USA']);
    // Britain fighting, she is the keeper of both. Britain beaten: Africa's keeper is America, where Asia's is Australia.
    if (!atWar(g, 'GER', 'UK')) declareWar(g, 'GER', 'UK');
    if (!atWar(g, 'GER', 'AUS')) declareWar(g, 'GER', 'AUS');
    if (!atWar(g, 'GER', 'USA')) declareWar(g, 'GER', 'USA');
    expect(protectorOf(g, 'GER', g.byName['Tananarive'])).toBe('UK');
    expect(protectorOf(g, 'GER', g.byName['Batavia'])).toBe('UK');
    fell(g, 'UK', 9, 'GER');
    expect(protectorOf(g, 'GER', g.byName['Tananarive'])).toBe('USA');
    expect(protectorOf(g, 'GER', g.byName['Batavia'])).toBe('AUS');
  });

  it("the Netherlands fall: the Indies are in Britain's keeping, and their garrison and fleet stay and answer to Britain", () => {
    const g = war(1, 9);
    declareWar(g, 'GER', 'NED');
    capitulate(g, 'NED', 'GER');
    expect(g.nations.NED.alive).toBe(false);
    for (const name of ['Batavia', 'Soerabaja', 'Palembang', 'Balikpapan']) expect(g.byName[name].ctrl, name).toBe('UK');
    const knil = corps(g, 'KNIL 1st Division (West Java)')!;
    expect(knil.nat).toBe('UK');
    expect(knil.hold).toBe(true);
    expect(at(g, 'KNIL 1st Division (West Java)')).toBe('Batavia');
    const fleet = g.fleets.find((f) => f.name === 'Dutch East Indies Fleet')!;
    expect(fleet.nat).toBe('UK');
    expect(g.regions[fleet.base].name).toBe('Soerabaja');
    // No German soldier or flag reaches the Indies.
    expect(g.regions.some((r) => r.owner === 'NED' && r.overseas && r.ctrl === 'GER')).toBe(false);
  });

  it('nobody on the far side is the Continent\'s guest: the kept garrison stays on Java, and Japan\'s army in Manchukuo when Japan fights beside the Axis', () => {
    const g = war(1, 9);
    declareWar(g, 'GER', 'NED');
    capitulate(g, 'NED', 'GER');
    const knil = corps(g, 'KNIL 1st Division (West Java)')!;
    expect(knil.nat).toBe('UK');
    expect(wantsOut(g, knil)).toBe(false);
    // (Cut off, or broken, it would come off as anyone does.)
    knil.supplied = false;
    expect(wantsOut(g, knil)).toBe(true);
    const h = war(1, 28);
    const guests = h.corps.filter((c) => c.nat === 'JAP' && !c.dead && h.regions[c.reg].owner === 'MAN');
    expect(guests.length).toBeGreaterThan(3);
    declareWar(h, 'JAP', 'UK');
    expect(bloc(h.nations.JAP)).toBe('axis');
    expect(guests.filter((c) => wantsOut(h, c)).map((c) => c.name)).toEqual([]);
  });

  it('Australia keeps it where Britain is beaten', () => {
    const g = war(1, 9);
    fell(g, 'UK', 8, 'GER');
    expect(atWar(g, 'AUS', 'GER')).toBe(true);
    declareWar(g, 'GER', 'NED');
    capitulate(g, 'NED', 'GER');
    expect(g.byName['Batavia'].ctrl).toBe('AUS');
    expect(corps(g, 'KNIL 1st Division (West Java)')!.nat).toBe('AUS');
  });

  it("France falls: Indochina is Vichy's with its garrison, and New Caledonia is in Britain's keeping", () => {
    const g = france(1);
    expect(g.nations.VIC.alive).toBe(true);
    for (const name of ['Saigon', 'Cam Ranh', 'Hanoi']) { expect(g.byName[name].owner, name).toBe('VIC'); expect(g.byName[name].ctrl, name).toBe('VIC'); }
    expect(corps(g, 'Cochinchina and Cambodia Division')!.nat).toBe('VIC');
    expect(at(g, 'Cochinchina and Cambodia Division')).toBe('Saigon');
    expect(g.byName['New Caledonia'].ctrl).toBe('UK');
  });
});

describe('the demand on Indochina (ruling 199)', () => {
  // (The garrison: the design director's 16:44Z and 16:46Z of 21 November 2026, mending his added line to 199(e).
  // It stays at its posts and answers to Japan, as a fallen country's corps answers to its keeper: far-indochina-kept.)
  it('by the book: July 1941, one step and no war, and the garrison stays at its posts under Japan; the freeze follows on its date', () => {
    const g = france(1);
    g.turn = 22;
    pacificOpening(g);
    expect(g.byName['Saigon'].ctrl).toBe('VIC');
    expect(mayDemand(g)).toBe(true);
    g.turn = 23;
    pacificOpening(g);
    // (Japan the holder, Vichy France still the owner: the design director's form 2 of 21:08Z, 21 November 2026; far-indochina-mark.)
    for (const name of ['Saigon', 'Cam Ranh', 'Hanoi', 'Phnom Penh', 'Vientiane']) { expect(g.byName[name].owner, name).toBe('VIC'); expect(g.byName[name].ctrl, name).toBe('JAP'); }
    expect(g.flags['pac:demand']).toBe(23);
    expect(atWar(g, 'JAP', 'VIC')).toBe(false);
    expect(g.nations.VIC.alive).toBe(true);
    expect(g.corps.some((c) => c.nat === 'VIC' && !c.dead && g.regions[c.reg].ctrl === 'JAP')).toBe(false);
    const kept = corps(g, 'Cochinchina and Cambodia Division')!;
    expect([kept.nat, kept.raised, at(g, kept.name), kept.hold, !!kept.stay]).toEqual(['JAP', 'VIC', 'Saigon', true, true]);
    expect(news(g, 23, 'demands Indochina of Vichy France')).toBe(true);
    expect(holds(g, 'holds:JAP,Saigon')).toBe(true);
    expect(mayDemand(g)).toBe(false);
    expect(freezeAt(g)).toBe(0);
    g.turn = 24;
    pacificOpening(g);
    expect(freezeAt(g)).toBe(24);
    // It is made once.
    expect(demand(g)).toBe(false);
  });

  it('where history is loose: the month after Germany and the Soviet Union go to war, and the freeze the month after that', () => {
    const g = france(3, 12);
    g.turn = 20;
    declareWar(g, 'GER', 'SOV');
    pacificOpening(g);
    expect(g.byName['Saigon'].ctrl).toBe('VIC');
    g.turn = 21;
    pacificOpening(g);
    expect(g.byName['Saigon'].ctrl).toBe('JAP');
    expect(g.flags['pac:demand']).toBe(21);
    expect(freezeAt(g)).toBe(0);
    g.turn = 22;
    pacificOpening(g);
    expect(freezeAt(g)).toBe(22);
  });

  it('where history is loose and the east is quiet: the thirteenth month after France fell, and never once Germany is beaten', () => {
    const g = france(3, 12);
    g.turn = 24;
    pacificOpening(g);
    expect(g.byName['Saigon'].ctrl).toBe('VIC');
    g.turn = 25;
    pacificOpening(g);
    expect(g.byName['Saigon'].ctrl).toBe('JAP');
    const h = france(3, 12);
    fell(h, 'GER', 20, 'SOV');
    h.turn = 30;
    pacificOpening(h);
    expect(h.byName['Saigon'].ctrl).toBe('VIC');
    expect(h.flags['pac:demand']).toBeUndefined();
  });

  it("a player's Japan is asked nothing by date: the step is his, and a declaration on Vichy is answered by the handing-over", () => {
    const g = france(1, 10, 'JAP');
    g.turn = 23;
    pacificOpening(g);
    expect(g.byName['Saigon'].ctrl).toBe('VIC');
    expect(mayDemand(g)).toBe(true);
    declareWar(g, 'JAP', 'VIC');
    expect(atWar(g, 'JAP', 'VIC')).toBe(false);
    expect([g.byName['Saigon'].owner, g.byName['Saigon'].ctrl]).toEqual(['VIC', 'JAP']);
    expect(g.byName['Cam Ranh'].ctrl).toBe('JAP');
    expect(g.flags['pac:demand']).toBe(23);
    expect(mayDemand(g)).toBe(false);
  });

  it('none where France stands', () => {
    const g = war(1, 23);
    expect(mayDemand(g)).toBe(false);
    pacificOpening(g);
    expect(g.byName['Saigon'].ctrl).toBe('FRA');
    expect(g.flags['pac:demand']).toBeUndefined();
    expect(demand(g)).toBe(false);
  });
});

describe('Siam beside Japan (ruling 200(b); riders b591 and b593 to ruling 234)', () => {
  it('at stops 1 to 3 of the dial: the day Japan is at war with Britain, Siam joins the Axis and declares on Britain (it let Japan pass within hours, a fact of the day)', () => {
    for (const stop of [1, 2, 3]) {
      const g = war(stop, 28);
      expect(bloc(g.nations.THA)).toBeNull();
      declareWar(g, 'JAP', 'UK');
      expect(bloc(g.nations.THA), `stop ${stop}`).toBe('axis');
      expect(atWar(g, 'THA', 'UK')).toBe(true);
      expect(atWar(g, 'THA', 'JAP')).toBe(false);
      expect(allied(g, 'JAP', 'THA')).toBe(true);
      expect(g.flags['event:siam-beside-japan']).toBe(28);
      expect(news(g, 28, 'Thailand stands with Japan')).toBe(true);
      // Read again at the next declaration, and as the month opens: once is all.
      declareWar(g, 'JAP', 'USA');
      monthlyDiplomacy(g);
      expect(g.headlines.filter((h) => h.text.includes('Thailand stands with Japan')).length).toBe(1);
    }
  });

  it("at stops 4 and 5 it chooses by its character: beside Japan only while it leans to the Axis that day, and never a player's Siam", () => {
    for (const stop of [4, 5]) {
      // As it opens the war, in neither camp and leaning to neither: it stays neutral, whatever Japan does.
      const g = war(stop, 28);
      expect(step(g.nations.THA)).toBe(0);
      declareWar(g, 'JAP', 'UK');
      expect(bloc(g.nations.THA), `stop ${stop}`).toBeNull();
      expect(atWar(g, 'THA', 'UK')).toBe(false);
      expect(g.flags['event:siam-beside-japan']).toBeUndefined();
      expect(g.flags['event:siam-chooses-japan']).toBeUndefined();
      // Courted to Japan's side since: the next declaration finds it leaning, and it stands with Japan.
      g.nations.THA.align = 'leaning_axis';
      declareWar(g, 'JAP', 'USA');
      expect(bloc(g.nations.THA), `stop ${stop}`).toBe('axis');
      expect(atWar(g, 'THA', 'UK')).toBe(true);
      expect(g.flags['event:siam-chooses-japan']).toBe(28);
      expect(g.flags['event:siam-beside-japan']).toBeUndefined();
      expect(g.headlines.filter((h) => h.text.includes('Thailand stands with Japan')).length).toBe(1);
      // Leaning on the day of the blow itself: that day.
      const w = war(stop, 28);
      w.nations.THA.align = 'leaning_axis';
      declareWar(w, 'JAP', 'UK');
      expect(bloc(w.nations.THA), `stop ${stop}`).toBe('axis');
      // Leaning to the Allies: neutral.
      const a = war(stop, 28);
      a.nations.THA.align = 'leaning_allies';
      declareWar(a, 'JAP', 'UK');
      expect(bloc(a.nations.THA), `stop ${stop}`).toBeNull();
    }
    // (The second row is not read at stops 1 to 3, nor the first at 4 and 5: one row a stop.)
    const g = war(3, 28);
    declareWar(g, 'JAP', 'UK');
    expect([g.flags['event:siam-beside-japan'], g.flags['event:siam-chooses-japan']]).toEqual([28, undefined]);
    const h = war(1, 28, 'THA');
    declareWar(h, 'JAP', 'UK');
    expect(bloc(h.nations.THA)).toBeNull();
    expect(atWar(h, 'THA', 'UK')).toBe(false);
  });
});

/**
 * A made-up December 1941: the Netherlands and France struck down by hand in their months, the opening read month by
 * month from June 1941, Japan's dated rows taken in, America's fleet put at Pearl Harbor as its row of May 1940 puts
 * it, the China front's dated choices counted as taken; then Japan's own month is played.
 */
const december = (stop: number): Game => {
  const g = war(stop, 9, null, 17);
  declareWar(g, 'GER', 'NED');
  capitulate(g, 'NED', 'GER');
  g.turn = 10;
  capitulate(g, 'FRA', 'GER');
  for (const t of [22, 23, 24, 25, 27]) { g.turn = t; bookMonth(g); monthlyDiplomacy(g); if (t === 24 || t === 27) startPhase(g, 'JAP'); }
  for (const op of ops('JAP')) if (op.id !== 'south') g.flags[`st:done:JAP:${op.id}`] = 1;
  // (America's months are not played here: its fleets are put at Pearl Harbor by hand, where the record's move of May 1940 has them.)
  const sea = g.seas.findIndex((z) => z.key === 'hawaii');
  for (const f of g.fleets.filter((x) => x.nat === 'USA' && (x.name === 'Pacific Fleet' || x.name === 'Pacific Carrier Fleet'))) {
    f.base = g.byName['Pearl Harbor'].id; f.port = 'Pearl Harbor'; f.home = sea; f.zone = sea; f.station = sea; f.stance = 'port';
  }
  g.turn = 28;
  bookMonth(g);
  monthlyDiplomacy(g);
  // America's month at sea, as the month's own order has it before Japan moves: by the book the carriers are out (ruling 234(d)).
  navalUpkeep(g, 'USA');
  return g;
};

describe("Japan's month of December 1941", () => {
  it('by the book: the armies wait on their quays in November, and the carrier striking force lies at Etorofu', () => {
    const g = december(1);
    expect(g.byName['Saigon'].ctrl).toBe('JAP');
    expect(freezeAt(g)).toBe(24);
    expect(onBook(g, 'pacific')).toBe(true);
    const quay: Record<string, string> = {
      '25th Army (5th Division)': 'Hainan', '25th Army (18th Division)': 'Hainan', 'South Seas Detachment': 'Chichi Jima', '14th Army (16th Division)': 'Okinawa',
      'Kawaguchi Detachment (35th Brigade, of the 18th Division)': 'Cam Ranh', 'Sakaguchi Detachment (56th Mixed Infantry Group)': 'Palau', '14th Army (48th Division)': 'Formosa',
    };
    for (const l of SOUTH().landings!) {
      const c = corps(g, l.corps);
      // (The 38th Division comes to Canton by a dated row of October 1939, which this made-up war never calls.)
      if (!c && l.corps === '38th Division (Canton)') continue;
      expect(c, l.corps).toBeDefined();
      const r = g.regions[c!.reg];
      expect(r.port && r.ctrl === 'JAP', l.corps).toBe(true);
      if (quay[l.corps]) expect(r.name, l.corps).toBe(quay[l.corps]);
    }
    const fleet = g.fleets.find((x) => x.nat === 'JAP' && x.name === 'Mobile Fleet')!;
    expect(g.regions[fleet.base].name).toBe('Etorofu');
    expect(g.seas[fleet.zone].key).toBe('kurile');
  });

  it('by the book: a corps the great blow still has to land waits for its landing, and no later row takes it to a quay first', () => {
    // The quay rule (the design director's 18:08Z of 22 November 2026: a dated landing's corps goes to its quay a month
    // ahead) knew only the table's own rows. The blow's ten landings are the book's and no rows of the table: Rabaul's
    // row of January took the South Seas Detachment from Chichi Jima to Truk in December with its month spent, and
    // Guam stood American at the month's end. The staff names the corps the blow still owes a landing: ai.ts, blowOwes.
    const g = december(1);
    const c = corps(g, 'South Seas Detachment')!;
    expect(at(g, 'South Seas Detachment')).toBe('Chichi Jima');
    const rabaul = stations()!.attacks!.find((a) => a.name === 'Rabaul')!;
    expect(rabaul).toMatchObject({ month: 29, sea: true, force: ['South Seas Detachment'], from: ['Guam', 'Truk'] });
    expect(SOUTH().landings!.some((l) => l.corps === 'South Seas Detachment' && l.to.includes('Guam'))).toBe(true);
    const brought: string[] = [];
    broughtWatch.tell = (e) => { if (e.corps === c) brought.push(`${e.row.name}: ${e.from.name} > ${e.to.name}`); };
    try { g.phase = g.order.indexOf('JAP'); runTurnAI(g); } finally { broughtWatch.tell = null; }
    expect(g.turn).toBe(29);
    expect(brought).toEqual([]);
    expect(g.byName['Guam'].ctrl).toBe('JAP');
    // (It stands on Guam, the first of Rabaul's quays, as the record's did: it sailed from Guam on 14 January 1942.)
    expect(at(g, 'South Seas Detachment')).toBe('Guam');
  });

  it('by the book: Japan declares on America and Britain, Siam joins, the carriers strike Pearl Harbor, the armies land, and Germany follows as the month closes', () => {
    const g = december(1);
    const before = g.fleets.find((x) => x.nat === 'USA' && x.name === 'Pacific Fleet')!;
    const afloat = before.ships.bb + before.ships.cv + before.ships.ca + before.ships.dd;
    g.phase = g.order.indexOf('JAP');
    runTurnAI(g);
    expect(g.turn).toBe(29);
    expect(g.flags['st:done:JAP:south']).toBe(28);
    expect(g.flags['war:JAP:USA']).toBe(28);
    expect(g.flags['war:JAP:UK']).toBe(28);
    expect(atWar(g, 'JAP', 'USA') && atWar(g, 'JAP', 'UK') && atWar(g, 'JAP', 'AUS')).toBe(true);
    expect(bloc(g.nations.JAP)).toBe('axis');
    expect(g.flags['event:siam-beside-japan']).toBe(28);
    expect(atWar(g, 'THA', 'UK')).toBe(true);
    // The strike: flown the month the blow falls, without warning, at what lay in the harbour.
    expect(news(g, 28, 'raid Pearl Harbor without warning')).toBe(true);
    const after = g.fleets.find((x) => x.nat === 'USA' && x.name === 'Pacific Fleet')!;
    expect(after.ships.bb + after.ships.cv + after.ships.ca + after.ships.dd).toBeLessThan(afloat);
    // The strike flown, the force comes home to Kure, its admiral's again in home waters: the game holds the striking
    // force as one fleet, and the row goes where most of it went. (Two of its six carriers stood off Wake for three
    // days by the record; a part of the fleet is no row of the game's, and Wake's landing of the 23rd sails without it.)
    const force = g.fleets.find((x) => x.nat === 'JAP' && x.name === 'Mobile Fleet')!;
    // (Beyond a month's steaming from the Kuriles, its way home is a voyage: release 2, the far chart, the design
    // director's ruling of 15:48Z, 25 November 2026, 3. It sails for Kure, its base already Hiroshima's.)
    expect(news(g, 28, 'The Mobile Fleet sails for its new base at Kure')).toBe(true);
    expect(g.regions[force.base].name).toBe('Hiroshima');
    expect(force.port).toBe('Kure');
    expect(force.stance).toBe('passage');
    expect(force.passage!.way).toContain(force.zone);
    expect(force.bookBase).toBeUndefined();
    // The quays: the two divisions on Hainan, which Japan holds and does not own, have sailed.
    // (The 5th lands at Singora, Siam's shore, and is carried down the west coast within the month by the record's dated
    // fights, Jitra and Kampar: far-pacific-war.json, 'attacks'.)
    expect(at(g, '25th Army (5th Division)')).toBe('Ipoh');
    expect(at(g, '25th Army (18th Division)')).toBe('Kota Bharu');
    // By the book the ten landings of the great blow sail on the record's own shipping, each in its record month, and
    // draw nothing from the month's lift of six (rider b626 to ruling 234: the record shows the shipping sufficed).
    // With Guam's garrison at the record's size (ruling 234(b): some 750 men, not a brigade) the South Seas
    // Detachment lands in December as it did. The Kawaguchi Detachment, ashore at Miri and Seria on the 16th, has gone
    // on by the book's dated rows to Brunei overland and to Kuching by sea, which fell on the 24th and 25th (the design
    // director's rulings of 03:59Z and 04:06Z, 22 November 2026: tests/far-borneo-book.test.ts). It stood at Brunei at
    // the month's end before those rows were laid.
    expect(at(g, 'South Seas Detachment')).toBe('Guam');
    expect(at(g, 'Kawaguchi Detachment (35th Brigade, of the 18th Division)')).toBe('Kuching');
    // All ten are ashore in December: nine stand on their own shores, and the 5th Division, landed at Singora, has been
    // carried on to Ipoh. The month's lift of six is not spent on them.
    const ashore = SOUTH().landings!.filter((l) => l.to.includes(at(g, l.corps) ?? '')).map((l) => l.corps);
    expect(SOUTH().landings!.map((l) => l.corps).filter((c) => !ashore.includes(c))).toEqual(['25th Army (5th Division)']);
    expect(g.flags['lift:JAP:28'] ?? 0).toBeLessThan(6);
    // The month closes: Germany and Italy declare on America, and the news names Pearl Harbor.
    expect(g.flags['event:pearl-harbor-world']).toBe(28);
    expect(atWar(g, 'USA', 'GER')).toBe(true);
    expect(news(g, 28, 'Japan attacks the American fleet at Pearl Harbor')).toBe(true);
    expect(onBook(g, 'pacific')).toBe(true);
  });

  it("where history is loose the strike south waits for the staff's word, and no date sends it", () => {
    const g = december(3);
    // (The staff's word is the readiness test's, far-ready.test.ts: here the freeze is a month old and its window is not open.)
    g.flags['pac:freeze'] = 27;
    g.phase = g.order.indexOf('JAP');
    runTurnAI(g);
    expect(g.flags['st:done:JAP:south']).toBeUndefined();
    expect(atWar(g, 'JAP', 'USA')).toBe(false);
    expect(atWar(g, 'JAP', 'UK')).toBe(false);
    expect(bloc(g.nations.THA)).toBeNull();
  });
});

describe("the plan's strike (ruling 200(c))", () => {
  it('a harbour with no ship of an enemy in it is not struck, and nothing sails', () => {
    const g = war(1, 28);
    declareWar(g, 'JAP', 'UK');
    expect(atWar(g, 'JAP', 'AUS')).toBe(true);
    const fleet = g.fleets.find((x) => x.nat === 'JAP' && x.name === 'Mobile Fleet')!;
    const was = [fleet.zone, fleet.stance];
    expect(plannedStrike(g, 'JAP', 'Mobile Fleet', 'Brisbane')).toBe(false);
    expect([fleet.zone, fleet.stance]).toEqual(was);
    // Nor a harbour of a country at peace, nor by a fleet that is not there.
    expect(plannedStrike(g, 'JAP', 'Mobile Fleet', 'Pearl Harbor')).toBe(false);
    expect(plannedStrike(g, 'JAP', 'No Such Fleet', 'Singapore')).toBe(false);
  });

  it("a player's fleets are his own", () => {
    const g = war(1, 28, 'JAP');
    declareWar(g, 'JAP', 'UK');
    expect(plannedStrike(g, 'JAP', 'Mobile Fleet', 'Singapore')).toBe(false);
  });
});

/** A state set by hand has fought none of Europe's war: what the book asks of the other theatres by a month counts as asked, so the Pacific's own marks are read alone. */
const pacificOnly = (g: Game): Game => {
  HISTORY.marks.forEach((m, k) => { if (m.theatre !== 'pacific' && m.by !== undefined) g.flags[`hist:mk:${k}`] = 1; });
  return g;
};

describe("the Pacific's page of history's book", () => {
  it("closes when the Pacific is at war before its time, and leaves the west's page open", () => {
    const g = pacificOnly(war(1, 20));
    declareWar(g, 'JAP', 'USA');
    const failed = bookMonth(g);
    expect(failed.map((m) => m.why)).toContain('war came there before its time');
    expect(onBook(g, 'pacific')).toBe(false);
    expect(onBook(g, 'west')).toBe(true);
    expect(news(g, 20, "History's book is closed in the Pacific: war came there before its time.")).toBe(true);
  });

  it('closes when Japan has stayed out of Indochina, and when Japan has not struck', () => {
    const g = pacificOnly(war(1, 24));
    expect(bookMonth(g).length).toBe(0);
    g.turn = 25;
    expect(bookMonth(g).map((m) => m.why)).toEqual(['Japan has stayed out of Indochina']);
    expect(onBook(g, 'pacific')).toBe(false);
    const h = pacificOnly(france(1));
    h.turn = 23;
    pacificOpening(h);
    h.turn = 25;
    expect(bookMonth(h).length).toBe(0);
    h.turn = 29;
    expect(bookMonth(h).length).toBe(0);
    h.turn = 30;
    expect(bookMonth(h).map((m) => m.why)).toEqual(["Japan did not strike when history's Japan did"]);
  });

  it('by June 1942 asks for Singapore and Manila, and the news names the one that stands', () => {
    const taken = (g: Game, ...places: string[]): Game => { for (const p of places) g.byName[p].ctrl = 'JAP'; return g; };
    const asked = (...places: string[]): string[] => {
      const g = taken(pacificOnly(war(1, 34)), 'Saigon', ...places);
      g.nations.JAP.align = 'axis';
      declareWar(g, 'JAP', 'USA');
      // (The month before the reckoning nothing is asked of the two fortresses.)
      HISTORY.marks.forEach((m, k) => { if (m.theatre === 'pacific' && m.by !== undefined && m.by < 34) g.flags[`hist:mk:${k}`] = 1; });
      expect(bookMonth(g).length).toBe(0);
      g.turn = 35;
      return bookMonth(g).map((m) => m.why);
    };
    expect(asked()).toEqual(['Singapore and Manila still stand']);
    expect(asked('Manila')).toEqual(['Singapore still stands']);
    expect(asked('Singapore')).toEqual(['Manila still stands']);
    expect(asked('Singapore', 'Manila')).toEqual([]);
  });

  it('tells on the dial what the book expects of the Pacific, each thing once', () => {
    const g = war(1, 1);
    expect(bookExpects(g).filter((x) => x.theatre === 'pacific').map((x) => x.text)).toEqual([
      'The book is closed in the Pacific if Japan has stayed out of Indochina when August 1941 is over.',
      'The book is closed in the Pacific if Japan goes to war with America or Britain before its time: that is, before December 1941.',
      'The book is closed in the Pacific if Japan has not yet struck when January 1942 is over.',
      'The book is closed in the Pacific if Singapore or Manila still stands when June 1942 is over.',
      'The book is closed in the Pacific if Pearl Harbor falls: that is, before August 1945.',
      'The book is closed in the Pacific if Brisbane falls: that is, before August 1945.',
    ]);
  });

  it('closes when America loses Pearl Harbor, or Australia Brisbane', () => {
    for (const [place, why] of [['Pearl Harbor', 'Pearl Harbor has fallen'], ['Brisbane', 'Brisbane has fallen']] as const) {
      const g = pacificOnly(war(1, 20));
      expect(bookMonth(g).length).toBe(0);
      g.byName[place].ctrl = 'JAP';
      expect(bookMonth(g).map((m) => m.why), place).toEqual([why]);
      expect(onBook(g, 'pacific')).toBe(false);
    }
  });
});
