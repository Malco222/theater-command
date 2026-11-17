// The far side of the world, laid round today's chart (world.mjs) without moving anything on it: the Pacific first
// (Hawaii and Alaska, the islands of the two roads to Japan, Japan, China and their neighbours), then the rest of
// the globe. tools/build-geo-far.mjs grows a province round every seed here, as the map of Europe and today's
// chart are made, and writes src/generated/world-next.json. The provinces follow today's in number (576 up), in
// the order of the slices below and of each slice's list. Add new ones at the end of the last slice.
//
// [name, lat, lon, owner, terrain, victory points, factories, flags]   (flags as in regions.mjs, and 'islet', 'disc')
// Longitudes are written as an atlas writes them, east positive: the tool counts them eastward from today's west
// edge (141 W), so Hawaii and Alaska lie at the far east end of the chart, beside the America they belong to.
//
// The seed is the province's centre, and the rules measure every distance from it (air.ts, km). An island group is
// seeded on the airfield that matters, as the Pacific planner's table has it (ruling 181): the Ellice group at
// Nanumea, the eastern Marshalls at Maloelap, Formosa's south by Takao.
//
// islet: 1 marks an island too small to see or to click. It is drawn larger than life, a round shape centred on
// its true place (ISLET_SIZE below), in the chart's coast and in its own province, and the flag goes into the chart.
// disc: 1 lays the same round shape under a seed and no flag: the province is more than the shape.
//
// Owner, ground, victory points, factories and the flags port, col, cap, depot, oil and fort are the clerk's
// table's business (data/campaign/tables, one row to a province by name): the table is the source once a row
// exists, and what is written here stands until then. Who holds a province at each start year, its harbour,
// airfield and the rest are in the table for the rules.

export const PACIFIC_REGIONS = [
  // ---- Hawaii. Oahu is two provinces: the fleet's harbour with the army's fields behind it, and the city.
  ['Pearl Harbor', 21.47, -158.06, 'USA', 'hills', 2, 1, { port: 1, col: 1, depot: 1, fort: [2, 'JAP'] }],
  ['Honolulu', 21.4, -157.78, 'USA', 'urban', 2, 1, { port: 1, col: 1 }],
  ['Maui', 20.8, -156.33, 'USA', 'hills', 0, 0, { port: 1, col: 1 }],
  ['Hawaii', 19.6, -155.5, 'USA', 'mountain', 0, 0, { port: 1, col: 1 }],
  ['Kauai', 22.05, -159.52, 'USA', 'hills', 0, 0, { port: 1, col: 1 }],
  // ---- Alaska, west of the 141st meridian. (The panhandle lies on today's chart, inside Canada's provinces.)
  ['Anchorage', 61.4, -149.6, 'USA', 'forest', 1, 0, { port: 1, col: 1 }],
  ['Fairbanks', 64.8, -147.6, 'USA', 'forest', 0, 0, { col: 1 }],
  ['Copper River', 61.9, -144.4, 'USA', 'mountain', 0, 0, { port: 1, col: 1 }],
  ['Nome', 64.9, -164.0, 'USA', 'hills', 0, 0, { port: 1, col: 1 }],
  ['Kuskokwim', 61.2, -159.6, 'USA', 'marsh', 0, 0, { port: 1, col: 1 }],
  ['Yukon Flats', 66.2, -152.5, 'USA', 'forest', 0, 0, { col: 1 }],
  ['Arctic Slope', 69.6, -153.5, 'USA', 'marsh', 0, 0, { col: 1 }],
  ['Bristol Bay', 58.6, -157.2, 'USA', 'hills', 0, 0, { port: 1, col: 1 }],
  ['Kodiak', 57.5, -153.4, 'USA', 'hills', 0, 0, { port: 1, col: 1 }],
  // The panhandle's islands, which today's chart leaves to no one (its mainland shore is drawn with Canada's
  // provinces): the capital with Sitka's islands, and the south round Ketchikan.
  ['Juneau', 57.9, -134.4, 'USA', 'forest', 1, 0, { port: 1, col: 1 }],
  ['Ketchikan', 55.45, -131.5, 'USA', 'forest', 0, 0, { port: 1, col: 1 }],
  // The Aleutians, from the fleet's base in the east to the two islands Japan took in June 1942.
  ['Dutch Harbor', 53.6, -166.9, 'USA', 'mountain', 0, 0, { port: 1, col: 1 }],
  ['Adak', 51.88, -176.65, 'USA', 'hills', 0, 0, { port: 1, col: 1 }],
  ['Kiska', 51.97, 177.5, 'USA', 'hills', 0, 0, { port: 1, col: 1, islet: 1 }],
  ['Attu', 52.9, 173.2, 'USA', 'mountain', 0, 0, { port: 1, col: 1 }],
  // ---- The central road (the historian's table, ruling 173; the planner's seeds, ruling 181): Hawaii to the Ryukyus.
  ['Midway', 28.21, -177.37, 'USA', 'plains', 1, 0, { port: 1, col: 1, islet: 1 }],
  ['Wake', 19.28, 166.65, 'USA', 'plains', 1, 0, { port: 1, col: 1, islet: 1 }],
  ['Tarawa', 1.35, 172.93, 'UK', 'plains', 0, 0, { port: 1, col: 1, islet: 1 }],
  ['Funafuti', -5.67, 176.1, 'UK', 'plains', 0, 0, { port: 1, col: 1, islet: 1 }],
  ['Kwajalein', 8.72, 167.73, 'JAP', 'plains', 1, 0, { port: 1, col: 1, islet: 1 }],
  ['Eniwetok', 11.5, 162.33, 'JAP', 'plains', 0, 0, { port: 1, col: 1, islet: 1 }],
  ['Eastern Marshalls', 8.7, 171.23, 'JAP', 'plains', 0, 0, { port: 1, col: 1, islet: 1 }],
  ['Truk', 7.42, 151.78, 'JAP', 'hills', 1, 0, { port: 1, col: 1, islet: 1 }],
  ['Saipan', 15.19, 145.75, 'JAP', 'hills', 1, 0, { port: 1, col: 1, islet: 1 }],
  ['Guam', 13.45, 144.78, 'USA', 'hills', 1, 0, { port: 1, col: 1, islet: 1 }],
  ['Palau', 7.3, 134.5, 'JAP', 'hills', 1, 0, { port: 1, col: 1, islet: 1 }],
  ['Ulithi', 10.0, 139.7, 'JAP', 'plains', 0, 0, { port: 1, col: 1, islet: 1 }],
  ['Iwo Jima', 24.78, 141.32, 'JAP', 'hills', 1, 0, { port: 1, islet: 1 }],
  ['Chichi Jima', 27.07, 142.2, 'JAP', 'hills', 0, 0, { port: 1, islet: 1 }],
  ['Yap', 9.5, 138.1, 'JAP', 'hills', 0, 0, { port: 1, col: 1, islet: 1 }],
  ['Ponape', 6.9, 158.2, 'JAP', 'hills', 0, 0, { port: 1, col: 1, islet: 1 }],
  ['Nauru', -0.53, 166.93, 'AUS', 'plains', 0, 0, { port: 1, col: 1, islet: 1 }],
  ['Marcus Island', 24.29, 153.98, 'JAP', 'plains', 0, 0, { port: 1, islet: 1 }],
  // ---- The islands of the south-west road and of the lifeline to Australia. The large ones are the atlas's own
  // shapes; the small islands beside each go with it (Malaita and Santa Isabel with Guadalcanal, Vanua Levu with Fiji).
  ['Guadalcanal', -9.43, 160.05, 'UK', 'forest', 0, 0, { port: 1, col: 1 }],
  ['New Georgia', -8.3, 157.3, 'UK', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Bougainville', -6.2, 155.3, 'AUS', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Admiralty Islands', -2.05, 147.3, 'AUS', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Espiritu Santo', -15.4, 166.9, 'UK', 'forest', 0, 0, { port: 1, col: 1 }],
  ['New Caledonia', -21.5, 165.5, 'FRA', 'hills', 1, 0, { port: 1, col: 1 }],
  ['Fiji', -17.8, 178.0, 'UK', 'hills', 1, 0, { port: 1, col: 1 }],
  ['Samoa', -14.3, -170.7, 'USA', 'hills', 0, 0, { port: 1, col: 1, disc: 1 }],
  ['Biak', -1.0, 136.0, 'NED', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Morotai', 2.3, 128.4, 'NED', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Halmahera', 1.2, 127.9, 'NED', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Okinawa', 26.5, 127.95, 'JAP', 'hills', 2, 0, { port: 1 }],
];

/** Land of the Pacific slice that belongs to no province: seeds that take their share so the provinces beside them keep their shape. [what, lat, lon] */
export const PACIFIC_BEYOND = [];

/** Frontiers drawn true, as in world.mjs: a country's own outline from the atlas, part of one ('clip'), or a line drawn by hand. */
export const PACIFIC_FRONTIERS = [];

/** Short crossings armies can use without a fleet. */
export const PACIFIC_LINKS = [];
/** Land borders the map-maker misses. */
export const PACIFIC_ROADS = [];
/** Borders the map-maker finds that are not there. A name of today's chart may stand on one side. */
export const PACIFIC_NO_BORDER = [];

/**
 * The slices of the far side, in the order they are numbered. Each frontier, and each island no frontier runs
 * through, belongs to one slice, so a later slice never moves an earlier one's provinces.
 */
export const FAR_SLICES = [
  { name: 'pacific', regions: PACIFIC_REGIONS, beyond: PACIFIC_BEYOND, frontiers: PACIFIC_FRONTIERS, links: PACIFIC_LINKS, roads: PACIFIC_ROADS, noBorder: PACIFIC_NO_BORDER },
];

/** The lakes cut out of the new land by hand, as the Great Lakes are: name, surface in metres above the sea, outline in [lat, lon]. */
export const FAR_LAKES = [
  ['Lake Baikal', 456, [[51.5, 103.7], [51.85, 104.9], [52.3, 106.0], [52.9, 107.0], [53.4, 107.6], [54.0, 108.6], [54.9, 109.3], [55.75, 109.6], [55.7, 109.95], [55.0, 109.8], [54.3, 109.5], [53.7, 109.0], [53.0, 108.2], [52.5, 107.2], [52.2, 106.4], [51.75, 105.7], [51.45, 104.6]]],
  // The Aral Sea at its 1939 shore.
  ['Aral Sea', 53, [[46.75, 61.6], [46.5, 60.6], [46.0, 59.7], [45.3, 58.9], [44.6, 58.3], [43.9, 58.25], [43.55, 59.2], [43.75, 60.3], [44.3, 61.1], [44.9, 61.7], [45.5, 61.95], [46.2, 61.95]]],
];

/** How large an islet is drawn: half its width and half its height in map units (a unit is 2.8 km north to south). */
export const ISLET_SIZE = {
  _default: [5, 5],
  // The eastern Marshalls are four atolls 300 km from end to end; Truk's lagoon is 60 km across; Saipan stands with
  // Tinian south of it; Guam and Babeldaob are long islands.
  'Eastern Marshalls': [6, 9], Truk: [6, 7], Saipan: [4, 7], Guam: [5, 6], Palau: [4, 7],
};
