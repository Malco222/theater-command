// The cities added to the world chart after its slices were handed over (Brian, 2026-11-17: "more cities in US and
// the rest of the world map that didnt get the attention that europe got").
//
// Every province has one chief place, so a new city is a new province. It is cut out of ONE province that stands
// already, outside the map of Europe: the ground nearer the city than the parent's own seed (and than its sister
// cities) becomes the city's. The parent keeps its number, its name, its seed and its owner, and every border it has
// with the world outside it stays where it is. A city takes the next free number, in the order of this file, and
// keeps it for good: add new groups at the END, and never move or remove a row.
//
// CITY_GROUPS: [parent, [[name, lat, lon, ground, victory points, factories, flags], ...], change]. The city belongs
// to the parent's owner. 'change' (optional) is what the parent itself becomes once its city is its own province:
// { vp, fac, port: 0 ... } (a flag of 0 is taken off). A row in the clerk's tables (data/campaign/tables/*.csv)
// stands over all of it, as for every province of the far side.
export const CITY_GROUPS = [
  // ---- The United States (handed over first, 2026-11-17) ----
  ['Washington', [
    ['Baltimore', 39.33, -76.7, 'urban', 2, 2, { port: 1 }],
  ]],
  ['Norfolk', [
    ['Richmond', 37.54, -77.47, 'plains', 1, 1, {}],
  ]],
  ['Virginia', [
    ['Roanoke', 37.27, -79.94, 'hills', 0, 0, {}],
  ]],
  ['Upstate New York', [
    ['Albany', 42.68, -73.85, 'hills', 0, 1, {}],
  ]],
  ['Buffalo', [
    ['Rochester', 43.13, -77.62, 'plains', 1, 1, {}],
  ]],
  ['Boston', [
    ['Providence', 41.82, -71.45, 'urban', 1, 1, { port: 1 }],
  ]],
  ['Kennebec', [
    ['Bath', 43.95, -69.85, 'forest', 0, 1, { port: 1 }],
  ]],
  ['New York', [
    ['Scranton', 41.41, -75.66, 'hills', 0, 0, {}],
  ]],
  ['Ohio Valley', [
    ['Cincinnati', 39.12, -84.5, 'hills', 1, 1, {}],
  ]],
  ['Detroit', [
    ['Toledo', 41.63, -83.6, 'plains', 0, 1, {}],
  ]],
  ['Michigan', [
    ['Grand Rapids', 42.96, -85.67, 'plains', 0, 0, {}],
    ['Sault Ste Marie', 46.42, -84.4, 'forest', 0, 0, {}],
  ]],
  ['Indiana', [
    ['Fort Wayne', 41.08, -85.14, 'plains', 0, 0, {}],
  ]],
  ['Kentucky', [
    ['Evansville', 38, -87.55, 'plains', 0, 1, {}],
    ['Louisville', 38.25, -85.76, 'plains', 1, 1, {}],
  ]],
  ['Chicago', [
    ['Milwaukee', 43.04, -88, 'plains', 1, 2, {}],
    ['Davenport', 41.52, -90.58, 'plains', 0, 1, {}],
    ['South Bend', 41.68, -86.25, 'plains', 0, 1, {}],
  ]],
  ['Illinois', [
    ['Peoria', 40.69, -89.59, 'plains', 0, 1, {}],
  ], { fac: 0 }],
  ['Wisconsin', [
    ['Madison', 43.07, -89.4, 'plains', 0, 0, {}],
    ['Green Bay', 44.5, -88.1, 'forest', 0, 0, {}],
    ['Eau Claire', 44.81, -91.5, 'forest', 0, 0, {}],
  ]],
  ['Minnesota', [
    ['Minneapolis', 44.98, -93.27, 'plains', 1, 1, {}],
    ['Duluth', 46.8, -92.2, 'forest', 1, 0, {}],
    ['Hibbing', 47.43, -92.94, 'hills', 0, 0, {}],
    ['Fargo', 46.88, -96.79, 'plains', 0, 0, {}],
  ], { fac: 0 }],
  ['Iowa', [
    ['Sioux City', 42.5, -96.4, 'plains', 0, 0, {}],
    ['Omaha', 41.26, -95.94, 'plains', 1, 1, {}],
    ['Sioux Falls', 43.55, -96.73, 'plains', 0, 0, {}],
    ['Cedar Rapids', 41.98, -91.67, 'plains', 0, 0, {}],
  ]],
  ['Missouri', [
    ['St Louis', 38.63, -90.3, 'urban', 2, 2, {}],
    ['Kansas City', 39.1, -94.58, 'plains', 1, 1, {}],
    ['Joplin', 37.08, -94.51, 'hills', 0, 0, {}],
  ], { vp: 0, fac: 0 }],
  ['Kansas', [
    ['Wichita', 37.69, -97.34, 'plains', 0, 1, {}],
    ['Topeka', 39.05, -95.68, 'plains', 0, 0, {}],
    ['Dodge City', 37.75, -100.02, 'plains', 0, 0, {}],
  ], { fac: 0 }],
  ['Nebraska', [
    ['Scottsbluff', 41.87, -103.67, 'plains', 0, 0, {}],
    ['Grand Island', 40.92, -98.34, 'plains', 0, 0, {}],
  ]],
  ['Dakotas', [
    ['Rapid City', 44.08, -103.23, 'hills', 0, 0, {}],
    ['Minot', 48.23, -101.3, 'plains', 0, 0, {}],
    ['Pierre', 44.37, -100.35, 'plains', 0, 0, {}],
  ]],
  ['Tennessee', [
    ['Paducah', 37.08, -88.6, 'plains', 0, 0, {}],
    ['Knoxville', 35.96, -83.92, 'hills', 0, 1, {}],
    ['Chattanooga', 35.05, -85.31, 'hills', 0, 0, {}],
  ]],
  ['Arkansas', [
    ['Memphis', 35.15, -90, 'plains', 1, 0, {}],
    ['Fort Smith', 35.39, -94.4, 'hills', 0, 0, {}],
  ]],
  ['North Carolina', [
    ['Charlotte', 35.23, -80.84, 'plains', 0, 0, {}],
    ['Wilmington', 34.25, -77.95, 'plains', 0, 1, { port: 1 }],
  ], { port: 0 }],
  ['Charleston', [
    ['Greenville', 34.85, -82.4, 'hills', 0, 0, {}],
    ['Savannah', 32.08, -81.15, 'plains', 0, 0, { port: 1 }],
  ]],
  ['Georgia', [
    ['Atlanta', 33.75, -84.39, 'hills', 1, 1, {}],
    ['Tallahassee', 30.44, -84.28, 'forest', 0, 0, {}],
  ], { vp: 0, fac: 0 }],
  ['Florida', [
    ['Miami', 25.8, -80.3, 'plains', 1, 0, { port: 1 }],
    ['Tampa', 27.95, -82.4, 'plains', 0, 0, { port: 1 }],
    ['Jacksonville', 30.33, -81.7, 'plains', 0, 0, { port: 1 }],
  ]],
  ['Alabama', [
    ['Pensacola', 30.45, -87.22, 'plains', 0, 0, { port: 1 }],
  ], { port: 0 }],
  ['Louisiana', [
    ['Mobile', 30.72, -88.1, 'plains', 0, 1, { port: 1 }],
    ['New Orleans', 29.98, -90.1, 'marsh', 2, 1, { port: 1 }],
    ['Lake Charles', 30.23, -93.22, 'marsh', 0, 0, { port: 1 }],
  ], { vp: 0 }],
  ['Mississippi', [
    ['Meridian', 32.36, -88.7, 'plains', 0, 0, {}],
    ['Tupelo', 34.26, -88.7, 'plains', 0, 0, {}],
  ]],
  ['East Texas', [
    ['Shreveport', 32.52, -93.75, 'plains', 0, 0, {}],
    ['Austin', 30.27, -97.74, 'hills', 0, 0, {}],
  ]],
  ['Oklahoma', [
    ['Tulsa', 36.15, -95.99, 'plains', 1, 1, {}],
    ['Lawton', 34.6, -98.4, 'plains', 0, 0, {}],
  ]],
  ['North Texas', [
    ['Wichita Falls', 33.91, -98.49, 'plains', 0, 0, {}],
    ['Waco', 31.55, -97.15, 'plains', 0, 0, {}],
  ]],
  ['New Mexico', [
    ['El Paso', 31.76, -106.45, 'hills', 0, 0, {}],
    ['Amarillo', 35.2, -101.83, 'plains', 0, 0, {}],
    ['Santa Fe', 35.69, -105.94, 'mountain', 0, 0, {}],
    ['Roswell', 33.39, -104.52, 'plains', 0, 0, {}],
    ['Gallup', 35.53, -108.74, 'hills', 0, 0, {}],
  ]],
  ['West Texas', [
    ['Lubbock', 33.58, -101.86, 'plains', 0, 0, {}],
    ['Abilene', 32.45, -99.73, 'plains', 0, 0, {}],
    ['San Angelo', 31.46, -100.44, 'plains', 0, 0, {}],
  ]],
  ['Rio Grande', [
    ['San Antonio', 29.42, -98.49, 'plains', 1, 0, {}],
    ['Corpus Christi', 27.8, -97.45, 'plains', 0, 0, { port: 1 }],
    ['Brownsville', 25.95, -97.55, 'plains', 0, 0, { port: 1 }],
  ], { port: 0 }],
  ['Colorado', [
    ['Pueblo', 38.25, -104.61, 'plains', 0, 1, {}],
    ['Grand Junction', 39.06, -108.55, 'mountain', 0, 0, {}],
    ['Cheyenne', 41.14, -104.82, 'plains', 0, 0, {}],
  ]],
  ['Arizona', [
    ['Tucson', 32.22, -110.97, 'hills', 0, 0, {}],
    ['Flagstaff', 35.2, -111.65, 'mountain', 0, 0, {}],
  ]],
  ['San Diego', [
    ['Yuma', 32.69, -114.55, 'plains', 0, 0, {}],
  ]],
  ['Utah', [
    ['Ogden', 41.22, -111.97, 'mountain', 0, 0, {}],
    ['Cedar City', 37.68, -113.06, 'mountain', 0, 0, {}],
  ]],
  ['Nevada', [
    ['Reno', 39.53, -119.81, 'mountain', 0, 0, {}],
    ['Elko', 40.83, -115.76, 'mountain', 0, 0, {}],
  ]],
  ['Los Angeles', [
    ['Las Vegas', 36.17, -115.14, 'hills', 0, 0, {}],
    ['Santa Barbara', 34.45, -119.7, 'hills', 0, 0, { port: 1 }],
  ]],
  ['Wyoming', [
    ['Rock Springs', 41.59, -109.2, 'hills', 0, 0, {}],
    ['Sheridan', 44.8, -106.96, 'hills', 0, 0, {}],
  ]],
  ['Montana', [
    ['Great Falls', 47.5, -111.3, 'plains', 0, 0, {}],
    ['Butte', 46, -112.53, 'mountain', 1, 0, {}],
    ['Billings', 45.78, -108.5, 'plains', 0, 0, {}],
    ['Missoula', 46.87, -114, 'mountain', 0, 0, {}],
  ]],
  ['Idaho', [
    ['Boise', 43.62, -116.2, 'hills', 0, 0, {}],
    ['Pocatello', 42.87, -112.45, 'hills', 0, 0, {}],
  ]],
  ['Columbia Plateau', [
    ['Spokane', 47.66, -117.43, 'hills', 0, 1, {}],
    ['Yakima', 46.6, -120.5, 'hills', 0, 0, {}],
    ['Pendleton', 45.67, -118.79, 'hills', 0, 0, {}],
  ]],
  ['Puget Sound', [
    ['Bellingham', 48.75, -122.45, 'forest', 0, 0, { port: 1 }],
  ]],
  ['Oregon', [
    ['Portland', 45.52, -122.68, 'forest', 1, 1, { port: 1 }],
    ['Coos Bay', 43.37, -124.15, 'forest', 0, 0, { port: 1 }],
  ], { fac: 0 }],
  ['Northern California', [
    ['Medford', 42.33, -122.87, 'forest', 0, 0, {}],
    ['Eureka', 40.8, -124.1, 'forest', 0, 0, { port: 1 }],
  ]],
  ['San Francisco', [
    ['Sacramento', 38.58, -121.49, 'plains', 1, 0, {}],
  ]],
  ['San Joaquin', [
    ['Bakersfield', 35.37, -119.02, 'plains', 0, 0, {}],
  ]],
  ['Appalachia', [
    ['Huntington', 38.42, -82.45, 'hills', 0, 0, {}],
  ]],
  ['White Mountains', [
    ['Burlington', 44.48, -73.21, 'hills', 0, 0, {}],
  ]],
  // ---- Canada and Newfoundland ----
  ['Toronto', [
    ['Hamilton', 43.25, -79.87, 'urban', 1, 2, {}],
    ['Windsor', 42.3, -83, 'plains', 1, 1, {}],
    ['Sudbury', 46.5, -81, 'hills', 1, 0, {}],
    ['Trenton', 44.1, -77.58, 'plains', 0, 0, {}],
  ]],
  ['Ottawa', [
    ['Petawawa', 45.9, -77.28, 'forest', 0, 0, {}],
    ['North Bay', 46.31, -79.46, 'forest', 0, 0, {}],
  ]],
  ['Northern Ontario', [
    ['Timmins', 48.47, -81.33, 'forest', 0, 0, {}],
    ['Fort William', 48.38, -89.25, 'forest', 1, 0, {}],
    ['Moosonee', 51.18, -80.41, 'marsh', 0, 0, { port: 1 }],
    ['Rouyn', 48.24, -79.02, 'forest', 0, 0, {}],
  ]],
  ['Winnipeg', [
    ['Kenora', 49.77, -94.49, 'forest', 0, 0, {}],
    ['Brandon', 49.85, -99.95, 'plains', 0, 0, {}],
  ]],
  ['Montreal', [
    ['Sherbrooke', 45.4, -71.9, 'hills', 0, 0, {}],
    ['Trois-Rivieres', 46.35, -72.55, 'plains', 0, 1, {}],
  ]],
  ['Quebec', [
    ['Rimouski', 48.45, -68.52, 'hills', 0, 0, { port: 1 }],
    ['Chicoutimi', 48.43, -71.07, 'forest', 1, 1, {}],
  ]],
  ['New Brunswick', [
    ['Gaspe', 48.71, -64.29, 'hills', 0, 0, { port: 1 }],
    ['Saint John', 45.27, -65.79, 'hills', 1, 0, {}],
    ['Moncton', 46.1, -64.8, 'forest', 0, 0, {}],
  ]],
  ['Laurentides', [
    ['Sept-Iles', 50.29, -66.62, 'forest', 0, 0, { port: 1 }],
  ]],
  ['Ungava', [
    ['Fort Chimo', 58.1, -68.13, 'forest', 0, 0, {}],
  ]],
  ['Nova Scotia', [
    ['Yarmouth', 43.84, -66.12, 'hills', 0, 0, { port: 1 }],
  ]],
  ['Newfoundland', [
    ["St John's", 47.56, -52.75, 'hills', 1, 0, { port: 1 }],
    ['Gander', 48.95, -54.57, 'forest', 0, 0, {}],
    ['Corner Brook', 48.95, -57.95, 'forest', 0, 0, { port: 1 }],
  ], { vp: 0 }],
  ['Saskatchewan', [
    ['The Pas', 53.82, -101.25, 'forest', 0, 0, {}],
    ['Regina', 50.45, -104.6, 'plains', 1, 0, {}],
    ['Prince Albert', 53.2, -105.75, 'forest', 0, 0, {}],
    ['Medicine Hat', 50.04, -110.68, 'plains', 0, 0, {}],
  ]],
  ['Keewatin', [
    ['Churchill', 58.68, -93.93, 'forest', 0, 0, { port: 1 }],
  ]],
  ['Alberta', [
    ['Calgary', 51.05, -114.07, 'plains', 1, 0, {}],
    ['Lethbridge', 49.7, -112.83, 'plains', 0, 0, {}],
    ['Fort McMurray', 56.73, -111.38, 'forest', 0, 0, {}],
  ]],
  ['British Columbia', [
    ['Grande Prairie', 55.17, -118.8, 'plains', 0, 0, {}],
    ['Prince Rupert', 54.3, -130.3, 'forest', 0, 0, { port: 1 }],
    ['Dawson Creek', 55.76, -120.24, 'plains', 0, 0, {}],
    ['Fort Nelson', 58.8, -122.7, 'forest', 0, 0, {}],
  ]],
  ['Vancouver Island', [
    ['Esquimalt', 48.43, -123.37, 'forest', 1, 0, { port: 1 }],
  ]],
  ['Vancouver', [
    ['Kamloops', 50.67, -120.33, 'hills', 0, 0, {}],
    ['Trail', 49.1, -117.7, 'mountain', 0, 1, {}],
  ]],
  ['Yukon', [
    ['Whitehorse', 60.72, -135.05, 'mountain', 0, 0, {}],
    ['Dawson City', 64.06, -139.43, 'mountain', 0, 0, {}],
    ['Watson Lake', 60.06, -128.7, 'forest', 0, 0, {}],
  ]],
  ['Mackenzie', [
    ['Yellowknife', 62.45, -114.37, 'forest', 0, 0, {}],
    ['Port Radium', 66.08, -118.03, 'forest', 0, 0, {}],
  ]],
  // ---- Mexico, Central America and the Caribbean ----
  ['Baja California', [
    ['Ensenada', 31.87, -116.6, 'hills', 0, 0, { port: 1 }],
  ]],
  ['Sonora', [
    ['Guaymas', 27.92, -110.9, 'desert', 0, 0, { port: 1 }],
    ['Nogales', 31.3, -110.94, 'hills', 0, 0, {}],
  ]],
  ['Chihuahua', [
    ['Parral', 26.93, -105.67, 'hills', 0, 0, {}],
  ]],
  ['Monterrey', [
    ['Torreon', 25.54, -103.45, 'desert', 1, 0, {}],
    ['Monclova', 26.9, -101.42, 'hills', 0, 1, {}],
  ]],
  ['Tampico', [
    ['Ciudad Victoria', 23.73, -99.15, 'hills', 0, 0, {}],
    ['San Luis Potosi', 22.15, -100.98, 'hills', 1, 0, {}],
  ]],
  ['Sinaloa', [
    ['Mazatlan', 23.22, -106.15, 'plains', 0, 0, { port: 1 }],
    ['Durango', 24.02, -104.67, 'hills', 0, 0, {}],
  ]],
  ['Mexico City', [
    ['Poza Rica', 20.53, -97.45, 'forest', 0, 0, {}],
    ['Morelia', 19.7, -101.19, 'hills', 0, 0, {}],
    ['Puebla', 19.04, -98.2, 'hills', 1, 1, {}],
  ]],
  ['Guadalajara', [
    ['Zacatecas', 22.77, -102.57, 'mountain', 0, 0, {}],
    ['Manzanillo', 19.05, -104.32, 'forest', 0, 0, { port: 1 }],
  ]],
  ['Oaxaca', [
    ['Acapulco', 16.86, -99.88, 'hills', 0, 0, { port: 1 }],
    ['Salina Cruz', 16.17, -95.2, 'hills', 0, 0, { port: 1 }],
  ]],
  ['Veracruz', [
    ['Coatzacoalcos', 18.15, -94.42, 'forest', 0, 0, { port: 1 }],
  ]],
  ['Chiapas', [
    ['Tapachula', 14.9, -92.26, 'forest', 0, 0, {}],
    ['Villahermosa', 17.99, -92.93, 'marsh', 0, 0, {}],
  ]],
  ['Yucatan', [
    ['Merida', 20.97, -89.62, 'plains', 1, 0, { port: 1 }],
    ['Campeche', 19.85, -90.53, 'forest', 0, 0, { port: 1 }],
  ]],
  ['Guatemala City', [
    ['Quetzaltenango', 14.83, -91.52, 'mountain', 0, 0, {}],
  ]],
  ['Peten', [
    ['Puerto Barrios', 15.73, -88.6, 'forest', 0, 0, { port: 1 }],
  ]],
  ['Tegucigalpa', [
    ['San Pedro Sula', 15.5, -88.03, 'forest', 0, 0, { port: 1 }],
  ]],
  ['Managua', [
    ['Corinto', 12.48, -87.17, 'plains', 0, 0, { port: 1 }],
  ]],
  ['Mosquito Coast', [
    ['Bluefields', 11.83, -83.84, 'forest', 0, 0, { port: 1 }],
  ]],
  ['San Jose', [
    ['Puerto Limon', 9.83, -82.96, 'forest', 0, 0, { port: 1 }],
  ]],
  ['Havana', [
    ['Pinar del Rio', 22.42, -83.7, 'hills', 0, 0, { port: 1 }],
  ]],
  ['Camaguey', [
    ['Santa Clara', 22.4, -79.97, 'plains', 0, 0, {}],
  ]],
  ['Port-au-Prince', [
    ['Cap-Haitien', 19.64, -72.01, 'hills', 0, 0, { port: 1 }],
  ]],
  ['Ciudad Trujillo', [
    ['Puerto Plata', 19.8, -70.69, 'hills', 0, 0, { port: 1 }],
  ]],
  // ---- South America ----
  ['Barranquilla', [
    ['Cartagena', 10.4, -75.5, 'plains', 1, 0, { port: 1 }],
  ]],
  ['Bogota', [
    ['Manizales', 5.07, -75.52, 'mountain', 0, 0, {}],
  ]],
  ['Cali', [
    ['Pasto', 1.21, -77.28, 'mountain', 0, 0, {}],
  ]],
  ['Caqueta', [
    ['Leticia', -4.2, -69.94, 'forest', 0, 0, {}],
  ]],
  ['Caracas', [
    ['Puerto Cabello', 10.47, -68.01, 'hills', 0, 0, { port: 1 }],
    ['Puerto La Cruz', 10.2, -64.63, 'plains', 0, 0, { port: 1 }],
  ]],
  ['Barquisimeto', [
    ['Coro', 11.4, -69.67, 'desert', 0, 0, { port: 1 }],
  ], { port: 0 }],
  ['Ciudad Bolivar', [
    ['Maturin', 9.75, -63.18, 'plains', 0, 0, {}],
  ]],
  ['Maracaibo', [
    ['San Cristobal', 7.77, -72.23, 'mountain', 0, 0, {}],
  ]],
  ['British Guiana', [
    ['Georgetown', 6.8, -58.16, 'plains', 1, 0, { port: 1 }],
  ]],
  ['Dutch Guiana', [
    ['Paramaribo', 5.85, -55.2, 'forest', 1, 0, { port: 1 }],
  ], { port: 0 }],
  ['French Guiana', [
    ['Cayenne', 4.93, -52.33, 'forest', 0, 0, { port: 1 }],
  ]],
  ['Guayaquil', [
    ['Cuenca', -2.9, -79, 'mountain', 0, 0, {}],
  ]],
  ['Quito', [
    ['Esmeraldas', 0.97, -79.65, 'forest', 0, 0, { port: 1 }],
  ]],
  ['Trujillo', [
    ['Chiclayo', -6.77, -79.84, 'desert', 0, 0, { port: 1 }],
    ['Chimbote', -9.08, -78.58, 'desert', 0, 0, { port: 1 }],
    ['Pucallpa', -8.38, -74.55, 'forest', 0, 0, {}],
  ]],
  ['Lima', [
    ['Huancayo', -12.07, -75.2, 'mountain', 0, 0, {}],
    ['Cerro de Pasco', -10.68, -76.26, 'mountain', 0, 0, {}],
  ]],
  ['Loreto', [
    ['Iquitos', -3.75, -73.25, 'forest', 0, 0, {}],
  ]],
  ['Arequipa', [
    ['Puno', -15.84, -70.02, 'mountain', 0, 0, {}],
    ['Tacna', -18, -70.25, 'desert', 0, 0, {}],
  ]],
  ['La Paz', [
    ['Oruro', -17.97, -67.11, 'mountain', 1, 0, {}],
    ['Cochabamba', -17.39, -66.16, 'mountain', 1, 0, {}],
  ]],
  ['Potosi', [
    ['Sucre', -19.03, -65.26, 'mountain', 0, 0, {}],
    ['Camiri', -20.04, -63.52, 'hills', 0, 0, {}],
  ]],
  ['Asuncion', [
    ['Encarnacion', -27.33, -55.87, 'plains', 0, 0, {}],
  ]],
  ['Tacuarembo', [
    ['Salto', -31.39, -57.96, 'plains', 0, 0, {}],
  ]],
  ['Montevideo', [
    ['Maldonado', -34.9, -54.95, 'plains', 0, 0, { port: 1 }],
  ]],
  ['Tarapaca', [
    ['Arica', -18.48, -70.32, 'desert', 0, 0, { port: 1 }],
  ]],
  ['Antofagasta', [
    ['Chuquicamata', -22.32, -68.93, 'desert', 1, 0, {}],
  ]],
  ['Atacama', [
    ['La Serena', -29.9, -71.25, 'hills', 0, 0, { port: 1 }],
  ]],
  ['Santiago', [
    ['Valparaiso', -33.05, -71.62, 'hills', 1, 0, { port: 1 }],
  ]],
  ['Concepcion', [
    ['Temuco', -38.74, -72.6, 'forest', 0, 0, {}],
  ]],
  ['Valdivia', [
    ['Puerto Montt', -41.47, -72.94, 'forest', 0, 0, { port: 1 }],
  ]],
  ['Magallanes', [
    ['Punta Arenas', -53.31, -71.05, 'hills', 1, 0, { port: 1 }],
  ]],
  ['Bahia Blanca', [
    ['Mar del Plata', -38, -57.55, 'plains', 0, 0, { port: 1 }],
    ['Tandil', -37.32, -59.13, 'hills', 0, 0, {}],
  ]],
  ['Cordoba', [
    ['Rio Cuarto', -33.13, -64.35, 'plains', 0, 0, {}],
    ['La Rioja', -29.41, -66.85, 'hills', 0, 0, {}],
  ]],
  ['Tucuman', [
    ['Santiago del Estero', -27.79, -64.26, 'plains', 0, 0, {}],
    ['Salta', -24.79, -65.41, 'mountain', 0, 0, {}],
  ]],
  ['Corrientes', [
    ['Paso de los Libres', -29.71, -57.09, 'plains', 0, 0, {}],
    ['Resistencia', -27.45, -58.99, 'marsh', 0, 0, {}],
  ]],
  ['Mendoza', [
    ['San Luis', -33.3, -66.34, 'hills', 0, 0, {}],
    ['San Rafael', -34.61, -68.33, 'hills', 0, 0, {}],
  ]],
  ['Neuquen', [
    ['Bariloche', -41.13, -71.3, 'mountain', 0, 0, {}],
  ]],
  ['Northern Patagonia', [
    ['Trelew', -43.25, -65.3, 'hills', 0, 0, { port: 1 }],
  ]],
  ['Southern Patagonia', [
    ['Rio Gallegos', -51.77, -69.08, 'hills', 0, 0, { port: 1 }],
  ]],
  ['Tierra del Fuego', [
    ['Ushuaia', -54.8, -68.3, 'mountain', 0, 0, { port: 1 }],
  ]],
  ['Sao Paulo', [
    ['Bauru', -22.32, -49.07, 'plains', 0, 0, {}],
    ['Ribeirao Preto', -21.18, -47.81, 'plains', 0, 0, {}],
  ]],
  ['Rio de Janeiro', [
    ['Campos', -21.75, -41.33, 'plains', 0, 0, { port: 1 }],
    ['Juiz de Fora', -21.76, -43.35, 'hills', 0, 1, {}],
  ]],
  ['Minas Gerais', [
    ['Vitoria', -20.32, -40.34, 'hills', 0, 0, { port: 1 }],
    ['Uberaba', -19.75, -47.93, 'plains', 0, 0, {}],
    ['Montes Claros', -16.73, -43.86, 'hills', 0, 0, {}],
  ]],
  ['Bahia', [
    ['Salvador', -12.8, -38.5, 'hills', 1, 0, { port: 1 }],
    ['Ilheus', -14.79, -39.05, 'forest', 0, 0, { port: 1 }],
  ], { vp: 0, port: 0 }],
  ['Sertao', [
    ['Juazeiro', -9.42, -40.5, 'plains', 0, 0, {}],
  ]],
  ['Pernambuco', [
    ['Aracaju', -10.91, -37.07, 'plains', 0, 0, { port: 1 }],
    ['Maceio', -9.65, -35.73, 'plains', 0, 0, { port: 1 }],
  ]],
  ['Natal', [
    ['Joao Pessoa', -7.12, -34.88, 'plains', 0, 0, { port: 1 }],
  ]],
  ['Ceara', [
    ['Fortaleza', -3.73, -38.52, 'plains', 1, 0, { port: 1 }],
  ]],
  ['Maranhao', [
    ['Sao Luis', -2.53, -44.3, 'forest', 0, 0, { port: 1 }],
    ['Teresina', -5.09, -42.8, 'plains', 0, 0, {}],
  ], { port: 0 }],
  ['Para', [
    ['Belem', -1.34, -48.31, 'forest', 1, 0, { port: 1 }],
    ['Santarem', -2.44, -54.71, 'forest', 0, 0, {}],
  ], { vp: 0 }],
  ['Amapa', [
    ['Macapa', 0.03, -51.07, 'forest', 0, 0, { port: 1 }],
  ]],
  ['Amazonas', [
    ['Manaus', -3.1, -60.02, 'forest', 1, 0, {}],
    ['Tefe', -3.37, -64.72, 'forest', 0, 0, {}],
  ]],
  ['Guapore', [
    ['Porto Velho', -8.76, -63.9, 'forest', 0, 0, {}],
  ]],
  ['Acre', [
    ['Rio Branco', -9.97, -67.81, 'forest', 0, 0, {}],
  ]],
  ['Mato Grosso', [
    ['Cuiaba', -15.6, -56.1, 'plains', 0, 0, {}],
  ]],
  ['Goias', [
    ['Goiania', -16.68, -49.25, 'plains', 0, 0, {}],
  ]],
  ['Campo Grande', [
    ['Corumba', -19, -57.65, 'marsh', 0, 0, {}],
  ]],
  ['Parana', [
    ['Curitiba', -25.43, -49.27, 'hills', 1, 0, { port: 1 }],
    ['Florianopolis', -27.6, -48.43, 'hills', 0, 0, { port: 1 }],
  ], { port: 0 }],
  ['Rio Grande do Sul', [
    ['Pelotas', -31.77, -52.34, 'plains', 0, 0, { port: 1 }],
  ]],
  // ---- China, Manchuria, Korea and Mongolia ----
  ['Tientsin', [
    ['Tangshan', 39.63, 118.18, 'plains', 0, 0, {}],
  ]],
  ['Shihkiachwang', [
    ['Paoting', 38.87, 115.47, 'plains', 0, 0, {}],
  ]],
  ['Tsingtao', [
    ['Chefoo', 37.53, 121.4, 'hills', 0, 0, { port: 1 }],
  ]],
  ['Hsuchow', [
    ['Haichow', 34.6, 119.2, 'plains', 0, 0, { port: 1 }],
  ]],
  ['Sian', [
    ['Paoki', 34.37, 107.15, 'hills', 0, 0, {}],
  ]],
  ['Hanchung', [
    ['Tienshui', 34.58, 105.72, 'hills', 0, 0, {}],
  ]],
  ['Yenan', [
    ['Yulin', 38.29, 109.74, 'hills', 0, 0, {}],
  ]],
  ['Suiyuan', [
    ['Paotow', 40.65, 109.85, 'plains', 0, 0, {}],
    ['Wuyuan', 41.09, 108.27, 'plains', 0, 0, {}],
  ]],
  ['Tihwa', [
    ['Turfan', 42.95, 89.18, 'plains', 0, 0, {}],
  ]],
  ['Kuldja', [
    ['Aksu', 41.17, 80.26, 'plains', 0, 0, {}],
    ['Chuguchak', 46.75, 82.98, 'hills', 0, 0, {}],
  ]],
  ['Kashgar', [
    ['Yarkand', 38.42, 77.24, 'plains', 0, 0, {}],
  ]],
  ['Yumen', [
    ['Anhsi', 40.53, 95.8, 'plains', 0, 0, {}],
    ['Changyeh', 38.93, 100.45, 'plains', 0, 0, {}],
  ]],
  ['Sining', [
    ['Liangchow', 37.93, 102.64, 'plains', 0, 0, {}],
  ]],
  ['Lhasa', [
    ['Shigatse', 29.27, 88.88, 'mountain', 0, 0, {}],
  ]],
  ['Sikang', [
    ['Sichang', 27.9, 102.26, 'mountain', 0, 0, {}],
  ]],
  ['Chengtu', [
    ['Kiating', 29.57, 103.76, 'hills', 0, 0, {}],
  ]],
  ['Chungking', [
    ['Suifu', 28.77, 104.63, 'hills', 0, 0, {}],
    ['Wanhsien', 30.82, 108.4, 'hills', 0, 0, {}],
  ]],
  ['Kweiyang', [
    ['Tsunyi', 27.7, 106.92, 'hills', 0, 0, {}],
    ['Tushan', 25.83, 107.54, 'hills', 0, 0, {}],
  ]],
  ['Kunming', [
    ['Mengtsz', 23.37, 103.38, 'hills', 0, 0, {}],
  ]],
  ['Paoshan', [
    ['Tali', 25.6, 100.27, 'mountain', 0, 0, {}],
  ]],
  ['Kweilin', [
    ['Liuchow', 24.32, 109.4, 'hills', 1, 0, {}],
  ]],
  ['Nanning', [
    ['Lungchow', 22.34, 106.85, 'hills', 0, 0, {}],
  ]],
  ['Canton', [
    ['Waichow', 23.1, 114.4, 'hills', 0, 0, {}],
  ]],
  ['Hengyang', [
    ['Lingling', 26.22, 111.62, 'hills', 0, 0, {}],
  ]],
  ['Changsha', [
    ['Changteh', 29.03, 111.68, 'plains', 0, 0, {}],
  ]],
  ['Siangyang', [
    ['Sinyang', 32.13, 114.07, 'hills', 0, 0, {}],
  ]],
  ['Anking', [
    ['Kiukiang', 29.72, 116, 'plains', 0, 0, {}],
  ]],
  ['Nanking', [
    ['Wuhu', 31.35, 118.37, 'plains', 0, 0, {}],
  ]],
  ['Chuchow', [
    ['Wenchow', 28, 120.67, 'hills', 0, 0, { port: 1 }],
  ]],
  ['Foochow', [
    ['Nanping', 26.64, 118.17, 'mountain', 0, 0, {}],
  ]],
  ['Jehol', [
    ['Chihfeng', 42.27, 118.95, 'hills', 0, 0, {}],
  ]],
  ['Hsinking', [
    ['Kirin', 43.85, 126.55, 'hills', 1, 0, {}],
    ['Szepingkai', 43.17, 124.37, 'plains', 0, 0, {}],
  ]],
  ['Tungliao', [
    ['Taonan', 45.33, 122.78, 'plains', 0, 0, {}],
  ]],
  ['Hailar', [
    ['Manchouli', 49.6, 117.43, 'plains', 0, 0, {}],
    ['Arshan', 47.17, 119.95, 'mountain', 0, 0, {}],
  ]],
  ['Heiho', [
    ['Peian', 48.25, 126.6, 'plains', 0, 0, {}],
  ]],
  ['Kiamusze', [
    ['Fuchin', 47.25, 132.03, 'marsh', 0, 0, {}],
    ['Hulin', 45.77, 132.95, 'marsh', 0, 0, {}],
  ]],
  ['Mutankiang', [
    ['Suifenho', 44.4, 131.15, 'forest', 0, 0, {}],
    ['Yenki', 42.9, 129.5, 'mountain', 0, 0, {}],
  ]],
  ['Pyongyang', [
    ['Sinuiju', 40.1, 124.4, 'plains', 0, 0, {}],
  ]],
  ['Uliassutai', [
    ['Tsetserleg', 47.47, 101.45, 'mountain', 0, 0, {}],
  ]],
  ['Choibalsan', [
    ['Tamsag Bulak', 47.25, 117.3, 'plains', 0, 0, {}],
  ]],
  // ---- Soviet Asia ----
  ['Molotov', [
    ['Berezniki', 59.4, 56.8, 'forest', 0, 1, {}],
  ]],
  ['Sverdlovsk', [
    ['Kamensk-Uralsky', 56.42, 61.93, 'plains', 0, 1, {}],
  ]],
  ['Chelyabinsk', [
    ['Zlatoust', 55.17, 59.67, 'mountain', 0, 1, {}],
  ]],
  ['Aktyubinsk', [
    ['Orsk', 51.23, 58.47, 'plains', 0, 1, {}],
  ]],
  ['Ufa', [
    ['Sterlitamak', 53.63, 55.95, 'plains', 0, 0, {}],
  ]],
  ['Syktyvkar', [
    ['Ukhta', 63.57, 53.7, 'forest', 0, 0, {}],
  ]],
  ['Tobolsk', [
    ['Ishim', 56.11, 69.48, 'plains', 0, 0, {}],
  ]],
  ['Omsk', [
    ['Tatarsk', 55.22, 75.97, 'plains', 0, 0, {}],
  ]],
  ['Tomsk', [
    ['Kemerovo', 55.35, 86.09, 'hills', 0, 1, {}],
  ]],
  ['Barnaul', [
    ['Biysk', 52.54, 85.21, 'hills', 0, 0, {}],
  ]],
  ['Semipalatinsk', [
    ['Rubtsovsk', 51.5, 81.2, 'plains', 0, 1, {}],
    ['Ust-Kamenogorsk', 49.95, 82.62, 'hills', 0, 1, {}],
  ]],
  ['Krasnoyarsk', [
    ['Achinsk', 56.27, 90.5, 'forest', 0, 0, {}],
    ['Kansk', 56.2, 95.7, 'forest', 0, 0, {}],
  ]],
  ['Nizhneudinsk', [
    ['Taishet', 55.94, 98, 'forest', 0, 0, {}],
  ]],
  ['Irkutsk', [
    ['Cheremkhovo', 53.15, 103.07, 'forest', 0, 0, {}],
  ]],
  ['Kirensk', [
    ['Ust-Kut', 56.8, 105.8, 'forest', 0, 0, {}],
  ]],
  ['Turukhansk', [
    ['Igarka', 67.47, 86.57, 'forest', 0, 0, {}],
  ]],
  ['Ulan Bator', [
    ['Altan Bulak', 50.3, 106.5, 'hills', 0, 0, {}],
  ]],
  ['Dauria', [
    ['Nerchinsk', 51.98, 116.58, 'hills', 0, 0, {}],
  ]],
  ['Blagoveshchensk', [
    ['Svobodny', 51.38, 128.13, 'forest', 0, 0, {}],
  ]],
  ['Khabarovsk', [
    ['Birobidzhan', 48.8, 132.93, 'forest', 0, 0, {}],
  ]],
  ['Vladivostok', [
    ['Spassk', 44.6, 132.82, 'plains', 0, 0, {}],
  ]],
  ['Magadan', [
    ['Seymchan', 62.93, 152.42, 'mountain', 0, 0, {}],
  ]],
  ['Koryak', [
    ['Markovo', 64.68, 170.42, 'marsh', 0, 0, {}],
  ]],
  ['Akmolinsk', [
    ['Kokchetav', 53.28, 69.38, 'plains', 0, 0, {}],
  ]],
  ['Tashkent', [
    ['Chimkent', 42.3, 69.6, 'plains', 0, 1, {}],
    ['Leninabad', 40.28, 69.63, 'plains', 0, 0, {}],
  ]],
  ['Alma-Ata', [
    ['Taldy-Kurgan', 45.02, 78.37, 'plains', 0, 0, {}],
    ['Przhevalsk', 42.48, 78.4, 'mountain', 0, 0, {}],
  ]],
  ['Bukhara', [
    ['Chardzhou', 39.1, 63.57, 'plains', 0, 0, {}],
  ]],
  ['Khiva', [
    ['Nukus', 42.47, 59.6, 'plains', 0, 0, {}],
  ]],
  ['Ashkhabad', [
    ['Kizyl-Arvat', 38.98, 56.28, 'plains', 0, 0, {}],
  ]],
  ['Merv', [
    ['Kushka', 35.28, 62.33, 'hills', 0, 0, {}],
  ]],
  // ---- India, Afghanistan and Burma ----
  ['Lahore', [
    ['Sialkot', 32.5, 74.53, 'plains', 0, 0, {}],
    ['Jullundur', 31.33, 75.58, 'plains', 0, 0, {}],
  ]],
  ['Simla', [
    ['Ambala', 30.38, 76.78, 'plains', 0, 0, {}],
    ['Dehra Dun', 30.32, 78.03, 'hills', 0, 0, {}],
  ]],
  ['Multan', [
    ['Bahawalpur', 29.4, 71.68, 'plains', 0, 0, {}],
    ['Dera Ismail Khan', 31.83, 70.9, 'plains', 0, 0, {}],
  ]],
  ['Peshawar', [
    ['Razmak', 32.7, 69.9, 'mountain', 0, 0, {}],
  ]],
  ['Kalat', [
    ['Pasni', 25.26, 63.47, 'hills', 0, 0, { port: 1 }],
  ]],
  ['Karachi', [
    ['Mirpur Khas', 25.53, 69.01, 'plains', 0, 0, {}],
  ]],
  ['Kabul', [
    ['Jalalabad', 34.43, 70.45, 'mountain', 0, 0, {}],
    ['Ghazni', 33.55, 68.42, 'mountain', 0, 0, {}],
  ]],
  ['Jaipur', [
    ['Ajmer', 26.45, 74.64, 'hills', 0, 0, {}],
  ]],
  ['Ahmedabad', [
    ['Udaipur', 24.58, 73.68, 'hills', 0, 0, {}],
    ['Baroda', 22.3, 73.2, 'plains', 0, 0, {}],
    ['Surat', 21.17, 72.83, 'plains', 0, 0, { port: 1 }],
  ]],
  ['Bhopal', [
    ['Indore', 22.72, 75.86, 'plains', 0, 0, {}],
  ]],
  ['Jhansi', [
    ['Gwalior', 26.22, 78.18, 'plains', 0, 0, {}],
  ]],
  ['Nagpur', [
    ['Amraoti', 20.93, 77.75, 'plains', 0, 0, {}],
    ['Bhusawal', 21.05, 75.78, 'plains', 0, 0, {}],
  ]],
  ['Raipur', [
    ['Bilaspur', 22.08, 82.15, 'plains', 0, 0, {}],
    ['Sambalpur', 21.47, 83.97, 'hills', 0, 0, {}],
  ]],
  ['Delhi', [
    ['Bareilly', 28.37, 79.43, 'plains', 0, 0, {}],
  ]],
  ['Lucknow', [
    ['Cawnpore', 26.45, 80.33, 'plains', 1, 1, {}],
  ]],
  ['Benares', [
    ['Allahabad', 25.45, 81.85, 'plains', 0, 0, {}],
    ['Gorakhpur', 26.76, 83.37, 'plains', 0, 0, {}],
  ]],
  ['Patna', [
    ['Bhagalpur', 25.25, 87, 'plains', 0, 0, {}],
  ]],
  ['Jamshedpur', [
    ['Asansol', 23.68, 86.98, 'plains', 0, 1, {}],
    ['Ranchi', 23.35, 85.33, 'hills', 0, 0, {}],
  ]],
  ['Calcutta', [
    ['Kharagpur', 22.33, 87.32, 'plains', 0, 0, {}],
  ]],
  ['Dacca', [
    ['Comilla', 23.46, 91.18, 'plains', 0, 0, {}],
  ]],
  ['Gauhati', [
    ['Sylhet', 24.9, 91.87, 'plains', 0, 0, {}],
    ['Tezpur', 26.63, 92.8, 'plains', 0, 0, {}],
  ]],
  ['Dibrugarh', [
    ['Jorhat', 26.75, 94.2, 'plains', 0, 0, {}],
  ]],
  ['Bombay', [
    ['Nasik', 19.95, 73.83, 'hills', 0, 0, {}],
  ]],
  ['Poona', [
    ['Ahmednagar', 19.08, 74.73, 'hills', 0, 0, {}],
    ['Sholapur', 17.67, 75.9, 'plains', 0, 0, {}],
  ]],
  ['Hyderabad', [
    ['Warangal', 17.98, 79.6, 'plains', 0, 0, {}],
  ]],
  ['Bellary', [
    ['Belgaum', 15.85, 74.5, 'hills', 0, 0, {}],
  ]],
  ['Vizagapatam', [
    ['Cocanada', 16.95, 82.25, 'plains', 0, 0, { port: 1 }],
  ]],
  ['Bangalore', [
    ['Mysore', 12.3, 76.65, 'hills', 0, 0, {}],
  ]],
  ['Madura', [
    ['Trichinopoly', 10.8, 78.7, 'plains', 0, 0, {}],
  ]],
  ['Yenangyaung', [
    ['Meiktila', 20.88, 95.86, 'plains', 0, 0, {}],
  ]],
  ['Myitkyina', [
    ['Bhamo', 24.25, 97.23, 'hills', 0, 0, {}],
    ['Fort Hertz', 27.33, 97.42, 'mountain', 0, 0, {}],
  ]],
  ['Mandalay', [
    ['Taunggyi', 20.78, 97.03, 'hills', 0, 0, {}],
  ]],
  // ---- Persia and Arabia ----
  ['Resht', [
    ['Qazvin', 36.27, 50, 'plains', 0, 0, {}],
  ]],
  ['Tehran', [
    ['Qom', 34.64, 50.88, 'plains', 0, 0, {}],
  ]],
  ['Hamadan', [
    ['Arak', 34.09, 49.69, 'hills', 0, 0, {}],
  ]],
  ['Ahwaz', [
    ['Andimeshk', 32.46, 48.35, 'plains', 0, 0, {}],
  ]],
  ['Bandar Abbas', [
    ['Lingeh', 26.56, 54.88, 'hills', 0, 0, { port: 1 }],
  ]],
  ['Meshed', [
    ['Sabzawar', 36.21, 57.68, 'plains', 0, 0, {}],
  ]],
  ['Hail', [
    ['Buraida', 26.33, 43.97, 'plains', 0, 0, {}],
  ]],
  ['Medina', [
    ['Yanbu', 24.09, 38.06, 'plains', 0, 0, { port: 1 }],
  ]],
  ['Mukalla', [
    ['Seiyun', 15.94, 48.79, 'hills', 0, 0, {}],
  ]],
  // ---- Australia and New Zealand ----
  ['Darwin', [
    ['Katherine', -14.47, 132.27, 'plains', 0, 0, {}],
  ]],
  ['Wyndham', [
    ['Daly Waters', -16.25, 133.37, 'plains', 0, 0, {}],
    ['Halls Creek', -18.23, 127.67, 'plains', 0, 0, {}],
  ]],
  ['Broome', [
    ['Derby', -17.3, 123.63, 'plains', 0, 0, { port: 1 }],
  ]],
  ['Port Hedland', [
    ['Marble Bar', -21.17, 119.75, 'hills', 0, 0, {}],
  ]],
  ['Carnarvon', [
    ['Exmouth Gulf', -22, 114.1, 'plains', 0, 0, { port: 1 }],
    ['Meekatharra', -26.6, 118.5, 'plains', 0, 0, {}],
  ]],
  ['Perth', [
    ['Geraldton', -28.77, 114.6, 'plains', 0, 0, { port: 1 }],
    ['Merredin', -31.48, 118.28, 'plains', 0, 0, {}],
    ['Bunbury', -33.33, 115.63, 'plains', 0, 0, { port: 1 }],
  ]],
  ['Kalgoorlie', [
    ['Esperance', -33.86, 122.16, 'plains', 0, 0, { port: 1 }],
  ]],
  ['Adelaide', [
    ['Port Lincoln', -34.73, 135.86, 'plains', 0, 0, { port: 1 }],
    ['Port Pirie', -33.18, 138, 'plains', 0, 0, { port: 1 }],
    ['Mildura', -34.2, 142.15, 'plains', 0, 0, {}],
  ]],
  ['Oodnadatta', [
    ['Marree', -29.65, 138.07, 'plains', 0, 0, {}],
  ]],
  ['Melbourne', [
    ['Mount Gambier', -37.83, 140.78, 'plains', 0, 0, {}],
    ['Horsham', -36.71, 142.2, 'plains', 0, 0, {}],
    ['Ballarat', -37.56, 143.86, 'hills', 0, 0, {}],
    ['Bendigo', -36.76, 144.28, 'hills', 0, 0, {}],
    ['Sale', -38.1, 147.07, 'plains', 0, 0, {}],
  ]],
  ['Canberra', [
    ['Albury', -36.08, 146.92, 'plains', 0, 0, {}],
    ['Wagga Wagga', -35.12, 147.37, 'plains', 0, 0, {}],
  ]],
  ['Sydney', [
    ['Wollongong', -34.42, 150.9, 'hills', 0, 1, { port: 1 }],
    ['Newcastle NSW', -32.93, 151.78, 'plains', 1, 1, { port: 1 }],
    ['Dubbo', -32.25, 148.6, 'plains', 0, 0, {}],
    ['Tamworth', -31.09, 150.93, 'hills', 0, 0, {}],
  ]],
  ['Broken Hill', [
    ['Bourke', -30.09, 145.94, 'plains', 0, 0, {}],
  ]],
  ['Brisbane', [
    ['Grafton', -29.69, 152.93, 'hills', 0, 0, {}],
    ['Toowoomba', -27.56, 151.95, 'hills', 0, 0, {}],
    ['Maryborough', -25.54, 152.7, 'plains', 0, 0, { port: 1 }],
  ]],
  ['Charleville', [
    ['Roma', -26.57, 148.79, 'plains', 0, 0, {}],
    ['Longreach', -23.44, 144.25, 'plains', 0, 0, {}],
  ]],
  ['Rockhampton', [
    ['Mackay', -21.14, 149.19, 'plains', 0, 0, { port: 1 }],
  ]],
  ['Townsville', [
    ['Charters Towers', -20.08, 146.26, 'hills', 0, 0, {}],
    ['Hughenden', -20.85, 144.2, 'plains', 0, 0, {}],
    ['Cairns', -16.92, 145.77, 'hills', 0, 0, { port: 1 }],
  ]],
  ['Cloncurry', [
    ['Normanton', -17.67, 141.07, 'plains', 0, 0, { port: 1 }],
  ]],
  ['Cape York', [
    ['Cooktown', -15.47, 145.25, 'hills', 0, 0, { port: 1 }],
  ]],
  ['Hobart', [
    ['Launceston', -41.44, 147.14, 'hills', 0, 0, { port: 1 }],
  ]],
  ['Wellington', [
    ['Napier', -39.49, 176.92, 'hills', 0, 0, { port: 1 }],
  ]],
  ['Christchurch', [
    ['Nelson', -41.27, 173.28, 'hills', 0, 0, { port: 1 }],
  ]],
  ['Dunedin', [
    ['Invercargill', -46.41, 168.35, 'plains', 0, 0, { port: 1 }],
  ]],
  // ---- Africa ----
  ['Leopoldville', [
    ['Kikwit', -5.04, 18.82, 'forest', 0, 0, {}],
  ]],
  ['Luluabourg', [
    ['Port-Francqui', -4.33, 20.58, 'forest', 0, 0, {}],
  ]],
  ['Elisabethville', [
    ['Kolwezi', -10.72, 25.47, 'hills', 0, 0, {}],
  ]],
  ['Costermansville', [
    ['Kindu', -2.95, 25.92, 'forest', 0, 0, {}],
  ]],
  ['Buta', [
    ['Lisala', 2.15, 21.52, 'forest', 0, 0, {}],
  ]],
  ['Bunia', [
    ['Paulis', 2.77, 27.62, 'forest', 0, 0, {}],
    ['Watsa', 3.04, 29.53, 'hills', 0, 0, {}],
  ]],
  ['Atar', [
    ['Fort Gouraud', 22.68, -12.71, 'desert', 0, 0, {}],
  ]],
  ['Tambacounda', [
    ['Ziguinchor', 12.58, -16.27, 'forest', 0, 0, {}],
  ]],
  ['Bamako', [
    ['Segou', 13.45, -6.27, 'plains', 0, 0, {}],
    ['Sikasso', 11.32, -5.67, 'plains', 0, 0, {}],
  ]],
  ['Conakry', [
    ['Labe', 11.32, -12.28, 'hills', 0, 0, {}],
  ]],
  ['Bouake', [
    ['Man', 7.4, -7.55, 'forest', 0, 0, {}],
    ['Korhogo', 9.45, -5.63, 'plains', 0, 0, {}],
  ]],
  ['Abidjan', [
    ['Sassandra', 4.95, -6.08, 'forest', 0, 0, { port: 1 }],
  ]],
  ['Niamey', [
    ['Tahoua', 14.89, 5.26, 'plains', 0, 0, {}],
  ]],
  ['Cotonou', [
    ['Parakou', 9.35, 2.62, 'plains', 0, 0, {}],
  ]],
  ['Fort-Lamy', [
    ['Mao', 14.12, 15.31, 'plains', 0, 0, {}],
  ]],
  ['Abeche', [
    ['Ati', 13.21, 18.34, 'plains', 0, 0, {}],
  ]],
  ['Fort-Archambault', [
    ['Moundou', 8.57, 16.08, 'plains', 0, 0, {}],
  ]],
  ['Garoua', [
    ['Maroua', 10.6, 14.32, 'plains', 0, 0, {}],
    ['Ngaoundere', 7.32, 13.58, 'hills', 0, 0, {}],
  ]],
  ['Douala', [
    ['Kribi', 2.95, 9.91, 'forest', 0, 0, { port: 1 }],
  ]],
  ['Bangui', [
    ['Bouar', 5.93, 15.6, 'hills', 0, 0, {}],
    ['Bangassou', 4.74, 22.82, 'forest', 0, 0, {}],
  ]],
  ['Belet Uen', [
    ['Dolo', 4.17, 42.07, 'plains', 0, 0, {}],
  ]],
  ['Monrovia', [
    ['Harper', 4.38, -7.53, 'forest', 0, 0, { port: 1 }],
  ]],
  ['Luso', [
    ['Henrique de Carvalho', -9.66, 20.39, 'plains', 0, 0, {}],
  ]],
  ['Nampula', [
    ['Mocambique', -15.15, 40.54, 'plains', 0, 0, { port: 1 }],
  ]],
  ['Pretoria', [
    ['Nelspruit', -25.47, 30.97, 'hills', 0, 0, {}],
  ]],
  ['Klerksdorp', [
    ['Mafeking', -25.85, 25.64, 'plains', 0, 0, {}],
  ]],
  ['Beaufort West', [
    ['De Aar', -30.65, 24.01, 'plains', 0, 0, {}],
    ['Mossel Bay', -34.23, 21.87, 'hills', 0, 0, { port: 1 }],
  ]],
  ['Cape Town', [
    ['Saldanha', -33, 17.95, 'plains', 0, 0, { port: 1 }],
  ]],
  ['Bahariya', [
    ['Farafra', 27.06, 27.97, 'desert', 0, 0, {}],
  ]],
  ['Kassala', [
    ['Gedaref', 14.03, 35.38, 'plains', 0, 0, {}],
  ]],
  ['Wad Medani', [
    ['Kosti', 13.17, 32.67, 'plains', 0, 0, {}],
  ]],
  ['El Fasher', [
    ['Geneina', 13.45, 22.45, 'plains', 0, 0, {}],
  ]],
  ['Kano', [
    ['Sokoto', 13.06, 5.24, 'plains', 0, 0, {}],
  ]],
  ['Kaduna', [
    ['Jos', 9.92, 8.89, 'hills', 0, 0, {}],
  ]],
  ['Enugu', [
    ['Makurdi', 7.73, 8.54, 'plains', 0, 0, {}],
    ['Benin City', 6.34, 5.63, 'forest', 0, 0, {}],
  ]],
  ['Accra', [
    ['Kumasi', 6.69, -1.62, 'forest', 0, 0, {}],
  ]],
  ['Nairobi', [
    ['Nanyuki', 0.02, 37.07, 'hills', 0, 0, {}],
  ]],
  ['Neghelli', [
    ['Moyale', 3.52, 39.05, 'hills', 0, 0, {}],
  ]],
  ['Tabora', [
    ['Kigoma', -4.88, 29.63, 'hills', 0, 0, {}],
  ]],
  ['Mbeya', [
    ['Iringa', -7.77, 35.69, 'hills', 0, 0, {}],
  ]],
  ['Kasama', [
    ['Abercorn', -8.83, 31.37, 'hills', 0, 0, {}],
  ]],
  ['Lusaka', [
    ['Livingstone', -17.85, 25.86, 'plains', 0, 0, {}],
  ]],
  ['Salisbury', [
    ['Umtali', -18.97, 32.67, 'hills', 0, 0, {}],
  ]],
  ['Bulawayo', [
    ['Gwelo', -19.45, 29.82, 'plains', 0, 0, {}],
  ]],
  // ---- Alaska ----
  ['Fairbanks', [
    ['Big Delta', 64.15, -145.84, 'forest', 0, 0, {}],
  ]],
  ['Yukon Flats', [
    ['Galena', 64.74, -156.93, 'forest', 0, 0, {}],
  ]],
  ['Nome', [
    ['Kotzebue', 66.9, -162.33, 'marsh', 0, 0, { port: 1 }],
  ]],
  ['Kuskokwim', [
    ['McGrath', 62.95, -155.6, 'forest', 0, 0, {}],
  ]],
  ['Anchorage', [
    ['Seward', 60.1, -149.44, 'mountain', 0, 0, { port: 1 }],
  ]],
  ['Copper River', [
    ['Cordova', 60.54, -145.76, 'mountain', 0, 0, { port: 1 }],
  ]],
  ['Bristol Bay', [
    ['Cold Bay', 55.2, -162.72, 'hills', 0, 0, { port: 1 }],
  ]],
  // ---- Part 4: China's war belt, Manchuria and Korea (the widest settled provinces cut to Europe's measure) ----
  ['Ichang', [
    ['Shasi', 30.31, 112.25, 'plains', 0, 0, {}],
    ['Enshih', 30.3, 109.48, 'mountain', 0, 0, {}],
  ]],
  ['Hankow', [
    ['Macheng', 31.18, 115.03, 'hills', 0, 0, {}],
  ]],
  ['Nanking', [
    ['Yencheng', 33.39, 120.12, 'plains', 0, 0, {}],
  ]],
  ['Nanchang', [
    ['Linchwan', 27.97, 116.36, 'plains', 0, 0, {}], // Fuchow's county town: the historian's form, beside Foochow
  ]],
  ['Pengpu', [
    ['Fowyang', 32.9, 115.82, 'plains', 0, 0, {}],
  ]],
  ['Chungking', [
    ['Pengshui', 29.29, 108.17, 'mountain', 0, 0, {}],
  ]],
  ['Chengtu', [
    ['Mienyang', 31.47, 104.75, 'hills', 0, 0, {}],
    ['Sungpan', 32.64, 103.6, 'mountain', 0, 0, {}],
  ]],
  ['Tsinan', [
    ['Lintsing', 36.85, 115.7, 'plains', 0, 0, {}],
    ['Sintai', 35.91, 117.77, 'hills', 0, 0, {}],
  ]],
  ['Kaifeng', [
    ['Anyang', 36.1, 114.35, 'plains', 0, 0, {}],
  ]],
  ['Nanyang', [
    ['Chumatien', 32.98, 114.02, 'plains', 0, 0, {}],
  ]],
  ['Taiyuan', [
    ['Lishih', 37.52, 111.13, 'hills', 0, 0, {}],
  ]],
  ['Canton', [
    ['Wuchow', 23.48, 111.31, 'hills', 0, 0, {}],
  ]],
  ['Chuchow', [
    ['Shangjao', 28.46, 117.97, 'hills', 0, 0, {}],
  ]],
  ['Kweilin', [
    ['Chuanhsien', 25.93, 111.07, 'hills', 0, 0, {}],
  ]],
  ['Kanchow', [
    ['Ningtu', 26.47, 116.02, 'hills', 0, 0, {}],
  ]],
  ['Nanning', [
    ['Kweihsien', 23.1, 109.6, 'plains', 0, 0, {}],
    ['Poseh', 23.9, 106.61, 'hills', 0, 0, {}],
  ]],
  ['Kweiyang', [
    ['Pichieh', 27.3, 105.29, 'mountain', 0, 0, {}],
  ]],
  ['Sian', [
    ['Tungkwan', 34.61, 110.28, 'hills', 0, 0, {}],
  ]],
  ['Yenan', [
    ['Kingyang', 36.01, 107.88, 'hills', 0, 0, {}],
  ]],
  ['Hanchung', [
    ['Kwangyuan', 32.44, 105.84, 'mountain', 0, 0, {}],
    ['Ankang', 32.68, 109.02, 'mountain', 0, 0, {}],
  ]],
  ['Harbin', [
    ['Suihwa', 46.63, 126.98, 'plains', 0, 0, {}],
    ['Anta', 46.4, 125.32, 'plains', 0, 0, {}],
  ]],
  ['Tsitsihar', [
    ['Koshan', 48.03, 125.87, 'plains', 0, 0, {}],
    ['Chalantun', 48, 122.72, 'forest', 0, 0, {}],
    ['Wangyehmiao', 46.08, 122.08, 'hills', 0, 0, {}],
  ]],
  ['Tunghwa', [
    ['Fusung', 42.33, 127.28, 'forest', 0, 0, {}],
  ]],
  ['Kiamusze', [
    ['Sansing', 46.32, 129.57, 'forest', 0, 0, {}],
  ]],
  ['Tungliao', [
    ['Lintung', 43.97, 119.38, 'plains', 0, 0, {}],
  ]],
  ['Seoul', [
    ['Kangnung', 37.75, 128.9, 'mountain', 0, 0, {}],
  ]],
  ['Hamhung', [
    ['Kanggye', 40.97, 126.6, 'mountain', 0, 0, {}],
  ]],
  ['Kalgan', [
    ['Shangtu', 41.55, 113.53, 'plains', 0, 0, {}],
  ]],
  ['Lanchow', [
    ['Pingliang', 35.54, 106.68, 'hills', 0, 0, {}],
    ['Minhsien', 34.44, 104.04, 'mountain', 0, 0, {}],
  ]],
  ['Kunming', [
    ['Kutsing', 25.49, 103.8, 'hills', 0, 0, {}],
    ['Hweitseh', 26.41, 103.3, 'mountain', 0, 0, {}],
    ['Szemao', 22.78, 100.97, 'forest', 0, 0, {}],
  ]],
  ['Paoshan', [
    ['Likiang', 26.87, 100.23, 'mountain', 0, 0, {}],
    ['Mienning', 23.88, 100.09, 'mountain', 0, 0, {}],
  ]],
  // ---- Part 4: Asia outside China (Burma, India, Siam, Indochina, the East Indies, Persia, Afghanistan, Arabia, Soviet Asia) ----
  ['Lashio', [
    ['Kengtung', 21.29, 99.61, 'mountain', 0, 0, {}],
  ]],
  ['Kalewa', [
    ['Tamu', 24.22, 94.31, 'forest', 0, 0, {}],
  ]],
  ['Toungoo', [
    ['Loikaw', 19.67, 97.21, 'hills', 0, 0, {}],
  ]],
  ['Dibrugarh', [
    ['Ledo', 27.29, 95.73, 'forest', 0, 0, {}],
  ]],
  ['Myitkyina', [
    ['Maingkwan', 26.33, 96.6, 'forest', 0, 0, {}],
  ]],
  ['Dacca', [
    ['Rajshahi', 24.37, 88.6, 'plains', 0, 0, {}],
  ]],
  ['Darjeeling', [
    ['Dinajpur', 25.63, 88.64, 'plains', 0, 0, {}],
  ]],
  ['Bangkok', [
    ['Kanchanaburi', 14.02, 99.53, 'hills', 0, 0, {}],
  ]],
  ['Korat', [
    ['Ubon', 15.23, 104.86, 'plains', 0, 0, {}],
    ['Surin', 14.88, 103.49, 'plains', 0, 0, {}],
  ]],
  ['Udon', [
    ['Sakon Nakhon', 17.16, 104.15, 'plains', 0, 0, {}],
  ]],
  ['Phitsanulok', [
    ['Paknampo', 15.7, 100.13, 'plains', 0, 0, {}],
    ['Raheng', 16.88, 99.13, 'hills', 0, 0, {}],
  ]],
  ['Chiang Mai', [
    ['Chiang Rai', 19.91, 99.83, 'hills', 0, 0, {}],
    ['Phrae', 18.15, 100.14, 'hills', 0, 0, {}],
  ]],
  ['Hanoi', [
    ['Haiphong', 20.86, 106.68, 'plains', 0, 0, { port: 1 }],
    ['Lang Son', 21.85, 106.76, 'hills', 0, 0, {}],
    ['Lao Kay', 22.48, 103.97, 'mountain', 0, 0, {}],
  ]],
  ['Phnom Penh', [
    ['Kratie', 12.49, 106.02, 'forest', 0, 0, {}],
  ]],
  ['Brunei', [
    ['Miri', 4.39, 113.98, 'forest', 0, 0, { port: 1 }],
  ]],
  ['Balikpapan', [
    ['Samarinda', -0.5, 117.15, 'forest', 0, 0, {}],
  ]],
  ['Pontianak', [
    ['Singkawang', 0.91, 108.98, 'forest', 0, 0, {}],
    ['Sintang', 0.07, 111.5, 'forest', 0, 0, {}],
    ['Ketapang', -1.85, 109.97, 'forest', 0, 0, {}],
  ]],
  ['Bandjermasin', [
    ['Sampit', -2.53, 112.95, 'forest', 0, 0, {}],
    ['Amoentai', -2.42, 115.25, 'plains', 0, 0, {}],
  ]],
  ['Medan', [
    ['Pematang Siantar', 2.96, 99.06, 'hills', 0, 0, {}],
  ]],
  ['Pakanbaru', [
    ['Padang Sidempoean', 1.38, 99.27, 'mountain', 0, 0, {}],
  ]],
  ['Palembang', [
    ['Benkoelen', -3.8, 102.53, 'hills', 0, 0, { port: 1 }],
    ['Pangkalpinang', -2.13, 106.11, 'forest', 0, 0, { port: 1 }],
  ]],
  ['Quetta', [
    ['Sibi', 29.54, 67.88, 'hills', 0, 0, {}],
    ['Loralai', 30.37, 68.6, 'mountain', 0, 0, {}],
  ]],
  ['Sukkur', [
    ['Jacobabad', 28.28, 68.44, 'plains', 0, 0, {}],
    ['Dadu', 26.73, 67.78, 'plains', 0, 0, {}],
    ['Rahimyar Khan', 28.42, 70.3, 'plains', 0, 0, {}],
  ]],
  ['Delhi', [
    ['Hissar', 29.15, 75.72, 'plains', 0, 0, {}],
  ]],
  ['Lucknow', [
    ['Fyzabad', 26.78, 82.14, 'plains', 0, 0, {}],
  ]],
  ['Patna', [
    ['Darbhanga', 26.15, 85.9, 'plains', 0, 0, {}],
  ]],
  ['Jubbulpore', [
    ['Katni', 23.83, 80.39, 'hills', 0, 0, {}],
  ]],
  ['Jhansi', [
    ['Guna', 24.65, 77.31, 'hills', 0, 0, {}],
  ]],
  ['Nagpur', [
    ['Chanda', 19.95, 79.3, 'forest', 0, 0, {}],
  ]],
  ['Hyderabad', [
    ['Gulbarga', 17.33, 76.83, 'plains', 0, 0, {}],
  ]],
  ['Bellary', [
    ['Kurnool', 15.83, 78.04, 'plains', 0, 0, {}],
  ]],
  ['Vizagapatam', [
    ['Jagdalpur', 19.07, 82.03, 'forest', 0, 0, {}],
  ]],
  ['Cuttack', [
    ['Berhampur', 19.31, 84.79, 'plains', 0, 0, {}],
  ]],
  ['Rajkot', [
    ['Bhavnagar', 21.77, 72.15, 'plains', 0, 0, { port: 1 }],
    ['Bhuj', 23.25, 69.67, 'plains', 0, 0, {}],
  ]],
  ['Resht', [
    ['Ardebil', 38.25, 48.3, 'hills', 0, 0, {}],
  ]],
  ['Tehran', [
    ['Semnan', 35.57, 53.39, 'plains', 0, 0, {}],
  ]],
  ['Bandar Shah', [
    ['Damghan', 36.17, 54.35, 'plains', 0, 0, {}],
  ]],
  ['Meshed', [
    ['Quchan', 37.1, 58.51, 'hills', 0, 0, {}],
    ['Turbat-i-Haidari', 35.27, 59.22, 'hills', 0, 0, {}],
  ]],
  ['Isfahan', [
    ['Kashan', 33.98, 51.44, 'plains', 0, 0, {}],
  ]],
  ['Shiraz', [
    ['Jahrum', 28.5, 53.56, 'hills', 0, 0, {}],
  ]],
  ['Kandahar', [
    ['Girishk', 31.82, 64.57, 'plains', 0, 0, {}],
    ['Kalat-i-Ghilzai', 32.11, 66.9, 'hills', 0, 0, {}],
  ]],
  ['Herat', [
    ['Farah', 32.37, 62.12, 'plains', 0, 0, {}],
  ]],
  ['Mazar-i-Sharif', [
    ['Maimana', 35.92, 64.78, 'hills', 0, 0, {}],
  ]],
  ['Faizabad', [
    ['Khanabad', 36.68, 69.11, 'plains', 0, 0, {}],
  ]],
  ['Dhahran', [
    ['Hofuf', 25.38, 49.59, 'plains', 0, 0, {}],
  ]],
  ['Jeddah', [
    ['Taif', 21.27, 40.42, 'hills', 0, 0, {}],
  ]],
  ['Kuibyshev', [
    ['Buzuluk', 52.78, 52.26, 'plains', 0, 0, {}],
  ]],
  ['Kazan', [
    ['Chistopol', 55.37, 50.64, 'plains', 0, 0, {}],
    ['Yoshkar-Ola', 56.63, 47.89, 'forest', 0, 0, {}],
  ]],
  ['Kirov', [
    ['Kotelnich', 58.31, 48.33, 'forest', 0, 0, {}],
    ['Omutninsk', 58.67, 52.19, 'forest', 0, 0, {}],
  ]],
  ['Izhevsk', [
    ['Glazov', 58.14, 52.66, 'forest', 0, 0, {}],
    ['Sarapul', 56.47, 53.8, 'forest', 0, 0, {}],
  ]],
  ['Molotov', [
    ['Kungur', 57.43, 56.94, 'hills', 0, 0, {}],
  ]],
  ['Chelyabinsk', [
    ['Troitsk', 54.08, 61.57, 'plains', 0, 0, {}],
  ]],
  ['Magnitogorsk', [
    ['Beloretsk', 53.97, 58.4, 'mountain', 0, 0, {}],
    ['Kartaly', 53.05, 60.65, 'plains', 0, 0, {}],
  ]],
  ['Ufa', [
    ['Birsk', 55.42, 55.53, 'plains', 0, 0, {}],
    ['Bugulma', 54.54, 52.8, 'plains', 0, 0, {}],
  ]],
  ['Tyumen', [
    ['Irbit', 57.67, 63.07, 'forest', 0, 0, {}],
  ]],
  ['Pugachev', [
    ['Yershov', 51.35, 48.28, 'plains', 0, 0, {}],
  ]],
  ['Kurgan', [
    ['Shadrinsk', 56.09, 63.63, 'plains', 0, 0, {}],
    ['Petropavlovsk Kazakh', 54.87, 69.15, 'plains', 0, 0, {}],
  ]],
  ['Omsk', [
    ['Tyukalinsk', 55.87, 72.2, 'plains', 0, 0, {}],
    ['Isilkul', 54.91, 71.27, 'plains', 0, 0, {}],
  ]],
  ['Novosibirsk', [
    ['Barabinsk', 55.35, 78.35, 'plains', 0, 0, {}],
    ['Kamen-na-Obi', 53.79, 81.35, 'plains', 0, 0, {}],
  ]],
  ['Stalinsk', [
    ['Leninsk-Kuznetsky', 54.66, 86.17, 'hills', 0, 0, {}],
    ['Oirot-Tura', 51.96, 85.96, 'mountain', 0, 0, {}],
  ]],
  ['Nizhneudinsk', [
    ['Tulun', 54.56, 100.58, 'forest', 0, 0, {}],
  ]],
  ['Irkutsk', [
    ['Slyudyanka', 51.66, 103.71, 'mountain', 0, 0, {}],
  ]],
  ['Ulan-Ude', [
    ['Petrovsk-Zabaikalsky', 51.28, 108.83, 'forest', 0, 0, {}],
  ]],
  ['Skovorodino', [
    ['Mogocha', 53.74, 119.77, 'forest', 0, 0, {}],
    ['Zeya', 53.73, 127.27, 'forest', 0, 0, {}],
  ]],
  ['Blagoveshchensk', [
    ['Zavitaya', 50.11, 129.44, 'plains', 0, 0, {}],
  ]],
  ['Iman', [
    ['Tetyukhe', 44.55, 135.57, 'mountain', 0, 0, {}],
  ]],
  ['North Sakhalin', [
    ['Okha', 53.59, 142.95, 'forest', 0, 0, {}],
  ]],
  ['Sain Shand', [
    ['Undur Khan', 47.32, 110.66, 'plains', 0, 0, {}],
  ]],
  ['Fergana', [
    ['Kokand', 40.53, 70.94, 'plains', 0, 0, {}],
    ['Andizhan', 40.78, 72.34, 'plains', 0, 0, {}],
  ]],
  ['Samarkand', [
    ['Karshi', 38.86, 65.8, 'plains', 0, 0, {}],
  ]],
  ['Stalinabad', [
    ['Kulyab', 37.91, 69.78, 'hills', 0, 0, {}],
  ]],
  ['Alma-Ata', [
    ['Dzharkent', 44.17, 80, 'plains', 0, 0, {}],
  ]],
  ['Aralsk', [
    ['Chelkar', 47.83, 59.6, 'plains', 0, 0, {}],
  ]],
  ['Semipalatinsk', [
    ['Ayaguz', 47.96, 80.43, 'plains', 0, 0, {}],
  ]],
  // ---- Part 4: Africa, the Americas and Oceania ----
  ['Mekele', [
    ['Adowa', 14.17, 38.9, 'mountain', 0, 0, {}],
    ['Quoram', 12.5, 39.52, 'mountain', 0, 0, {}],
  ]],
  ['Gondar', [
    ['Debra Tabor', 11.85, 38.02, 'mountain', 0, 0, {}],
  ]],
  ['Debra Markos', [
    ['Dangila', 11.27, 36.83, 'mountain', 0, 0, {}],
  ]],
  ['Dessie', [
    ['Waldia', 11.83, 39.6, 'mountain', 0, 0, {}],
  ]],
  ['Lekemti', [
    ['Asosa', 10.07, 34.53, 'hills', 0, 0, {}],
    ['Gambela', 8.25, 34.58, 'plains', 0, 0, {}],
  ]],
  ['Jimma', [
    ['Gore', 8.15, 35.54, 'forest', 0, 0, {}],
    ['Shashamanna', 7.2, 38.59, 'hills', 0, 0, {}],
  ]],
  ['Neghelli', [
    ['Yavello', 4.88, 38.08, 'plains', 0, 0, {}],
    ['Ginir', 7.13, 40.7, 'hills', 0, 0, {}],
  ]],
  ['Harar', [
    ['Asba Tafari', 9.08, 40.87, 'hills', 0, 0, {}],
    ['Dagabur', 8.22, 43.56, 'plains', 0, 0, {}],
  ]],
  ['Mogadishu', [
    ['Baidoa', 3.12, 43.65, 'plains', 0, 0, {}],
    ['Bardera', 2.34, 42.28, 'plains', 0, 0, {}],
  ]],
  ['Berbera', [
    ['Burao', 9.52, 45.54, 'desert', 0, 0, {}],
  ]],
  ['Wad Medani', [
    ['Gallabat', 12.97, 36.15, 'hills', 0, 0, {}],
    ['Roseires', 11.85, 34.38, 'plains', 0, 0, {}],
  ]],
  ['Malakal', [
    ['Kurmuk', 10.55, 34.28, 'hills', 0, 0, {}],
  ]],
  ['Port Sudan', [
    ['Tokar', 18.43, 37.73, 'desert', 0, 0, {}],
  ]],
  ['Marsabit', [
    ['Lodwar', 3.12, 35.6, 'desert', 0, 0, {}],
  ]],
  ['Kisumu', [
    ['Nakuru', -0.28, 36.07, 'hills', 0, 0, {}],
  ]],
  ['Tananarive', [
    ['Antsirabe', -19.87, 47.03, 'hills', 0, 0, {}],
  ]],
  ['Majunga', [
    ['Maevatanana', -16.95, 46.83, 'plains', 0, 0, {}],
  ]],
  ['Fianarantsoa', [
    ['Ihosy', -22.4, 46.13, 'hills', 0, 0, {}],
  ]],
  ['Abeche', [
    ['Mongo', 12.18, 18.69, 'plains', 0, 0, {}],
  ]],
  ['Fort-Archambault', [
    ['Am Timan', 11.03, 20.28, 'plains', 0, 0, {}],
  ]],
  ['Libreville', [
    ['Mitzic', 0.78, 11.55, 'forest', 0, 0, {}],
  ]],
  ['Yaounde', [
    ['Ebolowa', 2.9, 11.15, 'forest', 0, 0, {}],
    ['Batouri', 4.43, 14.37, 'forest', 0, 0, {}],
  ]],
  ['Douala', [
    ['Dschang', 5.45, 10.07, 'hills', 0, 0, {}],
  ]],
  ['Bangui', [
    ['Bambari', 5.76, 20.67, 'plains', 0, 0, {}],
  ]],
  ['Maiduguri', [
    ['Yola', 9.2, 12.48, 'plains', 0, 0, {}],
    ['Potiskum', 11.71, 11.08, 'plains', 0, 0, {}],
  ]],
  ['Kaduna', [
    ['Minna', 9.61, 6.55, 'plains', 0, 0, {}],
    ['Kontagora', 10.4, 5.47, 'plains', 0, 0, {}],
    ['Bauchi', 10.31, 9.84, 'plains', 0, 0, {}],
  ]],
  ['Kano', [
    ['Katsina', 12.99, 7.6, 'plains', 0, 0, {}],
    ['Gusau', 12.17, 6.66, 'plains', 0, 0, {}],
  ]],
  ['Ibadan', [
    ['Jebba', 9.13, 4.82, 'plains', 0, 0, {}],
  ]],
  ['Tamale', [
    ['Wa', 10.06, -2.5, 'plains', 0, 0, {}],
    ['Kintampo', 8.05, -1.73, 'plains', 0, 0, {}],
  ]],
  ['Freetown', [
    ['Bo', 7.96, -11.74, 'forest', 0, 0, {}],
  ]],
  ['Zinder', [
    ['Maradi', 13.5, 7.1, 'plains', 0, 0, {}],
  ]],
  ['Ouagadougou', [
    ['Ouahigouya', 13.58, -2.42, 'plains', 0, 0, {}],
    ["Fada N'Gourma", 12.06, 0.36, 'plains', 0, 0, {}],
  ]],
  ['Kankan', [
    ['Siguiri', 11.42, -9.17, 'plains', 0, 0, {}],
    ['Kissidougou', 9.18, -10.1, 'forest', 0, 0, {}],
  ]],
  ['Mopti', [
    ['San', 13.3, -4.9, 'plains', 0, 0, {}],
  ]],
  ['Kampala', [
    ['Mbarara', -0.61, 30.65, 'hills', 0, 0, {}],
    ['Mbale', 1.08, 34.18, 'hills', 0, 0, {}],
  ]],
  ['Mwanza', [
    ['Bukoba', -1.49, 31.56, 'hills', 0, 0, {}],
  ]],
  ['Mbeya', [
    ['Songea', -10.68, 35.65, 'hills', 0, 0, {}],
    ['Sumbawanga', -7.97, 31.62, 'hills', 0, 0, {}],
  ]],
  ['Lindi', [
    ['Kilwa', -8.75, 39.36, 'plains', 0, 0, { port: 1 }],
  ]],
  ['El Obeid', [
    ['En Nahud', 12.7, 28.43, 'plains', 0, 0, {}],
  ]],
  ['El Fasher', [
    ['Nyala', 12.05, 24.88, 'plains', 0, 0, {}],
  ]],
  ['Juba', [
    ['Torit', 4.41, 32.57, 'plains', 0, 0, {}],
  ]],
  ['Stanleyville', [
    ['Ponthierville', -0.35, 25.48, 'forest', 0, 0, {}],
  ]],
  ['Albertville', [
    ['Kongolo', -5.38, 27, 'plains', 0, 0, {}],
    ['Manono', -7.3, 27.42, 'plains', 0, 0, {}],
  ]],
  ['Kamina', [
    ['Dilolo', -10.7, 22.33, 'plains', 0, 0, {}],
    ['Bukama', -9.2, 25.85, 'plains', 0, 0, {}],
  ]],
  ['Luluabourg', [
    ['Lusambo', -4.97, 23.44, 'plains', 0, 0, {}],
    ['Tshikapa', -6.42, 20.8, 'plains', 0, 0, {}],
  ]],
  ['Luanda', [
    ['Ambriz', -7.85, 13.39, 'plains', 0, 0, { port: 1 }],
    ['Sao Salvador', -6.27, 14.25, 'plains', 0, 0, {}],
  ]],
  ['Nova Lisboa', [
    ['Silva Porto', -12.38, 16.93, 'hills', 0, 0, {}],
  ]],
  ['Porto Amelia', [
    ['Mocimboa da Praia', -11.35, 40.35, 'plains', 0, 0, { port: 1 }],
    ['Vila Cabral', -13.3, 35.25, 'hills', 0, 0, {}],
  ]],
  ['Beira', [
    ['Vila Pery', -19.12, 33.47, 'hills', 0, 0, {}],
  ]],
  ['Kasama', [
    ['Fort Rosebery', -11.2, 28.89, 'plains', 0, 0, {}],
  ]],
  ['Salisbury', [
    ['Gatooma', -18.33, 29.92, 'plains', 0, 0, {}],
    ['Sinoia', -17.37, 30.2, 'hills', 0, 0, {}],
  ]],
  ['Bulawayo', [
    ['Fort Victoria', -20.07, 30.83, 'hills', 0, 0, {}],
    ['Wankie', -18.37, 26.48, 'plains', 0, 0, {}],
  ]],
  ['Pietersburg', [
    ['Louis Trichardt', -23.05, 29.9, 'hills', 0, 0, {}],
  ]],
  ['Ladysmith', [
    ['Vryheid', -27.77, 30.8, 'hills', 0, 0, {}],
  ]],
  ['East London', [
    ['Queenstown', -31.9, 26.88, 'hills', 0, 0, {}],
    ['Umtata', -31.58, 28.78, 'hills', 0, 0, {}],
  ]],
  ['Rio Grande do Sul', [
    ['Passo Fundo', -28.26, -52.41, 'hills', 0, 0, {}],
    ['Bage', -31.33, -54.1, 'plains', 0, 0, {}],
    ['Alegrete', -29.78, -55.79, 'plains', 0, 0, {}],
    ['Lages', -27.82, -50.33, 'hills', 0, 0, {}],
  ]],
  ['Parana', [
    ['Londrina', -23.31, -51.16, 'forest', 0, 0, {}],
    ['Uniao da Vitoria', -26.23, -51.09, 'forest', 0, 0, {}],
  ]],
  ['Sao Paulo', [
    ['Taubate', -23.03, -45.56, 'hills', 0, 0, {}],
  ]],
  ['Minas Gerais', [
    ['Itabira', -19.62, -43.23, 'hills', 0, 0, {}],
    ['Diamantina', -18.24, -43.6, 'hills', 0, 0, {}],
    ['Teofilo Otoni', -17.86, -41.5, 'hills', 0, 0, {}],
    ['Uberlandia', -18.92, -48.28, 'plains', 0, 0, {}],
  ]],
  ['Bahia', [
    ['Jequie', -13.86, -40.08, 'hills', 0, 0, {}],
    ['Conquista', -14.86, -40.84, 'hills', 0, 0, {}],
    ['Jacobina', -11.18, -40.52, 'hills', 0, 0, {}],
  ]],
  ['Pernambuco', [
    ['Crato', -7.23, -39.41, 'hills', 0, 0, {}],
  ]],
  ['Natal', [
    ['Mossoro', -5.19, -37.34, 'plains', 0, 0, {}],
  ]],
  ['Ceara', [
    ['Iguatu', -6.36, -39.3, 'plains', 0, 0, {}],
  ]],
  ['Buenos Aires', [
    ['Junin', -34.59, -60.95, 'plains', 0, 0, {}],
    ['Dolores', -36.31, -57.68, 'plains', 0, 0, {}],
  ]],
  ['Rosario', [
    ['Santa Fe de la Vera Cruz', -31.63, -60.7, 'plains', 0, 0, {}],
    ['Pergamino', -33.89, -60.57, 'plains', 0, 0, {}],
    ['Gualeguaychu', -33.01, -58.52, 'plains', 0, 0, {}],
  ]],
  ['Pampas', [
    ['Santa Rosa', -36.62, -64.29, 'plains', 0, 0, {}],
    ['General Pico', -35.66, -63.76, 'plains', 0, 0, {}],
    ['Bolivar', -36.23, -61.11, 'plains', 0, 0, {}],
  ]],
  ['Cordoba', [
    ['Dean Funes', -30.42, -64.35, 'hills', 0, 0, {}],
  ]],
  ['Tucuman', [
    ['Catamarca', -28.47, -65.78, 'hills', 0, 0, {}],
  ]],
  ['Mendoza', [
    ['San Juan', -31.54, -68.53, 'hills', 0, 0, {}],
  ]],
  ['Neuquen', [
    ['General Roca', -39.03, -67.58, 'plains', 0, 0, {}],
  ]],
  ['Tacuarembo', [
    ['Melo', -32.37, -54.17, 'plains', 0, 0, {}],
  ]],
  ['Asuncion', [
    ['Villa Concepcion', -23.41, -57.43, 'plains', 0, 0, {}],
  ]],
  ['Concepcion', [
    ['Talca', -35.43, -71.66, 'plains', 0, 0, {}],
  ]],
  ['Cuzco', [
    ['Ayacucho', -13.16, -74.22, 'mountain', 0, 0, {}],
  ]],
  ['Potosi', [
    ['Uyuni', -20.46, -66.83, 'mountain', 0, 0, {}],
    ['Tarija', -21.53, -64.73, 'hills', 0, 0, {}],
  ]],
  ['Santander', [
    ['Socorro', 6.47, -73.26, 'hills', 0, 0, {}],
  ]],
  ['Bogota', [
    ['Tunja', 5.53, -73.36, 'mountain', 0, 0, {}],
  ]],
  ['Cali', [
    ['Popayan', 2.44, -76.61, 'hills', 0, 0, {}],
    ['Neiva', 2.93, -75.28, 'hills', 0, 0, {}],
  ]],
  ['Barquisimeto', [
    ['Barinas', 8.62, -70.21, 'plains', 0, 0, {}],
  ]],
  ['Apure', [
    ['San Fernando de Apure', 7.89, -67.47, 'plains', 0, 0, {}],
  ]],
  ['Chihuahua', [
    ['Ciudad Juarez', 31.74, -106.49, 'plains', 0, 0, {}],
  ]],
  ['Monterrey', [
    ['Matamoros', 25.87, -97.5, 'plains', 0, 0, {}],
    ['Nuevo Laredo', 27.48, -99.52, 'plains', 0, 0, {}],
  ]],
  ['Guadalajara', [
    ['Aguascalientes', 21.88, -102.29, 'hills', 0, 0, {}],
    ['Leon', 21.12, -101.68, 'hills', 0, 0, {}],
    ['Tepic', 21.5, -104.89, 'hills', 0, 0, {}],
  ]],
  ['Sinaloa', [
    ['Los Mochis', 25.79, -108.99, 'plains', 0, 0, {}],
  ]],
  ['Alberta', [
    ['Red Deer', 52.27, -113.81, 'plains', 0, 0, {}],
    ['Lloydminster', 53.28, -110, 'plains', 0, 0, {}],
    ['Edson', 53.58, -116.43, 'forest', 0, 0, {}],
    ['Drumheller', 51.46, -112.71, 'plains', 0, 0, {}],
  ]],
  ['Saskatchewan', [
    ['Yorkton', 51.21, -102.46, 'plains', 0, 0, {}],
    ['Swift Current', 50.29, -107.8, 'plains', 0, 0, {}],
    ['North Battleford', 52.78, -108.3, 'plains', 0, 0, {}],
  ]],
  ['Winnipeg', [
    ['Dauphin', 51.15, -100.05, 'plains', 0, 0, {}],
  ]],
  ['Vancouver', [
    ['Kelowna', 49.89, -119.5, 'hills', 0, 0, {}],
  ]],
  ['British Columbia', [
    ['Prince George', 53.92, -122.75, 'forest', 0, 0, {}],
  ]],
  ['Dakotas', [
    ['Williston', 48.15, -103.62, 'plains', 0, 0, {}],
    ['Bismarck', 46.81, -100.78, 'plains', 0, 0, {}],
  ]],
  ['Wyoming', [
    ['Casper', 42.85, -106.31, 'plains', 0, 0, {}],
  ]],
  ['Nebraska', [
    ['North Platte', 41.12, -100.77, 'plains', 0, 0, {}],
  ]],
  ['Nevada', [
    ['Ely', 39.25, -114.89, 'mountain', 0, 0, {}],
  ]],
  ['West Texas', [
    ['Marfa', 30.31, -104.02, 'hills', 0, 0, {}],
  ]],
  ['Arizona', [
    ['Kingman', 35.19, -114.05, 'hills', 0, 0, {}],
    ['Prescott', 34.54, -112.47, 'mountain', 0, 0, {}],
  ]],
  ['Utah', [
    ['Price', 39.6, -110.81, 'mountain', 0, 0, {}],
  ]],
  ['Montana', [
    ['Havre', 48.55, -109.68, 'plains', 0, 0, {}],
  ]],
  ['Fairbanks', [
    ['Nenana', 64.56, -149.09, 'forest', 0, 0, {}],
  ]],
  ['Wewak', [
    ['Aitape', -3.14, 142.35, 'forest', 0, 0, { port: 1 }],
  ]],
  ['Rockhampton', [
    ['Emerald', -23.53, 148.16, 'plains', 0, 0, {}],
    ['Bundaberg', -24.87, 152.35, 'plains', 0, 0, { port: 1 }],
  ]],
  ['Brisbane', [
    ['Moree', -29.47, 149.85, 'plains', 0, 0, {}],
  ]],
  ['Sydney', [
    ['Parkes', -33.14, 148.17, 'plains', 0, 0, {}],
  ]],
  ['Canberra', [
    ['Griffith', -34.29, 146.04, 'plains', 0, 0, {}],
  ]],
  ['Melbourne', [
    ['Shepparton', -36.38, 145.4, 'plains', 0, 0, {}],
  ]],
  ['Adelaide', [
    ['Port Augusta', -32.49, 137.77, 'plains', 0, 0, { port: 1 }],
  ]],
  ['Perth', [
    ['Albany WA', -35.02, 117.88, 'plains', 0, 0, { port: 1 }],
  ]],
  // ---- Part 4, the historian's cut (ruling 215): Yochow, on the Canton-Hankow railway and the east shore of
  // Tungting Lake, Japan's from November 1938 and the base for all four battles of Changsha ----
  ['Changsha', [
    ['Yochow', 29.37, 113.1, 'plains', 0, 0, {}],
  ]],
  // Tungkun, on the Canton-Kowloon railway behind Hong Kong's frontier: Japan's from late 1938 (the frontier closed
  // at Shumchun in August 1939), the 38th Division's road in 1941. Under the 700 floor (about 450), waived on the
  // historian's word so that Hong Kong touches it.
  ['Canton', [
    ['Tungkun', 23.02, 113.75, 'plains', 0, 0, {}],
  ]],
  // ---- America's great cities (Brian, 17 November 2026: "the USA should have more major cities too"). Every city of
  // the 1940 census over 250,000 is a city of the map. These five stood inside a province named for its state or
  // country, far enough from its seed to be cut: Houston (384,514), Denver (322,412), Columbus (306,087), Dallas
  // (294,734) and Birmingham (267,583). Urban ground, as Providence (253,504) and Milwaukee have; what each is worth
  // comes from its parent (ruling 210). Indianapolis and Seattle stand at their province's own seed, and Newark and
  // Jersey City inside New York's, so they are not cut. ----
  ['East Texas', [
    ['Houston', 29.76, -95.37, 'urban', 0, 0, { port: 1 }],
  ], { port: 0 }], // the whole Gulf shore, Beaumont and Port Arthur too, is nearer Houston
  ['North Texas', [
    ['Dallas', 32.78, -96.8, 'urban', 0, 0, {}],
  ]],
  ['Colorado', [
    ['Denver', 39.74, -104.99, 'urban', 0, 0, {}],
  ]],
  ['Ohio Valley', [
    ['Columbus', 39.96, -83.0, 'urban', 0, 0, {}],
  ]],
  ['Alabama', [
    ['Birmingham', 33.52, -86.81, 'urban', 0, 0, {}],
  ]],
  // Western Samoa (Upolu and Savaii), New Zealand's mandate from 1920, which the chart handed over folded into
  // America's Samoa; Tutuila and Pago Pago stay America's (the Pacific planner, 17 November 2026). The owner is the
  // clerk's row; the small islands go whole to the nearer seed.
  ['Samoa', [
    ['Apia', -13.9, -171.76, 'hills', 0, 0, { port: 1, col: 1 }], // Apia itself (-13.83) is on the shore; the seed is inland on Upolu
  ]],
];

// Which seas a city stands on where the count of water squares misleads: name -> [sea keys].
export const CITY_SHORES = {};

// A crossing over a strait ('links') whose end is now one particular piece of a province that was cut: [a, b] by name.
export const CITY_LINKS = [];

// The Alaska panhandle's mainland strip as the 1903 award drew it, with the sea beside it (see CITY_GIVE).
const PANHANDLE = [[54.72, -130.62], [55.05, -130.2], [55.45, -130.05], [55.92, -130.02], [56.13, -130.1], [56.4, -130.55], [56.58, -131.1], [56.7, -131.8], [56.72, -132.12], [57.09, -132.36], [57.45, -132.65], [57.85, -133.05], [58.25, -133.35], [58.65, -133.7], [59.0, -134.2], [59.33, -134.95], [59.62, -135.15], [59.69, -135.29], [59.8, -135.5], [59.6, -136.0], [59.45, -136.37], [59.2, -136.75], [58.9, -137.5], [59.35, -138.5], [60.32, -139.07], [60.31, -140.46], [60.29, -140.93], [60.31, -141.0], [59.0, -141.0], [56.0, -137.0], [54.6, -133.5], [54.6, -130.75]];

// Ground given by ruling from one province to another on the chart with the cities: [from, to, the ground as
// [lat, lon] points]. Only the part of it that lies in 'from' moves.
export const CITY_GIVE = [
  // Shanhaikwan, where the Peking-Mukden main line crossed the Great Wall into Manchukuo, lay on the Hopei side of
  // the wall and stayed in Hopei after the truce of 1933; the frontier handed over runs a few km south of the town.
  // The strip between it and the wall, from the Old Dragon's Head on the sea north to Chiumenkou, goes to Tangshan,
  // which then touches Chinchow (the map fact-checker; the design director, 17 November 2026).
  ['Chinchow', 'Tangshan', [[39.9, 119.6], [39.93, 119.8], [39.967, 119.797], [40.01, 119.785], [40.06, 119.765], [40.12, 119.72], [40.14, 119.6]]],
  // The Alaska panhandle's mainland strip, the United States' by the award of 1903: up the Portland Canal to Hyder,
  // along the Coast Mountains by the Stikine, the Taku, the White and Chilkoot passes and the Chilkat, then by Mt
  // Fairweather and Mt Hubbard to Mt St Elias and the 141st meridian. The chart handed over drew it Canada's, which
  // left Juneau and Ketchikan islands with no land beside them (the Pacific planner, 17 November 2026). What lay in
  // Whitehorse goes to Juneau (Skagway, Haines, Glacier Bay, Yakutat), and the sliver of Watson Lake's by the Stikine
  // too; what lay in Prince Rupert to Ketchikan.
  ['Whitehorse', 'Juneau', PANHANDLE],
  ['Prince Rupert', 'Ketchikan', PANHANDLE],
  ['Watson Lake', 'Juneau', PANHANDLE],
];

// Straits laid on the chart with the cities (Brian's ruling of 17 November: straits to Europe's level everywhere):
// short crossings armies can use without a fleet, as Europe's Great Belt, Sound, Messina, Bosporus and Kerch. Each
// had a ferry or a regular crossing in 1939 and is no wider than about 25 kilometres at its narrows: [a, b] by name.
export const CITY_STRAITS = [
  ['Aomori', 'Sapporo'], // the Tsugaru strait: the Aomori-Hakodate train ferry
  ['Okayama', 'Shikoku'], // the Inland Sea: the Uno-Takamatsu train ferry (Shikoku had no other way)
  ['Madura', 'Trincomalee'], // the Palk strait: the Dhanushkodi-Talaimannar ferry, the Boat Mail to Ceylon (1914)
  ['Batavia', 'Oosthaven'], // the Sunda strait: the Merak-Oosthaven train ferry (1912)
  ['Pakhoi', 'Hainan'], // the Hainan strait: Hai-an on the Luichow peninsula to Hoihow
  ['Wellington', 'Nelson'], // Cook strait: Wellington to Picton
  ['Legaspi', 'Leyte'], // the San Bernardino strait: Matnog to Samar
  ['Leyte', 'Del Monte'], // the Surigao strait: Leyte to Surigao on Mindanao
];

// River crossings among the cities and what is left of their provinces, as in far.mjs: a road between two seeds
// that a river crosses near its middle is a crossing; CUT strikes a false one, ADD names one the test misses.
export const CITY_RIVER_CUT = [];
export const CITY_RIVER_ADD = [
  // ---- The United States ----
  // A city on a river's bank stands too near it for the test: the river runs past the end of its road.
  ['Cincinnati', 'Kentucky', 'Ohio'], ['Cincinnati', 'Louisville', 'Ohio'], ['Cincinnati', 'Huntington', 'Ohio'], ['Ohio Valley', 'Huntington', 'Ohio'],
  ['Indiana', 'Louisville', 'Ohio'], ['Evansville', 'Kentucky', 'Ohio'], ['Evansville', 'Louisville', 'Ohio'], ['Evansville', 'Paducah', 'Ohio'],
  ['St Louis', 'Illinois', 'Mississippi'], ['St Louis', 'Paducah', 'Mississippi'], ['St Louis', 'Memphis', 'Mississippi'], ['Missouri', 'Memphis', 'Mississippi'],
  ['Memphis', 'Arkansas', 'Mississippi'], ['Davenport', 'Chicago', 'Mississippi'], ['Davenport', 'Peoria', 'Mississippi'], ['Davenport', 'Milwaukee', 'Mississippi'],
  ['Davenport', 'Madison', 'Mississippi'], ['Minneapolis', 'Eau Claire', 'Mississippi'],
  ['Omaha', 'Iowa', 'Missouri'], ['Omaha', 'Sioux City', 'Missouri'], ['Sioux City', 'Grand Island', 'Missouri'], ['Kansas City', 'Iowa', 'Missouri'],
  ['Kansas City', 'Cedar Rapids', 'Missouri'], ['Missouri', 'Cedar Rapids', 'Missouri'], ['Pierre', 'Rapid City', 'Missouri'], ['Pierre', 'Nebraska', 'Missouri'],
  ['Pierre', 'Scottsbluff', 'Missouri'], ['Dakotas', 'Rapid City', 'Missouri'],
  ['Portland', 'Puget Sound', 'Columbia'], ['Portland', 'Yakima', 'Columbia'],
  ['Flagstaff', 'Las Vegas', 'Colorado'], ['Flagstaff', 'Nevada', 'Colorado'], ['Yuma', 'San Diego', 'Colorado'], ['Yuma', 'Ensenada', 'Colorado'],
  ['El Paso', 'Chihuahua', 'Rio Grande'], ['Brownsville', 'Monterrey', 'Rio Grande'], ['Burlington', 'Montreal', 'St Lawrence'],
  // ---- Canada ----
  ['Montreal', 'Sherbrooke', 'St Lawrence'], ['Trois-Rivieres', 'Sherbrooke', 'St Lawrence'], ['Quebec', 'Sherbrooke', 'St Lawrence'],
  ['Quebec', 'Rimouski', 'St Lawrence'],
  // ---- South America ----
  // The Amazon: Manaus, Macapa, Leticia and Iquitos stand on its north bank; Santarem, Tefe and Belem on its south.
  ['Manaus', 'Amazonas', 'Amazon'], ['Manaus', 'Tapajos', 'Amazon'], ['Santarem', 'Macapa', 'Amazon'], ['Macapa', 'Tapajos', 'Amazon'],
  ['Macapa', 'Belem', 'Amazon'], ['Tefe', 'Rio Negro', 'Amazon'], ['Tefe', 'Leticia', 'Amazon'], ['Leticia', 'Acre', 'Amazon'],
  ['Iquitos', 'Loreto', 'Amazon'], ['Iquitos', 'Acre', 'Amazon'],
  // The Plate's rivers: Resistencia stands west of the Parana and Encarnacion north of it; Paso de los Libres lies
  // between the Parana and the Uruguay, Salto east of the Uruguay; Corumba stands west of the Paraguay.
  ['Resistencia', 'Corrientes', 'Parana'], ['Resistencia', 'Encarnacion', 'Parana'], ['Encarnacion', 'Corrientes', 'Parana'],
  ['Paso de los Libres', 'Rosario', 'Parana'], ['Paso de los Libres', 'Rio Grande do Sul', 'Uruguay'], ['Paso de los Libres', 'Salto', 'Uruguay'],
  ['Salto', 'Rosario', 'Uruguay'], ['Corumba', 'Campo Grande', 'Paraguay'],
  ['Juazeiro', 'Sertao', 'Sao Francisco'], ['Juazeiro', 'Ceara', 'Sao Francisco'],
  // Porto Velho stands on the Madeira's east bank (the two crossings of the Madeira as handed over, carried on).
  ['Porto Velho', 'Amazonas', 'Madeira'], ['Porto Velho', 'Rio Branco', 'Madeira'],
  // ---- China, Manchuria and Korea ----
  // The frontier rivers: Hulin and Fuchin face Iman over the Ussuri, Svobodny faces Heiho over the Amur, Sinuiju
  // stands on the Yalu. The Yangtze: Wuhu, Kiukiang, Changteh and Tsunyi lie south of it; Anking, Pengpu, Hankow,
  // Ichang, Chungking and Suifu north.
  ['Hulin', 'Iman', 'Ussuri'], ['Fuchin', 'Iman', 'Ussuri'], ['Svobodny', 'Heiho', 'Amur'],
  ['Sinuiju', 'Anshan', 'Yalu'], ['Sinuiju', 'Tunghwa', 'Yalu'],
  ['Wuhu', 'Anking', 'Yangtze'], ['Wuhu', 'Pengpu', 'Yangtze'], ['Anking', 'Kiukiang', 'Yangtze'], ['Kiukiang', 'Hankow', 'Yangtze'],
  ['Ichang', 'Changteh', 'Yangtze'], ['Chungking', 'Changteh', 'Yangtze'], ['Chungking', 'Tsunyi', 'Yangtze'], ['Suifu', 'Tsunyi', 'Yangtze'],
  // ---- Soviet Central Asia: Chardzhou west of the Amu Darya, Nukus east of it; Leninabad south of the Syr Darya ----
  ['Chardzhou', 'Bukhara', 'Amu Darya'], ['Chardzhou', 'Samarkand', 'Amu Darya'], ['Nukus', 'Khiva', 'Amu Darya'],
  ['Leninabad', 'Tashkent', 'Syr Darya'], ['Stalinsk', 'Ust-Kamenogorsk', 'Ob'],
  // ---- India: Cawnpore and Allahabad south of the Ganges, Bhagalpur on its south bank; Dera Ismail Khan west of
  // the Indus; Tezpur north of the Brahmaputra ----
  ['Lucknow', 'Cawnpore', 'Ganges'], ['Allahabad', 'Lucknow', 'Ganges'], ['Allahabad', 'Benares', 'Ganges'], ['Allahabad', 'Gorakhpur', 'Ganges'],
  ['Gorakhpur', 'Patna', 'Ganges'], ['Bhagalpur', 'Darjeeling', 'Ganges'], ['Bhagalpur', 'Dacca', 'Ganges'], ['Comilla', 'Calcutta', 'Ganges'],
  ['Dera Ismail Khan', 'Multan', 'Indus'], ['Dera Ismail Khan', 'Lahore', 'Indus'], ['Sukkur', 'Bahawalpur', 'Indus'],
  ['Tezpur', 'Jorhat', 'Brahmaputra'], ['Tezpur', 'Kohima', 'Brahmaputra'],
  // ---- Australia: the Murray is the border of Victoria; Mildura stands on its south bank, Albury on its north ----
  ['Mildura', 'Broken Hill', 'Murray'], ['Albury', 'Bendigo', 'Murray'], ['Albury', 'Sale', 'Murray'],
  // ---- Africa: Parakou west of the Niger; Bangassou north of the Ubangi and Lisala south of it, on the Congo's
  // north bank; Livingstone on the Zambezi's north bank; Kosti west of the White Nile ----
  ['Niamey', 'Parakou', 'Niger'], ['Bangassou', 'Buta', 'Ubangi'], ['Bangassou', 'Lisala', 'Ubangi'], ['Bangui', 'Lisala', 'Ubangi'],
  ['Lisala', 'Coquilhatville', 'Congo'], ['Kindu', 'Costermansville', 'Congo'], ['Kindu', 'Albertville', 'Congo'],
  ['Livingstone', 'Bulawayo', 'Zambezi'], ['Livingstone', 'Gwelo', 'Zambezi'], ['Livingstone', 'Salisbury', 'Zambezi'],
  ['Kosti', 'Wad Medani', 'White Nile'],
  // ---- Alaska: Galena on the Yukon's north bank ----
  ['Galena', 'McGrath', 'Yukon'],
];

// Railways. A line that ran between two provinces runs on between whichever of their pieces lie along it, and every
// city is joined to the net by its easiest border unless it is named here (no railway in 1939).
export const CITY_NO_RAIL = [
  // Canada's north and the Gulf's north shore: the Alaska Highway of 1942 was a road, and the mines flew or shipped.
  // (Whitehorse is not here: the White Pass and Yukon Route ran to it from Skagway, laid by hand below.)
  'Sept-Iles', 'Fort Chimo', 'Fort Nelson', 'Watson Lake', 'Dawson City', 'Yellowknife', 'Port Radium',
  // Latin America: ports and river towns the railways of 1939 did not reach.
  'Acapulco', 'Bluefields', 'Cap-Haitien', 'Pucallpa', 'Iquitos', 'Leticia', 'Pasto', 'Esmeraldas', 'Puerto La Cruz', 'Maturin', 'Coro',
  'Georgetown', 'Paramaribo', 'Cayenne', 'Tefe', 'Manaus', 'Santarem', 'Macapa', 'Belem', 'Porto Velho', 'Rio Branco', 'Cuiaba',
  'Punta Arenas', 'Rio Gallegos', 'Ushuaia',
  // China: the west, the south-west and the coast south of the Yangtze had no line in 1939 (Tienshui's came in 1945).
  'Chefoo', 'Tienshui', 'Yulin', 'Wuyuan', 'Turfan', 'Aksu', 'Yarkand', 'Chuguchak', 'Anhsi', 'Changyeh', 'Liangchow', 'Shigatse',
  'Sichang', 'Kiating', 'Suifu', 'Wanhsien', 'Tsunyi', 'Lungchow', 'Tali', 'Changteh', 'Wenchow', 'Nanping', 'Fuchin', 'Tsetserleg',
  'Waichow', // the Canton-Kowloon line ran west of it, by Tungkun
  // Soviet Asia: the line to the Lena came in 1951 and Nukus's in 1955; the Pechora line to Ukhta ran through
  // country the chart gives no line.
  'Ust-Kut', 'Igarka', 'Altan Bulak', 'Seymchan', 'Markovo', 'Przhevalsk', 'Nukus', 'Ukhta',
  // India's frontier, Afghanistan, Burma's far north, Persia's east and Arabia.
  'Razmak', 'Dera Ismail Khan', 'Pasni', 'Jalalabad', 'Ghazni', 'Fort Hertz', 'Sabzawar', 'Lingeh', 'Buraida', 'Yanbu', 'Seiyun',
  // Australia and New Zealand: the north, and the lines that stood alone at Port Lincoln, Normanton, Cooktown and Nelson.
  'Daly Waters', 'Halls Creek', 'Derby', 'Exmouth Gulf', 'Port Lincoln', 'Normanton', 'Cooktown', 'Nelson',
  // Africa: the Dakar-Niger line ended at Koulikoro, short of Segou; the rest were river, road or caravan towns.
  'Kikwit', 'Lisala', 'Paulis', 'Watsa', 'Fort Gouraud', 'Ziguinchor', 'Segou', 'Sikasso', 'Man', 'Sassandra', 'Tahoua',
  'Mao', 'Ati', 'Moundou', 'Maroua', 'Ngaoundere', 'Bouar', 'Kribi', 'Bangassou', 'Dolo', 'Harper',
  'Farafra', 'Geneina', 'Sokoto', 'Benin City', 'Moyale', 'Iringa', 'Abercorn',
  // Alaska: the Copper River line from Cordova closed in 1938; the Alaska Railroad ran from Seward to Fairbanks only.
  'Cordova', 'Big Delta', 'McGrath', 'Galena', 'Kotzebue', 'Cold Bay',
  // ---- Part 4 ----
  // A town whose own streets had no line is still left off this list where a trunk line crosses its province: the
  // Tientsin-Pukow line at Tehchow (Lintsing) and Yenchow (Sintai), the lines to Kiamusze by Poli and Tiehli (Sansing),
  // the Rangoon-Mandalay line at Pyinmana (Loikaw), the Trans-Iranian by Sari (Damghan), the Trans-Siberian at Tygda
  // (Zeya), the Raipur-Vizianagram line at Rayagada (Jagdalpur), the Kassala line at Haiya (Tokar), the Conakry-Niger
  // line between Mamou and Dabola (Labe), the Tyumen-Omsk line by Nazyvayevsk (Tyukalinsk), the Benguela line at
  // Teixeira de Sousa (Henrique de Carvalho), the Myitkyina line at Wuntho (Tamu), and the lines handed over through
  // Mossoro, Tarija and Socorro.
  // China: the hill and mountain towns round Szechwan, the coast of Kiangsu, the south and the far north had no line in
  // September 1939 (Kutsing's came in 1941; the lines to Kingyang and Pingliang were never built).
  'Shasi', 'Enshih', 'Macheng', 'Yencheng', 'Linchwan', 'Fowyang', 'Pengshui', 'Mienyang', 'Sungpan', 'Lishih', 'Wuchow',
  'Ningtu', 'Kweihsien', 'Poseh', 'Pichieh', 'Kingyang', 'Kwangyuan', 'Ankang', 'Fusung', 'Lintung', 'Kangnung', 'Shangtu',
  'Pingliang', 'Minhsien', 'Kutsing', 'Hweitseh', 'Szemao', 'Likiang', 'Mienning',
  // Asia: the Burma Railway to Kanchanaburi came in 1942 and the Persian line to Kashan in 1949.
  'Kengtung', 'Maingkwan', 'Kanchanaburi', 'Sakon Nakhon', 'Raheng', 'Chiang Rai', 'Kratie', 'Miri', 'Samarinda',
  'Singkawang', 'Sintang', 'Ketapang', 'Sampit', 'Amoentai', 'Padang Sidempoean', 'Benkoelen', 'Pangkalpinang', 'Loralai', 
  'Bhuj', 'Ardebil', 'Quchan', 'Turbat-i-Haidari', 'Kashan', 'Jahrum', 'Girishk', 'Kalat-i-Ghilzai', 'Farah', 'Maimana',
  'Khanabad', 'Hofuf', 'Taif', 'Chistopol', 'Beloretsk', 'Birsk', 'Kamen-na-Obi', 'Oirot-Tura', 'Tetyukhe', 'Okha',
  'Undur Khan', 'Kulyab', 'Dzharkent',
  // Africa: Ethiopia had the one line from Jibuti; the rest were road, river and caravan towns.
  'Adowa', 'Quoram', 'Debra Tabor', 'Dangila', 'Waldia', 'Asosa', 'Gambela', 'Gore', 'Shashamanna', 'Yavello', 'Ginir', 'Dagabur',
  'Baidoa', 'Bardera', 'Burao', 'Gallabat', 'Roseires', 'Kurmuk', 'Lodwar', 'Maevatanana', 'Ihosy', 'Mongo', 'Am Timan',
  'Mitzic', 'Ebolowa', 'Batouri', 'Dschang', 'Bambari', 'Yola', 'Potiskum', 'Kontagora', 'Bauchi', 'Katsina', 'Wa', 'Kintampo',
  'Maradi', 'Ouahigouya', "Fada N'Gourma", 'Siguiri', 'Kissidougou', 'San', 'Mbarara', 'Bukoba', 'Songea', 'Sumbawanga', 'Kilwa',
  'En Nahud', 'Nyala', 'Torit', 'Manono', 'Lusambo', 'Tshikapa', 'Ambriz', 'Sao Salvador', 'Mocimboa da Praia', 'Vila Cabral',
  'Fort Rosebery',
  // The Americas and New Guinea: the line to Itabira came in 1943; Teofilo Otoni's stood alone.
  'Lages', 'Itabira', 'Teofilo Otoni', 'Villa Concepcion', 'Barinas',
  'San Fernando de Apure', 'Aitape',
  // The map fact-checker (17 November 2026): the Hunan-Kwangsi line reached Liuchow in December 1939 and the Kwangsi
  // line from it reached Tushan only in 1943; the line to Tamsag Bulak
  // was never built; Villahermosa's came in 1950 (the Yucatan was a rail island); Santa Cruz's in 1955, so Camiri had
  // none; the Rio-Bahia link by Conquista in 1950; the Huancayo line ended at Huancavelica, short of Ayacucho.
  'Liuchow', 'Tushan', 'Tamsag Bulak', 'Villahermosa', 'Camiri', 'Conquista', 'Ayacucho',
  // The historian (17 November 2026): no line reached Socorro; the Tabriz line was only begun in 1939 and reached
  // Zanjan, beyond Qazvin, by 1942.
  'Socorro', 'Qazvin',
];
// Lines laid by hand, each a chain of neighbouring provinces, and lines struck.
export const CITY_RAIL_LINES = [
  // ---- The United States: the trunk lines of 1939 that the joining leaves out ----
  ['Illinois', 'St Louis', 'Memphis', 'Mississippi'], ['Tennessee', 'Memphis', 'Arkansas'],
  ['Cincinnati', 'Louisville', 'Indiana'],
  ['Greenville', 'Atlanta', 'Alabama'], ['Atlanta', 'Chattanooga'],
  ['Florida', 'Miami'], ['Tallahassee', 'Pensacola', 'Alabama'],
  ['Omaha', 'Kansas City', 'Joplin'], ['Duluth', 'Minneapolis', 'Iowa'], ['Minneapolis', 'Sioux Falls'], ['Sioux City', 'Iowa'],
  ['Yuma', 'Tucson', 'El Paso', 'Marfa', 'West Texas'], ['El Paso', 'New Mexico'], ['Gallup', 'Flagstaff'],
  ['Reno', 'Elko', 'Ogden', 'Pocatello'], ['Ogden', 'Rock Springs'], ['Boise', 'Pendleton', 'Portland'],
  ['Billings', 'Butte', 'Missoula'], ['Butte', 'Pocatello'],
  ['Lubbock', 'Amarillo'], ['Wichita Falls', 'North Texas'], ['San Antonio', 'Corpus Christi'], ['Shreveport', 'Dallas'],
  ['New York', 'Albany'], ['Baltimore', 'Susquehanna'], ['Toledo', 'Fort Wayne'], ['Toledo', 'Columbus'],
  ['Grand Rapids', 'South Bend'], ['Cheyenne', 'Scottsbluff'], ['Sacramento', 'San Joaquin'],
  // ---- Canada: the Newfoundland Railway, and the Northern Alberta line to Dawson Creek ----
  ['Corner Brook', 'Newfoundland', 'Gander', "St John's"], ['Alberta', 'Edson', 'Grande Prairie', 'Dawson Creek'],
  ['Alberta', 'Fort McMurray'], ['Winnipeg', 'Brandon', 'Regina'], // the Waterways line; the Canadian Pacific main line
  // ---- Mexico and Central America: Torreon, the junction of the north; the lines to Corinto and Puerto Limon ----
  ['Monterrey', 'Torreon', 'Parral'], ['Durango', 'Torreon', 'Zacatecas'], ['Corinto', 'Managua'], ['Puerto Limon', 'San Jose'],
  // ---- South America ----
  ['La Paz', 'Oruro', 'Potosi'], ['Buenos Aires', 'Dolores', 'Mar del Plata'], ['Salvador', 'Jequie'],
  // ---- China and Manchuria: the Peiping-Hankow line by Chumatien and Sinyang (Nanyang had no line), the Lunghai line
  // to Sian (1934) and Paoki (1936), the lines to Peian and Heiho, to Hulin and to Arshan (1937, from Paichengtze by
  // Wangyehmiao), the Manpo line to Kanggye (1939). (The Kwangsi line from Liuchow reached Tushan only in 1943.) ----
  ['Hankow', 'Sinyang', 'Chumatien', 'Kaifeng'], ['Loyang', 'Tungkwan', 'Sian', 'Paoki'],
  ['Harbin', 'Suihwa', 'Peian', 'Heiho'], ['Kiamusze', 'Hulin'], ['Taonan', 'Wangyehmiao', 'Arshan'], ['Sinuiju', 'Kanggye'],
  ['Mutankiang', 'Sansing', 'Kiamusze'], // the Tumen-Kiamusze line of 1937, by Linkou and Poli
  // ---- Japan: the train ferries that carried the wagons across, as Europe's Great Belt and Messina do ----
  ['Aomori', 'Sapporo'], ['Okayama', 'Shikoku'], // Aomori-Hakodate (1925), Uno-Takamatsu (1921)
  // ---- Indochina, Siam and Burma: the line from Hanoi to Lang Son and the frontier; Siam's northern line, which left
  // the Korat line at Ban Phachi and ran by Lopburi and Paknampo; the Rangoon-Mandalay line by Pyinmana and Thazi ----
  ['Hanoi', 'Lang Son'], ['Korat', 'Paknampo', 'Phitsanulok'], ['Toungoo', 'Loikaw', 'Meiktila'],
  // ---- The Turksib (1930), east of Lake Balkhash by Aktogay and Ush-Tobe ----
  ['Semipalatinsk', 'Ayaguz', 'Balkhash', 'Taldy-Kurgan', 'Alma-Ata'],
  // ---- Soviet Asia and Burma ----
  ['Chkalov', 'Orsk'], ['Mandalay', 'Meiktila'],
  // ---- Australia: the North Australia Railway from Darwin, the Marble Bar line, the Tasmanian main line, the
  // Victorian lines to Bendigo and Mildura and to Albury, the Central Western line to Longreach ----
  ['Darwin', 'Katherine'], ['Port Hedland', 'Marble Bar'], ['Hobart', 'Launceston'],
  ['Geraldton', 'Meekatharra'], // the Northern Railway by Mullewa and Cue (1910), across ground now ruled desert
  ['Bendigo', 'Melbourne'], ['Bendigo', 'Mildura'], ['Rockhampton', 'Emerald', 'Longreach'],
  // ---- Africa: the Dahomey line to Parakou, the Gold Coast's two lines to Kumasi, the Sudan's lines by Sennar to
  // Gedaref and by Kosti to El Obeid, the Lualaba line from Kindu to Albertville, the Lumbo line to Nampula ----
  ['Cotonou', 'Parakou'], ['Takoradi', 'Kumasi', 'Accra'], ['Gedaref', 'Wad Medani'], ['Wad Medani', 'Kosti', 'El Obeid'],
  ['Kindu', 'Kongolo', 'Albertville'], ['Nampula', 'Mocambique'],
  // The line round the falls from Stanleyville to Ponthierville, Sierra Leone's line to Bo, Nigeria's branch to Gusau.
  ['Stanleyville', 'Ponthierville'], ['Freetown', 'Bo'], ['Kaduna', 'Gusau'], ['Nairobi', 'Nanyuki'],
  // ---- The map fact-checker's mends (17 November 2026, fact-check.md), each a line of 1939 by the provinces its
  // stations fall in ----
  // Burma: the Myitkyina line (1898) left Sagaing by Shwebo, Kanbalu (Kalewa), Wuntho (Tamu), Katha (Bhamo) and
  // Mogaung; Lashio was a dead end where the Burma Road began.
  ['Mandalay', 'Kalewa', 'Tamu', 'Bhamo', 'Myitkyina'],
  // Persia: the Trans-Iranian (1938) by Qom, Arak and Dorud (Arak) to Andimeshk; Hamadan had no line.
  ['Tehran', 'Qom', 'Arak', 'Andimeshk', 'Ahwaz'],
  // Mexico: the Veracruz al Istmo line to Coatzacoalcos, the Tehuantepec line across the isthmus to Salina Cruz
  // (Ixtepec), and the Pan-American line on to Chiapas.
  ['Veracruz', 'Coatzacoalcos', 'Oaxaca', 'Salina Cruz', 'Chiapas'],
  // Peru: Tacna's one line ran to Arica.
  ['Tacna', 'Arica'],
  // Angola and the Congo: the Benguela line (1931) by Teixeira de Sousa (Henrique de Carvalho) and Dilolo to Kolwezi.
  ['Luso', 'Henrique de Carvalho', 'Dilolo', 'Kolwezi', 'Elisabethville'],
  // Siberia and Central Asia: the Tyumen-Omsk line (1913) by Ishim and Nazyvayevsk (Tyukalinsk); Abakan's line from
  // Achinsk (1925); the line from Kagan by Karshi and Termez to Stalinabad (1929).
  ['Tyumen', 'Ishim', 'Tyukalinsk', 'Omsk'], ['Abakan', 'Achinsk'], ['Bukhara', 'Karshi', 'Stalinabad'],
  // Australia: Esperance's line north to Norseman and Kalgoorlie (1927); Moree's to Narrabri and Werris Creek; the
  // Broken Hill line by Ivanhoe and Condobolin (Griffith) to Parkes (1927); Bourke's by Nyngan (Dubbo).
  ['Esperance', 'Kalgoorlie'], ['Moree', 'Tamworth'], ['Broken Hill', 'Griffith', 'Parkes', 'Sydney'], ['Bourke', 'Dubbo', 'Sydney'],
  ['Albury', 'Wagga Wagga', 'Griffith'], ['Parkes', 'Dubbo'], // the Main Southern line and the Junee-Narrandera-Griffith line; Parkes-Narromine (1925)
  // Nigeria: the Eastern line by Makurdi met the Kaduna line at Kafanchan (Jos). Venezuela: the Bolivar line from
  // Tucacas (Puerto Cabello) to Barquisimeto, and the Great Venezuela line from Caracas to Valencia and Puerto Cabello;
  // no line ran from Valencia to Barquisimeto.
  ['Enugu', 'Makurdi', 'Jos', 'Kaduna'], ['Caracas', 'Puerto Cabello', 'Barquisimeto'],
  // The Yucatan's own lines, from Merida to Campeche, which no line joined to Mexico's until 1950.
  ['Campeche', 'Yucatan'],
  // The Peking-Mukden main line along the coast by Tangshan and Shanhaikwan (now Tangshan's) into Chinchow.
  ['Tientsin', 'Tangshan', 'Chinchow'],
  // Round the five great cities cut on Brian's word: the International-Great Northern from Austin by San Marcos to
  // San Antonio (beside the Southern Pacific's Sunset Route from Houston); Shreveport to Dallas is the Texas and
  // Pacific, Toledo to Columbus the Hocking Valley. The Cincinnati Southern into Kentucky by Lexington, kept by name
  // since Louisville's urban ground changed which way the net runs.
  ['Austin', 'San Antonio'], ['Cincinnati', 'Kentucky'],
  // The White Pass and Yukon Route, Skagway to Whitehorse (1900), now that Skagway is Juneau's again.
  ['Juneau', 'Whitehorse'],
  // Colombia (the historian): the Northern line from Bogota to Barbosa (Tunja), the Girardot and Tolima-Huila lines to
  // Neiva (1937), and the Caldas line from Manizales to the Pacific line (Cali). Bogota's system and the Pacific's
  // met only over the Quindio (1949); Medellin's line ran to the Magdalena at Puerto Berrio, Santander's from Puerto
  // Wilches, and the river joined them.
  ['Bogota', 'Tunja'], ['Bogota', 'Neiva'], ['Cali', 'Manizales'],
  // The Tachira line, from Encontrados on the lake's southern shore up to San Cristobal (closed 1955): kept, since
  // striking Maracaibo to Barquisimeto took it with it.
  ['Maracaibo', 'San Cristobal'],
];
export const CITY_RAIL_CUT = [
  ['Monclova', 'Chihuahua'], // the line from Monterrey to Chihuahua ran by Torreon
  ['Nanyang', 'Chumatien'], // the Peiping-Hankow line ran east of Nanyang, by Yencheng and Chumatien
];
// Lines handed over that no longer run from piece to piece of their two provinces: the course is laid by hand above
// (the line ran through a third province), or the historian struck the line (Nanyang to Hankow: no such line).
export const CITY_RAIL_RELAID = [
  ['Bangkok', 'Phitsanulok'], ['Toungoo', 'Mandalay'], ['Semipalatinsk', 'Alma-Ata'], ['Nanyang', 'Hankow'],
  // The map fact-checker (17 November 2026): lines laid by hand above by the stations they ran through ...
  ['Lashio', 'Myitkyina'], ['Tehran', 'Hamadan'], ['Hamadan', 'Ahwaz'], ['Veracruz', 'Chiapas'], ['Arequipa', 'Tarapaca'],
  ['Elisabethville', 'Luso'], ['Tyumen', 'Tobolsk'], ['Tobolsk', 'Omsk'], ['Abakan', 'Krasnoyarsk'], ['Samarkand', 'Stalinabad'],
  ['Broken Hill', 'Sydney'], ['Enugu', 'Kaduna'], ['Caracas', 'Barquisimeto'],
  // ... and lines not running in September 1939: Shihkiachwang-Tehchow (1940-41); Hangchow-Ningpo (the Chientang
  // bridge blown in December 1937, the Tsao-o gap bridged in 1955); Battambang-Bangkok (1942); Komsomolsk (1940) and
  // Sovetskaya Gavan (1947); the Transandine (cut by the flood of 1934 until 1944); the Yucatan (1950); Santa Cruz
  // (1955, 1957), and no line from Bolivia to Iquique; the Rio-Bahia link (1950); Harbin-Kiamusze (1940); Lima-Cuzco
  // (never joined).
  ['Shihkiachwang', 'Tsinan'], ['Hangchow', 'Ningpo'], ['Battambang', 'Bangkok'], ['Khabarovsk', 'Komsomolsk'],
  ['Komsomolsk', 'Sovetskaya Gavan'], ['Santiago', 'Mendoza'], ['Chiapas', 'Yucatan'], ['La Paz', 'Santa Cruz'],
  ['Potosi', 'Santa Cruz'], ['Potosi', 'Tarapaca'], ['Minas Gerais', 'Bahia'], ['Harbin', 'Kiamusze'], ['Lima', 'Cuzco'],
  // The main line into Manchukuo ran along the coast by Shanhaikwan, not through Jehol: the line from Peiping to
  // Chengteh reached only Huairou in 1938 (opened through in 1960).
  ['Jehol', 'Tientsin'], ['Jehol', 'Peiping'],
  // The historian (17 November 2026): Colombia's systems were not joined; the Borzya-Bayan Tumen line was not yet
  // working in September 1939 (Zhukov's army was supplied by lorry from Borzya); no line joined Maracaibo to
  // Barquisimeto.
  ['Bogota', 'Medellin'], ['Bogota', 'Cali'], ['Bogota', 'Santander'], ['Medellin', 'Santander'], ['Barranquilla', 'Santander'],
  ['Choibalsan', 'Dauria'], ['Maracaibo', 'Barquisimeto'],
];
