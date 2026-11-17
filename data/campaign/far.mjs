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

  // ==== The lands round the two roads. Everything from here on follows the islands in number, so the islands'
  // numbers never move.
  // ---- Japan. The Kanto plain is two provinces, the capital and the bay's mouth with the navy's yard; Osaka's
  // has Kobe; Kyoto's reaches the Sea of Japan at Maizuru. Tsushima goes with Fukuoka, Sado with Niigata.
  ['Sapporo', 43.06, 141.35, 'JAP', 'forest', 1, 0, { port: 1 }],
  ['Kushiro', 43.1, 144.3, 'JAP', 'forest', 0, 0, { port: 1 }],
  ['Aomori', 40.75, 140.8, 'JAP', 'forest', 0, 0, { port: 1 }],
  ['Akita', 39.72, 140.15, 'JAP', 'hills', 0, 0, { port: 1, oil: 1 }],
  ['Sendai', 38.27, 140.8, 'JAP', 'plains', 1, 0, { port: 1 }],
  ['Niigata', 37.85, 139.1, 'JAP', 'plains', 1, 0, { port: 1 }],
  ['Tokyo', 35.72, 139.65, 'JAP', 'urban', 6, 3, { port: 1, cap: 1 }],
  ['Yokohama', 35.4, 139.58, 'JAP', 'urban', 2, 1, { port: 1, fort: [2, 'USA', 'UK'] }],
  ['Nagano', 36.65, 138.19, 'JAP', 'mountain', 0, 0, {}],
  ['Shizuoka', 35.0, 138.38, 'JAP', 'hills', 1, 0, { port: 1 }],
  ['Nagoya', 35.18, 136.95, 'JAP', 'urban', 2, 2, { port: 1 }],
  ['Kanazawa', 36.56, 136.7, 'JAP', 'hills', 0, 0, { port: 1 }],
  ['Kyoto', 35.01, 135.77, 'JAP', 'urban', 2, 0, { port: 1, fort: [1, 'USA', 'UK', 'SOV'] }],
  ['Osaka', 34.69, 135.5, 'JAP', 'urban', 3, 4, { port: 1, fort: [1, 'USA', 'UK'] }],
  ['Okayama', 34.7, 133.92, 'JAP', 'hills', 0, 0, { port: 1 }],
  ['Hiroshima', 34.42, 132.46, 'JAP', 'hills', 2, 1, { port: 1, fort: [1, 'USA', 'UK'] }],
  ['Shimonoseki', 34.08, 131.05, 'JAP', 'hills', 0, 0, { port: 1, fort: [2, 'USA', 'UK', 'SOV'] }],
  ['Shikoku', 33.85, 133.3, 'JAP', 'mountain', 0, 0, { port: 1 }],
  ['Fukuoka', 33.59, 130.45, 'JAP', 'hills', 2, 2, { port: 1 }],
  ['Nagasaki', 32.88, 129.98, 'JAP', 'hills', 2, 1, { port: 1, fort: [1, 'USA', 'UK'] }],
  ['Kumamoto', 32.8, 130.78, 'JAP', 'hills', 0, 0, { port: 1 }],
  ['Kagoshima', 31.65, 130.48, 'JAP', 'hills', 1, 0, { port: 1 }],
  // The Kuriles' two harbours, the fleet's in the south and the army's in the north, and Japan's half of Sakhalin.
  ['Etorofu', 45.0, 147.7, 'JAP', 'hills', 0, 0, { port: 1 }],
  ['Paramushiro', 50.38, 155.75, 'JAP', 'hills', 0, 0, { port: 1 }],
  ['Karafuto', 46.96, 142.73, 'JAP', 'forest', 1, 0, { port: 1 }],
  // Formosa in two: the south by the Takao and Tainan fields (the planner's seed), the north with Keelung.
  ['Formosa', 22.7, 120.4, 'JAP', 'plains', 1, 1, { port: 1, col: 1 }],
  ['Taihoku', 24.95, 121.45, 'JAP', 'hills', 1, 0, { port: 1, col: 1, fort: [1, 'USA', 'UK', 'CHN'] }],
  // ---- Korea, Japan's since 1910.
  ['Seoul', 37.57, 126.98, 'JAP', 'hills', 2, 0, { port: 1, col: 1 }],
  ['Pyongyang', 39.02, 125.75, 'JAP', 'hills', 1, 0, { port: 1, col: 1 }],
  ['Hamhung', 39.92, 127.5, 'JAP', 'mountain', 0, 1, { port: 1, col: 1 }],
  ['Chongjin', 41.8, 129.65, 'JAP', 'mountain', 0, 0, { port: 1, col: 1, fort: [1, 'SOV', 'USA'] }],
  ['Pusan', 35.25, 128.95, 'JAP', 'hills', 1, 0, { port: 1, col: 1, fort: [1, 'SOV', 'USA'] }],
  ['Kwangju', 35.15, 126.92, 'JAP', 'hills', 0, 0, { port: 1, col: 1 }],
  // ---- Manchukuo, and Japan's leased territory at the tip of the Liaotung peninsula.
  ['Mukden', 41.8, 123.43, 'MAN', 'plains', 2, 2, {}],
  ['Anshan', 41.05, 122.9, 'MAN', 'plains', 0, 1, { port: 1 }],
  ['Dairen', 39.1, 121.75, 'JAP', 'hills', 2, 1, { port: 1, col: 1, fort: [1, 'SOV', 'USA', 'UK'] }],
  ['Chinchow', 41.1, 121.13, 'MAN', 'hills', 0, 0, { port: 1 }],
  ['Jehol', 40.97, 117.93, 'MAN', 'mountain', 0, 0, {}],
  ['Tunghwa', 41.73, 125.94, 'MAN', 'mountain', 0, 0, {}],
  ['Tungliao', 43.65, 122.25, 'MAN', 'plains', 0, 0, {}],
  ['Hsinking', 43.88, 125.32, 'MAN', 'plains', 2, 0, { cap: 1 }],
  ['Harbin', 45.75, 126.65, 'MAN', 'plains', 2, 0, {}],
  ['Mutankiang', 44.58, 129.6, 'MAN', 'forest', 0, 0, { fort: [2, 'SOV'] }],
  ['Kiamusze', 46.8, 130.32, 'MAN', 'marsh', 0, 0, { fort: [2, 'SOV'] }],
  ['Tsitsihar', 47.35, 123.92, 'MAN', 'plains', 0, 0, {}],
  ['Hailar', 49.21, 119.74, 'MAN', 'plains', 0, 0, { fort: [2, 'SOV', 'MGL'] }],
  ['Heiho', 50.0, 127.2, 'MAN', 'forest', 0, 0, {}],
  // ---- China, in the names of the day. North China and Inner Mongolia.
  ['Peiping', 39.9, 116.4, 'CHN', 'plains', 2, 0, {}],
  ['Tientsin', 39.13, 117.2, 'CHN', 'plains', 2, 0, { port: 1 }],
  ['Shihkiachwang', 38.04, 114.51, 'CHN', 'plains', 0, 0, {}],
  ['Kalgan', 40.81, 114.88, 'CHN', 'hills', 0, 0, {}],
  ['Silingol', 43.95, 116.08, 'CHN', 'plains', 0, 0, {}],
  ['Suiyuan', 40.82, 111.65, 'CHN', 'plains', 0, 0, {}],
  ['Tatung', 40.08, 113.3, 'CHN', 'hills', 0, 0, {}],
  ['Taiyuan', 37.87, 112.55, 'CHN', 'hills', 1, 0, {}],
  ['Linfen', 36.08, 111.52, 'CHN', 'hills', 0, 0, {}],
  ['Tsinan', 36.67, 117.0, 'CHN', 'plains', 1, 0, {}],
  ['Tsingtao', 36.3, 120.2, 'CHN', 'hills', 1, 0, { port: 1 }],
  ['Kaifeng', 34.8, 114.31, 'CHN', 'plains', 0, 0, {}],
  ['Loyang', 34.65, 112.45, 'CHN', 'hills', 0, 0, {}],
  ['Nanyang', 33.0, 112.53, 'CHN', 'plains', 0, 0, {}],
  ['Hsuchow', 34.26, 117.19, 'CHN', 'plains', 1, 0, { port: 1 }],
  // The lower Yangtze and the coast.
  ['Pengpu', 32.92, 117.39, 'CHN', 'plains', 0, 0, {}],
  ['Nanking', 32.06, 118.8, 'CHN', 'plains', 3, 0, { port: 1 }],
  ['Shanghai', 31.2, 121.35, 'CHN', 'urban', 3, 1, { port: 1 }],
  ['Hangchow', 30.25, 120.1, 'CHN', 'plains', 1, 0, { port: 1 }],
  ['Ningpo', 29.8, 121.4, 'CHN', 'hills', 0, 0, { port: 1 }],
  ['Anking', 30.51, 117.05, 'CHN', 'hills', 0, 0, {}],
  ['Chuchow', 28.97, 118.87, 'CHN', 'hills', 0, 0, { port: 1 }],
  ['Foochow', 26.2, 119.1, 'CHN', 'hills', 1, 0, { port: 1 }],
  ['Amoy', 24.7, 118.0, 'CHN', 'hills', 0, 0, { port: 1 }],
  ['Swatow', 23.55, 116.6, 'CHN', 'hills', 0, 0, { port: 1 }],
  // The middle Yangtze.
  ['Hankow', 30.58, 114.27, 'CHN', 'plains', 2, 1, {}],
  ['Ichang', 30.7, 111.29, 'CHN', 'hills', 0, 0, {}],
  ['Siangyang', 32.01, 112.12, 'CHN', 'hills', 0, 0, {}],
  ['Nanchang', 28.68, 115.86, 'CHN', 'plains', 1, 0, {}],
  ['Kanchow', 25.83, 114.93, 'CHN', 'hills', 0, 0, {}],
  ['Changsha', 28.23, 112.94, 'CHN', 'hills', 1, 0, {}],
  ['Hengyang', 26.89, 112.57, 'CHN', 'hills', 0, 0, {}],
  // The south. Hong Kong is its own outline; Macau and Kwangchowan are too small to draw.
  ['Canton', 23.13, 113.26, 'CHN', 'plains', 2, 0, { port: 1 }],
  ['Hong Kong', 22.42, 114.14, 'UK', 'urban', 2, 0, { port: 1, col: 1, fort: [1, 'JAP'] }],
  ['Kukong', 24.81, 113.6, 'CHN', 'hills', 0, 0, {}],
  ['Pakhoi', 21.7, 109.25, 'CHN', 'hills', 0, 0, { port: 1 }],
  ['Hainan', 19.2, 109.7, 'CHN', 'forest', 0, 0, { port: 1 }],
  ['Kweilin', 25.27, 110.29, 'CHN', 'hills', 1, 0, {}],
  ['Nanning', 22.82, 108.32, 'CHN', 'hills', 0, 0, {}],
  // Free China's west.
  ['Chungking', 29.56, 106.55, 'CHN', 'hills', 4, 1, { cap: 1 }],
  ['Chengtu', 30.66, 104.07, 'CHN', 'plains', 1, 0, {}],
  ['Kweiyang', 26.65, 106.63, 'CHN', 'hills', 0, 0, {}],
  ['Kunming', 25.04, 102.72, 'CHN', 'hills', 1, 0, {}],
  ['Paoshan', 25.11, 99.17, 'CHN', 'mountain', 0, 0, {}],
  ['Sikang', 30.05, 101.96, 'CHN', 'mountain', 0, 0, {}],
  ['Sian', 34.27, 108.94, 'CHN', 'plains', 1, 0, {}],
  ['Yenan', 36.6, 109.49, 'CHN', 'hills', 0, 0, {}],
  ['Hanchung', 33.07, 107.03, 'CHN', 'mountain', 0, 0, {}],
  ['Lanchow', 36.06, 103.83, 'CHN', 'hills', 1, 0, {}],
  ['Ningsia', 38.47, 106.27, 'CHN', 'desert', 0, 0, {}],
  ['Etsin Gol', 41.9, 101.1, 'CHN', 'desert', 0, 0, {}],
  ['Sining', 36.62, 101.78, 'CHN', 'mountain', 0, 0, {}],
  ['Yumen', 39.8, 97.7, 'CHN', 'desert', 0, 0, {}],
  ['Tsaidam', 36.4, 94.9, 'CHN', 'desert', 0, 0, {}],
  // Sinkiang and Tibet.
  ['Tihwa', 43.8, 87.6, 'CHN', 'hills', 0, 0, {}],
  ['Hami', 42.83, 93.51, 'CHN', 'desert', 0, 0, {}],
  ['Kuldja', 43.92, 81.32, 'CHN', 'hills', 0, 0, {}],
  ['Kashgar', 39.47, 75.99, 'CHN', 'desert', 0, 0, {}],
  ['Khotan', 37.11, 79.93, 'CHN', 'desert', 0, 0, {}],
  ['Charkhlik', 39.02, 88.17, 'CHN', 'desert', 0, 0, {}],
  ['Lhasa', 29.65, 91.1, 'CHN', 'mountain', 1, 0, {}],
  ['Chamdo', 31.14, 97.18, 'CHN', 'mountain', 0, 0, {}],
  ['Gartok', 31.75, 80.37, 'CHN', 'mountain', 0, 0, {}],
  ['Changtang', 33.5, 87.0, 'CHN', 'mountain', 0, 0, {}],
  // ---- Mongolia.
  ['Ulan Bator', 47.92, 106.92, 'MGL', 'hills', 2, 0, { cap: 1 }],
  ['Choibalsan', 48.07, 114.5, 'MGL', 'plains', 0, 0, {}],
  ['Sain Shand', 44.9, 110.1, 'MGL', 'desert', 0, 0, {}],
  ['Gobi', 43.57, 104.42, 'MGL', 'desert', 0, 0, {}],
  ['Uliassutai', 47.74, 96.85, 'MGL', 'mountain', 0, 0, {}],
  ['Kobdo', 48.0, 91.64, 'MGL', 'mountain', 0, 0, {}],
  // ---- The Soviet Far East, from Lake Baikal to the Bering Strait, and the north of Sakhalin.
  ['Vladivostok', 43.35, 132.05, 'SOV', 'hills', 3, 1, { port: 1, fort: [3, 'JAP'] }],
  ['Iman', 45.93, 133.73, 'SOV', 'forest', 0, 0, { fort: [2, 'JAP', 'MAN'] }],
  ['Khabarovsk', 48.6, 135.1, 'SOV', 'forest', 2, 1, { fort: [2, 'JAP', 'MAN'] }],
  ['Komsomolsk', 50.55, 137.0, 'SOV', 'forest', 1, 1, {}],
  ['Sovetskaya Gavan', 49.0, 140.1, 'SOV', 'forest', 0, 0, { port: 1 }],
  ['Nikolayevsk', 53.15, 140.5, 'SOV', 'forest', 0, 0, { port: 1 }],
  ['North Sakhalin', 51.8, 142.8, 'SOV', 'forest', 0, 0, { port: 1, oil: 1 }],
  ['Blagoveshchensk', 50.7, 127.8, 'SOV', 'plains', 1, 0, { fort: [2, 'JAP', 'MAN'] }],
  ['Skovorodino', 54.0, 123.94, 'SOV', 'forest', 0, 0, {}],
  ['Chita', 52.03, 113.5, 'SOV', 'forest', 1, 0, {}],
  ['Dauria', 50.6, 116.5, 'SOV', 'plains', 0, 0, {}],
  ['Ulan-Ude', 51.83, 107.6, 'SOV', 'forest', 1, 0, {}],
  ['Bodaibo', 57.85, 114.2, 'SOV', 'mountain', 0, 0, {}],
  ['Aldan', 58.6, 125.4, 'SOV', 'forest', 0, 0, {}],
  ['Yakutsk', 62.03, 129.73, 'SOV', 'forest', 1, 0, {}],
  ['Vilyuisk', 63.75, 121.63, 'SOV', 'forest', 0, 0, {}],
  ['Tiksi', 71.0, 127.5, 'SOV', 'marsh', 0, 0, {}],
  ['Verkhoyansk', 67.55, 133.39, 'SOV', 'mountain', 0, 0, {}],
  ['Kolyma', 67.45, 153.7, 'SOV', 'marsh', 0, 0, {}],
  ['Okhotsk', 59.8, 143.2, 'SOV', 'forest', 0, 0, { port: 1 }],
  ['Magadan', 59.9, 150.7, 'SOV', 'mountain', 1, 0, { port: 1 }],
  ['Koryak', 62.5, 166.0, 'SOV', 'mountain', 0, 0, { port: 1 }],
  ['Anadyr', 65.0, 177.0, 'SOV', 'marsh', 0, 0, { port: 1 }],
  ['Providenya', 65.2, -173.6, 'SOV', 'hills', 0, 0, { port: 1 }],
  ['Ust-Kamchatsk', 56.3, 160.8, 'SOV', 'mountain', 0, 0, { port: 1 }],
  ['Petropavlovsk', 53.2, 158.3, 'SOV', 'mountain', 1, 0, { port: 1 }],
  // ---- French Indochina.
  ['Hanoi', 21.03, 105.85, 'FRA', 'plains', 2, 0, { port: 1, col: 1 }],
  ['Vinh', 18.67, 105.6, 'FRA', 'hills', 0, 0, { port: 1, col: 1 }],
  ['Hue', 16.46, 107.5, 'FRA', 'hills', 1, 0, { port: 1, col: 1 }],
  ['Cam Ranh', 12.3, 108.95, 'FRA', 'hills', 0, 0, { port: 1, col: 1 }],
  ['Saigon', 10.85, 106.65, 'FRA', 'plains', 2, 0, { port: 1, col: 1, fort: [1, 'JAP'] }],
  ['Phnom Penh', 11.56, 104.92, 'FRA', 'plains', 1, 0, { port: 1, col: 1 }],
  ['Battambang', 13.1, 103.2, 'FRA', 'forest', 0, 0, { col: 1 }],
  ['Pakse', 15.2, 106.0, 'FRA', 'forest', 0, 0, { col: 1 }],
  ['Vientiane', 18.3, 102.7, 'FRA', 'forest', 0, 0, { col: 1 }],
  ['Luang Prabang', 19.89, 102.14, 'FRA', 'mountain', 0, 0, { col: 1 }],
  // ---- Thailand.
  ['Bangkok', 13.75, 100.5, 'THA', 'plains', 3, 0, { port: 1, cap: 1 }],
  ['Chiang Mai', 18.79, 98.98, 'THA', 'mountain', 0, 0, {}],
  ['Korat', 14.97, 102.1, 'THA', 'plains', 0, 0, {}],
  ['Udon', 17.2, 102.79, 'THA', 'plains', 0, 0, {}],
  ['Kra', 9.2, 99.2, 'THA', 'forest', 0, 0, { port: 1 }],
  ['Singora', 7.0, 100.47, 'THA', 'forest', 0, 0, { port: 1 }],
  // ---- Burma.
  ['Rangoon', 16.85, 96.15, 'UK', 'plains', 2, 0, { port: 1, col: 1 }],
  ['Moulmein', 16.5, 97.75, 'UK', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Mergui', 12.5, 98.85, 'UK', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Toungoo', 18.94, 96.43, 'UK', 'forest', 0, 0, { col: 1 }],
  ['Yenangyaung', 20.46, 94.87, 'UK', 'plains', 0, 0, { col: 1, oil: 1 }],
  ['Mandalay', 21.98, 96.08, 'UK', 'plains', 1, 0, { col: 1 }],
  ['Akyab', 20.6, 93.2, 'UK', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Kalewa', 23.2, 94.3, 'UK', 'forest', 0, 0, { col: 1 }],
  ['Lashio', 22.93, 97.75, 'UK', 'hills', 0, 0, { col: 1 }],
  ['Myitkyina', 25.38, 97.4, 'UK', 'forest', 0, 0, { col: 1 }],
  // ---- Malaya and Singapore. Penang island goes with the mainland behind it.
  ['Kota Bharu', 5.8, 102.2, 'UK', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Penang', 5.4, 100.5, 'UK', 'forest', 1, 0, { port: 1, col: 1, fort: [1, 'JAP'] }],
  ['Ipoh', 4.6, 101.08, 'UK', 'forest', 0, 0, { col: 1 }],
  ['Kuantan', 3.85, 103.1, 'UK', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Kuala Lumpur', 3.14, 101.69, 'UK', 'forest', 1, 0, { port: 1, col: 1 }],
  ['Johore', 2.03, 103.32, 'UK', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Singapore', 1.36, 103.82, 'UK', 'urban', 3, 1, { port: 1, col: 1, depot: 1, fort: [2, 'JAP'] }],
  // ---- British Borneo.
  ['Kuching', 1.45, 110.4, 'UK', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Brunei', 4.6, 114.7, 'UK', 'forest', 0, 0, { port: 1, col: 1, oil: 1 }],
  ['Jesselton', 5.8, 116.3, 'UK', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Sandakan', 5.6, 117.7, 'UK', 'forest', 0, 0, { port: 1, col: 1 }],
  // ---- The Netherlands Indies. Sumatra, with Bangka and Billiton.
  ['Atjeh', 5.2, 95.6, 'NED', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Medan', 3.59, 98.67, 'NED', 'forest', 1, 0, { port: 1, col: 1, oil: 1 }],
  ['Pakanbaru', 0.53, 101.45, 'NED', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Padang', -0.7, 100.6, 'NED', 'mountain', 0, 0, { port: 1, col: 1 }],
  ['Djambi', -1.6, 103.6, 'NED', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Palembang', -2.99, 104.76, 'NED', 'forest', 1, 0, { port: 1, col: 1, oil: 1 }],
  ['Oosthaven', -5.1, 105.2, 'NED', 'forest', 0, 0, { port: 1, col: 1 }],
  // Java, with Madura.
  ['Batavia', -6.3, 106.85, 'NED', 'plains', 3, 0, { port: 1, col: 1, fort: [1, 'JAP'] }],
  ['Bandoeng', -6.91, 107.61, 'NED', 'hills', 1, 0, { port: 1, col: 1 }],
  ['Semarang', -7.2, 110.4, 'NED', 'plains', 0, 0, { port: 1, col: 1 }],
  ['Soerabaja', -7.45, 112.6, 'NED', 'plains', 2, 1, { port: 1, col: 1, oil: 1, fort: [1, 'JAP'] }],
  // The lesser Sunda islands, and Timor's two halves.
  ['Bali', -8.4, 115.2, 'NED', 'hills', 0, 0, { port: 1, col: 1 }],
  ['Flores', -8.65, 121.3, 'NED', 'hills', 0, 0, { port: 1, col: 1 }],
  ['Koepang', -9.9, 124.0, 'NED', 'hills', 0, 0, { port: 1, col: 1 }],
  ['Dili', -8.75, 125.6, 'POR', 'hills', 0, 0, { port: 1, col: 1 }],
  // Borneo. Tarakan's island goes with the shore behind it.
  ['Pontianak', 0.1, 109.8, 'NED', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Bandjermasin', -3.2, 114.9, 'NED', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Balikpapan', -1.0, 116.7, 'NED', 'forest', 1, 0, { port: 1, col: 1, oil: 1 }],
  ['Tarakan', 3.3, 117.0, 'NED', 'forest', 0, 0, { port: 1, col: 1, oil: 1 }],
  // Celebes and the Moluccas. Ceram and Buru go with Ambon.
  ['Makassar', -5.0, 119.7, 'NED', 'hills', 1, 0, { port: 1, col: 1 }],
  ['Kendari', -3.9, 122.2, 'NED', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Palu', -1.3, 120.1, 'NED', 'mountain', 0, 0, { port: 1, col: 1 }],
  ['Menado', 1.2, 124.8, 'NED', 'hills', 0, 0, { port: 1, col: 1 }],
  ['Ambon', -3.63, 128.12, 'NED', 'hills', 0, 0, { port: 1, col: 1 }],
  // Netherlands New Guinea.
  ['Vogelkop', -0.9, 132.3, 'NED', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Sarmi', -2.1, 138.8, 'NED', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Hollandia', -2.65, 140.5, 'NED', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Mimika', -4.5, 136.9, 'NED', 'marsh', 0, 0, { port: 1, col: 1 }],
  ['Merauke', -8.0, 140.3, 'NED', 'marsh', 0, 0, { port: 1, col: 1 }],
  // ---- The Philippines. Clark Field lies in Lingayen's province, not Manila's (ruling 184); Corregidor is
  // Bataan's chief place. Samar goes with Leyte, Negros with its nearer neighbour.
  ['Aparri', 17.6, 121.7, 'USA', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Lingayen', 15.95, 120.35, 'USA', 'plains', 1, 0, { port: 1, col: 1 }],
  ['Manila', 14.6, 121.0, 'USA', 'urban', 3, 0, { port: 1, col: 1 }],
  ['Bataan', 14.6, 120.45, 'USA', 'hills', 0, 0, { port: 1, col: 1, fort: [3, 'JAP'] }],
  ['Legaspi', 13.3, 123.6, 'USA', 'hills', 0, 0, { port: 1, col: 1 }],
  ['Mindoro', 12.9, 121.1, 'USA', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Leyte', 11.0, 124.9, 'USA', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Cebu', 10.4, 123.8, 'USA', 'hills', 1, 0, { port: 1, col: 1 }],
  ['Panay', 11.0, 122.5, 'USA', 'hills', 0, 0, { port: 1, col: 1 }],
  ['Palawan', 9.85, 118.65, 'USA', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Del Monte', 8.3, 124.8, 'USA', 'hills', 0, 0, { port: 1, col: 1 }],
  ['Davao', 7.1, 125.55, 'USA', 'forest', 1, 0, { port: 1, col: 1 }],
  ['Zamboanga', 7.3, 122.2, 'USA', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Jolo', 6.0, 121.1, 'USA', 'forest', 0, 0, { port: 1, col: 1 }],
  // ---- Papua and the mandated territory of New Guinea. Wewak lies between Madang and Hollandia; New Ireland
  // goes with Rabaul; Kerema's is the Gulf of Papua and the Fly, so that Lae's coasts are its own two.
  ['Port Moresby', -9.4, 147.25, 'AUS', 'hills', 1, 0, { port: 1, col: 1 }],
  ['Milne Bay', -10.2, 150.1, 'AUS', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Buna', -8.75, 148.3, 'AUS', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Lae', -6.65, 146.95, 'AUS', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Madang', -5.3, 145.7, 'AUS', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Wewak', -3.7, 143.6, 'AUS', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Kerema', -7.5, 145.0, 'AUS', 'marsh', 0, 0, { port: 1, col: 1 }],
  ['Rabaul', -4.4, 152.0, 'AUS', 'forest', 1, 0, { port: 1, col: 1 }],
  ['Cape Gloucester', -5.6, 148.8, 'AUS', 'forest', 0, 0, { port: 1, col: 1 }],
  // ---- Australia and New Zealand.
  ['Darwin', -12.7, 131.1, 'AUS', 'plains', 1, 0, { port: 1 }],
  ['Wyndham', -15.8, 128.3, 'AUS', 'hills', 0, 0, { port: 1 }],
  ['Broome', -18.0, 122.6, 'AUS', 'desert', 0, 0, { port: 1 }],
  ['Port Hedland', -20.7, 118.8, 'AUS', 'desert', 0, 0, { port: 1 }],
  ['Carnarvon', -24.9, 114.1, 'AUS', 'desert', 0, 0, { port: 1 }],
  ['Perth', -31.95, 116.0, 'AUS', 'plains', 2, 0, { port: 1 }],
  ['Kalgoorlie', -30.75, 121.47, 'AUS', 'desert', 0, 0, { port: 1 }],
  ['Nullarbor', -30.9, 128.9, 'AUS', 'desert', 0, 0, { port: 1 }],
  ['Adelaide', -34.8, 138.75, 'AUS', 'plains', 2, 1, { port: 1 }],
  ['Oodnadatta', -27.55, 135.45, 'AUS', 'desert', 0, 0, {}],
  ['Alice Springs', -23.7, 133.88, 'AUS', 'desert', 0, 0, {}],
  ['Tennant Creek', -19.65, 134.19, 'AUS', 'desert', 0, 0, { port: 1 }],
  ['Cloncurry', -20.7, 140.5, 'AUS', 'plains', 0, 0, { port: 1 }],
  ['Cape York', -13.95, 143.2, 'AUS', 'forest', 0, 0, { port: 1 }],
  ['Townsville', -19.4, 146.7, 'AUS', 'plains', 1, 0, { port: 1 }],
  ['Rockhampton', -23.4, 150.4, 'AUS', 'plains', 0, 0, { port: 1 }],
  ['Charleville', -26.4, 146.25, 'AUS', 'plains', 0, 0, {}],
  ['Brisbane', -27.5, 152.9, 'AUS', 'plains', 2, 0, { port: 1 }],
  ['Broken Hill', -31.95, 141.47, 'AUS', 'desert', 0, 0, {}],
  ['Sydney', -33.85, 150.95, 'AUS', 'urban', 3, 2, { port: 1 }],
  ['Canberra', -35.28, 149.13, 'AUS', 'hills', 2, 0, { port: 1, cap: 1 }],
  ['Melbourne', -37.8, 145.0, 'AUS', 'urban', 2, 1, { port: 1 }],
  ['Hobart', -42.5, 147.0, 'AUS', 'forest', 0, 0, { port: 1 }],
  ['Auckland', -37.2, 175.0, 'NZL', 'hills', 1, 0, { port: 1 }],
  ['Wellington', -40.9, 175.5, 'NZL', 'hills', 2, 0, { port: 1, cap: 1 }],
  ['Christchurch', -43.5, 172.4, 'NZL', 'plains', 1, 0, { port: 1 }],
  ['Dunedin', -45.7, 170.3, 'NZL', 'hills', 0, 0, { port: 1 }],
];

/** Land of the Pacific slice that belongs to no province: seeds that take their share so the provinces beside them keep their shape. [what, lat, lon] */
export const PACIFIC_BEYOND = [];

/**
 * Frontiers drawn true, as in world.mjs: a country's own outline from the atlas (or several countries' as one),
 * part of one ('clip', a shape drawn by hand in [lat, lon]), or a line drawn by hand and closed through open
 * water. owners are the codes that may stand inside. A side listed earlier is cut out of one listed later.
 */
export const PACIFIC_FRONTIERS = [
  // Sakhalin at the 50th parallel: Japan's Karafuto south of it.
  { name: 'Karafuto', line: [[50, 141.4], [50, 145.2]], close: [[45.5, 145.2], [45.5, 141.4]], owners: ['JAP'] },
  { name: 'Korea', country: ['North Korea', 'South Korea'], owners: ['JAP'] },
  // Manchukuo out of China: the Great Wall from the sea at Shanhaikwan, then Jehol's and Hsingan's western
  // borders north to Mongolia's eastern tip. The rest of the shape lies outside China and cuts nothing.
  {
    name: 'Manchukuo', country: 'China', owners: ['MAN', 'JAP'],
    clip: [[39.95, 119.85], [40.25, 118.9], [40.45, 117.9], [40.7, 117.2], [41.5, 116.5], [42.4, 116.8], [43.6, 117.6], [44.7, 118.4], [45.8, 119.3], [46.72, 119.93],
      [46.95, 119.3], [47.5, 116.5], [48.5, 114.5], [50.5, 115.0], [54.5, 121.0], [54.5, 136.5], [47.0, 136.5], [42.0, 132.0], [41.5, 129.5], [39.5, 125.0], [38.3, 122.5], [38.7, 120.4], [39.65, 120.15]],
  },
  { country: 'Hong Kong', owners: ['UK'] },
  { country: 'China', owners: ['CHN'] },
  { country: 'Mongolia', owners: ['MGL'] },
  // The Soviet Far East out of Russia: east of a line through Lake Baikal and up the 110th meridian, with the
  // north of Sakhalin, and without the Kuriles, which were Japan's.
  {
    name: 'Soviet Far East', country: 'Russia', owners: ['SOV'],
    clip: [[49.0, 104.9], [51.2, 104.5], [51.7, 104.6], [52.6, 106.6], [53.6, 108.2], [55.0, 109.4], [55.9, 109.9], [57.5, 110.5], [60.0, 110.0], [66.0, 110.0], [74.2, 110.0], [78.5, 110.0],
      [78.5, 150.0], [76.0, 180.0], [72.5, -168.6], [64.0, -168.8], [60.0, -175.0], [54.0, 170.5], [50.3, 158.0], [50.6, 157.3], [50.87, 156.58], [51.3, 155.6], [50.0, 145.2], [50.0, 141.4],
      [48.0, 141.1], [46.0, 140.2], [43.0, 135.5], [42.0, 133.0], [42.1, 130.5], [41.5, 129.0], [44.0, 125.0], [47.5, 120.0], [48.5, 112.0]],
  },
  { name: 'French Indochina', country: ['Vietnam', 'Laos', 'Cambodia'], owners: ['FRA'] },
  { country: 'Thailand', owners: ['THA'] },
  { name: 'Burma', country: 'Myanmar', owners: ['UK'] },
  { name: 'Malaya and British Borneo', country: ['Malaysia', 'Brunei'], owners: ['UK'] },
  { name: 'Netherlands Indies', country: 'Indonesia', owners: ['NED'] },
  { name: 'Portuguese Timor', country: 'Timor-Leste', owners: ['POR'] },
  { name: 'Papua and New Guinea', country: 'Papua New Guinea', owners: ['AUS'] },
];

/** Short crossings armies can use without a fleet. */
export const PACIFIC_LINKS = [
  // The Shimonoseki strait, a kilometre of water; Singapore's causeway; the Bali strait; the narrows between
  // Sakhalin and the mainland, which freeze.
  ['Shimonoseki', 'Fukuoka'], ['Johore', 'Singapore'], ['Soerabaja', 'Bali'], ['Nikolayevsk', 'North Sakhalin'],
];
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
