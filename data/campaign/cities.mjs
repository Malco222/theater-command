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
  ['Flagstaff', 'Las Vegas', 'Colorado'], ['Flagstaff', 'Nevada', 'Colorado'], ['Yuma', 'San Diego', 'Colorado'], ['Yuma', 'Baja California', 'Colorado'],
  ['El Paso', 'Chihuahua', 'Rio Grande'], ['Brownsville', 'Monterrey', 'Rio Grande'], ['Burlington', 'Montreal', 'St Lawrence'],
];

// Railways. A line that ran between two provinces runs on between whichever of their pieces lie along it, and every
// city is joined to the net by its easiest border unless it is named here (no railway in 1939).
export const CITY_NO_RAIL = [];
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
];
export const CITY_RAIL_CUT = [];
