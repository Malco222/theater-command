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
  ['Sinaloa', 24.6, -106.2, 'MEX', 'mountain', 0, 0, { port: 1 }],
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
