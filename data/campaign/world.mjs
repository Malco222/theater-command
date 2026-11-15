// The wider world, laid beside the map of Europe: North America and the Atlantic islands first.
// tools/build-geo-world.mjs grows a province around every seed here, exactly as the map of Europe is made, and
// writes src/generated/world-map.json. The provinces of Europe are not touched: these are added after them, in
// this order, so a province keeps its number for good. Add new ones at the end of the list.
//
// [name, lat, lon, owner, terrain, victory points, factories, flags]   (flags as in regions.mjs)
// The grand campaign plays on the whole chart. The historical scenarios keep to the map of Europe.

/** The edges of the chart, in degrees. The top edge and the scale are those of the map of Europe. */
export const FRAME = { west: -141, east: 47, north: 71.4, south: 6 };

export const WORLD_REGIONS = [
  // The United States: the north-east
  ['Washington', 38.95, -77.1, 'USA', 'urban', 5, 2, { cap: 1 }],
  ['Norfolk', 36.9, -76.6, 'USA', 'plains', 1, 2, { port: 1 }],
  ['Virginia', 37.6, -78.9, 'USA', 'hills', 0, 0, {}],
  ['Philadelphia', 40.0, -75.3, 'USA', 'urban', 3, 4, { port: 1 }],
  ['New York', 41.05, -74.2, 'USA', 'urban', 6, 4, { port: 1 }],
  ['Upstate New York', 42.9, -75.5, 'USA', 'hills', 0, 1, {}],
  ['Buffalo', 42.8, -78.5, 'USA', 'urban', 1, 2, {}],
  ['Connecticut', 41.6, -72.6, 'USA', 'hills', 1, 2, { port: 1 }],
  ['Boston', 42.35, -71.2, 'USA', 'urban', 3, 3, { port: 1 }],
  ['White Mountains', 44.2, -71.8, 'USA', 'hills', 0, 0, {}],
  ['Kennebec', 45.0, -69.3, 'USA', 'forest', 0, 0, { port: 1 }],
  ['Pittsburgh', 40.45, -79.9, 'USA', 'urban', 3, 4, {}],
  ['Susquehanna', 41.0, -77.3, 'USA', 'hills', 0, 1, {}],
  // The Middle West
  ['Cleveland', 41.3, -81.6, 'USA', 'urban', 2, 3, {}],
  ['Ohio Valley', 39.6, -83.4, 'USA', 'plains', 1, 1, {}],
  ['Detroit', 42.5, -83.4, 'USA', 'urban', 4, 5, {}],
  ['Michigan', 44.3, -85.0, 'USA', 'forest', 0, 0, {}],
  ['Indiana', 39.9, -86.3, 'USA', 'plains', 1, 1, {}],
  ['Chicago', 41.8, -88.0, 'USA', 'urban', 5, 4, {}],
  ['Illinois', 39.6, -89.4, 'USA', 'plains', 0, 1, {}],
  ['Wisconsin', 44.4, -89.8, 'USA', 'forest', 0, 1, {}],
  ['Minnesota', 46.0, -94.5, 'USA', 'forest', 0, 1, {}],
  ['Iowa', 42.0, -93.5, 'USA', 'plains', 0, 0, {}],
  ['Missouri', 38.4, -91.8, 'USA', 'plains', 1, 1, {}],
  ['Kansas', 38.6, -98.2, 'USA', 'plains', 0, 1, {}],
  ['Nebraska', 41.4, -99.6, 'USA', 'plains', 0, 0, {}],
  ['Dakotas', 46.0, -100.6, 'USA', 'plains', 0, 0, {}],
  // The South
  ['Kentucky', 37.5, -85.3, 'USA', 'hills', 0, 0, {}],
  ['Tennessee', 35.8, -86.4, 'USA', 'hills', 0, 1, {}],
  ['Appalachia', 38.4, -80.9, 'USA', 'mountain', 0, 0, {}],
  ['North Carolina', 35.6, -79.2, 'USA', 'plains', 0, 1, { port: 1 }],
  ['Charleston', 33.6, -80.6, 'USA', 'plains', 0, 0, { port: 1 }],
  ['Georgia', 32.9, -83.6, 'USA', 'plains', 1, 1, { port: 1 }],
  ['Florida', 28.4, -81.6, 'USA', 'plains', 0, 0, { port: 1 }],
  ['Alabama', 33.0, -86.8, 'USA', 'hills', 0, 1, { port: 1 }],
  ['Mississippi', 32.8, -89.8, 'USA', 'plains', 0, 0, {}],
  ['Louisiana', 30.4, -91.6, 'USA', 'plains', 1, 1, { port: 1 }],
  ['Arkansas', 34.9, -92.5, 'USA', 'hills', 0, 0, {}],
  ['Oklahoma', 35.6, -97.4, 'USA', 'plains', 0, 0, { oil: 1 }],
  ['East Texas', 30.6, -95.4, 'USA', 'plains', 2, 2, { port: 1, oil: 1 }],
  ['North Texas', 32.9, -97.2, 'USA', 'plains', 1, 1, {}],
  ['West Texas', 31.6, -102.0, 'USA', 'plains', 0, 0, { oil: 1 }],
  ['Rio Grande', 28.3, -98.8, 'USA', 'plains', 0, 0, { port: 1 }],
  // The West
  ['Colorado', 39.4, -105.4, 'USA', 'mountain', 1, 0, {}],
  ['New Mexico', 34.6, -106.2, 'USA', 'hills', 0, 0, {}],
  ['Arizona', 33.8, -111.8, 'USA', 'hills', 0, 0, {}],
  ['Utah', 40.2, -111.8, 'USA', 'mountain', 0, 0, {}],
  ['Nevada', 39.4, -117.0, 'USA', 'hills', 0, 0, {}],
  ['Wyoming', 43.0, -107.6, 'USA', 'hills', 0, 0, {}],
  ['Montana', 47.0, -109.8, 'USA', 'hills', 0, 0, {}],
  ['Idaho', 44.2, -114.8, 'USA', 'mountain', 0, 0, {}],
  ['Puget Sound', 47.5, -122.0, 'USA', 'forest', 2, 2, { port: 1 }],
  ['Columbia Plateau', 46.6, -118.6, 'USA', 'hills', 0, 0, {}],
  ['Oregon', 44.4, -122.2, 'USA', 'forest', 0, 1, { port: 1 }],
  ['Northern California', 40.4, -122.4, 'USA', 'forest', 0, 0, {}],
  ['San Francisco', 37.8, -122.1, 'USA', 'urban', 3, 3, { port: 1 }],
  ['San Joaquin', 36.4, -119.6, 'USA', 'plains', 0, 0, { oil: 1 }],
  ['Los Angeles', 34.1, -118.0, 'USA', 'urban', 3, 3, { port: 1 }],
  ['San Diego', 33.0, -116.6, 'USA', 'hills', 0, 1, { port: 1 }],
  // Canada
  ['Ottawa', 45.5, -76.0, 'CAN', 'plains', 2, 1, { cap: 1 }],
  ['Montreal', 45.7, -73.3, 'CAN', 'urban', 3, 2, {}],
  ['Quebec', 47.2, -71.0, 'CAN', 'forest', 1, 1, { port: 1 }],
  ['Toronto', 43.9, -79.6, 'CAN', 'urban', 2, 3, {}],
  ['Northern Ontario', 48.6, -84.0, 'CAN', 'forest', 0, 0, {}],
  ['New Brunswick', 46.5, -66.4, 'CAN', 'forest', 0, 0, { port: 1 }],
  ['Nova Scotia', 45.1, -63.4, 'CAN', 'hills', 2, 0, { port: 1 }],
  ['Cape Breton', 46.2, -60.6, 'CAN', 'hills', 0, 0, { port: 1 }],
  ['Laurentides', 50.0, -72.5, 'CAN', 'forest', 0, 0, {}],
  ['Ungava', 56.5, -71.0, 'CAN', 'mountain', 0, 0, {}],
  ['Hudson Lowlands', 54.0, -88.5, 'CAN', 'marsh', 0, 0, {}],
  ['Winnipeg', 50.4, -97.6, 'CAN', 'plains', 1, 0, {}],
  ['Saskatchewan', 51.6, -106.0, 'CAN', 'plains', 0, 0, {}],
  ['Alberta', 53.0, -113.8, 'CAN', 'plains', 0, 0, {}],
  ['Vancouver', 49.5, -122.6, 'CAN', 'forest', 2, 1, { port: 1 }],
  ['Vancouver Island', 49.6, -125.6, 'CAN', 'forest', 0, 0, { port: 1 }],
  ['British Columbia', 54.0, -124.5, 'CAN', 'mountain', 0, 0, {}],
  ['Yukon', 62.5, -136.0, 'CAN', 'mountain', 0, 0, {}],
  ['Mackenzie', 62.5, -117.0, 'CAN', 'forest', 0, 0, {}],
  ['Keewatin', 61.0, -98.0, 'CAN', 'forest', 0, 0, {}],
  // Newfoundland and Labrador: a British dominion run from London since 1934
  ['Newfoundland', 48.6, -56.2, 'UK', 'forest', 1, 0, { port: 1, col: 1 }],
  ['Labrador', 53.6, -61.0, 'UK', 'forest', 0, 0, { col: 1 }],
  // Mexico
  ['Mexico City', 19.4, -99.1, 'MEX', 'urban', 3, 1, { cap: 1 }],
  ['Veracruz', 19.0, -96.6, 'MEX', 'plains', 1, 0, { port: 1 }],
  ['Tampico', 22.6, -98.6, 'MEX', 'plains', 0, 0, { port: 1, oil: 1 }],
  ['Monterrey', 25.8, -100.4, 'MEX', 'hills', 1, 1, {}],
  ['Chihuahua', 28.6, -106.1, 'MEX', 'hills', 0, 0, {}],
  ['Sonora', 29.3, -110.6, 'MEX', 'hills', 0, 0, { port: 1 }],
  ['Baja California', 30.3, -115.2, 'MEX', 'hills', 0, 0, {}],
  ['Sinaloa', 24.8, -107.4, 'MEX', 'mountain', 0, 0, { port: 1 }],
  ['Guadalajara', 20.7, -103.4, 'MEX', 'hills', 1, 0, {}],
  ['Oaxaca', 17.0, -96.8, 'MEX', 'mountain', 0, 0, { port: 1 }],
  ['Chiapas', 16.6, -92.9, 'MEX', 'forest', 0, 0, {}],
  ['Yucatan', 19.8, -89.4, 'MEX', 'forest', 0, 0, { port: 1 }],
  // Cuba
  ['Havana', 22.7, -82.2, 'CUB', 'plains', 2, 0, { cap: 1, port: 1 }],
  ['Camaguey', 21.5, -78.2, 'CUB', 'plains', 0, 0, { port: 1 }],
  ['Oriente', 20.5, -76.0, 'CUB', 'hills', 0, 0, { port: 1 }],
  // The islands
  ['Puerto Rico', 18.2, -66.4, 'USA', 'hills', 0, 0, { port: 1, col: 1 }],
  ['Jamaica', 18.15, -77.3, 'UK', 'hills', 0, 0, { port: 1, col: 1 }],
  ['Bahamas', 24.4, -78.0, 'UK', 'plains', 0, 0, { port: 1, col: 1 }],
  ['Iceland', 64.9, -18.6, 'DEN', 'hills', 1, 0, { port: 1, col: 1 }],
  ['Greenland', 61.3, -46.0, 'DEN', 'mountain', 0, 0, { port: 1, col: 1 }],
  ['Azores', 37.78, -25.5, 'POR', 'hills', 0, 0, { port: 1, col: 1 }],
  ['Canary Islands', 28.3, -16.55, 'ESP', 'hills', 0, 0, { port: 1, col: 1 }],
  ['Southern Baja', 25.6, -111.6, 'MEX', 'hills', 0, 0, {}],
];

/**
 * Land on the chart that belongs to no province yet: seeds that take their share of a land mass so the provinces
 * beside them keep their shape, and are then drawn as country beyond the war. [what, lat, lon]
 */
export const BEYOND = [
  ['the ice cap', 66.0, -43.0], ['the ice cap', 69.5, -38.0], ['the ice cap', 68.5, -50.5], ['the ice cap', 64.0, -49.5], ['the ice cap', 70.0, -26.5],
  ['the Arctic', 66.8, -95.0], ['the Arctic', 68.0, -107.0], ['the Arctic', 67.5, -128.0], ['the Arctic', 66.5, -63.5],
  ['Guatemala', 15.4, -90.4], ['Honduras', 14.8, -86.8], ['Nicaragua', 12.8, -85.2], ['Costa Rica', 10.0, -84.0], ['Panama', 8.5, -80.2],
  ['Colombia', 7.6, -74.6], ['Venezuela', 8.2, -66.0],
];

/**
 * Frontiers drawn true. Left to itself the map-maker shares a land mass out by nearness alone, and the line between
 * two countries wanders with it. Each of these cuts the continent in two: the provinces on its own side share out
 * the land of that side among themselves and take nothing across the line. 'line' is the frontier itself in
 * [lat, lon], from sea to sea; 'close' carries it round through open water (or beyond the chart) to shut the side
 * in; 'owners' are the countries whose provinces stand inside. A side listed earlier is cut out of one listed later:
 * the peninsula of Lower California is its own side inside Mexico's, so that Sonora takes nothing across the gulf.
 */
export const FRONTIERS = [
  {
    name: 'north', owners: ['CAN', 'UK'],
    line: [
      // The Strait of Juan de Fuca and Haro Strait, then the 49th parallel to the Lake of the Woods.
      [48.3, -128.0], [48.48, -124.8], [48.27, -123.9], [48.22, -123.3], [48.45, -123.17], [48.68, -123.25], [48.8, -123.0], [49.0, -123.3], [49.0, -95.15],
      // The Rainy River and the boundary waters to Lake Superior, and down the middle of the lake.
      [48.85, -94.7], [48.6, -93.4], [48.5, -92.6], [48.2, -91.8], [48.1, -90.8], [48.0, -89.6], [48.2, -88.5], [47.9, -87.0], [47.3, -85.5], [46.75, -84.75],
      // The St Marys River, Lake Huron, the St Clair and Detroit Rivers.
      [46.5, -84.6], [46.5, -84.35], [46.2, -84.0], [46.05, -83.8], [45.8, -83.3], [45.3, -82.5], [44.5, -82.3], [43.6, -82.2], [43.05, -82.42], [42.62, -82.52], [42.45, -82.7], [42.32, -83.07], [42.05, -83.12],
      // Lake Erie, the Niagara River and Lake Ontario.
      [41.85, -82.9], [41.9, -82.0], [42.3, -80.5], [42.65, -79.4], [42.87, -78.95], [43.1, -79.05], [43.3, -79.05], [43.5, -78.8], [43.6, -77.5], [43.95, -76.45], [44.1, -76.4],
      // The St Lawrence to the 45th parallel, the height of land round Maine, the St John and the St Croix.
      [44.35, -75.95], [44.7, -75.5], [45.0, -74.72], [45.01, -71.5], [45.3, -71.05], [45.35, -70.8], [45.9, -70.3], [46.4, -70.05], [46.7, -70.0], [47.46, -69.22], [47.2, -68.9], [47.36, -68.3], [47.07, -67.79],
      [45.9, -67.78], [45.6, -67.45], [45.15, -67.2], [44.85, -66.98], [44.5, -66.9], [43.0, -67.2], [41.5, -66.0],
    ],
    // Out into the Atlantic, up the Labrador Sea and Davis Strait, and round beyond the top of the chart.
    close: [[41.5, -48.0], [55.0, -48.0], [60.0, -55.0], [66.0, -58.0], [72.5, -62.0], [72.5, -179.0], [48.3, -179.0]],
  },
  {
    name: 'peninsula', owners: ['MEX'],
    // Lower California: the frontier above it, the Colorado to the head of the gulf.
    line: [[32.53, -119.0], [32.53, -117.12], [32.72, -114.72], [32.49, -114.81], [31.8, -114.8]],
    // Down the middle of the Gulf of California, round the cape and back up the Pacific.
    close: [[31.0, -114.4], [30.0, -113.6], [29.3, -112.9], [28.0, -112.0], [27.0, -111.0], [26.0, -110.3], [25.0, -109.5], [24.0, -108.8], [23.0, -108.5], [22.0, -109.5], [22.0, -119.0]],
  },
  {
    name: 'south', owners: ['MEX'],
    line: [
      // From the Pacific below San Diego to the Colorado, the Gadsden line to El Paso, and down the Rio Grande.
      [32.53, -119.0], [32.53, -117.12], [32.72, -114.72], [32.49, -114.81], [31.33, -111.07], [31.33, -108.21], [31.78, -108.21], [31.78, -106.53],
      [31.1, -105.6], [30.63, -104.99], [29.56, -104.37], [28.97, -103.15], [29.45, -102.8], [29.78, -102.4], [29.8, -101.5], [29.36, -100.9], [28.71, -100.5], [27.5, -99.5],
      [26.4, -99.1], [26.2, -98.3], [25.9, -97.5], [25.95, -97.15], [25.95, -95.0],
    ],
    // Across the Gulf, through the Yucatan Channel, along the Caribbean off the mainland, and round below the chart.
    close: [[24.0, -90.0], [22.0, -86.0], [19.0, -84.5], [16.5, -81.0], [14.0, -79.5], [13.2, -72.0], [12.6, -69.0], [12.0, -66.5], [11.5, -62.5], [11.5, -50.0], [2.0, -50.0], [2.0, -119.0]],
  },
];

/** Short crossings armies can use without a fleet. */
export const WORLD_LINKS = [
  ['Vancouver', 'Vancouver Island'], ['Nova Scotia', 'Cape Breton'],
];
/** Land borders the map-maker misses, because the line between the two runs mostly through water. */
export const WORLD_ROADS = [];
/** Borders the map-maker finds that are not there. */
export const WORLD_NO_BORDER = [];

/**
 * The Great Lakes, cut out of the land by hand: the coastline data draws them as dry ground. Outlines in
 * [lat, lon], rough but in the right places. No ship of the war sails them: they are water an army cannot cross.
 */
export const LAKES = [
  ['Lake Superior', [[46.75, -92.1], [47.3, -91.2], [47.95, -89.6], [48.5, -88.6], [48.85, -87.6], [48.7, -86.4], [47.95, -84.9], [47.0, -84.6], [46.5, -84.6], [46.65, -85.4], [46.5, -86.6], [46.55, -87.4], [46.9, -87.7], [47.45, -87.8], [46.9, -89.2], [46.6, -90.5]]],
  ['Lake Michigan', [[41.65, -87.4], [42.5, -87.8], [43.6, -87.75], [44.6, -87.5], [45.3, -87.3], [45.85, -86.6], [46.05, -85.6], [45.8, -84.9], [45.2, -85.4], [44.6, -86.2], [43.7, -86.5], [42.8, -86.2], [42.1, -86.5], [41.75, -86.9]]],
  ['Lake Huron', [[45.95, -84.5], [46.25, -83.6], [46.1, -82.4], [45.95, -81.4], [45.3, -80.3], [44.8, -79.95], [44.5, -80.4], [44.75, -81.2], [44.2, -81.6], [43.3, -81.75], [43.0, -82.4], [43.6, -82.6], [44.0, -83.0], [43.65, -83.85], [44.3, -83.4], [45.1, -83.4], [45.6, -84.2]]],
  ['Lake Erie', [[41.7, -83.4], [42.05, -83.1], [42.25, -81.9], [42.6, -81.0], [42.85, -79.5], [42.87, -78.95], [42.5, -79.4], [42.1, -80.2], [41.75, -81.3], [41.45, -82.2], [41.45, -82.9]]],
  ['Lake Ontario', [[43.25, -79.75], [43.65, -79.4], [43.95, -78.0], [44.2, -76.5], [43.9, -76.2], [43.45, -76.5], [43.3, -77.6], [43.3, -79.0]]],
  ['Lake Winnipeg', [[50.4, -96.9], [51.5, -96.6], [52.5, -97.2], [53.7, -98.0], [53.8, -98.9], [52.9, -99.1], [52.0, -97.6], [51.0, -96.95]]],
];

/**
 * The great rivers of the new world, in the form of data/campaign/rivers.mjs: each a line of [lat, lon] points from
 * its upper course to its mouth. tools/build-rivers.mjs (run with 'world') works out the crossings. The Rio Grande
 * below El Paso runs along the frontier, point for point.
 */
export const WORLD_RIVERS = [
  ['Mississippi', [[44.98, -93.27], [44.05, -91.65], [43.05, -91.15], [42.5, -90.65], [41.52, -90.58], [40.4, -91.4], [39.7, -91.36], [38.81, -90.12], [38.63, -90.18], [37.0, -89.17], [36.0, -89.7], [35.15, -90.05], [33.4, -91.06], [32.35, -90.88], [31.56, -91.4], [30.45, -91.19], [29.95, -90.07], [29.15, -89.25]]],
  ['Missouri', [[47.5, -111.3], [47.82, -110.67], [47.6, -108.0], [48.0, -106.4], [48.15, -103.6], [47.3, -101.4], [46.8, -100.78], [44.37, -100.35], [43.05, -98.5], [42.5, -96.4], [41.26, -95.93], [39.75, -94.85], [39.1, -94.58], [38.57, -92.17], [38.81, -90.12]]],
  ['Ohio', [[40.44, -80.0], [40.06, -80.72], [39.27, -81.56], [38.42, -82.45], [38.73, -83.0], [39.1, -84.51], [38.25, -85.76], [37.97, -87.56], [37.08, -88.6], [37.0, -89.17]]],
  ['St Lawrence', [[44.23, -76.48], [44.7, -75.5], [45.0, -74.72], [45.5, -73.55], [46.35, -72.55], [46.81, -71.2], [47.2, -70.6]]],
  ['Rio Grande', [[37.47, -105.87], [36.3, -105.7], [35.08, -106.65], [33.1, -107.2], [31.78, -106.53], [31.1, -105.6], [30.63, -104.99], [29.56, -104.37], [28.97, -103.15], [29.45, -102.8], [29.78, -102.4], [29.8, -101.5], [29.36, -100.9], [28.71, -100.5], [27.5, -99.5], [26.4, -99.1], [26.2, -98.3], [25.9, -97.5], [25.95, -97.15]]],
  ['Columbia', [[49.0, -117.63], [47.95, -118.98], [47.4, -120.3], [46.2, -119.1], [45.9, -119.3], [45.7, -121.5], [45.6, -122.7], [46.2, -123.8]]],
  ['Colorado', [[39.07, -108.55], [38.57, -109.55], [37.2, -110.9], [36.94, -111.48], [36.1, -112.1], [36.2, -113.3], [36.0, -114.74], [34.3, -114.14], [33.6, -114.55], [32.72, -114.72], [32.49, -114.81], [31.8, -114.8]]],
];
/** Crossings the geometry finds that the country does not have: [region, region]. */
export const WORLD_RIVER_CUT = [
  // Montreal and Quebec both stand on the north bank, and the road between them keeps to it.
  ['Montreal', 'Quebec'],
];
/** Crossings the geometry misses: [region, region, river]. */
export const WORLD_RIVER_ADD = [
  // Montreal and Quebec lie across the river from an army coming up out of New England.
  ['White Mountains', 'Montreal', 'St Lawrence'], ['Kennebec', 'Montreal', 'St Lawrence'], ['Kennebec', 'Quebec', 'St Lawrence'],
  // The whole frontier below El Paso is the river, and the lower Mississippi parts Louisiana from the state named for it.
  ['Rio Grande', 'Chihuahua', 'Rio Grande'], ['Louisiana', 'Mississippi', 'Mississippi'],
];

/**
 * The railways of the new world, laid by tools/build-rails.mjs (run with 'world') as Europe's are: a line to every
 * province, main lines between the cities. No line ran into these in 1939: the far north, and the foot of Lower
 * California.
 */
export const WORLD_NO_RAIL = ['Ungava', 'Keewatin', 'Mackenzie', 'Yukon', 'Labrador', 'Southern Baja', 'Greenland', 'Iceland'];
/** Lines the tool leaves out that history built: [region, region]. */
export const WORLD_RAIL_ADD = [
  // The Overland Route to San Francisco, the Santa Fe to Los Angeles, the northern lines to Puget Sound.
  ['Iowa', 'Nebraska'], ['Wyoming', 'Utah'], ['Missouri', 'Kansas'], ['Kansas', 'New Mexico'], ['Dakotas', 'Montana'], ['Montana', 'Columbia Plateau'],
  ['Colorado', 'Kansas'], ['Colorado', 'Wyoming'], ['Colorado', 'Utah'],
  // The Canadian Pacific round the lakes to the prairies.
  ['Ottawa', 'Northern Ontario'], ['Northern Ontario', 'Winnipeg'],
  // The New York Central up the Hudson, the Pennsylvania over the mountains, and the lines of the South.
  ['New York', 'Upstate New York'], ['Susquehanna', 'Pittsburgh'], ['Pittsburgh', 'Ohio Valley'], ['Virginia', 'North Carolina'], ['Tennessee', 'Alabama'], ['Missouri', 'Arkansas'],
  // Mexico: the west coast line from Guadalajara to Nogales, and the line to Oaxaca.
  ['Guadalajara', 'Sinaloa'], ['Sonora', 'Sinaloa'], ['Sonora', 'Arizona'], ['Mexico City', 'Oaxaca'],
];
/** Lines the tool lays that were never there: [region, region]. */
export const WORLD_RAIL_CUT = [
  // Sonora's line ran north to Arizona: none crossed the head of the Gulf of California until 1948, and none
  // crossed the Sierra Madre from Chihuahua to the Pacific until 1961.
  ['San Diego', 'Sonora'], ['Sonora', 'Baja California'], ['Chihuahua', 'Sinaloa'],
];
