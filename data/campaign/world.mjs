// The wider world, laid beside the map of Europe: North America and the Atlantic islands first (Version 74), then
// Central America, the Caribbean and South America (Version 76).
// tools/build-geo-world.mjs grows a province around every seed here, exactly as the map of Europe is made, and
// writes src/generated/world-map.json. The provinces of Europe are not touched: these are added after them, in
// this order, so a province keeps its number for good. Add new ones at the end of the list.
//
// [name, lat, lon, owner, terrain, victory points, factories, flags]   (flags as in regions.mjs)
// The grand campaign plays on the whole chart. The historical scenarios keep to the map of Europe.

/** The edges of the chart, in degrees. The top edge and the scale are those of the map of Europe. */
export const FRAME = { west: -141, east: 47, north: 71.4, south: -56 };

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
  // ---- Latin America (Version 76). Central America, and the Canal on its own ground.
  ['Guatemala City', 14.7, -90.6, 'GUA', 'mountain', 1, 0, { cap: 1, port: 1 }],
  ['Peten', 16.8, -89.9, 'GUA', 'forest', 0, 0, {}],
  ['British Honduras', 17.2, -88.6, 'UK', 'forest', 0, 0, { port: 1, col: 1 }],
  ['San Salvador', 13.7, -88.9, 'ELS', 'hills', 1, 0, { cap: 1, port: 1 }],
  ['Tegucigalpa', 14.3, -87.4, 'HON', 'mountain', 1, 0, { cap: 1, port: 1 }],
  ['Mosquitia', 15.2, -84.8, 'HON', 'forest', 0, 0, { port: 1 }],
  ['Managua', 12.4, -86.2, 'NIC', 'hills', 1, 0, { cap: 1, port: 1 }],
  ['Mosquito Coast', 13.2, -84.2, 'NIC', 'forest', 0, 0, { port: 1 }],
  ['San Jose', 9.93, -84.08, 'COS', 'mountain', 1, 0, { cap: 1, port: 1 }],
  ['Chiriqui', 8.45, -81.7, 'PAN', 'forest', 0, 0, { port: 1 }],
  ['Canal Zone', 9.1, -79.8, 'USA', 'forest', 2, 0, { port: 1, col: 1 }],
  ['Panama City', 9.25, -79.0, 'PAN', 'forest', 1, 0, { cap: 1, port: 1 }],
  // The Caribbean
  ['Port-au-Prince', 19.0, -72.3, 'HAI', 'hills', 1, 0, { cap: 1, port: 1 }],
  ['Ciudad Trujillo', 18.8, -70.3, 'DOM', 'hills', 1, 0, { cap: 1, port: 1 }],
  // No oilfield is marked on Trinidad: its oil reaches Britain in tankers, and the Atlantic lane (resources.json)
  // already counts them. A field marked here would be Britain's "at home", out of the submarines' reach.
  ['Trinidad', 10.4, -61.3, 'UK', 'plains', 1, 0, { port: 1, col: 1 }],
  ['Curacao', 12.18, -68.97, 'NED', 'hills', 1, 0, { port: 1, col: 1 }],
  ['Martinique', 14.65, -61.0, 'FRA', 'hills', 0, 0, { port: 1, col: 1 }],
  // Colombia
  ['Bogota', 4.7, -74.1, 'COL', 'mountain', 3, 1, { cap: 1 }],
  ['Medellin', 6.4, -75.6, 'COL', 'mountain', 1, 1, {}],
  ['Barranquilla', 10.3, -74.7, 'COL', 'plains', 1, 0, { port: 1 }],
  ['Cali', 3.4, -76.6, 'COL', 'hills', 1, 0, { port: 1 }],
  ['Santander', 7.4, -73.2, 'COL', 'hills', 0, 0, { oil: 1 }],
  ['Llanos', 4.8, -71.0, 'COL', 'plains', 0, 0, {}],
  ['Caqueta', 0.6, -73.0, 'COL', 'forest', 0, 0, {}],
  // Venezuela
  ['Caracas', 10.2, -66.8, 'VEN', 'hills', 3, 0, { cap: 1, port: 1 }],
  ['Maracaibo', 10.1, -72.2, 'VEN', 'plains', 2, 0, { port: 1, oil: 1 }],
  ['Barquisimeto', 9.6, -69.8, 'VEN', 'hills', 0, 0, { port: 1 }],
  ['Apure', 7.5, -68.5, 'VEN', 'plains', 0, 0, {}],
  ['Ciudad Bolivar', 8.6, -63.6, 'VEN', 'plains', 1, 0, { port: 1, oil: 1 }],
  ['Guayana', 5.6, -62.4, 'VEN', 'forest', 0, 0, {}],
  ['Alto Orinoco', 3.2, -66.0, 'VEN', 'forest', 0, 0, {}],
  // The Guianas
  ['British Guiana', 5.5, -58.8, 'UK', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Dutch Guiana', 4.5, -55.8, 'NED', 'forest', 0, 0, { port: 1, col: 1 }],
  ['French Guiana', 4.0, -53.0, 'FRA', 'forest', 0, 0, { port: 1, col: 1 }],
  // Ecuador and Peru
  ['Quito', -0.4, -78.5, 'ECU', 'mountain', 1, 0, { cap: 1 }],
  ['Guayaquil', -1.9, -79.9, 'ECU', 'plains', 1, 0, { port: 1 }],
  ['Napo', -1.6, -76.8, 'ECU', 'forest', 0, 0, {}],
  ['Lima', -11.9, -76.6, 'PER', 'urban', 3, 1, { cap: 1, port: 1 }],
  ['Piura', -5.2, -80.3, 'PER', 'plains', 0, 0, { port: 1, oil: 1 }],
  ['Trujillo', -7.9, -78.4, 'PER', 'hills', 0, 0, { port: 1 }],
  ['Arequipa', -16.2, -71.9, 'PER', 'mountain', 1, 0, { port: 1 }],
  ['Cuzco', -13.4, -72.2, 'PER', 'mountain', 0, 0, {}],
  ['Loreto', -4.8, -74.8, 'PER', 'forest', 0, 0, {}],
  ['Madre de Dios', -11.8, -70.6, 'PER', 'forest', 0, 0, {}],
  // Bolivia, Paraguay, Uruguay
  ['La Paz', -16.6, -68.0, 'BOL', 'mountain', 2, 0, { cap: 1 }],
  ['Potosi', -19.8, -66.2, 'BOL', 'mountain', 1, 0, {}],
  ['Santa Cruz', -17.6, -62.4, 'BOL', 'plains', 0, 0, {}],
  ['Beni', -13.2, -65.4, 'BOL', 'forest', 0, 0, {}],
  ['Asuncion', -25.4, -56.8, 'PAR', 'plains', 1, 0, { cap: 1 }],
  ['Chaco Boreal', -21.6, -60.2, 'PAR', 'forest', 0, 0, {}],
  ['Montevideo', -34.4, -56.0, 'URU', 'plains', 2, 0, { cap: 1, port: 1 }],
  ['Tacuarembo', -32.0, -56.4, 'URU', 'plains', 0, 0, {}],
  // Chile
  ['Santiago', -33.6, -70.8, 'CHL', 'urban', 3, 1, { cap: 1, port: 1 }],
  ['Tarapaca', -19.8, -69.5, 'CHL', 'hills', 0, 0, { port: 1 }],
  ['Antofagasta', -23.8, -69.4, 'CHL', 'hills', 1, 0, { port: 1 }],
  ['Atacama', -28.0, -70.4, 'CHL', 'hills', 0, 0, { port: 1 }],
  ['Concepcion', -37.2, -72.4, 'CHL', 'forest', 1, 0, { port: 1 }],
  ['Valdivia', -40.6, -72.6, 'CHL', 'forest', 0, 0, { port: 1 }],
  ['Aysen', -46.4, -72.8, 'CHL', 'mountain', 0, 0, { port: 1 }],
  ['Magallanes', -52.2, -71.6, 'CHL', 'hills', 0, 0, { port: 1 }],
  // Argentina
  ['Buenos Aires', -34.9, -58.9, 'ARG', 'urban', 4, 2, { cap: 1, port: 1 }],
  ['Rosario', -32.6, -61.0, 'ARG', 'plains', 1, 1, {}],
  ['Pampas', -36.6, -63.0, 'ARG', 'plains', 1, 0, {}],
  ['Bahia Blanca', -38.4, -61.8, 'ARG', 'plains', 0, 0, { port: 1 }],
  ['Cordoba', -31.4, -64.4, 'ARG', 'plains', 1, 0, {}],
  ['Mendoza', -33.4, -68.4, 'ARG', 'hills', 0, 0, {}],
  ['Tucuman', -26.6, -65.6, 'ARG', 'hills', 0, 0, {}],
  ['Gran Chaco', -26.2, -60.6, 'ARG', 'forest', 0, 0, {}],
  ['Corrientes', -28.6, -57.6, 'ARG', 'marsh', 0, 0, {}],
  ['Neuquen', -39.4, -69.2, 'ARG', 'hills', 0, 0, {}],
  ['Northern Patagonia', -41.6, -66.6, 'ARG', 'hills', 0, 0, { port: 1 }],
  ['Comodoro Rivadavia', -45.0, -68.6, 'ARG', 'hills', 0, 0, { port: 1, oil: 1 }],
  ['Southern Patagonia', -49.6, -70.2, 'ARG', 'hills', 0, 0, { port: 1 }],
  ['Tierra del Fuego', -54.2, -67.6, 'ARG', 'hills', 0, 0, { port: 1 }],
  ['Falkland Islands', -51.75, -58.6, 'UK', 'hills', 0, 0, { port: 1, col: 1 }],
  // Brazil
  ['Rio de Janeiro', -22.4, -43.2, 'BRA', 'urban', 4, 2, { cap: 1, port: 1 }],
  ['Sao Paulo', -23.2, -47.2, 'BRA', 'urban', 4, 3, { port: 1 }],
  ['Minas Gerais', -19.4, -44.4, 'BRA', 'hills', 1, 1, {}],
  ['Bahia', -12.6, -40.2, 'BRA', 'hills', 1, 0, { port: 1 }],
  ['Pernambuco', -8.4, -37.0, 'BRA', 'plains', 1, 0, { port: 1 }],
  ['Natal', -5.8, -36.2, 'BRA', 'plains', 1, 0, { port: 1 }],
  ['Ceara', -4.8, -39.8, 'BRA', 'plains', 0, 0, { port: 1 }],
  ['Maranhao', -4.6, -45.0, 'BRA', 'forest', 0, 0, { port: 1 }],
  ['Para', -2.6, -49.0, 'BRA', 'forest', 1, 0, { port: 1 }],
  ['Amapa', 1.4, -51.8, 'BRA', 'forest', 0, 0, {}],
  ['Amazonas', -3.6, -62.0, 'BRA', 'forest', 0, 0, {}],
  ['Rio Negro', 0.4, -65.0, 'BRA', 'forest', 0, 0, {}],
  ['Roraima', 2.2, -61.4, 'BRA', 'forest', 0, 0, {}],
  ['Acre', -9.0, -70.0, 'BRA', 'forest', 0, 0, {}],
  ['Guapore', -11.4, -63.0, 'BRA', 'forest', 0, 0, {}],
  ['Tapajos', -6.4, -55.6, 'BRA', 'forest', 0, 0, {}],
  ['Mato Grosso', -13.6, -56.0, 'BRA', 'forest', 0, 0, {}],
  ['Campo Grande', -20.4, -54.6, 'BRA', 'plains', 0, 0, {}],
  ['Goias', -15.4, -49.4, 'BRA', 'plains', 0, 0, {}],
  ['Sertao', -8.6, -43.0, 'BRA', 'hills', 0, 0, {}],
  ['Parana', -25.0, -51.6, 'BRA', 'forest', 0, 0, { port: 1 }],
  ['Rio Grande do Sul', -29.8, -53.4, 'BRA', 'plains', 1, 0, { port: 1 }],
];

/**
 * Land on the chart that belongs to no province yet: seeds that take their share of a land mass so the provinces
 * beside them keep their shape, and are then drawn as country beyond the war. [what, lat, lon]
 */
export const BEYOND = [
  ['the ice cap', 66.0, -43.0], ['the ice cap', 69.5, -38.0], ['the ice cap', 68.5, -50.5], ['the ice cap', 64.0, -49.5], ['the ice cap', 70.0, -26.5],
  ['the Arctic', 66.8, -95.0], ['the Arctic', 68.0, -107.0], ['the Arctic', 67.5, -128.0], ['the Arctic', 66.5, -63.5],
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
  // Latin America (Version 76): each country's own outline, from the atlas the coasts come from. 'country' is its
  // name there; 'within' picks the pieces of it inside a box of [west, south, east, north] (French Guiana out of
  // France). They stand before Mexico's side, whose closing line runs on south of them.
  { country: 'Guatemala', owners: ['GUA'] }, { country: 'Belize', owners: ['UK'] }, { country: 'El Salvador', owners: ['ELS'] },
  { country: 'Honduras', owners: ['HON'] }, { country: 'Nicaragua', owners: ['NIC'] }, { country: 'Costa Rica', owners: ['COS'] },
  // The Canal Zone lay inside Panama, in American hands.
  { country: 'Panama', owners: ['PAN', 'USA'] },
  { country: 'Colombia', owners: ['COL'] }, { country: 'Venezuela', owners: ['VEN'] }, { country: 'Guyana', owners: ['UK'] },
  { country: 'Suriname', owners: ['NED'] }, { country: 'France', owners: ['FRA'], within: [-60, 0, -50, 8] },
  { country: 'Ecuador', owners: ['ECU'] }, { country: 'Peru', owners: ['PER'] }, { country: 'Bolivia', owners: ['BOL'] },
  { country: 'Paraguay', owners: ['PAR'] }, { country: 'Uruguay', owners: ['URU'] }, { country: 'Chile', owners: ['CHL'] },
  { country: 'Argentina', owners: ['ARG'] }, { country: 'Brazil', owners: ['BRA'] },
  { country: 'Haiti', owners: ['HAI'] }, { country: 'Dominican Rep.', owners: ['DOM'] },
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
  // The Strait of Magellan at its narrows, from either country's shore.
  ['Magallanes', 'Tierra del Fuego'], ['Southern Patagonia', 'Tierra del Fuego'],
];
/** Land borders the map-maker misses, because the line between the two runs mostly through water. */
export const WORLD_ROADS = [];
/** Borders the map-maker finds that are not there. */
export const WORLD_NO_BORDER = [];

/**
 * The Great Lakes, cut out of the land by hand: the coastline data draws them as dry ground. Each has its name,
 * its surface in metres above sea level and its outline in [lat, lon], rough but in the right places. No ship of
 * the war sails them: they are water an army cannot cross.
 */
export const LAKES = [
  ['Lake Superior', 183, [[46.75, -92.1], [47.3, -91.2], [47.95, -89.6], [48.5, -88.6], [48.85, -87.6], [48.7, -86.4], [47.95, -84.9], [47.0, -84.6], [46.5, -84.6], [46.65, -85.4], [46.5, -86.6], [46.55, -87.4], [46.9, -87.7], [47.45, -87.8], [46.9, -89.2], [46.6, -90.5]]],
  ['Lake Michigan', 176, [[41.65, -87.4], [42.5, -87.8], [43.6, -87.75], [44.6, -87.5], [45.3, -87.3], [45.85, -86.6], [46.05, -85.6], [45.8, -84.9], [45.2, -85.4], [44.6, -86.2], [43.7, -86.5], [42.8, -86.2], [42.1, -86.5], [41.75, -86.9]]],
  ['Lake Huron', 176, [[45.95, -84.5], [46.25, -83.6], [46.1, -82.4], [45.95, -81.4], [45.3, -80.3], [44.8, -79.95], [44.5, -80.4], [44.75, -81.2], [44.2, -81.6], [43.3, -81.75], [43.0, -82.4], [43.6, -82.6], [44.0, -83.0], [43.65, -83.85], [44.3, -83.4], [45.1, -83.4], [45.6, -84.2]]],
  ['Lake Erie', 174, [[41.7, -83.4], [42.05, -83.1], [42.25, -81.9], [42.6, -81.0], [42.85, -79.5], [42.87, -78.95], [42.5, -79.4], [42.1, -80.2], [41.75, -81.3], [41.45, -82.2], [41.45, -82.9]]],
  ['Lake Ontario', 75, [[43.25, -79.75], [43.65, -79.4], [43.95, -78.0], [44.2, -76.5], [43.9, -76.2], [43.45, -76.5], [43.3, -77.6], [43.3, -79.0]]],
  ['Lake Winnipeg', 217, [[50.4, -96.9], [51.5, -96.6], [52.5, -97.2], [53.7, -98.0], [53.8, -98.9], [52.9, -99.1], [52.0, -97.6], [51.0, -96.95]]],
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
  // South America. The Amazon from the foot of the Andes, with the Madeira; the Parana with the Paraguay and the
  // Uruguay, which are frontiers for most of their length; the Orinoco, the Magdalena and the Sao Francisco.
  ['Amazon', [[-4.45, -77.6], [-4.9, -75.2], [-4.5, -73.6], [-3.75, -73.25], [-3.4, -71.8], [-4.2, -69.94], [-3.1, -67.9], [-2.5, -66.1], [-3.35, -64.7], [-3.9, -63.1], [-3.3, -60.6], [-3.13, -59.9], [-3.14, -58.44], [-2.63, -56.74], [-1.9, -55.52], [-2.44, -54.7], [-1.52, -52.58], [-1.4, -51.65], [-0.2, -50.9], [0.5, -50.0]]],
  ['Madeira', [[-10.8, -65.35], [-9.7, -65.4], [-8.76, -63.9], [-7.5, -63.02], [-5.81, -61.3], [-4.39, -59.59], [-3.37, -58.78]]],
  ['Parana', [[-20.1, -51.0], [-22.5, -53.1], [-24.08, -54.25], [-25.6, -54.6], [-26.8, -55.2], [-27.37, -55.9], [-27.45, -57.0], [-27.3, -58.6], [-27.47, -58.83], [-29.14, -59.26], [-30.75, -59.65], [-31.73, -60.53], [-32.95, -60.63], [-33.33, -60.2], [-34.1, -59.0], [-34.2, -58.4]]],
  ['Paraguay', [[-16.07, -57.68], [-17.9, -57.5], [-19.0, -57.65], [-20.2, -58.15], [-21.7, -57.9], [-22.1, -57.98], [-23.4, -57.43], [-25.28, -57.63], [-26.86, -58.3], [-27.3, -58.6]]],
  ['Uruguay', [[-27.15, -53.9], [-27.6, -55.0], [-28.66, -56.0], [-29.75, -57.08], [-30.2, -57.6], [-31.39, -57.96], [-32.32, -58.08], [-33.12, -58.3], [-34.0, -58.4]]],
  ['Orinoco', [[4.05, -67.7], [5.66, -67.63], [6.19, -67.48], [7.0, -66.9], [7.64, -66.17], [7.9, -64.8], [8.12, -63.55], [8.35, -62.65], [8.7, -62.2], [8.9, -61.3], [9.3, -60.9]]],
  ['Magdalena', [[2.93, -75.28], [4.3, -74.8], [5.2, -74.74], [6.49, -74.4], [7.06, -73.87], [8.3, -73.8], [9.0, -73.97], [9.24, -74.75], [10.25, -74.91], [10.97, -74.8], [11.1, -74.85]]],
  ['Sao Francisco', [[-17.35, -44.94], [-15.49, -44.36], [-13.25, -43.42], [-11.09, -43.14], [-9.62, -42.08], [-9.4, -40.5], [-8.51, -39.31], [-9.4, -38.2], [-10.29, -36.58], [-10.5, -36.4]]],
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
  // The Amazon runs through the middle of the province named for it: the country to the north lies over the river.
  ['Amazonas', 'Rio Negro', 'Amazon'], ['Amazonas', 'Roraima', 'Amazon'],
  // The oilfields of eastern Venezuela lie north of the Orinoco and Guayana south of it; the Magdalena parts the
  // coast and Antioquia from Santander.
  ['Ciudad Bolivar', 'Guayana', 'Orinoco'], ['Apure', 'Guayana', 'Orinoco'], ['Barranquilla', 'Santander', 'Magdalena'], ['Medellin', 'Santander', 'Magdalena'],
  // The Paraguay parts the Chaco from the country round Asuncion, and the Parana parts the land between the rivers from Rosario.
  ['Chaco Boreal', 'Asuncion', 'Paraguay'], ['Gran Chaco', 'Asuncion', 'Paraguay'], ['Rosario', 'Corrientes', 'Parana'],
  // The lower Sao Francisco is the border of Pernambuco and Bahia.
  ['Pernambuco', 'Bahia', 'Sao Francisco'],
];

/**
 * The railways of the new world, laid by tools/build-rails.mjs (run with 'world') as Europe's are: a line to every
 * province, main lines between the cities. No line ran into these in 1939: the far north, and the foot of Lower
 * California; the forests of Central America; the basin of the Amazon and the Orinoco, where the rivers were the
 * roads; the Chaco; and the far south of Chile and Argentina.
 */
export const WORLD_NO_RAIL = [
  'Ungava', 'Keewatin', 'Mackenzie', 'Yukon', 'Labrador', 'Southern Baja', 'Greenland', 'Iceland',
  'Peten', 'British Honduras', 'Mosquitia', 'Mosquito Coast',
  'Llanos', 'Caqueta', 'Apure', 'Ciudad Bolivar', 'Guayana', 'Alto Orinoco', 'British Guiana', 'Dutch Guiana', 'French Guiana',
  'Napo', 'Loreto', 'Madre de Dios', 'Beni', 'Chaco Boreal',
  'Para', 'Amapa', 'Amazonas', 'Rio Negro', 'Roraima', 'Acre', 'Guapore', 'Tapajos', 'Mato Grosso',
  'Aysen', 'Magallanes', 'Southern Patagonia', 'Tierra del Fuego',
];
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
  // South America's lines over a frontier: the Transandine from Mendoza to Santiago, the lines up to La Paz from
  // Arica, from Antofagasta and from the Argentine north, Tacna to Arica, and the train ferry to Paraguay.
  ['Mendoza', 'Santiago'], ['Tarapaca', 'La Paz'], ['Antofagasta', 'Potosi'], ['Tucuman', 'Potosi'], ['Arequipa', 'Tarapaca'], ['Corrientes', 'Asuncion'],
  ['Tacuarembo', 'Rio Grande do Sul'],
  // Brazil's lines ran inland from Rio and Sao Paulo through Minas Gerais, and Argentina's north from Cordoba.
  ['Minas Gerais', 'Goias'], ['Sao Paulo', 'Minas Gerais'], ['Minas Gerais', 'Bahia'], ['Cordoba', 'Tucuman'],
];
/** Lines the tool lays that were never there: [region, region]. */
export const WORLD_RAIL_CUT = [
  // Sonora's line ran north to Arizona: none crossed the head of the Gulf of California until 1948, and none
  // crossed the Sierra Madre from Chihuahua to the Pacific until 1961.
  ['San Diego', 'Sonora'], ['Sonora', 'Baja California'], ['Chihuahua', 'Sinaloa'],
  // No line ran the length of Central America below El Salvador, none crossed the Darien, and none joined
  // Colombia, Venezuela, Ecuador and Peru to each other or Brazil to its western neighbours.
  ['Tegucigalpa', 'Managua'], ['Managua', 'San Jose'], ['San Jose', 'Chiriqui'], ['Panama City', 'Medellin'],
  ['Santander', 'Maracaibo'], ['Barranquilla', 'Maracaibo'], ['Cali', 'Quito'], ['Guayaquil', 'Piura'],
  ['Santa Cruz', 'Campo Grande'], ['Asuncion', 'Parana'], ['Asuncion', 'Campo Grande'], ['Corrientes', 'Rio Grande do Sul'], ['Corrientes', 'Parana'],
  // Honduras had its banana lines and no more; Panama's western line met nothing; the two halves of Hispaniola
  // were never joined; no bridge crossed the Uruguay or the Pilcomayo; and in the south no line crossed the Andes.
  ['San Salvador', 'Tegucigalpa'], ['Chiriqui', 'Canal Zone'], ['Port-au-Prince', 'Ciudad Trujillo'],
  ['Tacuarembo', 'Rosario'], ['Tacuarembo', 'Corrientes'], ['Asuncion', 'Gran Chaco'],
  ['Concepcion', 'Neuquen'], ['Valdivia', 'Neuquen'], ['Antofagasta', 'Tucuman'], ['Atacama', 'Tucuman'], ['Cuzco', 'La Paz'],
  // The oil port of Comodoro Rivadavia had a line of its own into the hills, joined to nothing; nor did any line
  // run from Bahia or from Mato Grosso's south into Goias.
  ['Northern Patagonia', 'Comodoro Rivadavia'], ['Bahia', 'Goias'], ['Campo Grande', 'Goias'],
];
