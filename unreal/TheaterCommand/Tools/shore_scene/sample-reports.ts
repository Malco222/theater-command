// The shelling scene's stand-in reports (sea-scene/shore-guns/plan.md R0), in the shape the sea scene asked the rules for
// on 26 November 05:12Z (naval-battles/shore-guns-r2/scene-report-ask-1126.md): who shelled (the fleet, its nation, whose
// shelling it is), the ships that fired (class, name, bore), the target (kind, province, chief place, the sea), the
// defences (fort level, whether it fired), the outcome fixed first (ships sunk or kept afloat, the share), the
// conditions (month, hour, sea, spotting, sky) and one seed. The rules on this line write none of `look` yet (the navy
// thread's, plan section 2, built on navy-shore-guns 690599783 at 21:48Z and not joined here); each stand-in carries it
// in the shape the navy built, but two, which carry the report alone, as today's rules write it, so the stager's own
// stand-ins are staged too. As the navy writes it today most give no bore (until the historian's bores are on the hull
// rows) and none an hour (the rules keep none); Normandy's, Shanghai's and Narvik's give bores and Normandy's an hour (the
// nominal morning) as a later look may, and Calais's hour stands in for a night report. Spotting and air are written only
// with shoreGuns on (Normandy, Tobruk, Tarawa, Narvik), and a gale only with it off (Brittany).
//
// They cover the plan's list: the eight coast kits (Channel chalk, Normandy dunes, Mediterranean harbour, North African
// port, coral island, volcanic island, Arctic fjord, China coast); troops in the open, dug in, and fortress troops; a
// town's defences; a city far inland; a village coast; seven cruisers with no battleship, one battleship alone, fifty
// destroyers, a fleet with carriers and submarines; no fort, a fort that sank no ship, his coast shelled; a night
// report and a gale month (neither gets a scene before R6).
//
// Places are the default chart's provinces where it has the coast. The chart has no Pacific atoll and no China coast (the
// far chart's, TC_FAR), and no chief place a village within 3 km of its waterfront: those three coasts are laid by form
// (shore-scene.ts coastByForm), and their staged.standIns say so.
//
// Staged and written to a folder by samples.ts (Tools/shore_scene/run.sh [out-dir]); tests/shore-scene.test.ts stages them too.
import { createGame } from '../../../../src/campaign/game';
import { startOpen } from '../../../../src/campaign/staff';
import { townName } from '../../../../src/campaign/towns';
import type { Game, ShoreReport, Ships } from '../../../../src/campaign/types';
import type { TicketLine } from '../../../../src/core/ticket';
import { coastByForm, coastOf, shoreFactsOf, type Chart, type ShoreFacts, type ShoreLook, type LookedReport } from '../../Rules/shore-scene';
import type { TownSize } from '../../../../src/campaign/towns';

/** turn: September 1939 is 1 (src/core/ticket.ts turnToDate). */
export const turnOf = (year: number, month1: number): number => (year - 1939) * 12 + month1 - 9 + 1;
const ships = (s: Partial<Ships>): Ships => ({ bb: 0, cv: 0, ca: 0, dd: 0, ss: 0, ...s });
const line = (label: string, value: string, effect = ''): TicketLine => ({ label, value, effect } as TicketLine);

/** A coast laid by form, where the chart has none: its shape, the way the sea lies, where it is, and its chief place. */
export interface FormCoast {
  form: 'straight' | 'bay' | 'headland' | 'harbour';
  /** The way the sea lies from the waterfront (degrees from north); 'chart' takes the named province's own. */
  bearing: number | 'chart';
  lat?: number;
  lon?: number;
  island?: boolean;
  terrain: string;
  town: { name: string; size: TownSize; port: boolean };
  sea?: { key: string; name: string };
}

export interface ShoreSample {
  file: string;
  /** What it stands in for (plan R0's list). */
  stands: string[];
  human: string;
  /** The province on the default chart (its name) the report is of; for a coast by form, the place's own name. */
  region: string;
  /** The sea (its key), where the province has more than one. */
  sea?: string;
  form?: FormCoast;
  report: Omit<ShoreReport, 'id' | 'region' | 'place' | 'zone'> & { look?: ShoreLook };
  /** Why it gets no scene before R6. */
  skip?: 'night' | 'gale';
}

const NAMES_MORE: Record<string, { name: string; adj: string }> = {
  JAP: { name: 'Japan', adj: 'Japanese' }, CHN: { name: 'China', adj: 'Chinese' }, USA: { name: 'United States', adj: 'American' },
};

export const SAMPLES: ShoreSample[] = [
  {
    // R1.6's nominal scene: 2 bb (406), 2 ca (203), 4 dd (127) on five divisions, some dug in, behind a coast with a
    // village; fort level 2; one destroyer sunk, one cruiser kept afloat; morning; spotting.
    file: 'shore-normandy-dunes-village.json',
    stands: ['Normandy dunes', 'a village coast', 'troops dug in', 'the nominal scene (R1.6)'],
    human: 'USA', region: 'Normandy',
    form: { form: 'straight', bearing: 'chart', terrain: 'plains', town: { name: 'Vierville', size: 'village', port: false } },
    report: {
      turn: turnOf(1944, 6), nat: 'USA', fleet: 'Atlantic Fleet', admiral: null, defNat: 'GER', what: 'troops',
      ships: ships({ bb: 2, ca: 2, dd: 4 }), sunk: ships({ dd: 1 }), guns: 27.6, cut: 0.17, divs: 5, output: 0, nation: 0,
      lines: [line('Guns', 'Two battleships and two cruisers'), line('Troops', '2 corps, 5 divisions'), line('Shelter', 'Troops dug in'), line('Shore batteries', 'Fortress level 2')],
      result: "The shelling cut the organisation of the 2 corps at Normandy by 17%, less where they are dug in or under concrete. They fight the weaker for it until they have rested. The shore batteries sank a destroyer. A cruiser of the Atlantic Fleet was badly hit, but the repair parties kept the ship afloat.",
      look: {
        fleetId: 31, by: 'hand', split: ships({}),
        fired: [{ cls: 'bb', name: 'Washington', mm: 406, guns: 9 }, { cls: 'bb', name: 'North Carolina', mm: 406, guns: 9 }, { cls: 'ca', mm: 203, guns: 9 }, { cls: 'ca', mm: 203, guns: 9 },
          { cls: 'dd', mm: 127, guns: 5 }, { cls: 'dd', mm: 127, guns: 5 }, { cls: 'dd', mm: 127, guns: 5 }, { cls: 'dd', mm: 127, guns: 5 }],
        fort: { level: 2, fired: true }, outcome: { sunk: { dd: 1 }, kept: { ca: 1 }, share: 0.17 },
        conditions: { hour: 8, sea: 'normal', spot: 1, air: 'sky', sky: 'held' }, seed: 0x5eaf00d,
      },
    },
  },
  {
    file: 'shore-med-harbour-cruisers.json',
    stands: ['Mediterranean harbour', "a town's defences", 'seven cruisers, no battleship', 'a fort that sank no ship'],
    human: 'ITA', region: 'Malta',
    report: {
      turn: turnOf(1940, 8), nat: 'ITA', fleet: '2nd Fleet', admiral: null, defNat: 'UK', what: 'town', works: 9,
      ships: ships({ ca: 7, dd: 0 }), sunk: ships({}), guns: 21, cut: 0.02, divs: 1, output: 0, nation: 0,
      lines: [line('Guns', 'Seven cruisers'), line('The town', 'Valletta, a city'), line('Shore batteries', 'Fortress level 2')],
      result: 'The shells broke down 9% of the works of Valletta: troops inside now fight 41% stronger, not 50%. The garrison lost 2% of its organisation.',
      look: {
        fleetId: 12, by: 'staff', split: ships({}),
        fired: Array.from({ length: 7 }, () => ({ cls: 'ca' as const })),
        fort: { level: 2, fired: true }, outcome: { sunk: {}, kept: {}, share: 0.3 },
        conditions: { sea: 'normal', sky: null }, seed: 0x1b2c3d4,
      },
    },
  },
  {
    file: 'shore-north-africa-battleship.json',
    stands: ['North African port', "a town's defences", 'one battleship alone (the navy ruling 2)', "a battleship struck, never sunk (ruling 3: the look's outcome.struck; kept is cruisers and destroyers only)"],
    human: 'UK', region: 'Tobruk',
    report: {
      turn: turnOf(1941, 1), nat: 'UK', fleet: 'Mediterranean Fleet', admiral: null, defNat: 'ITA', what: 'town', works: 14,
      ships: ships({ bb: 1 }), sunk: ships({}), guns: 10, cut: 0.03, divs: 2, output: 0, nation: 0,
      lines: [line('Guns', 'A battleship'), line('The town', 'Tobruk, a town'), line('Shore batteries', 'Fortress level 2')],
      result: 'The shells broke down 14% of the works of Tobruk: troops inside now fight 26% stronger, not 40%. The garrison lost 3% of its organisation.',
      look: {
        fleetId: 4, by: 'hand', split: ships({}), fired: [{ cls: 'bb', name: 'Warspite' }],
        fort: { level: 2, fired: true }, outcome: { sunk: {}, kept: {}, share: 0.35, struck: 0.04 },
        conditions: { sea: 'normal', spot: 1, air: 'sky', sky: 'won' }, seed: 0xa11ce,
      },
    },
  },
  {
    file: 'shore-coral-island-carriers.json',
    stands: ['coral island', 'fortress troops', 'a fleet with carriers and submarines (neither drawn)', 'spotting by the carriers\' aircraft'],
    human: 'USA', region: 'Tarawa',
    form: { form: 'bay', bearing: 250, lat: 1.35, lon: 172.95, island: true, terrain: 'plains', town: { name: 'Betio', size: 'village', port: false }, sea: { key: 'gilberts', name: 'Central Pacific' } },
    report: {
      turn: turnOf(1943, 11), nat: 'USA', fleet: 'Pacific Fleet', admiral: null, defNat: 'JAP', what: 'troops',
      ships: ships({ bb: 2, cv: 2, ca: 3, dd: 8, ss: 2 }), sunk: ships({ dd: 1 }), guns: 32.2, cut: 0.08, divs: 1, output: 0, nation: 0,
      lines: [line('Guns', 'Two battleships and three cruisers'), line('Troops', '1 corps, 1 division'), line('Shelter', 'Fortress troops under concrete'), line('Shore batteries', 'Fortress level 1')],
      result: 'The shelling cut the organisation of the 4th Special Base Force at Tarawa by 8%, less where they are dug in or under concrete. They fight the weaker for it until they have rested. The shore batteries sank a destroyer.',
      look: {
        fleetId: 40, by: 'hand', split: ships({}),
        fired: [{ cls: 'bb', name: 'Maryland' }, { cls: 'bb', name: 'Colorado' }, { cls: 'ca' }, { cls: 'ca' }, { cls: 'ca' },
          ...Array.from({ length: 8 }, () => ({ cls: 'dd' as const }))],
        fort: { level: 1, fired: true }, outcome: { sunk: { dd: 1 }, kept: {}, share: 0.08 },
        conditions: { sea: 'normal', spot: 1, air: 'carrier', sky: 'held' }, seed: 0x7a7a3a,
      },
    },
  },
  {
    // The report alone, as today's rules write it (no look): the stager's own stand-ins.
    file: 'shore-volcanic-island-open.json',
    stands: ['volcanic island', 'troops in the open', 'no fort', 'the report alone (no look)'],
    human: 'GER', region: 'Azores',
    report: {
      turn: turnOf(1941, 3), nat: 'GER', fleet: 'Battle Fleet', admiral: null, defNat: 'UK', what: 'troops',
      ships: ships({ bb: 2, dd: 4 }), sunk: ships({}), guns: 21.6, cut: 0.07, divs: 2, output: 0, nation: 0,
      lines: [line('Guns', 'Two battleships'), line('Troops', '1 corps, 2 divisions')],
      result: 'The shelling cut the organisation of the 1st Corps at Azores by 7%. They fight the weaker for it until they have rested.',
    },
  },
  {
    file: 'shore-arctic-fjord-destroyers.json',
    stands: ['Arctic fjord', 'fifty destroyers', 'troops dug in', 'no fort', "a split fleet's name with a digit ('2nd Escort Fleet')"],
    human: 'UK', region: 'Narvik',
    report: {
      turn: turnOf(1940, 5), nat: 'UK', fleet: '2nd Escort Fleet', admiral: null, defNat: 'GER', what: 'troops',
      ships: ships({ dd: 50 }), sunk: ships({}), guns: 20, cut: 0.12, divs: 3, output: 0, nation: 0,
      lines: [line('Guns', 'Fifty destroyers'), line('Troops', '1 corps, 3 divisions'), line('Shelter', 'Troops dug in')],
      result: 'The shelling cut the organisation of the 3rd Mountain Corps at Narvik by 12%, less where they are dug in or under concrete. They fight the weaker for it until they have rested.',
      look: {
        fleetId: 77, by: 'ai', split: ships({}), fired: Array.from({ length: 50 }, () => ({ cls: 'dd' as const, mm: 120, guns: 4 })),
        fort: { level: 0, fired: false }, outcome: { sunk: {}, kept: {}, share: 0.12 },
        conditions: { sea: 'normal', spot: 0.6, air: null, sky: 'closed' }, seed: 0xf10d,
      },
    },
  },
  {
    file: 'shore-china-coast-open.json',
    stands: ['China coast', 'troops in the open', 'no fort'],
    human: 'JAP', region: 'Shanghai',
    form: { form: 'bay', bearing: 95, lat: 31.05, lon: 121.95, terrain: 'plains', town: { name: 'Wusong', size: 'town', port: true }, sea: { key: 'ecs', name: 'East China Sea' } },
    report: {
      turn: turnOf(1939, 10), nat: 'JAP', fleet: '3rd Fleet', admiral: null, defNat: 'CHN', what: 'troops',
      ships: ships({ bb: 2, ca: 4, dd: 8 }), sunk: ships({}), guns: 35.2, cut: 0.24, divs: 4, output: 0, nation: 0,
      lines: [line('Guns', 'Two battleships and four cruisers'), line('Troops', '2 corps, 4 divisions')],
      result: 'The shelling cut the organisation of the 2 corps at Shanghai by 24%. They fight the weaker for it until they have rested.',
      look: {
        fleetId: 9, by: 'hand', split: ships({}),
        fired: [{ cls: 'bb', name: 'Fuso', mm: 356, guns: 12 }, { cls: 'bb', name: 'Yamashiro', mm: 356, guns: 12 }, ...Array.from({ length: 4 }, () => ({ cls: 'ca' as const, mm: 203, guns: 10 })),
          ...Array.from({ length: 8 }, () => ({ cls: 'dd' as const, mm: 127, guns: 6 }))],
        fort: { level: 0, fired: false }, outcome: { sunk: {}, kept: {}, share: 0.24 },
        conditions: { sea: 'normal', sky: null }, seed: 0xc0a57,
      },
    },
  },
  {
    file: 'shore-channel-chalk-his-coast.json',
    stands: ['Channel chalk', 'his coast shelled', 'troops in the open', 'no fort'],
    human: 'UK', region: 'Kent',
    report: {
      turn: turnOf(1940, 9), nat: 'GER', fleet: 'Battle Fleet', admiral: null, defNat: 'UK', what: 'troops',
      ships: ships({ bb: 2, dd: 6 }), sunk: ships({}), guns: 22.4, cut: 0.11, divs: 3, output: 0, nation: 0,
      lines: [line('Guns', 'Two battleships'), line('Troops', '2 corps, 3 divisions')],
      result: 'The shelling cut the organisation of the 2 corps at Kent by 11%. They fight the weaker for it until they have rested.',
      look: {
        fleetId: 2, by: 'ai', split: ships({}), fired: [{ cls: 'bb', name: 'Scharnhorst' }, { cls: 'bb', name: 'Gneisenau' }, ...Array.from({ length: 6 }, () => ({ cls: 'dd' as const }))],
        fort: { level: 0, fired: false }, outcome: { sunk: {}, kept: {}, share: 0.11 },
        conditions: { sea: 'normal', sky: 'held' }, seed: 0xd0e5,
      },
    },
  },
  {
    // The report alone (no look), a city's works: Leeds lies far inland of the Yorkshire coast, so no skyline is drawn.
    file: 'shore-city-far-inland.json',
    stands: ['a city far inland', 'no fort', 'the report alone (no look)'],
    human: 'GER', region: 'Yorkshire',
    report: {
      turn: turnOf(1940, 10), nat: 'GER', fleet: 'Battle Fleet', admiral: null, defNat: 'UK', what: 'city',
      ships: ships({ bb: 2, ca: 2, dd: 4 }), sunk: ships({}), guns: 27.6, cut: 0, divs: 0, output: 0.3, nation: 0.021,
      lines: [line('Guns', 'Two battleships and two cruisers'), line('Industry', '2 factories')],
      result: "The shelling stopped 30% of Yorkshire's output for a month: 2.1% of British production.",
    },
  },
  {
    file: 'shore-night.json',
    stands: ['a night report (no scene before R6)'],
    human: 'UK', region: 'Calais', skip: 'night',
    report: {
      turn: turnOf(1941, 12), nat: 'UK', fleet: 'Dover Fleet', admiral: null, defNat: 'GER', what: 'troops',
      ships: ships({ ca: 7, dd: 2 }), sunk: ships({}), guns: 21.8, cut: 0.09, divs: 2, output: 0, nation: 0,
      lines: [line('Guns', 'Seven cruisers'), line('Troops', '1 corps, 2 divisions')],
      result: 'The shelling cut the organisation of the 1st Corps at Calais by 9%. They fight the weaker for it until they have rested.',
      look: {
        fleetId: 3, by: 'staff', split: ships({}), fired: Array.from({ length: 7 }, () => ({ cls: 'ca' as const })),
        fort: { level: 0, fired: false }, outcome: { sunk: {}, kept: {}, share: 0.09 },
        conditions: { hour: 23, sea: 'normal', sky: 'closed' }, seed: 0x4e16,
      },
    },
  },
  {
    file: 'shore-gale.json',
    stands: ['a gale month (no scene before R6)'],
    human: 'UK', region: 'Brittany', sea: 'biscay', skip: 'gale',
    report: {
      turn: turnOf(1942, 1), nat: 'UK', fleet: 'Home Fleet', admiral: null, defNat: 'GER', what: 'troops',
      ships: ships({ bb: 2, dd: 6 }), sunk: ships({}), guns: 22.4, cut: 0.1, divs: 2, output: 0, nation: 0,
      lines: [line('Guns', 'Two battleships'), line('Troops', '1 corps, 2 divisions')],
      result: 'The shelling cut the organisation of the 1st Corps at Brittany by 10%. They fight the weaker for it until they have rested.',
      look: {
        fleetId: 1, by: 'hand', split: ships({}), fired: [{ cls: 'bb', name: 'Nelson' }, { cls: 'bb', name: 'Rodney' }, ...Array.from({ length: 6 }, () => ({ cls: 'dd' as const }))],
        fort: { level: 0, fired: false }, outcome: { sunk: {}, kept: {}, share: 0.1 },
        conditions: { sea: 'gale', sky: null }, seed: 0x9a1e,
      },
    },
  },
];

/** The default chart, opened for a sample's player (look only: the samples read it and write nothing back). */
export function sampleGame(human: string, seed = 1941): Game {
  return startOpen(createGame(seed, human === 'JAP' || human === 'CHN' ? 'UK' : human));
}

/** A sample's report as the rules would keep it, with its id, province and sea from the chart. */
export function sampleReport(g: Game, s: ShoreSample, id: number): LookedReport {
  const r = g.regions.find((x) => x.name === s.region);
  const zone = r ? (s.sea ? g.seas.findIndex((z) => z.key === s.sea) : r.seas[0]) : -1;
  return { ...s.report, id, region: r ? r.id : -1, place: s.region, zone } as LookedReport;
}

/**
 * A sample's facts: the stager's own (shoreFactsOf) where the chart has the coast; else a coast by form, laid at the
 * named place (or at the province's own waterfront, its bearing and its place, with the chief place given), its
 * stand-ins said.
 */
export function sampleFacts(g: Game, s: ShoreSample, id = 1): ShoreFacts {
  const rep = sampleReport(g, s, id);
  const human = s.human;
  if (!s.form) return { ...shoreFactsOf(g, rep), human };
  const names: Record<string, { name: string; adj: string }> = { ...NAMES_MORE };
  for (const n of Object.values(g.nations)) names[n.id] = { name: n.name, adj: n.adj };
  let bearing = 0, lat = s.form.lat ?? 0, lon = s.form.lon ?? 0;
  let sea = s.form.sea ?? { key: 'open', name: 'open sea' };
  if (s.form.bearing === 'chart') {
    const c = coastOf(g as unknown as Chart, rep.region, rep.zone);
    bearing = c.bearing; lat = c.lat; lon = c.lon;
    sea = { key: g.seas[rep.zone].key, name: g.seas[rep.zone].name };
  } else bearing = s.form.bearing;
  const coast = coastByForm(s.form.form, bearing, lat, lon, !!s.form.island);
  const standIns = [
    s.form.bearing === 'chart'
      ? `the coast: laid by form at ${s.region}'s own waterfront (the chart's chief place, ${townName(g.regions[rep.region])}, lies far inland; the scene's village stands in for one by the shore)`
      : `the coast: laid by form at ${s.region} (not on the default chart)`,
  ];
  return { human, seed: g.seed, rep, names, sea, coast, town: s.form.town, terrain: s.form.terrain, hulls: [], gale: false, standIns };
}
