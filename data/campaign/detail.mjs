// The world outside Europe drawn to Europe's detail: what is decided by hand about its rivers and lakes.
// The courses themselves come from the Natural Earth survey (public domain, naturalearthdata.com), read by
// tools/survey-rivers.mjs, which writes data/campaign/more-rivers.mjs from that survey and these lists.
//
// The 60 rivers handed over with the chart (world-next-rivers.json) keep their numbers and their names, and every
// crossing they have. On the chart with the cities each is drawn on its real course, and each river, old or new,
// says how great it is ('great', 'middle' or 'small') and what it runs into. A great or middle river is a river in
// the rules, with crossings; a small one is a stream, drawn only.

/**
 * Reaches the survey misnames, or names in a tongue the chart does not use: [the survey's name, lat, lon, the name
 * the reach is read under, how many degrees round that place (4 unless given)]. Two reaches read under one name
 * join into one river where they meet.
 */
export const SURVEY_FIX = [
  ['Columbia', 44.5, -115, 'Snake', 3],
  ['R. des Outaouais', 47.5, -78, 'Ottawa'],
  ['Cowpasture', 37.8, -78.4, 'James (Virginia)', 2],
  ['Red', 48.5, -96.8, 'Red River of the North', 4],
  ['Red', 33, -96, 'Red River of the South', 5],
  ['Churchill', 53, -62.5, 'Hamilton', 4],
  ['Moctezuma', 20.8, -99, 'Panuco', 3],
  ['Pnuco', 22.1, -98.2, 'Panuco', 2],
  ['Jamanxim', -11.6, -55.7, 'Teles Pires', 4],
  ['Uele', 4.1, 20.4, 'Ubangi', 2.5],
  ['Han', 37.6, 127, 'Namhan', 1],
  ['Negro', -32.5, -56.5, 'Negro (Uruguay)', 3],
  ['Negro', -40, -65.5, 'Negro (Patagonia)', 3],
  ['Colorado', -37.5, -66.5, 'Colorado (Argentina)', 5],
  ['Salado', -35, -59.5, 'Salado (Buenos Aires)', 3],
  ['San Juan', -31.8, -68.4, 'San Juan (Cuyo)', 3],
  ['Grande', -17, -65.5, 'Guapay', 3],
  ['Caqueta', 0, -73, 'Japura', 4],
  ['Culuene', -13.2, -54, 'Xingu', 4],
  ['Juruena', -12.7, -58.9, 'Tapajos', 4],
  ['Corantijn', 4.5, -57.4, 'Courantyne', 3],
  ['Damqogkanbab', 29.3, 84.5, 'Tsangpo', 3],
  ['Brahmaputra', 29.3, 92, 'Tsangpo', 1.5],
  ['Dihang', 29.2, 95, 'Tsangpo', 2],
  ['Sanggan', 39.7, 114, 'Yongding', 3],
  ['Nanpan', 25.5, 105.5, 'Hongshui', 3],
  ['Esil', 52, 70, 'Ishim', 5],
  ['Mackenzie', -24.4, 149.3, 'Mackenzie (Queensland)', 3],
  ['Benoue', 8.5, 13, 'Benue'],
  ['Bandama Blanc', 8.8, -5.8, 'Bandama'],
  ['Baoule', 14, -9, 'Baoule (Kayes)', 2.5],
  ['Shabeelle', 3, 44, 'Shebele'],
  ['Genale', 5.5, 40.5, 'Jubba'],
  ['Cubango', -15.3, 16.8, 'Okavango', 5],
  ['Kibali', 3, 29.5, 'Uele', 3],
];

/** The survey's name of a lake, then the chart's (a 1939 atlas's). */
/** A line of the survey that is two rivers: it is cut at the place given, and SURVEY_FIX names the parts. */
export const SURVEY_SPLIT = [
  ['Uele', 4.1, 22.4],
];

export const LAKE_NAMES = [
  ['Great Slave Lake', 'Great Slave Lake'], ['Great Bear Lake', 'Great Bear Lake'], ['Lake Athabasca', 'Lake Athabasca'],
  ['Lago de Nicaragua', 'Lake Nicaragua'], ['Lago Titicaca', 'Lake Titicaca'], ['Lake Balkhash', 'Lake Balkhash'],
  ['Issyk-Kul', 'Issyk Kul'], ['Qinghai Hu', 'Koko Nor'], ['Lake Chad', 'Lake Chad'], ['Lake Turkana', 'Lake Rudolf'],
  ['Lake Albert', 'Lake Albert'], ['Lake Tana', 'Lake Tana'], ['Great Salt Lake', 'Great Salt Lake'],
  ['Poyang Hu', 'Poyang Lake'], ['Dongting Hu', 'Tungting Lake'], ['Lake Khanka', 'Lake Khanka'], ['Lake Urmia', 'Lake Urmia'],
  ['Lake Taupo', 'Lake Taupo'], ['Hongze Hu', 'Hungtse Lake'], ['Tai Hu', 'Tai Lake'], ['Bosten Hu', 'Bagrash Kol'],
  ['Hulun Nuur', 'Dalai Nor'], ['Uvs Nuur', 'Ubsa Nor'], ['Lac Mai-Ndombe', 'Lake Leopold II'], ['Lake Edward', 'Lake Edward'],
  ['Lake Kivu', 'Lake Kivu'], ['Lake Kyoga', 'Lake Kyoga'], ['Lake Rukwa', 'Lake Rukwa'], ['Lake Bangweulu', 'Lake Bangweulu'],
  ['Lac Saint-Jean', 'Lake St John'], ['Lake Mweru', 'Lake Mweru'], ['Lac Moeru', 'Lake Mweru'], ['Lake Abbe', 'Lake Abbe'],
  ['Lago de Chapala', 'Lake Chapala'],
];

/** The survey's names for a river of the chart, where it does not carry the chart's own name. */
export const RIVER_ALIASES = {
  'St Lawrence': ['St. Lawrence'],
  Amazon: ['Amazonas'],
  Paraguay: ['Parana'],
  'Yellow River': ['Huang', 'Yellow'],
  'West River': ['Xi', 'Xun', 'Yu', 'Yong'],
  Argun: ['Ergun'],
  Ussuri: ['Ussuri', 'Wusuli'],
  Sungari: ['Sungari', "Di'er Songhua"],
  'Red River': ['Red'],
  Salween: ['Salween', 'Nu'],
  Sittang: ['Sittong'],
  Irrawaddy: ['Irrawaddy'],
  'Middle Volga': ['Volga'],
  'Lower Volga': ['Volga'],
  Irtysh: ['Irtysh', 'Ertis'],
  Yenisei: ['Yenisey', 'Verkhniy Yenisey'],
  'Amu Darya': ['Amu Darya', 'Panj'],
  'Shatt al-Arab': ['Shatt al Arab'],
  'Upper Nile': ['Nile'],
  'White Nile': ['White Nile', 'Mountain Nile', 'Albert Nile', 'Victoria Nile'],
  'Blue Nile': ['Blue Nile', 'Abay'],
  Ubangi: ['Ubangi', 'Bomu'],
};

/**
 * A river that had left its bed in September 1939: its real course is followed down to the place given, and from
 * there the course handed over runs on. (The Yellow River's dykes were cut at Huayuankow in June 1938, and until
 * 1947 it ran south-east into the Hwai; the survey draws the bed of today, past Tsinan.)
 * (The Brahmaputra ends in the Ganges at Goalundo: the survey gives its name to the Padma below, down to the Meghna.)
 */
export const RIVER_LEAVE = { 'Yellow River': [34.9, 113.7], Brahmaputra: [23.84, 89.78] };
/**
 * The foot of a river handed over, where neither the survey nor the course handed over is its course of 1939:
 * places by hand ([lat, lon]) and reaches of the survey's lines ([the survey's name, lat, lon]: that line, followed
 * from where the course stands to the place named).
 *
 * The Yellow River (the historian, 17 November 2026): from the breach at Huayuankow in June 1938 until it was
 * closed in March 1947 the river ran south-east across Honan down the Chialu and the Ying, past Chowkiakow and
 * Fuyang, into the Hwai at Chengyangkwan; with the Hwai past Pengpu into Hungtse Lake; and from the lake south
 * through Kaoyu Lake and the Grand Canal to the Yangtze at Sanchiangying. The old mouth in north Kiangsu had been
 * dry since 1855. (The survey calls the Ying 'Quan' above Taiho, and the Hwai below Chengyangkwan 'Hudi'.)
 */
export const RIVER_FOOT = {
  'Yellow River': [[34.72, 114.02], [34.41, 114.19], [34.06, 114.39], [33.78, 114.52], ['Quan', 33.2, 115.6], ['Ying', 32.5, 116.5], ['Hudi', 32.4, 119.6]],
};
/**
 * Rivers the survey does not have, drawn by hand from the atlas: name, how great, into what, the course from head
 * to foot. The Karun is a river (the invasion of Persia in August 1941 crossed it at Ahwaz and Khorramshahr; the
 * design director, 17 November 2026); the others are streams.
 */
export const RIVER_HAND = [
  ['Karun', 'middle', 'Shatt al-Arab', [[32.3, 50.15], [31.95, 50.45], [31.6, 50.47], [31.75, 50.2], [31.8, 50.09], [31.95, 49.85], [32.05, 49.6], [32.03, 49.4], [32.15, 49.15], [32.27, 48.93], [32.2, 48.82], [32.05, 48.85], [31.65, 48.88], [31.48, 48.87], [31.32, 48.68], [31.0, 48.55], [30.74, 48.42], [30.55, 48.3], [30.43, 48.18]]],
  ['Kabul River', 'small', 'Indus', [[34.45, 68.4], [34.5, 68.8], [34.53, 69.17], [34.59, 69.76], [34.5, 70.1], [34.43, 70.45], [34.3, 70.8], [34.22, 71.03], [34.16, 71.4], [34.05, 71.7], [34.0, 71.98], [33.9, 72.23]]],
  ['Min (Szechwan)', 'small', 'Yangtze', [[32.64, 103.6], [32.1, 103.75], [31.68, 103.85], [31.48, 103.58], [31.0, 103.62], [30.72, 103.75], [30.42, 103.82], [30.05, 103.85], [29.75, 103.8], [29.57, 103.76], [29.21, 103.95], [28.95, 104.3], [28.77, 104.63]]],
  ['Murrumbidgee', 'small', 'Lachlan', [[-35.6, 148.6], [-36.0, 149.0], [-35.6, 149.1], [-35.3, 148.95], [-35.0, 148.8], [-35.0, 148.5], [-35.07, 148.1], [-35.1, 147.37], [-34.95, 146.95], [-34.75, 146.55], [-34.6, 145.9], [-34.5, 144.85], [-34.47, 144.3], [-34.4, 143.95]]],
  ['Taedong', 'small', 'Yellow Sea', [[40.3, 127.1], [40.0, 126.7], [39.75, 126.3], [39.42, 125.93], [39.2, 125.85], [39.02, 125.75], [38.85, 125.65], [38.75, 125.63], [38.72, 125.38], [38.68, 125.2]]],
];

/** Rivers of the chart that are one reach of a longer river: the real course is cut to the reach as handed over. */
export const RIVER_TRIM = ['St Lawrence', 'Middle Volga', 'Lower Volga', 'Upper Nile', 'White Nile', 'Blue Nile', 'Shatt al-Arab', 'Paraguay', 'Parana'];

/** The survey's name, then the name the chart gives the river (a 1939 atlas's); with a place where two share one. */
export const RIVER_NAMES = [
  ['Great Pee Dee', 'Pee Dee'], ['James', 45, -98, 'James (Dakota)'], ['Saint John', 'St John'], ['St.-Maurice', 'St Maurice'],
  ['Ro Grande de Santiago', 'Santiago'], ['Ro Grande de Matagalpa', 'Rio Grande de Matagalpa'], ['Ula', 'Ulua'],
  ['Negro', 'Rio Negro'], ['Guainia', 'Guaviare'], ['Jiparana', 'Ji-Parana'], ['Amapari', 'Araguari'],
  ['Yalong', 'Yalung'], ['Yarkant', 'Yarkand'], ['Jialing', 'Kialing'], ['Hongshui', 'Hungshui'], ['Black', 23, 103, 'Black River'],
  ['Dadu', 'Tatu'], ['Orhon', 'Orkhon'], ['Xiang', 'Siang'], ['Gan', 'Kan'], ['Nen', 'Nonni'], ['Huai', 'Hwai'],
  ['Dong', 'East River'], ['Bei', 'North River'], ['You', 'Yu'], ['Xar Moron', 'Shara Muren'], ['Xiliao', 'West Liao'],
  ['Konqi', 'Konche'], ['Yongding', 'Yungting'], ['Fuchun', 'Tsientang'], ['Luan', 'Lwan'],
  ['Nakdong', 'Naktong'], ['Namhan', 'Han (Korea)'],
  ["Anadyr'", 'Anadyr'], ["Tom'", 'Tom'], ["Amgun'", 'Amgun'], ['Ile', 'Ili'], ['Zarafshon', 'Zeravshan'], ['Harirud', 'Hari Rud'],
  ['Atrak', 'Atrek'], ['Nizhnyaya Tunguska', 'Lower Tunguska'], ['Oka', 'Oka (Siberia)'], ['Vilyuy', 'Vilyui'],
  ['Mae Klong', 'Meklong'], ['Kong', 'Se Kong'], ['Ou', 21, 102, 'Nam Ou'], ['Nmai', 'Nmai Hka'], ['Indragi', 'Indragiri'],
  ['Balak', 'Surma'], ['Masi', 'Musi'],
  ['Yamuna', 'Jumna'], ['Krishna', 'Kistna'], ['Ghaghara', 'Gogra'], ['Narmada', 'Narbada'], ['Mahana Nadi', 'Mahanadi'],
  ['Sapt', 'Kosi'], ['Tapi', 'Tapti'],
  ['Chari', 'Shari'], ['Shebele', 'Webi Shebeli'], ['Jubba', 'Juba'], ['Komoe', 'Comoe'], ['Ogooue', 'Ogowe'], ['Cunene', 'Kunene'],
  ['Tekeze', 'Takkaze'],
];

/** Survey rivers left out: [name] or [name, lat, lon] where two share one. */
export const RIVER_SKIP = [
  ['Tongtian'], ['Za'], ['Shiquan'], ['Ideriyn'], ['Shishhid Gol'], ['Braco Menor'], ['Indaia'], ['Madison'], ['Pit'], ['Holston'],
  ['Katanga'], ['Mackenzie (Queensland)'], ['Baoule (Kayes)'], ['N. Fork Kuskokwim'], ['Finlay'], ['Bell'], ['Lom'], ['Auob'],
  ['Molopo'], ['Irrawaddy Delta'], ['Thelon'], ['Back'], ['Coppermine'], ['Kheta'], ['Taz'], ['Olenek'],
];

/** Survey rivers taken though they are under the bar: [name, lat, lon]. */
export const RIVER_PICK = [
  ['Potomac', 39.2, -77.7], ['Delaware', 41, -75], ['Illinois', 40, -89.5], ['James (Virginia)', 37.8, -78.4], ['Sabine', 31.5, -95],
  ['Saguenay', 49.5, -72], ['St.-Maurice', 47.3, -73.3],
  ['Lerma', 19.7, -101.2], ['Yaqui', 29.2, -110], ['Panuco', 20.8, -99], ['Motagua', 15.3, -89.6], ['Lempa', 14, -89], ['Ula', 15, -87.8],
  ['Cauca', 6, -75.6], ['Limay', -40, -69.6], ['Courantyne', 4.5, -57.4], ['Maroni', 4.4, -54.1],
  ['Fen', 37.2, 111.4], ['Huai', 32.4, 115], ['Luan', 40.6, 118], ['Min', 27, 118.3], ['Bei', 24.2, 112.6], ['Fuchun', 30, 119], ['Yongding', 39.7, 115.5],
  ['Kerulen', 48.7, 113], ['Nakdong', 36, 129], ['Namhan', 37.5, 128], ['Tone', 36.3, 140], ['Ishikari', 43.4, 142.2],
  ['Belaya', 54.8, 55.7],
  ['Mun', 14.8, 103.7], ['Cagayan', 17.2, 121.5], ['Pahang', 4, 103], ['Kaladan', 22, 93.2], ['Balak', 24.5, 92.5], ['Masi', -2.9, 103.8],
  ['Son', 24.2, 83.4], ['Ravi', 31.4, 74.3], ['Gandak', 27.4, 84.6], ['Beas', 31.8, 76.1], ['Tista', 26.5, 89.1],
  ['Rufiji', -8.5, 38.3], ['Tugela', -29, 30.2], ['Pangani', -4.4, 38.1], ['Sobat', 8.6, 32.2], ['Tekeze', 13, 38.5],
];

/** The great rivers: a ribbon seen from every height. */
export const RIVER_GREAT = [
  'Mississippi', 'Missouri', 'St Lawrence', 'Mackenzie', 'Yukon', 'Amazon', 'Rio Negro', 'Madeira', 'Parana', 'Orinoco',
  'Yangtze', 'Yellow River', 'Amur', 'Mekong', 'Irrawaddy', 'Ganges', 'Brahmaputra', 'Indus', 'Lena', 'Ob', 'Yenisei',
  'Middle Volga', 'Lower Volga', 'Upper Nile', 'White Nile', 'Niger', 'Congo', 'Zambezi',
  // (For the look alone, the art director: great and middle are the same in the rules.)
  'Ohio', 'Columbia', 'Irtysh',
];

/**
 * What is drawn as a river is a river in the rules, with its crossings, as in Europe; the rest are streams, drawn
 * for the look of the ground and no obstacle. A river of the survey is a river where it is one of the world's
 * long ones (its course on the chart at least this long, in map units: 400 is about 1,100 km) and does not run
 * through empty country, or where the lists below name it: a river an army had to cross in the war, or the chief
 * river of a peopled land. The four figures are for the survey's ranks 5 and over, 6, 7 and 8. Europe has 12.6
 * crossings to 100 borders; the lists are kept so that the peopled lands outside it come out near that.
 */
export const RIVER_BAR = [400, 400, 400, 400];

/** Rivers the rules count though they are short of the bar, by part of the world; and streams though they are over it. */
export const RIVER_RIVERS = [
  // United States and Canada
  'Tennessee', 'Hudson', 'Potomac', 'Sacramento', 'Ottawa', 'Saskatchewan',
  // Mexico and Central America
  'Balsas', 'Santiago', 'Panuco', 'Usumacinta', 'San Juan',
  // South America
  'Cauca',
  // China and Manchuria: the Hwai and the Hsuchow fighting of 1938, the Siang and the battles of Changsha, the Kan
  // and Nanchang, the Yuan and Changteh, the Yungting and the Marco Polo Bridge, the Fen and Taiyuan, the Nonni
  // and Tsitsihar, the East River between Canton and Hong Kong
  'Hwai', 'Siang', 'Kan', 'Yuan', 'Kialing', 'Fen', 'Wei', 'Yungting', 'Nonni', 'East River',
  // South-East Asia and the islands: the Meklong and the Burma railway, the Kaladan and Arakan, the Cagayan
  // valley of Luzon, the Sepik of New Guinea
  'Meklong', 'Kaladan', 'Cagayan', 'Sepik',
  // Soviet Asia
  'Zeya', 'Selenga', 'Belaya', 'Tom',
  // India, Persia and Afghanistan
  'Chenab', 'Kistna', 'Godavari', 'Narbada', 'Helmand',
  // Australia
  'Darling', 'Lachlan',
  // Africa: the Juba, the Webi Shebeli, the Awash and the Omo of the East African campaign of 1941, the Atbara
  // and Kassala, the Tana that covered Kenya, the Ogowe and Lambarene in 1940, the Shari and Fort Lamy
  'Awash', 'Omo', 'Atbara', 'Tana', 'Ogowe', 'Shari', 'Benue', 'Uele', 'Vaal', 'Volta', 'Ruvuma', 'Kafue',
];
export const RIVER_STREAMS = ['Salado', 'Chuna'];

/** What a river runs into, where the survey cannot tell: a river, a lake, a sea, or a name for inland water. */
export const RIVER_INTO = {
  'Middle Volga': 'Volga', 'Lower Volga': 'Caspian Sea', Kama: 'Volga', Ural: 'Caspian Sea', 'Upper Nile': 'Nile',
  'White Nile': 'Upper Nile', 'Blue Nile': 'Upper Nile', 'Shatt al-Arab': 'Persian Gulf', Brahmaputra: 'Ganges',
  Colorado: 'Gulf of California', Paranaiba: 'Parana', Tarim: 'Lop Nor', Helmand: 'Hamun Lake', 'Hari Rud': 'the Kara Kum',
  Zeravshan: 'the Kyzyl Kum', Atrek: 'Caspian Sea', Vychegda: 'Northern Dvina', Chattahoochee: 'Gulf of Mexico',
  Siang: 'Tungting Lake', Kan: 'Poyang Lake', Yuan: 'Tungting Lake', Anadyr: 'Bering Sea', Okavango: 'the Okavango Swamp',
  Shari: 'Lake Chad', Kerulen: 'Dalai Nor', Awash: 'Lake Abbe', Omo: 'Lake Rudolf',
  Mackenzie: 'Beaufort Sea', Lena: 'Laptev Sea', Yenisei: 'Kara Sea', Ob: 'Kara Sea', Kolyma: 'East Siberian Sea',
  Indigirka: 'East Siberian Sea', Kamchatka: 'Bering Sea', Ottawa: 'St Lawrence', 'St John': 'Bay of Fundy',
  Saguenay: 'St Lawrence', Hamilton: 'Labrador Sea', Panuco: 'Gulf of Mexico', Yaqui: 'Gulf of California',
  Chixoy: 'Usumacinta', Xingu: 'Amazon', Apure: 'Orinoco', Hungshui: 'West River', Hwai: 'Yellow River', Surma: 'Ganges', Tatu: 'Yangtze',
  Nonni: 'Sungari', Yungting: 'Yellow Sea', Hailar: 'Argun', Yu: 'West River', 'Han (Korea)': 'Yellow Sea', Yalu: 'Yellow Sea',
  Mekong: 'South China Sea', Godavari: 'Bay of Bengal', Cuando: 'Zambezi', Ogowe: 'Gulf of Guinea', Tshuapa: 'Congo',
  Baoule: 'Bani', Semliki: 'Lake Albert', 'Webi Shebeli': 'the Shebeli Swamps',
};
/**
 * Rivers that begin where they leave a lake, where the survey runs their line up the lake's length and on to a
 * stream above it (the art director, 17 November 2026: a river's name is never written over a lake). The San Juan
 * leaves Lake Nicaragua at San Carlos, the Semliki leaves Lake Edward at its north end, the Winnipeg leaves the Lake
 * of the Woods at Kenora (Rat Portage), the Santiago leaves Lake Chapala at Ocotlan.
 */
export const RIVER_FROM_LAKE = { 'San Juan': 'Lake Nicaragua', Semliki: 'Lake Edward', Winnipeg: 'Lake of the Woods', Santiago: 'Lake Chapala' };
/**
 * Where a river's foot meets a river at the edge of the map of Europe, which the chart cannot see: a place ([lat,
 * lon]) the course ends at, or a river of the chart it ends on (the art director, 17 November 2026). The Middle Volga
 * ends where the map of Europe's Volga begins on its eastern edge; the Kama falls into the Volga below Kazan, on the
 * chart's own reach of it.
 */
export const RIVER_MEET = { 'Middle Volga': [52.5, 47.0013], Kama: 'Middle Volga' };
/**
 * A river that takes the lower course of the river it falls into, where the survey gives the lower course the wrong
 * name: { taker: the river whose name the survey gave }. Below Oxley the river is the Murrumbidgee, which the Lachlan
 * joins; the survey calls the whole of it the Lachlan (the map fact-checker, 17 November 2026). The taker takes the
 * class of the lower course too.
 */
export const RIVER_TAKE = { Murrumbidgee: 'Lachlan' };

/** Rivers that run dry for part of the year: drawn as a broken line. */
export const RIVER_SEASONAL = ['Barwon', 'Gila', 'Fitzroy', 'Ord', 'Victoria', 'Mitchell', 'Desaguadero', 'Atrek', 'Hari Rud', 'Konche', 'Shara Muren', 'Webi Shebeli', 'Atbara', 'Darling', 'Lachlan', 'Limpopo', 'Tarim'];

/**
 * A river the survey draws against the stream, where nothing on the chart shows it; and rivers handed over foot
 * first (the Nile's three reaches from the north, the Yalu from its mouth), which are turned to run from the head.
 */
export const RIVER_FLIP = ['Waikato', 'Hailar', 'Yalu', 'Upper Nile', 'White Nile', 'Blue Nile'];

/** New crossings struck and added by hand on the chart with the cities: ['Province', 'Province', 'River']. */
export const MORE_RIVER_CUT = [];
export const MORE_RIVER_ADD = [
  // The Karun lies between Ahwaz and Abadan (the island stands between it, the Shatt and the Bahmanshir): Ahwaz
  // stands on its bank, so the test does not see it. The Indian troops of August 1941 crossed it at Khorramshahr.
  ['Ahwaz', 'Abadan', 'Karun'],
];

// ---- Lakes -----------------------------------------------------------------------------------------------------------

/**
 * The lakes cut out of the chart with the cities, beside the eleven it was handed over with: where the survey has
 * the lake ('lakes' its lakes of today, 'historic' a lake at its old shore, 'playas' its dry pans), the survey's
 * name, the name on the chart (an atlas of 1939's), its water ('fresh', 'salt', or 'pan' for a salt pan that
 * seldom holds water) and the height of its surface above the sea in metres. No reservoir later than 1939 is here.
 * Lake Chad and Lop Nor are at their old shores: Chad has shrunk to a tenth since, and Lop Nor held water from
 * 1921 until the 1960s.
 */
/**
 * Shores laid by hand, as [lat, lon] points, in place of the survey's: the survey draws these lakes as they are
 * today, after sixty years of dykes and silt. Both are at the summer shore of an atlas of the 1930s (the design
 * director, the art director and the historian, 17 November): Tungting about 4,700 square km (4,700 surveyed in
 * 1932, 4,350 in 1949; today's survey shows 720), from Yochow and the Siang's mouth round by Yuankiang and Hanshou
 * to the delta of Nanhsien; Poyang about 4,700 (5,000 at high water in the 1940s; the survey shows 2,050), from the
 * neck at Hukow to Poyang town, Yukan and the Kan's delta below Nanchang.
 */
export const LAKE_HAND = {
  'Tungting Lake': [[29.42, 113.06], [29.34, 113.04], [29.2, 113.02], [29.03, 112.98], [28.87, 112.93], [28.77, 112.8], [28.72, 112.55], [28.74, 112.3], [28.81, 112.1], [28.9, 111.98], [29.05, 111.96], [29.2, 112.04], [29.28, 112.2], [29.2, 112.38], [29.08, 112.54], [29.07, 112.72], [29.2, 112.83], [29.36, 112.87], [29.46, 112.96]],
  'Poyang Lake': [[29.74, 116.21], [29.55, 116.21], [29.38, 116.22], [29.26, 116.3], [29.2, 116.5], [29.11, 116.67], [28.95, 116.7], [28.78, 116.62], [28.66, 116.46], [28.55, 116.3], [28.58, 116.14], [28.7, 116.06], [28.95, 116.03], [29.15, 115.99], [29.3, 116.03], [29.45, 116.05], [29.6, 116.1], [29.72, 116.14]],
};
export const MORE_LAKE_LIST = [
  // North America
  ['lakes', 'Great Bear Lake', 'Great Bear Lake', 'fresh', 156],
  ['lakes', 'Great Slave Lake', 'Great Slave Lake', 'fresh', 156],
  ['lakes', 'Lake Athabasca', 'Lake Athabasca', 'fresh', 213],
  ['lakes', 'Reindeer Lake', 'Reindeer Lake', 'fresh', 337],
  ['lakes', 'Dubawnt Lake', 'Dubawnt Lake', 'fresh', 236],
  ['lakes', 'Lake Winnipegosis', 'Lake Winnipegosis', 'fresh', 254],
  ['lakes', 'Lake Manitoba', 'Lake Manitoba', 'fresh', 248],
  ['lakes', 'Lake Nipigon', 'Lake Nipigon', 'fresh', 260],
  ['lakes', 'Lake of the Woods', 'Lake of the Woods', 'fresh', 323],
  ['lakes', 'Iliamna Lake', 'Iliamna Lake', 'fresh', 14],
  ['lakes', 'Great Salt Lake', 'Great Salt Lake', 'salt', 1280],
  ['lakes', 'Lake Champlain', 'Lake Champlain', 'fresh', 29],
  ['lakes', 'Lake Okeechobee', 'Lake Okeechobee', 'fresh', 4],
  ['lakes', 'Lago de Chapala', 'Lake Chapala', 'fresh', 1524],
  ['lakes', 'Lago de Nicaragua', 'Lake Nicaragua', 'fresh', 32],
  // South America
  ['lakes', 'Lago Titicaca', 'Lake Titicaca', 'fresh', 3812],
  ['lakes', 'Lago Poopo', 'Lake Poopo', 'salt', 3686],
  ['playas', 'Salar de Uyuni', 'Salar de Uyuni', 'pan', 3656],
  ['lakes', 'Mar Chiquita', 'Mar Chiquita', 'salt', 71],
  ['lakes', 'Lagoa Mirim', 'Lagoa Mirim', 'fresh', 1],
  // Africa
  ['historic', 'Lake Chad', 'Lake Chad', 'fresh', 282],
  ['lakes', 'Lake Turkana', 'Lake Rudolf', 'salt', 375],
  ['lakes', 'Lake Albert', 'Lake Albert', 'fresh', 619],
  ['lakes', 'Lake Edward', 'Lake Edward', 'fresh', 912],
  ['lakes', 'Lake Kivu', 'Lake Kivu', 'fresh', 1460],
  ['lakes', 'Lake Kyoga', 'Lake Kyoga', 'fresh', 1033],
  ['lakes', 'Lake Tana', 'Lake Tana', 'fresh', 1788],
  ['lakes', 'Lac Moeru', 'Lake Mweru', 'fresh', 917],
  ['lakes', 'Lake Bangweulu', 'Lake Bangweulu', 'fresh', 1140],
  ['lakes', 'Lake Rukwa', 'Lake Rukwa', 'salt', 800],
  ['lakes', 'Lac Mai-Ndombe', 'Lake Leopold II', 'fresh', 300],
  ['playas', 'Etosha Pan', 'Etosha Pan', 'pan', 1080],
  ['playas', 'Makgadikgadi Pans', 'Makarikari Pan', 'pan', 900],
  // Asia
  ['lakes', 'Lake Balkhash', 'Lake Balkhash', 'salt', 341],
  ['lakes', 'Issyk-Kul', 'Issyk Kul', 'salt', 1607],
  ['lakes', 'Alakol', 'Ala Kul', 'salt', 347],
  ['lakes', 'Tengiz Koli', 'Lake Tengiz', 'salt', 304],
  ['lakes', 'Lake Chany', 'Lake Chany', 'salt', 106],
  ['lakes', 'Lake Khanka', 'Lake Khanka', 'fresh', 69],
  ['lakes', 'Hulun Nuur', 'Dalai Nor', 'fresh', 545],
  ['lakes', 'Uvs Nuur', 'Ubsa Nor', 'salt', 759],
  ['lakes', 'Khovsgol Nuur', 'Kosogol', 'fresh', 1645],
  ['lakes', 'Qinghai Hu', 'Koko Nor', 'salt', 3195],
  ['lakes', 'Nam Co', 'Tengri Nor', 'salt', 4718],
  ['lakes', 'Bosten Hu', 'Bagrash Kol', 'fresh', 1048],
  ['historic', 'Lop Nur', 'Lop Nor', 'salt', 780],
  ['playas', 'Hamun-e Saberi', 'Hamun Lake', 'fresh', 475],
  ['lakes', 'Tai Hu', 'Tai Lake', 'fresh', 3],
  ['lakes', 'Hongze Hu', 'Hungtse Lake', 'fresh', 12],
  ['lakes', 'Poyang Hu', 'Poyang Lake', 'fresh', 16],
  ['lakes', 'Dongting Hu', 'Tungting Lake', 'fresh', 33],
  ['lakes', 'Tonle Sap', 'Tonle Sap', 'fresh', 5],
  ['lakes', 'Danau Toba', 'Lake Toba', 'fresh', 905],
  ['lakes', 'Biwa Ko', 'Lake Biwa', 'fresh', 85],
  // Australia and New Zealand
  ['lakes', 'Lake Eyre North', 'Lake Eyre', 'pan', -12],
  ['lakes', 'Lake Torrens', 'Lake Torrens', 'pan', 30],
  ['lakes', 'Lake Gairdner', 'Lake Gairdner', 'pan', 121],
  ['lakes', 'Lake Frome', 'Lake Frome', 'pan', 1],
  ['lakes', 'Lake Mackay', 'Lake Mackay', 'pan', 355],
  ['lakes', 'Lake Taupo', 'Lake Taupo', 'fresh', 356],
];

/** The water of the lakes the chart was handed over with (fresh where not named). */
export const OLD_LAKE_WATER = { 'Aral Sea': 'salt' };
/**
 * The eleven lakes the chart was handed over with, redrawn from the same survey and at the same grain as the sixty
 * (the art director, 17 November: Michigan wants its Green Bay, Huron its Georgian Bay, Superior its Keweenaw,
 * Winnipeg is not a teardrop): where the survey has the lake, its name there, and the name on the chart. The Aral
 * Sea is at its old shore, before the rivers were taken off it in the 1960s.
 */
export const OLD_LAKE_SURVEY = [
  ['lakes', 'Lake Superior', 'Lake Superior'], ['lakes', 'Lake Michigan', 'Lake Michigan'], ['lakes', 'Lake Huron', 'Lake Huron'],
  ['lakes', 'Lake Erie', 'Lake Erie'], ['lakes', 'Lake Ontario', 'Lake Ontario'], ['lakes', 'Lake Winnipeg', 'Lake Winnipeg'],
  ['lakes', 'Lake Baikal', 'Lake Baikal'], ['historic', 'Aral Sea', 'Aral Sea'], ['lakes', 'Lake Victoria', 'Lake Victoria'],
  ['lakes', 'Lake Tanganyika', 'Lake Tanganyika'], ['lakes', 'Lake Malawi', 'Lake Nyasa'],
];

// ---- Ground laid by ruling -----------------------------------------------------------------------------------
// Ground the design director has ruled, by province name, on the chart with the cities only. It stands over the
// seed and over the clerk's tables until their rows say the same (carve-cities.mjs counts what it changed).
const ground = (g, names) => Object.fromEntries(names.split(', ').map((n) => [n, g]));
export const RULED_GROUND = {
  // Ruling 212 (17 November): the true deserts outside Africa, where most of the ground is sand, gravel or salt even
  // if the chief place is an oasis. Arid steppe and dry hills stay what they are. The builder's reading less Tennant
  // Creek (grassland east of the town: 47% of its ground), and the historian's 23
  // (/mnt/project-files/historian/deserts-212-review.md). Left as they are: Nevada, Monclova, Nogales, Changtang,
  // Gartok, Leh, and the deserts the map of Europe already carries.
  ...ground('desert', 'Etsin Gol, Hami, Turfan, Anhsi, Khotan, Charkhlik, Tsaidam, Gobi, Sain Shand'),
  ...ground('desert', 'Krasnovodsk, Kizyl-Arvat, Khiva, Nukus, Aralsk, Kzyl-Orda, Fort Shevchenko, Balkhash, Dzhezkazgan'),
  ...ground('desert', 'Yazd, Zahedan, Birjand, Kandahar, Riyadh, Buraida, Hail, Tabuk, Medina, Yanbu, Dhahran, Rub al Khali, Kuwait, Doha, Sharjah, Seiyun'),
  ...ground('desert', 'Bikaner, Jodhpur, Bahawalpur, Pasni'),
  ...ground('desert', 'Kalgoorlie, Nullarbor, Oodnadatta, Alice Springs, Marble Bar, Meekatharra, Marree'),
  ...ground('desert', 'Yuma, Las Vegas, Arica, Tacna, Chuquicamata, Chimbote, Chiclayo'),
  ...ground('desert', 'Kashgar, Yarkand, Aksu, Yumen, Ningsia, Merv, Chardzhou, Bukhara, Jeddah, Hodeida, Aden, Mukalla, Mirpur Khas'),
  ...ground('desert', 'Tucson, El Paso, Guaymas, Sonora, Torreon, Baja California, Southern Baja, Antofagasta, Tarapaca, Atacama'),
  // Two towns of part 4 were cut out of a province the ruling names, and hold most of the desert it was named for:
  // Girishk the Dasht-i-Margo and the Registan (out of Kandahar), Hofuf the sands round the Hasa oasis (out of
  // Dhahran). The builder's reading, for the director and the historian to strike.
  ...ground('desert', 'Girishk, Hofuf'),
  // Ruling 215 (17 November), on the historian's word: the Loushan pass on the road from Kweiyang to Chungking.
  Tsunyi: 'mountain',
  // The design director's log b340 (17 November), on the map fact-checker's word: the Yellow River's flood of June 1938
  // to 1947 lay across eastern Honan and northern Anhwei as marsh, the front line until 1944. Fowyang is its heart.
  Fowyang: 'marsh',
};
