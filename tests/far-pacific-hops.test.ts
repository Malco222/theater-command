// The months after December 1941 with the far side's switch on (ruling 200, finding 2, and ruling 234(a); the Pacific
// planner's papers 14 and 18 and its south-line.json). The Pacific's own shores rows (who may land where, from which
// month), the ground they read, the record's line in the south (its ceiling, its floor, the field, the slide), the
// corps a great choice takes off hold, the eleventh crossing, and the staff's own landings played for one month. The
// states are set by hand on the first day's chart; one made-up December 1941 and the January after it are played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { hopWatch } from '../src/campaign/ai';
import { bookMonth } from '../src/campaign/claims';
import { monthlyDiplomacy } from '../src/campaign/diplomacy';
import { addCorps, atWar, capitulate, createGame, declareWar, startPhase } from '../src/campaign/game';
import { HISTORY, bookDate, onBook, setStop, shoreOpen } from '../src/campaign/history';
import { frontArmys, lineAny, lineGives, lineLeaves, lineShut, lines, neverHis } from '../src/campaign/line';
import { HOP_AIMS, PACIFIC, hopAims, hopShores, hopsBind, pacificGround } from '../src/campaign/pacific';
import { NAVY, closedTo, navalAI, seaPath } from '../src/campaign/navy';
import { freed, ops, startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';

/** A grand campaign on the whole world at a stop of the dial, moved by hand to a month of the war. */
const war = (stop: number, turn: number, human: string | null = null, seed = 3): Game => {
  const g = startOpen(createGame(seed, human));
  if (stop !== 3) setStop(g, stop, true);
  g.turn = turn;
  return g;
};
/** The same, with Japan at war with America, Britain, the Dutch and the dominions as the strike south leaves it. */
const pacificWar = (stop: number, turn: number, human: string | null = null, seed = 3): Game => {
  const g = war(stop, turn, human, seed);
  for (const e of ['USA', 'UK', 'NED', 'AUS', 'NZL']) if (!atWar(g, 'JAP', e)) declareWar(g, 'JAP', e);
  g.turn = turn;
  return g;
};
const corps = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.name === name && !c.dead);
const at = (g: Game, name: string): string | undefined => { const c = corps(g, name); return c && g.regions[c.reg].name; };
const names = (rs: Region[]): string[] => rs.map((r) => r.name);
const SOUTH = () => ops('JAP').find((op) => op.id === 'south')!;
const LINE = () => lines().find((L) => L.id === 'south')!;
type Hops = { theatre: string; slides: string; axis: [string, number][]; allies: [string, number][] };
const HOPS = (): Hops => (HISTORY as unknown as { hops: Hops }).hops;

describe('the Pacific\'s shores rows (ruling 200, finding 2)', () => {
  it('lays the rows: the ground, the two lists by month, and nothing dated before the record gives the place', () => {
    expect(PACIFIC!.shores).toEqual({ slice: 312, seas: ['bengal', 'andaman', 'ceylon', 'malacca'] });
    const H = HOPS();
    expect(H.theatre).toBe('pacific');
    expect(H.slides).toBe('JAP:south');
    expect(H.axis.length).toBe(84);
    expect(H.allies.length).toBe(49);
    const g = war(1, 28);
    const L = LINE();
    for (const [name, month] of [...H.axis, ...H.allies]) {
      const r = g.byName[name];
      expect(r, name).toBeDefined();
      expect(month, name).toBeGreaterThanOrEqual(28);
    }
    // (Both lists name places inland too: a month holds an attack by land as well as a landing, ruling 202(m). A staff
    // lands only on the shores among them.)
    expect(g.byName['Toungoo'].seas.length).toBe(0);
    expect(new Map(H.axis).has('Toungoo') && new Map(H.allies).has('Toungoo')).toBe(true);
    const all = pacificWar(1, 72);
    for (const nat of ['JAP', 'USA', 'UK']) for (const r of [...hopShores(all, nat)!.open, ...hopShores(all, nat)!.soon]) expect(r.seas.length, `${nat} ${r.name}`).toBeGreaterThan(0);
    // Japan's side lands nowhere before the line gives it the place (Singora is Siam's, and on no line).
    for (const [name, month] of H.axis) {
      const spans = L.spans[name];
      if (name === 'Singora') { expect(spans).toBeUndefined(); continue; }
      expect(spans, name).toBeDefined();
      expect(month, name).toBeGreaterThanOrEqual(Math.min(...spans.map((s) => s.from)));
    }
    // America's two roads and Britain's road back to Burma, with their first and last months.
    const allies = new Map(H.allies);
    expect(allies.get('Guadalcanal')).toBe(36);
    expect(allies.get('Okinawa')).toBe(68);
    expect(allies.get('Akyab')).toBe(40);
    expect(allies.get('Singapore')).toBe(76);
  });

  it('reads the Pacific\'s ground: the Pacific\'s own slice of the chart, and the coasts of the Bay of Bengal, the Andaman Sea, Ceylon\'s waters and the Strait of Malacca', () => {
    const g = war(1, 28);
    for (const nm of ['Pearl Harbor', 'Manila', 'Darwin', 'Tokyo', 'Rabaul', 'Singapore', 'Calcutta', 'Madras', 'Colombo', 'Akyab', 'Rangoon']) expect(pacificGround(g, g.byName[nm]), nm).toBe(true);
    for (const nm of ['London', 'Bombay', 'Karachi', 'San Francisco', 'Bremen', 'Aden', 'Vancouver']) expect(pacificGround(g, g.byName[nm]), nm).toBe(false);
  });

  it('by the book opens to Japan\'s side the shores whose month has come, and names next month\'s ahead', () => {
    const g = pacificWar(1, 29);
    expect(hopsBind(g)).toBe(true);
    const S = hopShores(g, 'JAP')!;
    expect(S.book).toBe(true);
    const open = names(S.open), soon = names(S.soon);
    for (const nm of ['Menado', 'Kendari', 'Ambon', 'Rabaul', 'Kuching', 'Jolo']) expect(open, nm).toContain(nm);
    for (const nm of ['Palembang', 'Makassar', 'Bali', 'Koepang']) { expect(open, nm).not.toContain(nm); expect(soon, nm).toContain(nm); }
    for (const nm of ['Batavia', 'Guadalcanal', 'Attu', 'Midway', 'Port Moresby', 'Fiji', 'Darwin', 'Pearl Harbor', 'Calcutta']) { expect(open, nm).not.toContain(nm); expect(soon, nm).not.toContain(nm); }
    // In the order of the rows: the record's own order of the month.
    const order = HOPS().axis.map(([nm]) => nm);
    expect([...open].sort((a, b) => order.indexOf(a) - order.indexOf(b))).toEqual(open);
    g.turn = 31;
    expect(names(hopShores(g, 'JAP')!.open)).toContain('Batavia');
    // A shore its side holds is no shore to land on.
    g.byName['Menado'].ctrl = 'JAP';
    expect(names(hopShores(g, 'JAP')!.open)).not.toContain('Menado');
  });

  it('by the book opens to America\'s side only the road back\'s places in their months, and at once what Japan took beyond the record', () => {
    const g = pacificWar(1, 30);
    for (const nm of ['Guadalcanal', 'Rabaul', 'Guam', 'Wake', 'Manila', 'Tarawa']) g.byName[nm].ctrl = 'JAP';
    expect(hopShores(g, 'USA')!.open).toEqual([]);
    expect(hopShores(g, 'AUS')!.open).toEqual([]);
    g.turn = 35;
    expect(names(hopShores(g, 'USA')!.soon)).toEqual(['Guadalcanal']);
    g.turn = 36;
    expect(names(hopShores(g, 'USA')!.open)).toEqual(['Guadalcanal']);
    // Rabaul, Wake and Truk are passed while the book stands; Manila waits for February 1945.
    g.turn = 60;
    const later = names(hopShores(g, 'USA')!.open);
    expect(later).toContain('Guam');
    for (const nm of ['Rabaul', 'Wake', 'Manila']) expect(later, nm).not.toContain(nm);
    // Ruling 200(i): Midway on its one try, and Fiji beyond the book, are open the month they are lost.
    g.turn = 34;
    g.byName['Midway'].ctrl = 'JAP';
    g.byName['Fiji'].ctrl = 'JAP';
    expect(neverHis(g, g.byName['Midway'])).toBe(true);
    expect(neverHis(g, g.byName['Fiji'])).toBe(true);
    expect(neverHis(g, g.byName['Rabaul'])).toBe(false);
    const lost = names(hopShores(g, 'USA')!.open);
    expect(lost).toContain('Midway');
    expect(lost).toContain('Fiji');
  });

  it('reads no row where history is loose or the Pacific\'s book is closed: every enemy coast on the Pacific\'s ground is open, and none elsewhere', () => {
    const g = pacificWar(3, 29);
    expect(hopsBind(g)).toBe(false);
    const S = hopShores(g, 'JAP')!;
    expect(S.book).toBe(false);
    expect(S.soon).toEqual([]);
    const open = names(S.open);
    for (const nm of ['Batavia', 'Darwin', 'Pearl Harbor', 'Calcutta', 'Midway']) expect(open, nm).toContain(nm);
    for (const nm of ['London', 'Bombay', 'San Francisco']) expect(open, nm).not.toContain(nm);
    for (const r of S.open) expect(pacificGround(g, r) && atWar(g, 'JAP', r.ctrl), r.name).toBe(true);
    const h = pacificWar(1, 29);
    expect(hopShores(h, 'JAP')!.book).toBe(true);
    h.flags['hist:off:pacific'] = 29;
    expect(onBook(h, 'pacific')).toBe(false);
    expect(hopShores(h, 'JAP')!.book).toBe(false);
    expect(lineShut(h, 'JAP', h.byName['Darwin'])).toBe(false);
  });

  it('gives the player no list, a country at peace with Japan none, and nobody one once Japan has fallen', () => {
    const g = pacificWar(1, 29, 'JAP');
    expect(hopShores(g, 'JAP')).toBeNull();
    expect(hopShores(g, 'USA')).not.toBeNull();
    const h = pacificWar(1, 29);
    expect(hopShores(h, 'SWE')).toBeNull();
    expect(hopShores(h, 'SOV')).toBeNull();
    h.nations.JAP.alive = false;
    expect(hopShores(h, 'USA')).toBeNull();
  });

  it('shuts the second front\'s own rule out of the Pacific\'s ground while the Pacific\'s book stands', () => {
    const g = pacificWar(1, 60);
    expect(shoreOpen(g, 'USA', true, undefined, true)).toBe(false);
    expect(shoreOpen(g, 'JAP', false, undefined, true)).toBe(false);
    const h = pacificWar(3, 60);
    expect(shoreOpen(h, 'USA', true, undefined, true)).toBe(true);
  });

  it('keeps the shores a staff has named in the flags, six at most, for the fleet and the fighters to read', () => {
    const g = pacificWar(1, 29);
    expect(hopAims(g, 'JAP')).toEqual([]);
    g.flags['st:hop:JAP:0'] = g.byName['Jolo'].id + 1;
    g.flags['st:hopQ:JAP:0'] = g.byName['Palau'].id + 1;
    g.flags[`st:hop:JAP:${HOP_AIMS}`] = g.byName['Menado'].id + 1;
    g.flags[`st:hopQ:JAP:${HOP_AIMS}`] = g.byName['Palau'].id + 1;
    const aims = hopAims(g, 'JAP');
    expect(aims.length).toBe(1);
    expect([aims[0].from.name, aims[0].to.name]).toEqual(['Palau', 'Jolo']);
    expect(hopAims(g, 'USA')).toEqual([]);
  });
});

describe('the record\'s line in the south (ruling 234(a))', () => {
  it('lays one line on the ground of six countries, with the road back\'s months and the places it never gives', () => {
    const L = LINE();
    expect(L.theatre).toBe('pacific');
    expect(L.taker).toBe('JAP');
    // (Japan's side takes from five; New Zealand's ground is on the line too, named only as never taken.)
    expect(L.lands).toEqual(['AUS', 'NED', 'NZL', 'POR', 'UK', 'USA']);
    expect([L.stays, L.field, L.slides, L.ground, L.until]).toEqual([true, true, 'JAP:south', 'pacific', 72]);
    expect(L.floor).toEqual({ until: 72, kinds: ['opening', 'release', 'later'] });
    const g = war(1, 28);
    for (const name of [...Object.keys(L.spans), ...Object.keys(L.back ?? {}), ...(L.shut ?? [])]) {
      const r = g.byName[name];
      expect(r, name).toBeDefined();
      // (New Caledonia is France's: it is named, and bound by its name; France is not one of the line's lands.)
      if ((L.spans[name] || L.shut!.includes(name)) && name !== 'New Caledonia') expect(L.lands, name).toContain(r.owner);
    }
    expect(g.byName['New Caledonia'].owner).toBe('FRA');
    expect(L.shut).toContain('New Caledonia');
    expect(L.spans['Milne Bay']).toEqual([{ from: 36, to: 36, kind: 'tried' }]);
    expect([L.spans['Madang'][0].from, L.spans['Wewak'][0].from, L.spans['New Georgia'][0].from]).toEqual([40, 40, 39]);
    // The historian's months (ruling 234, section 9), and the two tries.
    expect(L.spans['Mindoro'][0].from).toBe(30);
    expect(L.spans['Pontianak'][0].from).toBe(29);
    expect(L.spans['Midway']).toEqual([{ from: 34, to: 34, kind: 'tried' }]);
    expect(L.back!['Guadalcanal']).toBe(36);
    expect(L.back!['Singapore']).toBe(76);
    // China's line stands first, and is not the south's.
    expect(lines()[0].id).not.toBe('south');
  });

  it('the ceiling: Japan\'s side neither attacks nor enters a place before its month, and never one the record does not give it', () => {
    const g = pacificWar(1, 28);
    const shut = (nat: string, nm: string): boolean => lineShut(g, nat, g.byName[nm]);
    for (const nm of ['Kota Bharu', 'Hong Kong', 'Guam', 'Legaspi']) expect(shut('JAP', nm), nm).toBe(false);
    for (const nm of ['Rabaul', 'Menado', 'Singapore', 'Batavia', 'Calcutta', 'Darwin', 'Fiji', 'Pearl Harbor', 'Chittagong', 'Auckland', 'Apia']) expect(shut('JAP', nm), nm).toBe(true);
    // An ally of Japan's is bound as Japan is.
    expect(shut('THA', 'Singapore')).toBe(true);
    expect(shut('GER', 'Rabaul')).toBe(true);
    g.turn = 29;
    expect(shut('JAP', 'Rabaul')).toBe(false);
    expect(shut('JAP', 'Singapore')).toBe(true);
    g.turn = 30;
    expect(shut('JAP', 'Singapore')).toBe(false);
    // Midway's one month, and never again.
    g.turn = 34;
    expect(shut('JAP', 'Midway')).toBe(false);
    g.turn = 35;
    expect(shut('JAP', 'Midway')).toBe(true);
    // A place the line names is bound whoever owns it; the rest of France's ground, Indochina, is not on the line.
    expect(shut('JAP', 'New Caledonia')).toBe(true);
    for (const nm of ['Saigon', 'Hanoi', 'Cam Ranh']) expect(shut('JAP', nm), nm).toBe(false);
    // Milne Bay's one try, in August 1942.
    g.turn = 36;
    expect(shut('JAP', 'Milne Bay')).toBe(false);
    g.turn = 37;
    expect(shut('JAP', 'Milne Bay')).toBe(true);
    // The line is drawn nowhere off its ground: Britain's home, Germany's road in Africa.
    for (const nm of ['London', 'Alexandria', 'Bombay']) expect(shut('JAP', nm), nm).toBe(false);
    expect(lineShut(g, 'GER', g.byName['Alexandria'])).toBe(false);
  });

  it('the floor: the Allied staffs do not retake what the record gives Japan before the road back\'s month for the place', () => {
    const g = pacificWar(1, 30);
    const kb = g.byName['Kota Bharu'], gc = g.byName['Guadalcanal'], rab = g.byName['Rabaul'], mid = g.byName['Midway'];
    expect(lineShut(g, 'UK', kb)).toBe(false); // (Britain's own ground, still Britain's)
    for (const r of [kb, gc, rab, mid]) r.ctrl = 'JAP';
    expect(lineShut(g, 'UK', kb)).toBe(true);
    expect(lineShut(g, 'AUS', kb)).toBe(true);
    expect(lineShut(g, 'USA', rab)).toBe(true);
    g.turn = 73;
    expect(lineShut(g, 'UK', kb)).toBe(false); // (the line's last month is 72)
    g.turn = 35;
    expect(lineShut(g, 'USA', gc)).toBe(true);
    g.turn = 36;
    expect(lineShut(g, 'USA', gc)).toBe(false); // (August 1942)
    expect(lineShut(g, 'USA', rab)).toBe(true); // (passed: no month of its own)
    // A place Japan only tried for is nobody's to wait for.
    g.turn = 34;
    expect(lineShut(g, 'USA', mid)).toBe(false);
    // The floor never holds China's staff off its own ground's line, nor a neutral.
    expect(lineShut(g, 'SWE', kb)).toBe(false);
  });

  it('the floor reads who holds the field: a beach Japan\'s army stands on is not counter-attacked while its town holds out', () => {
    const g = pacificWar(1, 29);
    const leg = g.byName['Legaspi'];
    expect(leg.ctrl).toBe('USA');
    expect(lineShut(g, 'USA', leg)).toBe(false);
    const c = addCorps(g, 'JAP', 'A landing force', 'inf', 1, leg.id, {});
    expect(lineShut(g, 'USA', leg)).toBe(true);
    c.dead = true;
    expect(lineShut(g, 'USA', leg)).toBe(false);
  });

  it('nobody marches home and nothing is handed back when a span ends: the south\'s line stays', () => {
    const g = pacificWar(1, 43);
    const gc = g.byName['Guadalcanal'];
    gc.ctrl = 'JAP';
    expect(lineShut(g, 'JAP', gc)).toBe(true); // (its span ended in February 1943: Japan does not come again)
    expect(lineLeaves(g, 'JAP', gc, 43)).toBe(false);
    expect(lineGives(g, 'JAP', 43).some((x) => x.r === gc)).toBe(false);
    // China's line still sends its raids home, and keeps Japan's army of that front off the ships.
    const canton = g.byName['Canton'];
    expect(frontArmys(g, 'JAP', canton)).toBe(true);
    expect(frontArmys(g, 'JAP', g.byName['Rabaul'])).toBe(false);
    expect(frontArmys(g, 'USA', canton)).toBe(false);
    expect(typeof lineAny(g, 'JAP')).toBe('boolean');
  });

  it('binds computer against computer only: never the player, never the player\'s own ground, and nobody with the player as Japan', () => {
    const uk = pacificWar(1, 28, 'UK');
    expect(lineShut(uk, 'JAP', uk.byName['Singapore'])).toBe(false); // (the player's ground: the staff decides for itself)
    expect(lineShut(uk, 'JAP', uk.byName['Rabaul'])).toBe(true); // (Australia's, computer against computer)
    expect(lineShut(uk, 'JAP', uk.byName['Manila'])).toBe(true);
    uk.byName['Guam'].ctrl = 'JAP';
    expect(lineShut(uk, 'UK', uk.byName['Guam'])).toBe(false); // (the player's own corps go where he sends them)
    expect(lineShut(uk, 'USA', uk.byName['Guam'])).toBe(true);
    const jap = pacificWar(1, 28, 'JAP');
    for (const nm of ['Singapore', 'Rabaul', 'Manila', 'Darwin']) expect(lineShut(jap, 'JAP', jap.byName[nm]), nm).toBe(false);
    jap.byName['Guam'].ctrl = 'JAP';
    expect(lineShut(jap, 'USA', jap.byName['Guam'])).toBe(false);
  });

  it('is gone where history is loose, and at stop 2 the whole calendar slides with the month Japan strikes', () => {
    const loose = pacificWar(3, 28);
    for (const nm of ['Rabaul', 'Singapore', 'Darwin']) expect(lineShut(loose, 'JAP', loose.byName[nm]), nm).toBe(false);
    const slides = new Set<number>();
    for (const seed of [3, 5, 8, 11, 17, 21, 30]) {
      const g = pacificWar(2, 28, null, seed);
      const d = bookDate(g, 'JAP', 'south')! - 28;
      slides.add(d);
      expect(Math.abs(d)).toBeLessThanOrEqual(1);
      const rab = g.byName['Rabaul'], gc = g.byName['Guadalcanal'];
      g.turn = 28 + d;
      expect(lineShut(g, 'JAP', rab), `seed ${seed}`).toBe(true);
      expect(lineShut(g, 'JAP', g.byName['Kota Bharu']), `seed ${seed}`).toBe(false);
      expect(names(hopShores(g, 'JAP')!.open), `seed ${seed}`).not.toContain('Rabaul');
      g.turn = 29 + d;
      expect(lineShut(g, 'JAP', rab), `seed ${seed}`).toBe(false);
      expect(names(hopShores(g, 'JAP')!.open), `seed ${seed}`).toContain('Rabaul');
      // The road back slides with it.
      gc.ctrl = 'JAP';
      g.turn = 35 + d;
      expect(lineShut(g, 'USA', gc), `seed ${seed}`).toBe(true);
      expect(hopShores(g, 'USA')!.open, `seed ${seed}`).toEqual([]);
      g.turn = 36 + d;
      expect(lineShut(g, 'USA', gc), `seed ${seed}`).toBe(false);
      expect(names(hopShores(g, 'USA')!.open), `seed ${seed}`).toEqual(['Guadalcanal']);
    }
    expect(slides.size).toBeGreaterThan(1);
  });
});

describe('the strike south\'s other rows (rulings 200 and 234)', () => {
  it('names ten crossings and a third quay: Hong Kong has left the landing rows for the overland road (rider b572 to ruling 234)', () => {
    const op = SOUTH();
    expect(op.landings!.length).toBe(10);
    expect(op.landings!.some((l) => l.to.includes('Hong Kong') || l.corps === '38th Division (Canton)')).toBe(false);
    // (The 38th Division and the 23rd Army's guns stand at Tungkun on the eve, by the book's stations: far-stations.test.ts.)
    expect(op.quays).toEqual(['Hainan', 'Cam Ranh', 'Canton']);
    expect(op.frees).toEqual(['25th Army (Imperial Guards Division)', 'Naval landing forces (Sasebo 1st and 2nd, Yokosuka 1st)']);
    expect(op.freesAfter).toBe(2);
  });

  it('takes its reserves off hold the month it goes, and its own landing forces two months after', () => {
    const g = war(1, 27);
    expect(freed(g, 'JAP')).toEqual([]);
    g.flags['st:done:JAP:south'] = 28;
    expect(freed(g, 'JAP')).toEqual([]);
    g.turn = 28;
    expect(freed(g, 'JAP')).toEqual(SOUTH().frees);
    g.turn = 29;
    expect(freed(g, 'JAP')).toEqual([]);
    g.turn = 30;
    const later = freed(g, 'JAP');
    expect(later.length).toBe(10);
    expect(later).toContain('South Seas Detachment');
    expect(later).not.toContain('38th Division (Canton)');
    g.turn = 31;
    expect(freed(g, 'JAP')).toEqual([]);
    // Nobody else's great choices free anybody.
    for (const nat of ['GER', 'ITA', 'SOV', 'USA', 'UK']) expect(freed(g, nat), nat).toEqual([]);
  });
});

describe('the fleets cover the staff\'s own landings (ruling 200)', () => {
  it('a closed sea on the way to a named shore is opened by the patrol fleets together, and no fleet is sent where all of them would not do', () => {
    const g = pacificWar(1, 29);
    const jolo = g.byName['Jolo'], palau = g.byName['Palau'];
    const path = seaPath(g, 'JAP', palau, jolo, { max: NAVY.landing.range, surprise: true })!;
    expect(path.length).toBeGreaterThan(0);
    const before = new Map(g.fleets.filter((f) => f.nat === 'JAP').map((f) => [f.id, f.zone]));
    // An American squadron stands in Leyte Gulf, on the way, and closes it to Japan's troopships. (The coasts alone
    // close no far sea: the air over one is counted from the groups in reach, rulings 184 and 187.)
    const foe = g.fleets.find((f) => f.nat === 'USA' && f.name === 'Asiatic Fleet')!;
    expect(path.map((z) => g.seas[z].key)).toContain('leyte');
    foe.zone = g.seas.findIndex((z) => z.key === 'leyte');
    foe.stance = 'patrol';
    foe.ships = { ...foe.ships, ca: foe.ships.ca + 4 };
    // With no shore named, nobody sails for those seas on the landings' account.
    const shutBefore = path.filter((z) => closedTo(g, z, 'JAP'));
    expect(shutBefore).toEqual([foe.zone]);
    g.flags['st:hop:JAP:0'] = jolo.id + 1;
    g.flags['st:hopQ:JAP:0'] = palau.id + 1;
    navalAI(g, 'JAP');
    const came = (z: number): number => g.fleets.filter((f) => f.nat === 'JAP' && f.zone === z && f.stance !== 'port' && before.get(f.id) !== z).length;
    // Wherever fleets came to a sea that was closed, it is closed no longer: they came in strength or not at all.
    for (const z of shutBefore) if (came(z)) expect(closedTo(g, z, 'JAP'), g.seas[z].key).toBe(false);
    expect(shutBefore.some((z) => came(z) > 0)).toBe(true);
  });
});

/**
 * A made-up December 1941 (as in far-pacific-south.test.ts): the Netherlands and France struck down by hand in their
 * months, the opening read month by month from June 1941, America's fleet put at Pearl Harbor, the China front's
 * dated choices counted as taken; then Japan's own month is played, and the whole of January 1942 after it.
 */
const december = (stop: number): Game => {
  const g = war(stop, 9, null, 17);
  declareWar(g, 'GER', 'NED');
  capitulate(g, 'NED', 'GER');
  g.turn = 10;
  capitulate(g, 'FRA', 'GER');
  for (const t of [22, 23, 24, 25, 27]) { g.turn = t; bookMonth(g); monthlyDiplomacy(g); if (t === 24 || t === 27) startPhase(g, 'JAP'); }
  for (const op of ops('JAP')) if (op.id !== 'south') g.flags[`st:done:JAP:${op.id}`] = 1;
  const f = g.fleets.find((x) => x.nat === 'USA' && x.name === 'Pacific Fleet')!, sea = g.seas.findIndex((z) => z.key === 'hawaii');
  f.base = g.byName['Pearl Harbor'].id; f.port = 'Pearl Harbor'; f.home = sea; f.zone = sea; f.station = sea; f.stance = 'port';
  g.turn = 28;
  bookMonth(g);
  monthlyDiplomacy(g);
  return g;
};

describe('the staff\'s own landings, played (ruling 200, finding 2)', () => {
  it('December 1941 and January 1942 by the book: only forces in the Pacific\'s harbours sail, only for shores whose month has come, within the lift, and nobody ashore is thrown back', () => {
    const g = december(1);
    type Hop = Parameters<NonNullable<typeof hopWatch.tell>>[0];
    const seen: (Hop & { turn: number; fromName: string; toName: string })[] = [];
    hopWatch.tell = (e) => { seen.push({ ...e, turn: g.turn, fromName: e.from.name, toName: e.to.name }); };
    try {
      g.phase = g.order.indexOf('JAP');
      runTurnAI(g);
      expect(g.turn).toBe(29);
      expect(g.flags['st:done:JAP:south']).toBe(28);
      const ashore = SOUTH().landings!.filter((l) => l.to.includes(at(g, l.corps) ?? '')).map((l) => [l.corps, at(g, l.corps)!] as [string, string]);
      expect(ashore.length).toBeGreaterThanOrEqual(5);
      // The month of the strike: the plan's own landings take the lift, and the staff names its shores for the month after.
      for (const e of seen) expect(e.nat, `${e.kind} ${e.toName}`).toBe('JAP');
      runTurnAI(g);
      expect(g.turn).toBe(30);
    } finally { hopWatch.tell = null; }
    const month = new Map(HOPS().axis), road = new Map(HOPS().allies);
    expect(seen.length).toBeGreaterThan(0);
    for (const e of seen) {
      const what = `${e.nat} ${e.kind} ${e.fromName} for ${e.toName} in ${e.turn}`;
      // Nobody outside the Pacific sends a corps: no German sails for Guam.
      expect(['GER', 'ITA', 'SOV'], what).not.toContain(e.nat);
      expect(pacificGround(g, e.from), what).toBe(true);
      expect(pacificGround(g, e.to), what).toBe(true);
      const list = e.nat === 'JAP' || e.nat === 'THA' ? month : road;
      if (e.kind === 'land') expect(list.get(e.toName) ?? 999, what).toBeLessThanOrEqual(e.turn);
      else expect(list.get(e.toName) ?? 999, what).toBeLessThanOrEqual(e.turn + 1);
    }
    // The Allied staffs have no shore open in January 1942: none of them lands anywhere.
    expect(seen.filter((e) => e.kind === 'land' && e.nat !== 'JAP')).toEqual([]);
    // Within the lift: no more landings in a month than Japan's six.
    for (const t of [28, 29]) expect(seen.filter((e) => e.kind === 'land' && e.nat === 'JAP' && e.turn === t).length).toBeLessThanOrEqual(6);
    // The floor: whoever was ashore when December closed is still on his beach or beyond it, not in the sea.
    for (const l of SOUTH().landings!) {
      const c = corps(g, l.corps);
      if (!c) continue;
      const r = g.regions[c.reg];
      if (l.to.includes(r.name)) expect(c.str, l.corps).toBeGreaterThan(0);
    }
    expect(onBook(g, 'pacific')).toBe(true);
  }, 120000);
});
