// The far chart's town guard, land by land (the design director's rulings of 03:44Z to 03:50Z, 04:51Z and 05:54Z
// of 21 November 2026 on the historian's far-chart-town-guards.md, and of 06:03Z on its globe table;
// data/campaign/far-guards.json, read by guardOf).
//
// The old chart's guard is 1, 2 and 3 divisions' worth for a town, a city and a capital, under a cap by the army's
// size, and a tenth of that on colonial ground. Beyond the old chart that stood a division of police in every town
// of Japan's and made a Chinese port's guard most of its defence: in April 1941 the plan read Ningpo, Wenchow and
// Foochow under its mark to land with the guard and four to ten times over it without. The table gives each land
// the record's figure for a town, a city and a capital, at its owner's own scale, and by month where the record's
// figure changed (Australia's Volunteer Defence Corps, Hawaii's and Alaska's Territorial Guards, the Soviet works
// cities, Kuibyshev as the reserve capital, the Volga's destroyer battalions). Behind the far switch.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { armySize, createGame, removeCorps } from '../src/campaign/game';
import { farGuard, farGuardRow } from '../src/campaign/economy';
import { SIEGE, guardOf } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { townSize } from '../src/campaign/towns';
import { shoreOf } from '../src/campaign/world';
import type { Game, Region } from '../src/campaign/types';
import guards from '../data/campaign/far-guards.json';

type Fig = number | [number, number][];
type Land = { name: string; owner: string; town: Fig; city: Fig; capital: Fig; named?: Record<string, Fig>; places?: string[] };
const LANDS = (guards as any).siege.farGuard as Land[];
const war = (): Game => startOpen(createGame(5, null));
const r3 = (x: number): number => Math.round(x * 1000) / 1000;
/** The old reckoning: the class's figure under the cap by the army's size, a tenth on colonial ground, none at a post. */
const old = (g: Game, r: Region): number => { const n = g.nations[r.owner]; const full = Math.min(SIEGE.guard[townSize(g, r)], SIEGE.guardShare * Math.max(n.armyPeak, armySize(g, n.id))) * farGuard(r); return full > 0.02 ? full : 0; };

/** Every town of the far side that stands under no row, by its owner: the old reckoning is theirs until the historian's tables reach them. */
const NONE: Record<string, string[]> = {
  USA: ['Pearl Harbor', 'Midway', 'Wake', 'Samoa'],
  UK: ['Tarawa', 'Funafuti', 'Guadalcanal', 'New Georgia', 'Espiritu Santo', 'Fiji'],
  JAP: [
    'Kwajalein', 'Eniwetok', 'Eastern Marshalls', 'Truk', 'Saipan', 'Palau', 'Ulithi', 'Iwo Jima', 'Chichi Jima',
    'Yap', 'Ponape', 'Marcus Island', 'Karafuto', 'Dairen',
  ],
  AUS: ['Nauru'],
  FRA: ['New Caledonia'],
  TIB: ['Lhasa'],
  POR: ['Dili'],
  NZL: ['Auckland', 'Wellington', 'Christchurch', 'Dunedin', 'Napier', 'Nelson', 'Invercargill', 'Apia'],
};

describe("the far chart's town guard is the record's, land by land", () => {
  it('the table: forty-nine lands, each place a province of its owner on the far side and in one land only', () => {
    expect(SIEGE.farGuard).toEqual(LANDS);
    // (The first chart's seventeen, then the globe table's thirty-two. A figure with months is a list of month and figure.)
    expect(LANDS.map((l) => [l.name, l.owner, l.town, l.city, l.capital, l.places?.length ?? 'all'])).toEqual([
      ['China', 'CHN', 0.02, 0.055, 0.1, 'all'],
      ['Manchukuo', 'MAN', 0.01, 0.03, 0.05, 'all'],
      ['Mongolia', 'MGL', 0.005, 0.01, 0.05, 'all'],
      ['Thailand', 'THA', 0.02, 0.05, 0.15, 'all'],
      ["Japan's home islands", 'JAP', 0.03, 0.15, 0.3, 25],
      ['Korea', 'JAP', 0.02, 0.07, 0.15, 6],
      ['Formosa', 'JAP', 0.02, 0.07, 0.15, 2],
      ['the Dutch East Indies', 'NED', 0.01, 0.04, 0.1, 31],
      ['British Malaya and Singapore', 'UK', 0.01, 0.04, 0.1, 7],
      ['British Borneo', 'UK', 0.005, 0.015, 0.025, 4],
      ['Burma', 'UK', 0.01, 0.04, 0.1, 11],
      ['Hong Kong', 'UK', 0.1, 0.1, 0.1, 1],
      ['the American Philippines', 'USA', 0.01, 0.025, 0.1, 14],
      ['Guam', 'USA', 0.01, 0.01, 0.01, 1],
      ['French Indochina', 'FRA', 0.02, 0.05, 0.1, 11],
      ["Australia's New Guinea (Papua and the mandate)", 'AUS', 0.002, 0.01, 0.02, 11],
      ['the Soviet Far East (Chita and east)', 'SOV', 0.03, 0.1, 0.2, 25],
      ["British India (with the princely states' seats and the frontiers)", 'UK', 0.015, 0.06, 0.15, 49],
      ['Ceylon, the Andamans and the Maldives', 'UK', 0.01, 0.05, 0.05, 4],
      ['Aden and the Hadhramaut', 'UK', 0.02, 0.1, 0.1, 2],
      ['the Gulf', 'UK', 0.005, 0.02, 0.02, 6],
      ['Egypt beyond the old chart', 'UK', 0.015, 0.015, 0.015, 8],
      ['the Sudan', 'UK', 0.005, 0.03, 0.03, 12],
      ['British East Africa', 'UK', 0.005, 0.05, 0.05, 19],
      ['British central and southern Africa', 'UK', 0.005, 0.05, 0.05, 14],
      ['British West Africa', 'UK', 0.0075, 0.05, 0.05, 12],
      ['Mauritius and the Seychelles', 'UK', 0.005, 0.02, 0.02, 2],
      ['Portuguese India (Goa)', 'POR', 0.03, 0.03, 0.03, 1],
      ['Portuguese Africa and the islands', 'POR', 0.005, 0.03, 0.03, 18],
      ["Australia's home ground (the state police, and the Volunteer Defence Corps from month 11 to about 60)", 'AUS', [[1, 0.0075], [11, 0.025], [60, 0.0075]], [[1, 0.03], [11, 0.1], [60, 0.03]], [[1, 0.01], [11, 0.02], [60, 0.01]], 15],
      ["Australia's north", 'AUS', 0.005, 0.005, 0.005, 8],
      ['Hawaii (the police, and the Territorial Guard from month 28)', 'USA', [[1, 0.005], [28, 0.03]], [[1, 0.03], [28, 0.1]], [[1, 0.03], [28, 0.1]], 4],
      ['Alaska (the marshals, and the Territorial Guard from month 34)', 'USA', [[1, 0.005], [34, 0.01]], [[1, 0.005], [34, 0.01]], 0.01, 15],
      ["the Soviet Union west of Chita (the record's figures at 0.6, the scale of the old chart's Soviet rows)", 'SOV', 0.012, 0.042, 0.06, 73],
      ['Iran', 'IRN', 0.01, 0.03, 0.1, 'all'],
      ["Iraq's Basra", 'IRQ', 0.02, 0.02, 0.02, 1],
      ['Saudi Arabia', 'SAU', 0.005, 0.005, 0.02, 'all'],
      ['Yemen', 'YEM', 0.005, 0.005, 0.02, 'all'],
      ['Afghanistan', 'AFG', 0.005, 0.005, 0.03, 'all'],
      ['Nepal', 'NEP', 0.005, 0.005, 0.02, 'all'],
      ["French West Africa (with the Sahara's posts)", 'FRA', 0.005, 0.05, 0.05, 30],
      ['French Equatorial Africa and the Cameroons', 'FRA', 0.0075, 0.02, 0.02, 16],
      ['Madagascar, Reunion and Djibouti', 'FRA', 0.005, 0.03, 0.03, 11],
      ['Italian East Africa', 'ITA', 0.01, 0.1, 0.1, 19],
      ["Italian Libya's south (the Fezzan and Kufra)", 'ITA', 0.005, 0.005, 0.005, 5],
      ['the Belgian Congo and Ruanda-Urundi', 'BEL', 0.01, 0.03, 0.05, 12],
      ["Spain's Sahara and Rio Muni", 'ESP', 0.005, 0.01, 0.01, 3],
      ['South Africa and South-West Africa', 'SAF', 0.01, 0.05, 0.07, 'all'],
      ['Liberia', 'LBR', 0.002, 0.002, 0.005, 'all'],
    ]);
    const g = war();
    const places = LANDS.flatMap((l) => l.places ?? []);
    expect(new Set(places).size).toBe(places.length);
    for (const l of LANDS) {
      for (const nm of l.places ?? []) { const r = g.byName[nm]; expect(r, nm).toBeTruthy(); expect([r.owner, shoreOf(r)], nm).toEqual([l.owner, 2]); expect(farGuardRow(g, r)?.name, nm).toBe(l.name); }
      // A place the row names stands in the row's own land.
      for (const nm of Object.keys(l.named ?? {})) { const r = g.byName[nm]; expect(r, nm).toBeTruthy(); expect(farGuardRow(g, r)?.name, nm).toBe(l.name); }
    }
    // A land that names no places is all of its owner's ground on the far side, and only one row may be so.
    const whole = LANDS.filter((l) => !l.places).map((l) => l.owner);
    expect(new Set(whole).size).toBe(whole.length);
    for (const r of g.regions) if (whole.includes(r.owner) && shoreOf(r) === 2) expect(farGuardRow(g, r)?.owner, r.name).toBe(r.owner);
  });

  it("a town stands at its class's figure, a named place at its own, a village at none; 0.02 or under is none", () => {
    const g = war();
    const at = (nm: string): [string, string, number] => [nm, townSize(g, g.byName[nm]), r3(guardOf(g, g.byName[nm]))];
    expect(['Chungking', 'Ningpo', 'Wenchow', 'Foochow', 'Changsha'].map(at)).toEqual([
      ['Chungking', 'capital', 0.1], ['Ningpo', 'town', 0], ['Wenchow', 'town', 0], ['Foochow', 'town', 0], ['Changsha', 'town', 0],
    ]);
    expect(['Tokyo', 'Osaka', 'Sapporo', 'Nagano', 'Okinawa', 'Etorofu', 'Seoul', 'Pusan', 'Taihoku', 'Formosa'].map(at)).toEqual([
      ['Tokyo', 'capital', 0.3], ['Osaka', 'city', 0.15], ['Sapporo', 'town', 0.03], ['Nagano', 'village', 0], ['Okinawa', 'city', 0.15], ['Etorofu', 'town', 0.03],
      ['Seoul', 'city', 0.15], ['Pusan', 'town', 0], ['Taihoku', 'city', 0.15], ['Formosa', 'town', 0],
    ]);
    expect(['Hsinking', 'Mukden', 'Harbin', 'Kirin', 'Anshan', 'Ulan Bator', 'Bangkok', 'Singora'].map(at)).toEqual([
      ['Hsinking', 'capital', 0.05], ['Mukden', 'city', 0.045], ['Harbin', 'city', 0.03], ['Kirin', 'village', 0.03], ['Anshan', 'town', 0], ['Ulan Bator', 'capital', 0.05],
      ['Bangkok', 'capital', 0.15], ['Singora', 'town', 0],
    ]);
    expect(['Bandoeng', 'Soerabaja', 'Makassar', 'Semarang', 'Kuala Lumpur', 'Penang', 'Ipoh', 'Jesselton', 'Sandakan', 'Mandalay', 'Moulmein', 'Prome'].map(at)).toEqual([
      ['Bandoeng', 'town', 0.1], ['Soerabaja', 'city', 0.1], ['Makassar', 'town', 0.04], ['Semarang', 'town', 0], ['Kuala Lumpur', 'town', 0.1], ['Penang', 'town', 0.04], ['Ipoh', 'village', 0.04],
      ['Jesselton', 'town', 0.025], ['Sandakan', 'town', 0], ['Mandalay', 'town', 0.04], ['Moulmein', 'town', 0.04], ['Prome', 'town', 0],
    ]);
    expect(['Cebu', 'Panay', 'Davao', 'Aparri', 'Haiphong', 'Vinh', 'Qui Nhon', 'Lae', 'Madang'].map(at)).toEqual([
      ['Cebu', 'town', 0.025], ['Panay', 'town', 0.025], ['Davao', 'town', 0.025], ['Aparri', 'town', 0], ['Haiphong', 'town', 0.05], ['Vinh', 'town', 0.05], ['Qui Nhon', 'town', 0],
      ['Lae', 'town', 0], ['Madang', 'town', 0],
    ]);
    expect(['Vladivostok', 'Khabarovsk', 'Chita', 'Komsomolsk', 'Blagoveshchensk', 'Petropavlovsk', 'Iman'].map(at)).toEqual([
      ['Vladivostok', 'city', 0.2], ['Khabarovsk', 'city', 0.2], ['Chita', 'town', 0.1], ['Komsomolsk', 'town', 0.1], ['Blagoveshchensk', 'town', 0.1], ['Petropavlovsk', 'town', 0.03], ['Iman', 'village', 0],
    ]);
  });

  it("the globe table's lands: seats and presidency cities stand, the small towns of the colonies have none", () => {
    const g = war();
    const at = (nm: string): [string, string, number] => [nm, townSize(g, g.byName[nm]), r3(guardOf(g, g.byName[nm]))];
    // India: the presidency cities and the Raj's seat, the provincial seats, the princely states' seats, the frontiers.
    expect(['Calcutta', 'Delhi', 'Bombay', 'Madras', 'Lahore', 'Jamshedpur', 'Karachi', 'Lucknow', 'Hyderabad', 'Srinagar', 'Bangalore', 'Peshawar', 'Kohima', 'Imphal', 'Gilgit', 'Rawalpindi', 'Chittagong', 'Rajkot'].map(at)).toEqual([
      ['Calcutta', 'city', 0.2], ['Delhi', 'city', 0.15], ['Bombay', 'city', 0.15], ['Madras', 'city', 0.1], ['Lahore', 'city', 0.07], ['Jamshedpur', 'city', 0.03], ['Karachi', 'town', 0.04], ['Lucknow', 'village', 0.04],
      ['Hyderabad', 'town', 0.15], ['Srinagar', 'village', 0.2], ['Bangalore', 'town', 0.07], ['Peshawar', 'village', 0.15], ['Kohima', 'village', 0.05], ['Imphal', 'village', 0.05], ['Gilgit', 'village', 0.04],
      ['Rawalpindi', 'town', 0], ['Chittagong', 'town', 0], ['Rajkot', 'town', 0],
    ]);
    expect(['Aden', 'Mukalla', 'Bahrain', 'Kuwait', 'Trincomalee', 'Port Blair', 'Goa', 'Khartoum', 'Port Sudan', 'Asyut', 'Nairobi', 'Mombasa', 'Kampala', 'Salisbury', 'Bulawayo', 'Lagos', 'Accra', 'Freetown', 'Mauritius'].map(at)).toEqual([
      ['Aden', 'town', 0.1], ['Mukalla', 'town', 0], ['Bahrain', 'town', 0], ['Kuwait', 'town', 0], ['Trincomalee', 'town', 0], ['Port Blair', 'town', 0], ['Goa', 'town', 0.03], ['Khartoum', 'city', 0.03], ['Port Sudan', 'town', 0],
      ['Asyut', 'town', 0], ['Nairobi', 'town', 0.05], ['Mombasa', 'town', 0.03], ['Kampala', 'town', 0], ['Salisbury', 'town', 0.05], ['Bulawayo', 'town', 0.03], ['Lagos', 'city', 0.05], ['Accra', 'town', 0.03], ['Freetown', 'town', 0],
      ['Mauritius', 'town', 0],
    ]);
    expect(['Dakar', 'Saint-Louis', 'Brazzaville', 'Douala', 'Tananarive', 'Diego Suarez', 'Djibouti', 'Addis Ababa', 'Asmara', 'Mogadishu', 'Massawa', 'Kufra', 'Leopoldville', 'Elisabethville', 'Stanleyville', 'Luanda', 'Lourenco Marques', 'Bissau', 'Villa Cisneros'].map(at)).toEqual([
      ['Dakar', 'city', 0.05], ['Saint-Louis', 'town', 0], ['Brazzaville', 'town', 0], ['Douala', 'town', 0], ['Tananarive', 'town', 0.03], ['Diego Suarez', 'town', 0], ['Djibouti', 'town', 0], ['Addis Ababa', 'city', 0.1],
      ['Asmara', 'town', 0.05], ['Mogadishu', 'town', 0.03], ['Massawa', 'town', 0], ['Kufra', 'village', 0], ['Leopoldville', 'town', 0.05], ['Elisabethville', 'city', 0.03], ['Stanleyville', 'town', 0], ['Luanda', 'town', 0.03],
      ['Lourenco Marques', 'town', 0.03], ['Bissau', 'town', 0], ['Villa Cisneros', 'town', 0],
    ]);
    // The globe's seats: a ruler's guard of 0.02 or under is none, as everywhere.
    expect(['Tehran', 'Abadan', 'Isfahan', 'Basra', 'Riyadh', 'Jeddah', 'Sanaa', 'Kabul', 'Kathmandu', 'Monrovia', 'Pretoria', 'Johannesburg', 'Cape Town', 'Durban', 'Kimberley', 'Windhoek'].map(at)).toEqual([
      ['Tehran', 'capital', 0.1], ['Abadan', 'city', 0.03], ['Isfahan', 'town', 0], ['Basra', 'town', 0], ['Riyadh', 'capital', 0], ['Jeddah', 'town', 0], ['Sanaa', 'capital', 0], ['Kabul', 'capital', 0.03], ['Kathmandu', 'capital', 0],
      ['Monrovia', 'capital', 0], ['Pretoria', 'capital', 0.07], ['Johannesburg', 'city', 0.07], ['Cape Town', 'city', 0.05], ['Durban', 'city', 0.05], ['Kimberley', 'town', 0], ['Windhoek', 'village', 0],
    ]);
    // The Soviet Union west of Chita, at 0.6 of the record's figures: a city 0.042, a republic's capital 0.06, a town none.
    expect(['Irkutsk', 'Krasnoyarsk', 'Tashkent', 'Alma-Ata', 'Frunze', 'Baku', 'Ulan-Ude', 'Karaganda', 'Norilsk'].map(at)).toEqual([
      ['Irkutsk', 'city', 0.042], ['Krasnoyarsk', 'city', 0.042], ['Tashkent', 'city', 0.06], ['Alma-Ata', 'city', 0.06], ['Frunze', 'town', 0.06], ['Baku', 'city', 0.06], ['Ulan-Ude', 'town', 0], ['Karaganda', 'town', 0], ['Norilsk', 'town', 0],
    ]);
  });

  it("a figure that changed by month stands at the record's for the month, as the airfield table's rows do", () => {
    const g = war();
    const by = (nm: string, months: number[]): number[] => months.map((m) => { g.turn = m; return r3(guardOf(g, g.byName[nm])); });
    // Australia: the state police to month 10, the Volunteer Defence Corps from 11 (July 1940) to about 60.
    expect(by('Sydney', [1, 10, 11, 28, 59, 60, 72])).toEqual([0.07, 0.07, 0.2, 0.2, 0.2, 0.07, 0.07]);
    expect(by('Perth', [1, 10, 11, 59, 60])).toEqual([0.03, 0.03, 0.1, 0.1, 0.03]);
    expect(by('Townsville', [1, 10, 11, 59, 60])).toEqual([0, 0, 0.025, 0.025, 0]);
    expect(by('Canberra', [1, 11, 60])).toEqual([0, 0, 0]);
    // Its north has a few police to a town throughout.
    expect(by('Darwin', [1, 11, 30, 60])).toEqual([0, 0, 0, 0]);
    // Hawaii: the police to month 27, the Territorial Guard from 28 (7 December 1941).
    expect(by('Honolulu', [1, 27, 28, 72])).toEqual([0.03, 0.03, 0.1, 0.1]);
    expect(by('Maui', [1, 27, 28, 72])).toEqual([0, 0, 0.03, 0.03]);
    // Alaska: the marshals, and the Territorial Guard from 34: under the floor before and after.
    expect(by('Juneau', [1, 33, 34, 72])).toEqual([0, 0, 0, 0]);
    expect(by('Anchorage', [1, 33, 34, 72])).toEqual([0, 0, 0, 0]);
    // The Soviet works cities from month 27, Kuibyshev while it was the reserve capital (26 to about 47), the Volga
    // and the Caspian's destroyer battalions from 35.
    expect(by('Sverdlovsk', [1, 26, 27, 72])).toEqual([0.042, 0.042, 0.06, 0.06]);
    expect(by('Kuibyshev', [1, 25, 26, 46, 47, 72])).toEqual([0.042, 0.042, 0.12, 0.12, 0.042, 0.042]);
    expect(by('Guryev', [1, 34, 35, 72])).toEqual([0, 0, 0.03, 0.03]);
    expect(by('Makhachkala', [1, 34, 35, 72])).toEqual([0, 0, 0.03, 0.03]);
    g.turn = 1;
  });

  it('the post rule stays first: a post has no guard, whatever its row gives it', () => {
    const g = war();
    for (const nm of ['Hong Kong', 'Manila', 'Singapore', 'Rangoon', 'Batavia', 'Hanoi', 'Saigon', 'Hue', 'Guam', 'Port Moresby', 'Rabaul', 'Kuching', 'Medan', 'Palembang', 'Colombo']) {
      const r = g.byName[nm];
      expect([farGuard(r), !!farGuardRow(g, r), guardOf(g, r)], nm).toEqual([0, true, 0]);
    }
  });

  it("the rest stands: none where the holder is not the owner, less what bombardment broke, and no cap by the army's size", () => {
    const g = war();
    const tokyo = g.byName.Tokyo, peiping = g.byName.Peiping, ulan = g.byName['Ulan Bator'];
    // Peiping is China's and Japan holds it: nobody's guard stands there.
    expect([peiping.owner, peiping.ctrl, townSize(g, peiping), guardOf(g, peiping)]).toEqual(['CHN', 'JAP', 'city', 0]);
    g.flags[`guard:${tokyo.id}`] = 50;
    expect(r3(guardOf(g, tokyo))).toBe(0.15);
    g.flags[`guard:${tokyo.id}`] = 95;
    expect(guardOf(g, tokyo)).toBe(0);
    // Mongolia with no army left: the old cap would read nothing, and the row's figure is the record's size already.
    for (const c of [...g.corps]) if (c.nat === 'MGL') removeCorps(g, c);
    g.nations.MGL.armyPeak = 0;
    expect([armySize(g, 'MGL'), guardOf(g, ulan)]).toEqual([0, 0.05]);
  });

  it('far ground under no row keeps the old reckoning, and every such town is named here', () => {
    const g = war();
    const none: Record<string, string[]> = {};
    for (const r of g.regions) if (shoreOf(r) === 2 && townSize(g, r) !== 'village' && !farGuardRow(g, r)) (none[r.owner] ??= []).push(r.name);
    expect(none).toEqual(NONE);
    for (const names of Object.values(NONE)) for (const nm of names) expect(guardOf(g, g.byName[nm]), nm).toBe(old(g, g.byName[nm]));
    // (Pearl Harbor, a city as the game reads it, has no figure in Hawaii's row: the historian gives Honolulu and the other islands.)
    expect(farGuardRow(g, g.byName['Pearl Harbor'])).toBeNull();
    // Karafuto and Dairen, Japan's own and under no row, stand as they did: a home town's whole guard and a colony's tenth.
    expect([guardOf(g, g.byName.Karafuto), r3(guardOf(g, g.byName.Dairen))]).toEqual([1, 0.2]);
  });

  it('a province of the old chart is never looked up, nor one of the Americas', () => {
    const g = war();
    for (const r of g.regions) if (shoreOf(r) !== 2) { expect(farGuardRow(g, r), r.name).toBeNull(); expect(guardOf(g, r), r.name).toBe(r.ctrl === r.owner && g.nations[r.owner]?.alive ? old(g, r) : 0); }
  });
});
