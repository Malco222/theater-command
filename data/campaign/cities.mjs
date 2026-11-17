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
];

// Which seas a city stands on where the count of water squares misleads: name -> [sea keys].
export const CITY_SHORES = {};

// A crossing over a strait ('links') whose end is now one particular piece of a province that was cut: [a, b] by name.
export const CITY_LINKS = [];

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
];

// Railways. A line that ran between two provinces runs on between whichever of their pieces lie along it, and every
// city is joined to the net by its easiest border unless it is named here (no railway in 1939).
export const CITY_NO_RAIL = [
  // Canada's north and the Gulf's north shore: the Alaska Highway of 1942 was a road, and the mines flew or shipped.
  'Sept-Iles', 'Fort Chimo', 'Fort Nelson', 'Watson Lake', 'Whitehorse', 'Dawson City', 'Yellowknife', 'Port Radium',
  // Latin America: ports and river towns the railways of 1939 did not reach.
  'Acapulco', 'Bluefields', 'Cap-Haitien', 'Pucallpa', 'Iquitos', 'Leticia', 'Pasto', 'Esmeraldas', 'Puerto La Cruz', 'Maturin', 'Coro',
  'Georgetown', 'Paramaribo', 'Cayenne', 'Tefe', 'Manaus', 'Santarem', 'Macapa', 'Belem', 'Porto Velho', 'Rio Branco', 'Cuiaba',
  'Punta Arenas', 'Rio Gallegos', 'Ushuaia',
];
// Lines laid by hand, each a chain of neighbouring provinces, and lines struck.
export const CITY_RAIL_LINES = [
  // ---- The United States: the trunk lines of 1939 that the joining leaves out ----
  ['Illinois', 'St Louis', 'Memphis', 'Mississippi'], ['Tennessee', 'Memphis', 'Arkansas'],
  ['Cincinnati', 'Louisville', 'Indiana'],
  ['Greenville', 'Atlanta', 'Alabama'], ['Atlanta', 'Chattanooga'],
  ['Florida', 'Miami'], ['Tallahassee', 'Pensacola', 'Alabama'],
  ['Omaha', 'Kansas City', 'Joplin'], ['Duluth', 'Minneapolis', 'Iowa'], ['Minneapolis', 'Sioux Falls'], ['Sioux City', 'Iowa'],
  ['Yuma', 'Tucson', 'El Paso', 'West Texas'], ['El Paso', 'New Mexico'], ['Gallup', 'Flagstaff'],
  ['Reno', 'Elko', 'Ogden', 'Pocatello'], ['Ogden', 'Rock Springs'], ['Boise', 'Pendleton', 'Portland'],
  ['Billings', 'Butte', 'Missoula'], ['Butte', 'Pocatello'],
  ['Lubbock', 'Amarillo'], ['Wichita Falls', 'North Texas'], ['San Antonio', 'Corpus Christi'], ['Shreveport', 'North Texas'],
  ['New York', 'Albany'], ['Baltimore', 'Susquehanna'], ['Toledo', 'Fort Wayne'], ['Toledo', 'Ohio Valley'],
  ['Grand Rapids', 'South Bend'], ['Cheyenne', 'Scottsbluff'], ['Sacramento', 'San Joaquin'],
  // ---- Canada: the Newfoundland Railway, and the Northern Alberta line to Dawson Creek ----
  ['Corner Brook', 'Newfoundland', 'Gander', "St John's"], ['Alberta', 'Grande Prairie', 'Dawson Creek'],
  // ---- Mexico and Central America: Torreon, the junction of the north; the lines to Corinto and Puerto Limon ----
  ['Monterrey', 'Torreon', 'Parral'], ['Durango', 'Torreon', 'Zacatecas'], ['Corinto', 'Managua'], ['Puerto Limon', 'San Jose'],
  // ---- South America ----
  ['La Paz', 'Oruro', 'Potosi'], ['Buenos Aires', 'Mar del Plata'],
];
export const CITY_RAIL_CUT = [
  ['Monclova', 'Chihuahua'], // the line from Monterrey to Chihuahua ran by Torreon
];
