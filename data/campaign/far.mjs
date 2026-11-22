// The far side of the world, laid round today's chart (world.mjs) without moving anything on it: the Pacific first
// (Hawaii and Alaska, the islands of the two roads to Japan, Japan, China and their neighbours), then the rest of
// the globe. tools/build-geo-far.mjs grows a province round every seed here, as the map of Europe and today's
// chart are made, and writes src/generated/world-next.json. The provinces follow today's in number (576 up), in
// the order of the slices below and of each slice's list. Add new ones at the end of the last slice.
//
// To build: node tools/build-geo-far.mjs, then node tools/build-rivers.mjs far, then node tools/build-rails.mjs far
// (world-next.json, world-next-rivers.json, world-next-rails.json), and node tools/check-reach.mjs to judge the
// seas by the Pacific planner's test.
//
// [name, lat, lon, owner, terrain, victory points, factories, flags]   (flags as in regions.mjs, and 'islet', 'disc')
// Longitudes are written as an atlas writes them, east positive: the tool counts them eastward from today's west
// edge (141 W), so Hawaii and Alaska lie at the far east end of the chart, beside the America they belong to.
//
// The seed is the province's centre, and the rules measure every distance from it (air.ts, km). The places the
// Pacific planner measured its flights between (rulings 181 and 184) are seeded on the planner's own points, or
// the nearest land to them: move one and a raid history flew may fall out of reach. An island group is
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
  ['Sendai', 38.27, 140.87, 'JAP', 'plains', 1, 0, { port: 1 }],
  ['Niigata', 37.85, 139.1, 'JAP', 'plains', 1, 0, { port: 1 }],
  ['Tokyo', 35.68, 139.7, 'JAP', 'urban', 6, 3, { port: 1, cap: 1 }],
  ['Yokohama', 35.4, 139.58, 'JAP', 'urban', 2, 1, { port: 1, fort: [2, 'USA', 'UK'] }],
  ['Nagano', 36.65, 138.19, 'JAP', 'mountain', 0, 0, {}],
  ['Shizuoka', 35.0, 138.38, 'JAP', 'hills', 1, 0, { port: 1 }],
  ['Nagoya', 35.15, 136.9, 'JAP', 'urban', 2, 2, { port: 1 }],
  ['Kanazawa', 36.56, 136.7, 'JAP', 'hills', 0, 0, { port: 1 }],
  ['Kyoto', 35.01, 135.77, 'JAP', 'urban', 2, 0, { port: 1, fort: [1, 'USA', 'UK', 'SOV'] }],
  ['Osaka', 34.7, 135.5, 'JAP', 'urban', 3, 4, { port: 1, fort: [1, 'USA', 'UK'] }],
  ['Okayama', 34.7, 133.92, 'JAP', 'hills', 0, 0, { port: 1 }],
  ['Hiroshima', 34.42, 132.46, 'JAP', 'hills', 2, 1, { port: 1, fort: [1, 'USA', 'UK'] }],
  ['Shimonoseki', 34.08, 131.05, 'JAP', 'hills', 0, 0, { port: 1, fort: [2, 'USA', 'UK', 'SOV'] }],
  ['Shikoku', 33.85, 133.3, 'JAP', 'mountain', 0, 0, { port: 1 }],
  ['Fukuoka', 33.59, 130.45, 'JAP', 'hills', 2, 2, { port: 1 }],
  ['Nagasaki', 32.88, 129.98, 'JAP', 'hills', 2, 1, { port: 1, fort: [1, 'USA', 'UK'] }],
  ['Kumamoto', 32.8, 130.78, 'JAP', 'hills', 0, 0, { port: 1 }],
  ['Kagoshima', 31.6, 130.55, 'JAP', 'hills', 1, 0, { port: 1 }],
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
  ['Chengtu', 30.67, 104.07, 'CHN', 'plains', 1, 0, {}],
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
  // (Tibet's own, governed from Lhasa and independent in fact from 1913; Chamdo was Tibet's from 1918 to October
  // 1950. Neutral throughout: rider b632 to ruling 234, historian/tibet-1937-45.md. Shigatse, cut from Lhasa on
  // the cities' chart, is Tibet's with it. Lhasa is its capital. The owner and the capital the chart reads are the
  // clerk's row: tables/pacific.csv.)
  ['Lhasa', 29.65, 91.1, 'TIB', 'mountain', 1, 0, { cap: 1 }],
  ['Chamdo', 31.14, 97.18, 'TIB', 'mountain', 0, 0, {}],
  ['Gartok', 31.75, 80.37, 'TIB', 'mountain', 0, 0, {}],
  ['Changtang', 33.5, 87.0, 'TIB', 'mountain', 0, 0, {}],
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
  ['Qui Nhon', 13.9, 109.0, 'FRA', 'hills', 0, 0, { port: 1, col: 1 }],
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
  ['Phitsanulok', 16.82, 100.26, 'THA', 'plains', 0, 0, {}],
  ['Korat', 14.97, 102.1, 'THA', 'plains', 0, 0, {}],
  ['Udon', 17.2, 102.79, 'THA', 'plains', 0, 0, {}],
  ['Kra', 9.2, 99.2, 'THA', 'forest', 0, 0, { port: 1 }],
  ['Singora', 7.0, 100.47, 'THA', 'forest', 0, 0, { port: 1 }],
  // ---- Burma.
  ['Rangoon', 16.85, 96.15, 'UK', 'plains', 2, 0, { port: 1, col: 1 }],
  ['Moulmein', 16.5, 97.75, 'UK', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Mergui', 12.5, 98.85, 'UK', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Prome', 18.82, 95.3, 'UK', 'plains', 0, 0, { port: 1, col: 1 }],
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
  ['Singapore', 1.35, 103.82, 'UK', 'urban', 3, 1, { port: 1, col: 1, depot: 1, fort: [2, 'JAP'] }],
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
  ['Palembang', -2.98, 104.75, 'NED', 'forest', 1, 0, { port: 1, col: 1, oil: 1 }],
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
  ['Balikpapan', -1.24, 116.74, 'NED', 'forest', 1, 0, { port: 1, col: 1, oil: 1 }],
  ['Tarakan', 3.3, 117.0, 'NED', 'forest', 0, 0, { port: 1, col: 1, oil: 1 }],
  // Celebes and the Moluccas. Ceram and Buru go with Ambon.
  ['Makassar', -5.0, 119.7, 'NED', 'hills', 1, 0, { port: 1, col: 1 }],
  ['Kendari', -3.9, 122.2, 'NED', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Palu', -1.3, 120.1, 'NED', 'mountain', 0, 0, { port: 1, col: 1 }],
  ['Menado', 1.2, 124.8, 'NED', 'hills', 0, 0, { port: 1, col: 1 }],
  ['Ambon', -3.7, 128.2, 'NED', 'hills', 0, 0, { port: 1, col: 1 }],
  // Netherlands New Guinea.
  ['Vogelkop', -0.9, 132.3, 'NED', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Sarmi', -2.1, 138.8, 'NED', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Hollandia', -2.53, 140.72, 'NED', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Mimika', -4.5, 136.9, 'NED', 'marsh', 0, 0, { port: 1, col: 1 }],
  ['Merauke', -8.0, 140.3, 'NED', 'marsh', 0, 0, { port: 1, col: 1 }],
  // ---- The Philippines. Clark Field lies in Lingayen's province, not Manila's (ruling 184); Corregidor is
  // Bataan's chief place. Samar goes with Leyte, Negros with its nearer neighbour.
  ['Aparri', 17.6, 121.7, 'USA', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Lingayen', 16.0, 120.3, 'USA', 'plains', 1, 0, { port: 1, col: 1 }],
  ['Manila', 14.6, 121.0, 'USA', 'urban', 3, 0, { port: 1, col: 1 }],
  ['Bataan', 14.6, 120.45, 'USA', 'hills', 0, 0, { port: 1, col: 1, fort: [3, 'JAP'] }],
  ['Legaspi', 13.3, 123.6, 'USA', 'hills', 0, 0, { port: 1, col: 1 }],
  ['Mindoro', 12.9, 121.1, 'USA', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Leyte', 11.0, 124.9, 'USA', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Cebu', 10.4, 123.8, 'USA', 'hills', 1, 0, { port: 1, col: 1 }],
  ['Panay', 11.0, 122.5, 'USA', 'hills', 0, 0, { port: 1, col: 1 }],
  ['Palawan', 9.85, 118.65, 'USA', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Del Monte', 8.3, 124.8, 'USA', 'hills', 0, 0, { port: 1, col: 1 }],
  ['Davao', 7.07, 125.6, 'USA', 'forest', 1, 0, { port: 1, col: 1 }],
  ['Zamboanga', 7.3, 122.2, 'USA', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Jolo', 6.0, 121.1, 'USA', 'forest', 0, 0, { port: 1, col: 1 }],
  // ---- Papua and the mandated territory of New Guinea. Wewak lies between Madang and Hollandia; New Ireland
  // goes with Rabaul; Kerema's is the Gulf of Papua and the Fly, so that Lae's coasts are its own two.
  ['Port Moresby', -9.45, 147.2, 'AUS', 'hills', 1, 0, { port: 1, col: 1 }],
  ['Milne Bay', -10.2, 150.1, 'AUS', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Buna', -8.67, 148.4, 'AUS', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Lae', -6.73, 147.0, 'AUS', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Madang', -5.23, 145.74, 'AUS', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Wewak', -3.55, 143.63, 'AUS', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Kerema', -7.5, 145.0, 'AUS', 'marsh', 0, 0, { port: 1, col: 1 }],
  ['Rabaul', -4.25, 152.12, 'AUS', 'forest', 1, 0, { port: 1, col: 1 }],
  ['Cape Gloucester', -5.6, 148.8, 'AUS', 'forest', 0, 0, { port: 1, col: 1 }],
  // ---- Australia and New Zealand.
  ['Darwin', -12.44, 130.92, 'AUS', 'plains', 1, 0, { port: 1 }],
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
  ['Townsville', -19.26, 146.8, 'AUS', 'plains', 1, 0, { port: 1 }],
  ['Rockhampton', -23.4, 150.4, 'AUS', 'plains', 0, 0, { port: 1 }],
  ['Charleville', -26.4, 146.25, 'AUS', 'plains', 0, 0, {}],
  ['Brisbane', -27.47, 153.03, 'AUS', 'plains', 2, 0, { port: 1 }],
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
  // (Tibet's ground lies inside the atlas's China: its provinces are Tibet's own, rider b632 to ruling 234.)
  { country: 'China', owners: ['CHN', 'TIB'] },
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
export const PACIFIC_NO_BORDER = [
  // The high plateau (rider b632 to ruling 234; historian/tibet-1937-45.md). Tibet let no troops of any nation
  // through, and the going closed the rest: Sining to Lhasa was three to four months with 1,000 km above 4,500 m,
  // Sikang to Lhasa a dozen passes over 4,500 m, Lhasa to India weeks to months over passes of 4,300 to 5,000 m.
  // Nothing above about a fifth of a division ever crossed, and Lhasa could not feed a few thousand men. At the
  // game's scale that is closed ground: no march, attack or supply line crosses any border of Tibet's. Its own
  // provinces still border one another. To India and Burma:
  ['Lhasa', 'Darjeeling'], ['Lhasa', 'Gauhati'], ['Lhasa', 'Dibrugarh'], ['Chamdo', 'Dibrugarh'], ['Chamdo', 'Myitkyina'],
  ['Gartok', 'Leh'], ['Gartok', 'Simla'],
  // to Nepal:
  ['Lhasa', 'Kathmandu'], ['Gartok', 'Kathmandu'],
  // and to China (Sikang and Sining stay China's, with the mountain going ruling 215 (g) gives them):
  ['Chamdo', 'Chengtu'], ['Chamdo', 'Sikang'], ['Chamdo', 'Lanchow'], ['Chamdo', 'Sining'], ['Chamdo', 'Tsaidam'],
  ['Gartok', 'Khotan'], ['Changtang', 'Tsaidam'], ['Changtang', 'Khotan'], ['Changtang', 'Charkhlik'],
  // The Karakoram, Tibet's sibling (the rider of 19:25Z to ruling 234, item 7; historian/sinkiang-karakoram-1937-45.md).
  // From Sinkiang's oases to Leh and to Gilgit ran caravan roads over passes of about 4,700 to 5,540 m, open in
  // summer, ten marches with no grazing or fuel: no army crossed them in the record, a few hundred men with pack
  // animals at most, and Ladakh could not feed a corps. By Tibet's form they carry none. Sinkiang's desert stays
  // open: armies did cross it from Kansu by Hami.
  // (Yarkand is a city cut out of Kashgar's province once the chart is laid: its border with Gilgit is Kashgar's here.)
  ['Khotan', 'Leh'], ['Khotan', 'Gilgit'], ['Kashgar', 'Gilgit'],
];

// ---- The rest of the globe: the Soviet Union from the map of Europe to Lake Baikal, Persia, Arabia, Afghanistan,
// India, and Africa south of the map of Europe. The map of Europe ends at 47 E and at 28.6 N: a province here
// begins exactly on that edge and meets the old one across it ('joins'). Deserts are written as desert in Africa
// only (the standing rule until supply is retuned); the clerk's table has the climate.

export const GLOBE_REGIONS = [
  // The Soviet Union: the Volga, the Caspian shore and the Urals
  ['Kazan', 55.8, 49.1, 'SOV', 'plains', 2, 2, {}],
  ['Ulyanovsk', 54.3, 48.4, 'SOV', 'plains', 0, 1, {}],
  ['Kirov', 58.6, 49.65, 'SOV', 'forest', 0, 1, {}],
  ['Syktyvkar', 61.67, 50.8, 'SOV', 'forest', 0, 0, {}],
  ['Naryan-Mar', 67.3, 53.0, 'SOV', 'marsh', 0, 0, {}],
  ['Vorkuta', 67.5, 64.0, 'SOV', 'hills', 0, 0, {}],
  ['Izhevsk', 56.85, 53.2, 'SOV', 'forest', 1, 2, {}],
  ['Molotov', 58.0, 56.25, 'SOV', 'forest', 1, 2, {}],
  ['Kuibyshev', 53.2, 50.15, 'SOV', 'urban', 3, 3, {}],
  ['Pugachev', 52.02, 48.8, 'SOV', 'plains', 0, 0, {}],
  ['Urda', 48.77, 47.6, 'SOV', 'plains', 0, 0, {}],
  ['Volga Delta', 46.6, 48.3, 'SOV', 'marsh', 0, 0, {}],
  ['Makhachkala', 42.85, 47.35, 'SOV', 'hills', 0, 0, {}],
  ['Baku', 40.45, 49.6, 'SOV', 'urban', 3, 2, { oil: 1 }],
  ['Uralsk', 51.23, 51.37, 'SOV', 'plains', 0, 0, {}],
  ['Guryev', 47.3, 52.0, 'SOV', 'plains', 0, 0, { oil: 1 }],
  ['Ufa', 54.75, 56.0, 'SOV', 'hills', 1, 2, { oil: 1 }],
  ['Chkalov', 51.77, 55.1, 'SOV', 'plains', 1, 1, {}],
  ['Aktyubinsk', 50.28, 57.2, 'SOV', 'plains', 0, 0, {}],
  ['Serov', 59.6, 60.57, 'SOV', 'forest', 0, 1, {}],
  ['Nizhny Tagil', 57.92, 59.97, 'SOV', 'hills', 1, 3, {}],
  ['Sverdlovsk', 56.85, 60.6, 'SOV', 'urban', 3, 4, {}],
  ['Chelyabinsk', 55.15, 61.4, 'SOV', 'urban', 2, 4, {}],
  ['Magnitogorsk', 53.4, 59.05, 'SOV', 'hills', 1, 3, {}],
  // West Siberia, to Lake Baikal and the 110th meridian
  ['Kurgan', 55.45, 65.33, 'SOV', 'plains', 0, 0, {}],
  ['Tyumen', 57.15, 65.53, 'SOV', 'forest', 1, 1, {}],
  ['Tobolsk', 58.2, 68.25, 'SOV', 'forest', 0, 0, {}],
  ['Berezovo', 63.93, 65.05, 'SOV', 'forest', 0, 0, {}],
  ['Salekhard', 66.53, 66.9, 'SOV', 'marsh', 0, 0, {}],
  ['Tazovsky', 67.47, 78.7, 'SOV', 'marsh', 0, 0, {}],
  ['Surgut', 61.25, 73.4, 'SOV', 'marsh', 0, 0, {}],
  ['Omsk', 54.98, 73.37, 'SOV', 'urban', 2, 2, {}],
  ['Novosibirsk', 55.03, 82.92, 'SOV', 'urban', 3, 3, {}],
  ['Barnaul', 53.35, 83.75, 'SOV', 'plains', 0, 1, {}],
  ['Tomsk', 56.5, 84.97, 'SOV', 'forest', 1, 1, {}],
  ['Kolpashevo', 58.3, 82.9, 'SOV', 'marsh', 0, 0, {}],
  ['Stalinsk', 53.76, 87.12, 'SOV', 'hills', 1, 3, {}],
  ['Abakan', 53.72, 91.43, 'SOV', 'hills', 0, 0, {}],
  ['Kyzyl', 51.72, 94.45, 'SOV', 'mountain', 0, 0, {}],
  ['Krasnoyarsk', 56.01, 92.87, 'SOV', 'forest', 2, 2, {}],
  ['Yeniseisk', 58.45, 92.17, 'SOV', 'forest', 0, 0, {}],
  ['Turukhansk', 65.8, 87.97, 'SOV', 'forest', 0, 0, {}],
  ['Norilsk', 69.35, 88.2, 'SOV', 'mountain', 0, 1, {}],
  ['Khatanga', 70.7, 101.0, 'SOV', 'marsh', 0, 0, {}],
  ['Tura', 64.28, 100.22, 'SOV', 'forest', 0, 0, {}],
  ['Nizhneudinsk', 54.9, 99.03, 'SOV', 'forest', 0, 0, {}],
  ['Bratsk', 56.15, 101.63, 'SOV', 'forest', 0, 0, {}],
  ['Irkutsk', 52.29, 104.3, 'SOV', 'forest', 2, 2, {}],
  ['Kirensk', 57.78, 108.1, 'SOV', 'forest', 0, 0, {}],
  // Kazakhstan and Central Asia
  ['Kustanay', 53.2, 63.62, 'SOV', 'plains', 0, 0, {}],
  ['Akmolinsk', 51.17, 71.43, 'SOV', 'plains', 0, 0, {}],
  ['Karaganda', 49.8, 73.1, 'SOV', 'plains', 1, 1, {}],
  ['Pavlodar', 52.3, 76.95, 'SOV', 'plains', 0, 0, {}],
  ['Semipalatinsk', 50.4, 80.25, 'SOV', 'plains', 0, 1, {}],
  ['Dzhezkazgan', 47.8, 67.7, 'SOV', 'plains', 0, 0, {}],
  ['Balkhash', 46.85, 75.0, 'SOV', 'plains', 0, 1, {}],
  ['Alma-Ata', 43.25, 76.9, 'SOV', 'hills', 2, 1, {}],
  ['Frunze', 42.87, 74.6, 'SOV', 'mountain', 1, 0, {}],
  ['Dzhambul', 42.9, 71.37, 'SOV', 'plains', 0, 0, {}],
  ['Aralsk', 47.3, 61.8, 'SOV', 'plains', 0, 0, {}],
  ['Kzyl-Orda', 44.85, 65.5, 'SOV', 'plains', 0, 0, {}],
  ['Fort Shevchenko', 44.2, 51.6, 'SOV', 'plains', 0, 0, {}],
  ['Khiva', 41.38, 60.36, 'SOV', 'plains', 0, 0, {}],
  ['Krasnovodsk', 40.1, 53.4, 'SOV', 'plains', 0, 0, {}],
  ['Ashkhabad', 38.1, 58.38, 'SOV', 'plains', 1, 0, {}],
  ['Merv', 37.6, 61.83, 'SOV', 'plains', 0, 0, {}],
  ['Bukhara', 39.77, 64.42, 'SOV', 'plains', 0, 0, {}],
  ['Samarkand', 39.65, 66.96, 'SOV', 'hills', 1, 0, {}],
  ['Tashkent', 41.3, 69.25, 'SOV', 'urban', 2, 2, {}],
  ['Fergana', 40.38, 71.78, 'SOV', 'plains', 1, 1, {}],
  ['Stalinabad', 38.56, 68.78, 'SOV', 'mountain', 1, 0, {}],
  ['Khorog', 37.8, 72.5, 'SOV', 'mountain', 0, 0, {}],
  // Persia east of the old sheet. (Its capital flag stands on the old province 'Iran', which is the rules' to move.)
  ['Tehran', 35.7, 51.4, 'IRN', 'urban', 2, 1, {}],
  ['Resht', 37.2, 49.6, 'IRN', 'forest', 0, 0, {}],
  ['Hamadan', 34.8, 48.5, 'IRN', 'mountain', 0, 0, {}],
  ['Ahwaz', 31.5, 48.9, 'IRN', 'plains', 1, 0, { oil: 1 }],
  ['Abadan', 30.5, 48.6, 'IRN', 'marsh', 2, 1, { oil: 1, port: 1 }],
  ['Isfahan', 32.65, 51.67, 'IRN', 'hills', 1, 0, {}],
  ['Shiraz', 29.6, 52.53, 'IRN', 'mountain', 0, 0, {}],
  ['Bushire', 28.9, 51.2, 'IRN', 'hills', 0, 0, { port: 1 }],
  ['Bandar Abbas', 27.5, 56.3, 'IRN', 'hills', 0, 0, { port: 1 }],
  ['Yazd', 31.9, 54.37, 'IRN', 'plains', 0, 0, {}],
  ['Kerman', 30.28, 57.07, 'IRN', 'hills', 0, 0, {}],
  ['Zahedan', 29.5, 60.6, 'IRN', 'plains', 0, 0, {}],
  ['Birjand', 32.87, 59.2, 'IRN', 'hills', 0, 0, {}],
  ['Meshed', 36.3, 59.6, 'IRN', 'hills', 1, 0, {}],
  ['Bandar Shah', 36.7, 54.5, 'IRN', 'plains', 0, 0, {}],
  // The head of the Gulf
  ['Basra', 30.6, 47.6, 'IRQ', 'marsh', 1, 0, { port: 1 }],
  ['Kuwait', 29.3, 47.7, 'UK', 'plains', 0, 0, { port: 1, col: 1 }],
  // Arabia
  ['Riyadh', 24.65, 46.7, 'SAU', 'plains', 1, 0, { cap: 1 }],
  ['Jeddah', 21.6, 39.6, 'SAU', 'plains', 1, 0, { port: 1 }],
  ['Medina', 24.47, 39.6, 'SAU', 'hills', 0, 0, {}],
  ['Tabuk', 28.0, 36.6, 'SAU', 'plains', 0, 0, {}],
  ['Hail', 27.52, 41.7, 'SAU', 'plains', 0, 0, {}],
  ['Dhahran', 26.0, 49.7, 'SAU', 'plains', 1, 0, { oil: 1, port: 1 }],
  ['Abha', 18.22, 42.6, 'SAU', 'mountain', 0, 0, {}],
  ['Rub al Khali', 20.5, 50.0, 'SAU', 'plains', 0, 0, {}],
  ['Bahrain', 26.05, 50.55, 'UK', 'plains', 0, 0, { oil: 1, port: 1, col: 1 }],
  ['Doha', 25.3, 51.25, 'UK', 'plains', 0, 0, { col: 1 }],
  ['Sharjah', 24.6, 55.2, 'UK', 'plains', 0, 0, { port: 1, col: 1 }],
  ['Muscat', 23.2, 58.0, 'UK', 'mountain', 0, 0, { port: 1, col: 1 }],
  ['Salalah', 17.4, 54.1, 'UK', 'plains', 0, 0, { port: 1, col: 1 }],
  ['Sanaa', 15.35, 44.2, 'YEM', 'mountain', 0, 0, { cap: 1 }],
  ['Hodeida', 14.8, 43.3, 'YEM', 'plains', 0, 0, { port: 1 }],
  ['Aden', 13.1, 45.1, 'UK', 'plains', 1, 0, { port: 1, col: 1 }],
  ['Mukalla', 15.0, 49.0, 'UK', 'plains', 0, 0, { port: 1, col: 1 }],
  // Afghanistan
  ['Kabul', 34.53, 69.17, 'AFG', 'mountain', 1, 0, { cap: 1 }],
  ['Kandahar', 31.6, 65.7, 'AFG', 'hills', 0, 0, {}],
  ['Herat', 34.35, 62.2, 'AFG', 'hills', 0, 0, {}],
  ['Mazar-i-Sharif', 36.5, 67.1, 'AFG', 'plains', 0, 0, {}],
  ['Faizabad', 36.9, 70.58, 'AFG', 'mountain', 0, 0, {}],
  // India: the north-west and Kashmir
  ['Karachi', 25.1, 67.3, 'UK', 'plains', 1, 0, { port: 1, col: 1 }],
  ['Sukkur', 27.7, 68.86, 'UK', 'plains', 0, 0, { col: 1 }],
  ['Kalat', 27.6, 64.8, 'UK', 'mountain', 0, 0, { col: 1 }],
  ['Quetta', 30.2, 67.2, 'UK', 'mountain', 0, 0, { col: 1 }],
  ['Multan', 30.2, 71.47, 'UK', 'plains', 0, 0, { col: 1 }],
  ['Lahore', 31.55, 74.1, 'UK', 'plains', 2, 1, { col: 1 }],
  ['Rawalpindi', 33.6, 73.05, 'UK', 'hills', 1, 0, { col: 1 }],
  ['Peshawar', 33.9, 71.7, 'UK', 'mountain', 0, 0, { col: 1 }],
  ['Gilgit', 35.92, 74.31, 'UK', 'mountain', 0, 0, { col: 1 }],
  ['Srinagar', 34.08, 74.8, 'UK', 'mountain', 0, 0, { col: 1 }],
  ['Leh', 34.15, 77.58, 'UK', 'mountain', 0, 0, { col: 1 }],
  // The plain of the Ganges, Rajputana and the centre
  ['Delhi', 28.65, 77.2, 'UK', 'urban', 3, 1, { col: 1 }],
  ['Simla', 31.1, 77.17, 'UK', 'mountain', 0, 0, { col: 1 }],
  ['Bikaner', 28.02, 73.3, 'UK', 'plains', 0, 0, { col: 1 }],
  ['Jodhpur', 26.3, 73.02, 'UK', 'plains', 0, 0, { col: 1 }],
  ['Jaipur', 26.9, 75.8, 'UK', 'hills', 0, 0, { col: 1 }],
  ['Agra', 27.18, 78.0, 'UK', 'plains', 0, 0, { col: 1 }],
  ['Lucknow', 26.85, 80.95, 'UK', 'plains', 1, 1, { col: 1 }],
  ['Benares', 25.32, 83.0, 'UK', 'plains', 0, 0, { col: 1 }],
  ['Patna', 25.6, 85.14, 'UK', 'plains', 0, 0, { col: 1 }],
  ['Jhansi', 25.45, 78.57, 'UK', 'hills', 0, 0, { col: 1 }],
  ['Bhopal', 23.26, 77.41, 'UK', 'hills', 0, 0, { col: 1 }],
  ['Ahmedabad', 23.03, 72.58, 'UK', 'plains', 1, 1, { col: 1 }],
  ['Rajkot', 22.3, 70.8, 'UK', 'plains', 0, 0, { port: 1, col: 1 }],
  ['Bombay', 19.3, 73.1, 'UK', 'urban', 3, 2, { port: 1, col: 1 }],
  ['Poona', 18.52, 73.86, 'UK', 'hills', 0, 0, { col: 1 }],
  ['Nagpur', 21.15, 79.09, 'UK', 'plains', 1, 0, { col: 1 }],
  ['Jubbulpore', 23.17, 79.94, 'UK', 'hills', 0, 1, { col: 1 }],
  ['Raipur', 21.25, 81.63, 'UK', 'forest', 0, 0, { col: 1 }],
  // Bengal, Assam and the Burma frontier
  ['Jamshedpur', 22.8, 86.2, 'UK', 'hills', 1, 2, { col: 1 }],
  ['Calcutta', 22.7, 88.4, 'UK', 'urban', 3, 3, { port: 1, col: 1 }],
  ['Cuttack', 20.46, 85.88, 'UK', 'plains', 0, 0, { port: 1, col: 1 }],
  ['Dacca', 23.9, 90.41, 'UK', 'marsh', 0, 0, { col: 1 }],
  ['Chittagong', 22.5, 91.95, 'UK', 'hills', 0, 0, { port: 1, col: 1 }],
  ['Darjeeling', 27.04, 88.26, 'UK', 'mountain', 0, 0, { col: 1 }],
  ['Gauhati', 26.15, 91.74, 'UK', 'hills', 0, 0, { col: 1 }],
  ['Dibrugarh', 27.48, 94.9, 'UK', 'forest', 0, 0, { oil: 1, col: 1 }],
  ['Kohima', 25.67, 94.1, 'UK', 'mountain', 0, 0, { col: 1 }],
  ['Imphal', 24.82, 93.94, 'UK', 'mountain', 0, 0, { col: 1 }],
  // The Deccan and the south
  ['Hyderabad', 17.38, 78.48, 'UK', 'hills', 1, 0, { col: 1 }],
  ['Vizagapatam', 17.9, 83.0, 'UK', 'hills', 0, 0, { port: 1, col: 1 }],
  ['Bellary', 15.14, 76.92, 'UK', 'hills', 0, 0, { col: 1 }],
  ['Bezwada', 16.3, 80.4, 'UK', 'plains', 0, 0, { port: 1, col: 1 }],
  ['Mangalore', 13.2, 75.1, 'UK', 'hills', 0, 0, { port: 1, col: 1 }],
  ['Goa', 15.35, 74.05, 'POR', 'hills', 0, 0, { port: 1, col: 1 }],
  ['Bangalore', 12.97, 77.59, 'UK', 'hills', 1, 1, { col: 1 }],
  ['Calicut', 11.4, 76.0, 'UK', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Madras', 13.1, 80.1, 'UK', 'plains', 2, 1, { port: 1, col: 1 }],
  ['Cochin', 9.97, 76.5, 'UK', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Madura', 9.93, 78.12, 'UK', 'plains', 0, 0, { col: 1 }],
  ['Kathmandu', 27.7, 85.32, 'NEP', 'mountain', 1, 0, { cap: 1 }],
  ['Colombo', 7.0, 80.1, 'UK', 'forest', 1, 0, { port: 1, col: 1 }],
  ['Trincomalee', 8.5, 81.0, 'UK', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Port Blair', 11.7, 92.7, 'UK', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Addu Atoll', -0.69, 73.15, 'UK', 'plains', 0, 0, { port: 1, islet: 1, col: 1 }],
  // Africa: the Atlantic shore and the western Sahara
  ['El Aaiun', 27.1, -12.9, 'ESP', 'desert', 0, 0, { port: 1, col: 1 }],
  ['Villa Cisneros', 23.6, -15.5, 'ESP', 'desert', 0, 0, { port: 1, col: 1 }],
  ['Port-Etienne', 20.9, -16.5, 'FRA', 'desert', 0, 0, { port: 1, vichy: 1, col: 1 }],
  ['Atar', 20.52, -13.05, 'FRA', 'desert', 0, 0, { vichy: 1, col: 1 }],
  ['Boutilimit', 17.7, -14.9, 'FRA', 'desert', 0, 0, { port: 1, vichy: 1, col: 1 }],
  ['Kiffa', 16.62, -11.4, 'FRA', 'desert', 0, 0, { vichy: 1, col: 1 }],
  ['Dakar', 14.75, -17.2, 'FRA', 'plains', 2, 0, { port: 1, vichy: 1, col: 1 }],
  ['Saint-Louis', 16.0, -16.2, 'FRA', 'plains', 0, 0, { port: 1, vichy: 1, col: 1 }],
  ['Tambacounda', 13.77, -13.67, 'FRA', 'plains', 0, 0, { vichy: 1, col: 1 }],
  ['Bathurst', 13.3, -16.0, 'UK', 'plains', 0, 0, { port: 1, col: 1 }],
  ['Bissau', 12.2, -14.9, 'POR', 'marsh', 0, 0, { port: 1, col: 1 }],
  ['Conakry', 10.0, -13.2, 'FRA', 'forest', 0, 0, { port: 1, vichy: 1, col: 1 }],
  ['Kankan', 10.38, -9.3, 'FRA', 'plains', 0, 0, { vichy: 1, col: 1 }],
  ['Freetown', 8.4, -12.8, 'UK', 'forest', 1, 0, { port: 1, col: 1 }],
  ['Monrovia', 6.5, -10.4, 'LBR', 'forest', 0, 0, { port: 1, cap: 1 }],
  ['Abidjan', 5.6, -4.0, 'FRA', 'forest', 1, 0, { port: 1, vichy: 1, col: 1 }],
  ['Bouake', 7.69, -5.03, 'FRA', 'plains', 0, 0, { vichy: 1, col: 1 }],
  ['Bobo-Dioulasso', 11.18, -4.3, 'FRA', 'plains', 0, 0, { vichy: 1, col: 1 }],
  ['Ouagadougou', 12.37, -1.53, 'FRA', 'plains', 0, 0, { vichy: 1, col: 1 }],
  ['Bamako', 12.65, -8.0, 'FRA', 'plains', 1, 0, { vichy: 1, col: 1 }],
  ['Kayes', 14.45, -11.2, 'FRA', 'plains', 0, 0, { vichy: 1, col: 1 }],
  ['Mopti', 14.5, -4.2, 'FRA', 'marsh', 0, 0, { vichy: 1, col: 1 }],
  ['Timbuktu', 16.77, -3.0, 'FRA', 'desert', 0, 0, { vichy: 1, col: 1 }],
  ['Gao', 16.27, -0.05, 'FRA', 'desert', 0, 0, { vichy: 1, col: 1 }],
  ['Taoudenni', 22.68, -3.98, 'FRA', 'desert', 0, 0, { vichy: 1, col: 1 }],
  ['Niamey', 13.6, 2.3, 'FRA', 'plains', 0, 0, { vichy: 1, col: 1 }],
  ['Zinder', 14.0, 8.99, 'FRA', 'plains', 0, 0, { vichy: 1, col: 1 }],
  ['Agadez', 16.97, 7.99, 'FRA', 'desert', 0, 0, { vichy: 1, col: 1 }],
  ['Bilma', 18.69, 12.92, 'FRA', 'desert', 0, 0, { vichy: 1, col: 1 }],
  ['Cotonou', 6.7, 2.4, 'FRA', 'plains', 0, 0, { port: 1, vichy: 1, col: 1 }],
  ['Lome', 6.5, 1.2, 'FRA', 'plains', 0, 0, { port: 1, vichy: 1, col: 1 }],
  ['Accra', 5.8, -0.3, 'UK', 'plains', 1, 0, { port: 1, col: 1 }],
  ['Takoradi', 5.2, -1.9, 'UK', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Tamale', 9.4, -0.84, 'UK', 'plains', 0, 0, { col: 1 }],
  ['Lagos', 6.7, 3.4, 'UK', 'plains', 2, 0, { port: 1, col: 1 }],
  ['Ibadan', 7.9, 4.3, 'UK', 'forest', 0, 0, { col: 1 }],
  ['Port Harcourt', 5.0, 7.0, 'UK', 'marsh', 0, 0, { port: 1, col: 1 }],
  ['Enugu', 6.6, 7.6, 'UK', 'forest', 0, 0, { col: 1 }],
  ['Kaduna', 10.52, 7.44, 'UK', 'plains', 0, 0, { col: 1 }],
  ['Kano', 12.0, 8.52, 'UK', 'plains', 1, 0, { col: 1 }],
  ['Maiduguri', 11.6, 13.0, 'UK', 'plains', 0, 0, { col: 1 }],
  // The Sahara south of the old sheet
  ['Tindouf', 27.67, -7.8, 'FRA', 'desert', 0, 0, { vichy: 1, col: 1 }],
  ['Adrar', 27.87, -0.29, 'FRA', 'desert', 0, 0, { vichy: 1, col: 1 }],
  ['In Salah', 27.2, 2.47, 'FRA', 'desert', 0, 0, { vichy: 1, col: 1 }],
  ['Fort Flatters', 27.9, 6.83, 'FRA', 'desert', 0, 0, { vichy: 1, col: 1 }],
  ['Tamanrasset', 22.79, 5.53, 'FRA', 'desert', 0, 0, { vichy: 1, col: 1 }],
  ['Sebha', 27.04, 14.43, 'ITA', 'desert', 0, 0, { col: 1 }],
  ['Murzuk', 25.6, 13.92, 'ITA', 'desert', 0, 0, { col: 1 }],
  ['Ghat', 25.2, 10.6, 'ITA', 'desert', 0, 0, { col: 1 }],
  ['Waw el Kebir', 25.33, 17.5, 'ITA', 'desert', 0, 0, { col: 1 }],
  ['Kufra', 24.18, 23.0, 'ITA', 'desert', 0, 0, { col: 1 }],
  ['Fort-Lamy', 12.2, 15.4, 'FRA', 'plains', 1, 0, { col: 1 }],
  ['Fort-Archambault', 9.3, 18.39, 'FRA', 'plains', 0, 0, { col: 1 }],
  ['Abeche', 13.83, 20.6, 'FRA', 'plains', 0, 0, { col: 1 }],
  ['Faya-Largeau', 17.93, 19.1, 'FRA', 'desert', 0, 0, { col: 1 }],
  ['Zouar', 20.45, 16.52, 'FRA', 'desert', 0, 0, { col: 1 }],
  // Egypt above the old sheet's edge, and the Sudan
  ['Minya', 28.1, 30.75, 'UK', 'plains', 0, 0, { col: 1 }],
  ['Bahariya', 28.2, 28.4, 'UK', 'desert', 0, 0, { col: 1 }],
  ['Asyut', 27.18, 31.18, 'UK', 'plains', 1, 0, { col: 1 }],
  ['Qena', 26.16, 32.6, 'UK', 'plains', 0, 0, { col: 1 }],
  ['Aswan', 24.09, 32.8, 'UK', 'desert', 0, 0, { col: 1 }],
  ['Kharga', 25.45, 29.5, 'UK', 'desert', 0, 0, { col: 1 }],
  ['Hurghada', 27.0, 33.4, 'UK', 'desert', 0, 0, { port: 1, col: 1 }],
  ['Tor', 28.2, 34.0, 'UK', 'desert', 0, 0, { col: 1 }],
  ['Wadi Halfa', 21.3, 31.0, 'UK', 'desert', 0, 0, { col: 1 }],
  ['Dongola', 19.17, 30.0, 'UK', 'desert', 0, 0, { col: 1 }],
  ['Atbara', 17.7, 33.98, 'UK', 'desert', 0, 0, { col: 1 }],
  ['Port Sudan', 19.5, 36.9, 'UK', 'desert', 1, 0, { port: 1, col: 1 }],
  ['Khartoum', 15.6, 32.53, 'UK', 'plains', 2, 0, { col: 1 }],
  ['Wad Medani', 14.3, 33.3, 'UK', 'plains', 1, 0, { col: 1 }],
  ['Kassala', 15.3, 35.9, 'UK', 'plains', 0, 0, { col: 1 }],
  ['El Obeid', 13.18, 30.22, 'UK', 'plains', 0, 0, { col: 1 }],
  ['El Fasher', 13.63, 25.35, 'UK', 'plains', 0, 0, { col: 1 }],
  ['Malakal', 9.53, 31.65, 'UK', 'marsh', 0, 0, { col: 1 }],
  ['Wau', 7.7, 28.0, 'UK', 'plains', 0, 0, { col: 1 }],
  ['Juba', 5.0, 31.6, 'UK', 'plains', 0, 0, { col: 1 }],
  // Italian East Africa and the Horn
  ['Asmara', 15.2, 38.93, 'ITA', 'mountain', 1, 0, { col: 1 }],
  ['Keren', 15.9, 38.1, 'ITA', 'mountain', 0, 0, { col: 1 }],
  ['Massawa', 15.6, 39.3, 'ITA', 'desert', 0, 0, { port: 1, col: 1 }],
  ['Assab', 13.3, 42.2, 'ITA', 'desert', 0, 0, { port: 1, col: 1 }],
  ['Addis Ababa', 9.03, 38.74, 'ITA', 'mountain', 2, 0, { col: 1 }],
  ['Gondar', 12.6, 37.47, 'ITA', 'mountain', 0, 0, { col: 1 }],
  ['Mekele', 13.5, 39.47, 'ITA', 'mountain', 0, 0, { col: 1 }],
  ['Dessie', 11.13, 39.63, 'ITA', 'mountain', 0, 0, { col: 1 }],
  ['Debra Markos', 10.33, 37.72, 'ITA', 'mountain', 0, 0, { col: 1 }],
  ['Lekemti', 9.08, 36.2, 'ITA', 'hills', 0, 0, { col: 1 }],
  ['Jimma', 7.67, 36.83, 'ITA', 'forest', 0, 0, { col: 1 }],
  ['Harar', 9.31, 42.0, 'ITA', 'hills', 0, 0, { col: 1 }],
  ['Gabredarre', 6.74, 44.27, 'ITA', 'desert', 0, 0, { col: 1 }],
  ['Neghelli', 5.33, 39.58, 'ITA', 'plains', 0, 0, { col: 1 }],
  ['Djibouti', 11.3, 42.7, 'FRA', 'desert', 0, 0, { port: 1, vichy: 1, col: 1 }],
  ['Berbera', 10.2, 45.2, 'UK', 'desert', 0, 0, { port: 1, col: 1 }],
  ['Hargeisa', 9.7, 44.06, 'UK', 'desert', 0, 0, { col: 1 }],
  ['Mogadishu', 2.3, 45.2, 'ITA', 'plains', 1, 0, { port: 1, col: 1 }],
  ['Kismayu', 0.0, 42.3, 'ITA', 'plains', 0, 0, { port: 1, col: 1 }],
  ['Belet Uen', 4.74, 45.4, 'ITA', 'desert', 0, 0, { col: 1 }],
  ['Obbia', 5.6, 48.2, 'ITA', 'desert', 0, 0, { port: 1, col: 1 }],
  ['Bender Cassim', 10.8, 49.3, 'ITA', 'desert', 0, 0, { port: 1, col: 1 }],
  // East Africa
  ['Nairobi', -1.29, 36.82, 'UK', 'hills', 1, 0, { col: 1 }],
  ['Mombasa', -3.8, 39.4, 'UK', 'plains', 1, 0, { port: 1, col: 1 }],
  ['Kisumu', 0.3, 34.9, 'UK', 'plains', 0, 0, { col: 1 }],
  ['Garissa', -0.46, 39.65, 'UK', 'desert', 0, 0, { col: 1 }],
  ['Wajir', 1.75, 40.06, 'UK', 'desert', 0, 0, { col: 1 }],
  ['Marsabit', 2.33, 37.6, 'UK', 'desert', 0, 0, { col: 1 }],
  ['Kampala', 0.7, 32.58, 'UK', 'plains', 0, 0, { col: 1 }],
  ['Gulu', 2.77, 32.3, 'UK', 'plains', 0, 0, { col: 1 }],
  ['Dar es Salaam', -7.0, 39.0, 'UK', 'plains', 1, 0, { port: 1, col: 1 }],
  ['Tanga', -5.3, 38.8, 'UK', 'plains', 0, 0, { port: 1, col: 1 }],
  ['Arusha', -3.6, 36.68, 'UK', 'hills', 0, 0, { col: 1 }],
  ['Mwanza', -3.3, 33.0, 'UK', 'plains', 0, 0, { col: 1 }],
  ['Tabora', -5.4, 32.8, 'UK', 'plains', 0, 0, { col: 1 }],
  ['Dodoma', -6.17, 35.74, 'UK', 'plains', 0, 0, { col: 1 }],
  ['Mbeya', -8.7, 33.8, 'UK', 'mountain', 0, 0, { col: 1 }],
  ['Lindi', -10.0, 39.3, 'UK', 'plains', 0, 0, { port: 1, col: 1 }],
  ['Zanzibar', -6.1, 39.35, 'UK', 'plains', 0, 0, { port: 1, col: 1 }],
  // The Congo, the Gulf of Guinea and Angola
  ['Douala', 4.3, 10.0, 'FRA', 'forest', 1, 0, { port: 1, col: 1 }],
  ['Yaounde', 3.87, 12.0, 'FRA', 'forest', 0, 0, { col: 1 }],
  ['Garoua', 9.0, 13.6, 'FRA', 'plains', 0, 0, { col: 1 }],
  ['Bata', 1.6, 10.2, 'ESP', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Libreville', 0.2, 10.0, 'FRA', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Lambarene', -1.4, 10.4, 'FRA', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Franceville', -1.63, 13.3, 'FRA', 'forest', 0, 0, { col: 1 }],
  ['Brazzaville', -3.9, 15.0, 'FRA', 'forest', 1, 0, { col: 1 }],
  ['Pointe-Noire', -4.4, 12.2, 'FRA', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Ouesso', 1.4, 15.8, 'FRA', 'forest', 0, 0, { col: 1 }],
  ['Bangui', 4.9, 18.4, 'FRA', 'forest', 0, 0, { col: 1 }],
  ['Ndele', 8.0, 21.5, 'FRA', 'plains', 0, 0, { col: 1 }],
  ['Usumbura', -3.0, 29.9, 'BEL', 'hills', 0, 0, { col: 1 }],
  ['Leopoldville', -4.8, 15.6, 'BEL', 'forest', 1, 0, { col: 1 }],
  ['Matadi', -5.4, 14.3, 'BEL', 'hills', 0, 0, { port: 1, col: 1 }],
  ['Coquilhatville', -0.2, 19.0, 'BEL', 'forest', 0, 0, { col: 1 }],
  ['Stanleyville', 0.52, 25.2, 'BEL', 'forest', 1, 0, { col: 1 }],
  ['Buta', 2.8, 24.0, 'BEL', 'forest', 0, 0, { col: 1 }],
  ['Bunia', 1.8, 29.6, 'BEL', 'hills', 0, 0, { col: 1 }],
  ['Costermansville', -2.6, 28.3, 'BEL', 'mountain', 0, 0, { col: 1 }],
  ['Albertville', -6.0, 28.6, 'BEL', 'hills', 0, 0, { col: 1 }],
  ['Luluabourg', -5.9, 22.42, 'BEL', 'plains', 0, 0, { col: 1 }],
  ['Kamina', -8.74, 25.0, 'BEL', 'plains', 0, 0, { col: 1 }],
  ['Elisabethville', -11.4, 27.3, 'BEL', 'plains', 2, 1, { col: 1 }],
  ['Luanda', -9.0, 13.6, 'POR', 'plains', 1, 0, { port: 1, col: 1 }],
  ['Cabinda', -5.1, 12.4, 'POR', 'forest', 0, 0, { port: 1, col: 1 }],
  ['Malange', -9.54, 16.6, 'POR', 'plains', 0, 0, { col: 1 }],
  ['Lobito', -12.4, 13.9, 'POR', 'plains', 0, 0, { port: 1, col: 1 }],
  ['Nova Lisboa', -12.78, 16.0, 'POR', 'hills', 0, 0, { col: 1 }],
  ['Luso', -11.78, 19.92, 'POR', 'plains', 0, 0, { col: 1 }],
  ['Mossamedes', -15.0, 12.6, 'POR', 'desert', 0, 0, { port: 1, col: 1 }],
  ['Serpa Pinto', -14.66, 17.69, 'POR', 'plains', 0, 0, { col: 1 }],
  // The Rhodesias, Mozambique and the south
  ['Lusaka', -15.42, 28.28, 'UK', 'plains', 0, 0, { col: 1 }],
  ['Ndola', -13.3, 28.3, 'UK', 'plains', 1, 1, { col: 1 }],
  ['Mongu', -15.25, 23.4, 'UK', 'plains', 0, 0, { col: 1 }],
  ['Kasama', -10.6, 31.18, 'UK', 'plains', 0, 0, { col: 1 }],
  ['Salisbury', -17.83, 31.05, 'UK', 'hills', 1, 0, { col: 1 }],
  ['Bulawayo', -20.15, 28.58, 'UK', 'plains', 1, 0, { col: 1 }],
  ['Blantyre', -15.5, 35.0, 'UK', 'hills', 0, 0, { col: 1 }],
  ['Mzimba', -11.9, 33.6, 'UK', 'hills', 0, 0, { col: 1 }],
  ['Lourenco Marques', -25.6, 32.4, 'POR', 'plains', 1, 0, { port: 1, col: 1 }],
  ['Inhambane', -23.6, 34.9, 'POR', 'plains', 0, 0, { port: 1, col: 1 }],
  ['Beira', -19.6, 34.5, 'POR', 'marsh', 1, 0, { port: 1, col: 1 }],
  ['Tete', -16.5, 33.4, 'POR', 'hills', 0, 0, { col: 1 }],
  ['Quelimane', -17.5, 36.7, 'POR', 'marsh', 0, 0, { port: 1, col: 1 }],
  ['Nampula', -15.12, 39.27, 'POR', 'plains', 0, 0, { port: 1, col: 1 }],
  ['Porto Amelia', -12.8, 40.0, 'POR', 'plains', 0, 0, { port: 1, col: 1 }],
  ['Gaberones', -24.2, 25.4, 'UK', 'plains', 0, 0, { col: 1 }],
  ['Francistown', -21.3, 27.2, 'UK', 'plains', 0, 0, { col: 1 }],
  ['Maun', -19.98, 23.42, 'UK', 'marsh', 0, 0, { col: 1 }],
  ['Ghanzi', -22.2, 21.65, 'UK', 'desert', 0, 0, { col: 1 }],
  ['Windhoek', -22.57, 17.08, 'SAF', 'hills', 0, 0, { col: 1 }],
  ['Walvis Bay', -22.6, 15.0, 'SAF', 'desert', 0, 0, { port: 1, col: 1 }],
  ['Luderitz', -26.5, 15.7, 'SAF', 'desert', 0, 0, { port: 1, col: 1 }],
  ['Keetmanshoop', -26.3, 18.13, 'SAF', 'desert', 0, 0, { col: 1 }],
  ['Tsumeb', -19.25, 17.72, 'SAF', 'plains', 0, 0, { col: 1 }],
  ['Cape Town', -33.7, 18.8, 'SAF', 'hills', 3, 1, { port: 1 }],
  ['Port Elizabeth', -33.7, 25.5, 'SAF', 'plains', 1, 1, { port: 1 }],
  ['East London', -32.7, 27.6, 'SAF', 'hills', 0, 0, { port: 1 }],
  ['Durban', -29.6, 30.7, 'SAF', 'hills', 2, 1, { port: 1 }],
  ['Ladysmith', -28.3, 29.9, 'SAF', 'hills', 0, 0, {}],
  ['Johannesburg', -26.35, 27.9, 'SAF', 'urban', 3, 3, {}],
  ['Pretoria', -25.5, 28.4, 'SAF', 'hills', 2, 1, { cap: 1 }],
  ['Pietersburg', -23.7, 29.45, 'SAF', 'plains', 0, 0, {}],
  ['Klerksdorp', -26.85, 26.3, 'SAF', 'plains', 0, 0, {}],
  ['Kimberley', -28.74, 24.5, 'SAF', 'plains', 1, 0, {}],
  ['Bloemfontein', -29.12, 26.21, 'SAF', 'plains', 1, 0, {}],
  ['Upington', -28.6, 21.25, 'SAF', 'desert', 0, 0, {}],
  ['Beaufort West', -32.35, 22.58, 'SAF', 'desert', 0, 0, {}],
  ['Springbok', -29.9, 18.2, 'SAF', 'desert', 0, 0, {}],
  ['Maseru', -29.5, 28.2, 'UK', 'mountain', 0, 0, { col: 1 }],
  ['Mbabane', -26.5, 31.5, 'UK', 'hills', 0, 0, { col: 1 }],
  // Madagascar and the islands
  ['Tananarive', -18.9, 47.52, 'FRA', 'hills', 1, 0, { vichy: 1, col: 1 }],
  ['Diego Suarez', -12.6, 49.2, 'FRA', 'hills', 1, 0, { port: 1, vichy: 1, col: 1 }],
  ['Majunga', -16.0, 46.6, 'FRA', 'plains', 0, 0, { port: 1, vichy: 1, col: 1 }],
  ['Tamatave', -17.9, 49.1, 'FRA', 'forest', 0, 0, { port: 1, vichy: 1, col: 1 }],
  ['Tulear', -23.1, 44.2, 'FRA', 'plains', 0, 0, { port: 1, vichy: 1, col: 1 }],
  ['Fort-Dauphin', -24.6, 46.7, 'FRA', 'hills', 0, 0, { port: 1, vichy: 1, col: 1 }],
  ['Morondava', -20.3, 44.8, 'FRA', 'plains', 0, 0, { port: 1, vichy: 1, col: 1 }],
  ['Fianarantsoa', -21.6, 47.3, 'FRA', 'hills', 0, 0, { port: 1, vichy: 1, col: 1 }],
  ['Maroantsetra', -15.2, 49.6, 'FRA', 'forest', 0, 0, { port: 1, vichy: 1, col: 1 }],
  ['Reunion', -21.1, 55.5, 'FRA', 'mountain', 0, 0, { port: 1, vichy: 1, col: 1 }],
  ['Mauritius', -20.25, 57.6, 'UK', 'hills', 0, 0, { port: 1, col: 1 }],
  ['Seychelles', -4.65, 55.47, 'UK', 'hills', 0, 0, { port: 1, islet: 1, col: 1 }],
  ['Cape Verde', 15.1, -23.65, 'POR', 'hills', 0, 0, { port: 1, col: 1 }],
  ['Sao Tome', 0.25, 6.6, 'POR', 'forest', 0, 0, { port: 1, col: 1 }],
];

/**
 * The frontiers of the globe slice, as 1939 had them, made of today's countries in the atlas. An enclave is listed
 * before the country round it, so its edge is its own.
 */
export const GLOBE_FRONTIERS = [
  // Portuguese Goa, drawn by hand out of India.
  { name: 'Goa', line: [[15.8, 73.5], [15.72, 73.95], [15.62, 74.2], [15.3, 74.33], [14.95, 74.25], [14.85, 74.0]], close: [[14.85, 73.5]], owners: ['POR'] },
  { country: 'Nepal', owners: ['NEP'] },
  { name: 'India', country: ['India', 'Pakistan', 'Bangladesh', 'Bhutan', 'Siachen Glacier'], owners: ['UK'] },
  { country: 'Afghanistan', owners: ['AFG'] },
  { name: 'Persia', country: 'Iran', owners: ['IRN'] },
  { country: 'Kuwait', owners: ['UK'] },
  { country: 'Iraq', owners: ['IRQ'] },
  // The Imam's Yemen out of today's: the highlands and the Red Sea coast, north and west of the line agreed with
  // Britain in 1934. The rest is the Aden Protectorate, with Socotra.
  {
    name: 'Yemen', country: 'Yemen', owners: ['YEM'],
    clip: [[12.55, 43.4], [13.05, 44.1], [13.4, 44.65], [13.8, 45.05], [14.4, 45.75], [15.2, 46.1], [16.2, 46.3], [17.8, 46.4], [18.2, 44.0], [17.2, 42.0], [15.0, 41.3], [12.4, 42.4]],
  },
  { name: 'Aden Protectorate', country: 'Yemen', owners: ['UK'] },
  { name: 'Muscat and Oman', country: 'Oman', owners: ['UK'] },
  { name: 'Trucial Coast', country: 'United Arab Emirates', owners: ['UK'] },
  { country: 'Qatar', owners: ['UK'] },
  { country: 'Bahrain', owners: ['UK'] },
  { country: 'Saudi Arabia', owners: ['SAU'] },
  // The Soviet Union west of the Far East (which the Pacific slice has): its republics as one.
  // (Kept west of 112 E: the Kuriles are in Russia's outline today, and were Japan's.)
  { name: 'Soviet Union', country: ['Russia', 'Kazakhstan', 'Uzbekistan', 'Turkmenistan', 'Kyrgyzstan', 'Tajikistan', 'Azerbaijan', 'Armenia', 'Georgia'], owners: ['SOV'], clip: [[34, 44], [82, 44], [82, 112], [34, 112]] },
  // Africa
  { country: 'Gambia', owners: ['UK'] },
  { name: 'Portuguese Guinea', country: 'Guinea-Bissau', owners: ['POR'] },
  { country: 'Sierra Leone', owners: ['UK'] },
  { country: 'Liberia', owners: ['LBR'] },
  { country: 'Togo', owners: ['FRA'] },
  { name: 'Gold Coast', country: 'Ghana', owners: ['UK'] },
  { country: 'Nigeria', owners: ['UK'] },
  { name: 'Spanish Guinea', country: 'Eq. Guinea', owners: ['ESP'] },
  { name: 'Spanish Sahara', country: ['Morocco', 'W. Sahara'], owners: ['ESP'] },
  { country: 'Mauritania', owners: ['FRA'] },
  { country: 'Senegal', owners: ['FRA'] },
  { name: 'French Guinea', country: 'Guinea', owners: ['FRA'] },
  { name: 'Ivory Coast', country: ["Côte d'Ivoire", 'Burkina Faso'], owners: ['FRA'] },
  { name: 'French Sudan', country: 'Mali', owners: ['FRA'] },
  { country: 'Niger', owners: ['FRA'] },
  { name: 'Dahomey', country: 'Benin', owners: ['FRA'] },
  { country: 'Algeria', owners: ['FRA'] },
  { country: 'Libya', owners: ['ITA'] },
  { country: 'Egypt', owners: ['UK'] },
  { name: 'Anglo-Egyptian Sudan', country: ['Sudan', 'S. Sudan'], owners: ['UK'] },
  { country: 'Chad', owners: ['FRA'] },
  { country: 'Cameroon', owners: ['FRA'] },
  { name: 'Ubangi-Shari', country: 'Central African Rep.', owners: ['FRA'] },
  { country: 'Gabon', owners: ['FRA'] },
  { name: 'Middle Congo', country: 'Congo', owners: ['FRA'] },
  { name: 'Ruanda-Urundi', country: ['Rwanda', 'Burundi'], owners: ['BEL'] },
  { name: 'Belgian Congo', country: 'Dem. Rep. Congo', owners: ['BEL'] },
  { country: 'Angola', owners: ['POR'] },
  { name: 'French Somaliland', country: 'Djibouti', owners: ['FRA'] },
  { country: 'Eritrea', owners: ['ITA'] },
  { name: 'British Somaliland', country: 'Somaliland', owners: ['UK'] },
  { name: 'Italian Somaliland', country: 'Somalia', owners: ['ITA'] },
  { country: 'Ethiopia', owners: ['ITA'] },
  { country: 'Kenya', owners: ['UK'] },
  { country: 'Uganda', owners: ['UK'] },
  { name: 'Tanganyika', country: 'Tanzania', owners: ['UK'] },
  { name: 'Nyasaland', country: 'Malawi', owners: ['UK'] },
  { name: 'Northern Rhodesia', country: 'Zambia', owners: ['UK'] },
  { name: 'Southern Rhodesia', country: 'Zimbabwe', owners: ['UK'] },
  { country: 'Mozambique', owners: ['POR'] },
  { name: 'Bechuanaland', country: 'Botswana', owners: ['UK'] },
  { name: 'Basutoland', country: 'Lesotho', owners: ['UK'] },
  { name: 'Swaziland', country: 'eSwatini', owners: ['UK'] },
  { name: 'South West Africa', country: 'Namibia', owners: ['SAF'] },
  { country: 'South Africa', owners: ['SAF'] },
  { name: 'Madagascar', country: ['Madagascar', 'Comoros'], owners: ['FRA'] },
  { name: 'Cape Verde', country: 'Cabo Verde', owners: ['POR'] },
  { name: 'Sao Tome', country: 'São Tomé and Principe', owners: ['POR'] },
];

export const GLOBE_LINKS = [];
export const GLOBE_ROADS = [];
export const GLOBE_NO_BORDER = [
  // The Libyan Desert between Kufra and the Nile (the design director's note of 14:18Z, 22 November 2026). Only
  // patrols and a small garrison's track crossed it, never an army, and nothing was fed over it: the record's Kufra
  // fell from Chad (March 1941) and the Fezzan from Chad (January 1943). So no march, attack or supply line crosses
  // Kufra's borders with the Sudan. Chad's with Kufra stay open.
  ['Kufra', 'Dongola'], ['Kufra', 'Wadi Halfa'],
  // The Great Sand Sea between Kufra and Egypt's oases (the director's note of 14:23Z): only patrols and a car party
  // crossed it, never an army or an army's supply. (Farafra is a city cut from an oasis province: its border is its
  // province's there. Siwa is a province of today's chart, and its border with Kufra crosses the same sand.)
  ['Kufra', 'Bahariya'], ['Kufra', 'Kharga'], ['Kufra', 'Siwa'],
];

/**
 * The slices of the far side, in the order they are numbered. Each frontier, and each island no frontier runs
 * through, belongs to one slice, so a later slice never moves an earlier one's provinces.
 */
export const FAR_SLICES = [
  { name: 'pacific', regions: PACIFIC_REGIONS, beyond: PACIFIC_BEYOND, frontiers: PACIFIC_FRONTIERS, links: PACIFIC_LINKS, roads: PACIFIC_ROADS, noBorder: PACIFIC_NO_BORDER },
  { name: 'globe', regions: GLOBE_REGIONS, beyond: [], frontiers: GLOBE_FRONTIERS, links: GLOBE_LINKS, roads: GLOBE_ROADS, noBorder: GLOBE_NO_BORDER },
];

/** The lakes cut out of the new land by hand, as the Great Lakes are: name, surface in metres above the sea, outline in [lat, lon]. */
export const FAR_LAKES = [
  ['Lake Baikal', 456, [[51.5, 103.7], [51.85, 104.9], [52.3, 106.0], [52.9, 107.0], [53.4, 107.6], [54.0, 108.6], [54.9, 109.3], [55.75, 109.6], [55.7, 109.95], [55.0, 109.8], [54.3, 109.5], [53.7, 109.0], [53.0, 108.2], [52.5, 107.2], [52.2, 106.4], [51.75, 105.7], [51.45, 104.6]]],
  // The Aral Sea at its 1939 shore.
  ['Aral Sea', 53, [[46.75, 61.6], [46.5, 60.6], [46.0, 59.7], [45.3, 58.9], [44.6, 58.3], [43.9, 58.25], [43.55, 59.2], [43.75, 60.3], [44.3, 61.1], [44.9, 61.7], [45.5, 61.95], [46.2, 61.95]]],
  // The great lakes of Africa. They lie in land of today's chart, whose coast is not rewritten: they are cut out of
  // the provinces and listed with the lakes.
  ['Lake Victoria', 1134, [[0.15, 31.85], [0.1, 32.5], [0.45, 33.2], [0.3, 33.9], [-0.1, 34.1], [-0.15, 34.7], [-0.45, 34.65], [-0.5, 34.15], [-1.0, 34.0], [-1.6, 33.8], [-2.2, 33.6], [-2.2, 33.05], [-2.6, 32.9], [-2.9, 32.3], [-2.6, 31.85], [-1.8, 31.7], [-1.0, 31.75]]],
  ['Lake Tanganyika', 773, [[-3.35, 29.3], [-4.0, 29.45], [-4.9, 29.6], [-5.6, 29.85], [-6.3, 29.95], [-6.9, 30.55], [-7.6, 30.75], [-8.3, 31.15], [-8.8, 31.1], [-8.6, 30.6], [-7.9, 30.35], [-7.2, 30.0], [-6.6, 29.5], [-5.9, 29.2], [-5.0, 29.1], [-4.2, 29.1], [-3.5, 29.15]]],
  ['Lake Nyasa', 474, [[-9.5, 34.0], [-10.0, 34.55], [-11.0, 34.85], [-12.0, 34.95], [-13.0, 34.95], [-13.7, 35.0], [-14.4, 35.25], [-14.3, 34.9], [-13.7, 34.6], [-13.0, 34.4], [-12.0, 34.1], [-11.2, 34.25], [-10.4, 34.2], [-9.9, 33.95]]],
];

/**
 * The great rivers of the far side, as in world.mjs: [name, course in [lat, lon]]. tools/build-rivers.mjs (run
 * with 'far') finds the crossings: neighbouring provinces whose centres lie on opposite banks.
 */
export const FAR_RIVERS = [
  // China. The Yellow River runs as it ran from June 1938 to 1947, south-east from the broken dykes above
  // Chengchow to the Huai: the flood was the front between the two armies in Honan.
  ['Yangtze', [[30.0, 99.1], [28.0, 99.3], [26.87, 99.95], [27.6, 100.4], [26.6, 101.7], [26.2, 102.9], [27.4, 103.2], [28.77, 104.63], [28.88, 105.44], [29.56, 106.58], [29.7, 107.4], [30.81, 108.38], [31.02, 109.47], [30.7, 111.29], [30.32, 112.25], [29.75, 112.9], [29.45, 113.15], [30.0, 113.9], [30.58, 114.29], [30.22, 115.1], [29.73, 116.0], [30.51, 117.05], [31.35, 118.37], [32.08, 118.75], [32.2, 119.45], [31.95, 120.3], [31.75, 121.1], [31.4, 121.9]]],
  ['Yellow River', [[36.06, 103.8], [37.5, 105.0], [38.5, 106.4], [39.7, 106.8], [40.7, 107.4], [40.6, 109.85], [40.3, 111.2], [39.0, 111.1], [38.0, 110.5], [37.0, 110.45], [36.1, 110.45], [35.6, 110.6], [34.6, 110.3], [34.8, 111.2], [34.85, 112.5], [34.9, 113.65], [34.2, 114.3], [33.6, 114.65], [32.9, 115.8], [32.6, 116.6], [32.95, 117.35], [33.2, 118.4], [33.8, 119.3], [34.3, 120.2]]],
  ['Han', [[33.07, 107.03], [32.7, 109.0], [32.8, 110.8], [32.38, 111.67], [32.02, 112.15], [31.2, 112.6], [30.4, 113.4], [30.57, 114.28]]],
  ['West River', [[22.8, 108.3], [23.1, 109.6], [23.4, 110.1], [23.48, 111.3], [23.15, 112.0], [23.05, 112.5], [22.75, 113.1], [22.3, 113.55]]],
  // Manchuria and the Soviet frontier: the Argun and the Amur, the Ussuri, the Sungari, the Liao, and Korea's two
  // frontier rivers.
  ['Argun', [[49.6, 117.9], [50.25, 119.3], [51.3, 120.0], [52.3, 120.7], [53.33, 121.5]]],
  ['Amur', [[53.33, 121.5], [53.4, 124.1], [52.5, 126.3], [51.2, 126.9], [50.25, 127.5], [49.4, 129.0], [48.8, 130.5], [47.7, 131.0], [47.7, 132.5], [48.2, 134.4], [48.45, 135.05], [49.4, 136.5], [50.55, 137.0], [51.3, 138.6], [52.4, 140.4], [53.15, 140.7], [53.0, 141.2]]],
  ['Ussuri', [[45.1, 133.5], [45.93, 133.7], [46.8, 134.05], [47.7, 134.7], [48.4, 134.9]]],
  ['Sungari', [[43.85, 126.55], [44.6, 126.1], [45.3, 124.8], [45.78, 126.6], [46.0, 128.7], [46.8, 130.3], [47.3, 131.5], [47.7, 132.5]]],
  ['Liao', [[43.6, 122.3], [43.0, 123.6], [42.0, 123.0], [41.3, 122.4], [40.7, 122.1]]],
  ['Yalu', [[39.9, 124.3], [40.1, 124.4], [40.45, 124.9], [40.9, 125.9], [41.3, 126.5], [41.8, 126.9], [41.5, 127.8], [41.4, 128.2]]],
  ['Tumen', [[42.0, 128.9], [42.45, 129.6], [42.95, 129.85], [42.85, 130.2], [42.3, 130.7]]],
  // South-East Asia.
  ['Red River', [[22.5, 103.97], [21.7, 104.87], [21.03, 105.85], [20.3, 106.55]]],
  ['Mekong', [[25.2, 99.35], [22.0, 100.8], [21.2, 101.15], [20.35, 100.1], [19.9, 102.14], [19.25, 101.75], [18.2, 101.45], [17.9, 101.67], [17.97, 102.6], [18.4, 103.7], [17.4, 104.8], [16.55, 104.75], [15.1, 105.8], [13.55, 105.95], [12.5, 106.0], [11.56, 104.93], [10.75, 105.2], [10.0, 106.3], [9.8, 106.6]]],
  ['Chao Phraya', [[15.7, 100.13], [14.9, 100.4], [13.75, 100.5], [13.55, 100.6]]],
  ['Salween', [[28.0, 98.65], [26.5, 98.9], [25.3, 98.85], [24.3, 98.95], [23.5, 98.8], [22.0, 98.7], [20.5, 98.6], [19.5, 97.9], [18.5, 97.5], [17.5, 97.75], [16.55, 97.6]]],
  ['Sittang', [[19.8, 96.2], [18.93, 96.45], [17.9, 96.8], [17.2, 96.9], [16.4, 96.95]]],
  ['Irrawaddy', [[25.4, 97.4], [24.25, 97.23], [24.18, 96.33], [23.0, 96.0], [21.98, 96.06], [21.46, 95.38], [20.9, 94.82], [20.46, 94.87], [18.82, 95.22], [17.65, 95.45], [15.9, 95.2]]],
  ['Chindwin', [[26.0, 95.7], [24.87, 94.9], [23.2, 94.3], [22.1, 95.13], [21.45, 95.25]]],
  // Siberia, Alaska and Australia.
  ['Lena', [[57.8, 108.1], [59.45, 112.57], [60.72, 114.93], [60.37, 120.43], [61.48, 129.15], [62.03, 129.73], [63.92, 127.47], [66.77, 123.37], [70.68, 127.37], [71.4, 127.3]]],
  ['Yukon', [[64.8, -141.0], [65.83, -144.06], [66.56, -145.27], [66.0, -149.1], [65.17, -152.08], [64.74, -155.5], [64.74, -156.9], [64.33, -158.72], [62.2, -159.77], [61.78, -161.32], [62.6, -164.4]]],
  ['Murray', [[-36.8, 148.2], [-36.2, 148.0], [-36.0, 147.5], [-36.08, 146.9], [-36.13, 144.75], [-35.34, 143.55], [-34.2, 142.15], [-34.17, 140.75], [-34.03, 139.67], [-35.12, 139.27], [-35.55, 138.88]]],
  // The globe slice. The Volga east of the old sheet, where the map of Europe's two stretches leave off.
  ['Middle Volga', [[56.1, 47.0], [55.8, 48.85], [55.35, 49.5], [54.3, 48.6], [53.5, 49.6], [53.15, 49.95], [53.1, 48.6], [52.5, 47.0]]],
  ['Lower Volga', [[47.2, 47.0], [46.35, 47.95], [45.7, 47.9]]],
  ['Kama', [[59.6, 56.8], [58.0, 56.05], [57.0, 54.9], [56.47, 53.8], [55.75, 52.4], [55.37, 50.6], [55.25, 49.5]]],
  ['Ural', [[54.5, 59.3], [53.4, 58.9], [52.4, 58.7], [51.2, 58.5], [51.4, 57.0], [51.7, 55.25], [51.4, 53.5], [51.15, 51.55], [50.0, 51.2], [48.5, 51.7], [47.1, 51.85], [46.9, 51.8]]],
  ['Irtysh', [[47.9, 84.9], [49.95, 82.6], [50.4, 80.05], [52.3, 76.75], [54.98, 73.15], [56.9, 74.5], [58.2, 68.05], [61.0, 68.9]]],
  ['Ob', [[52.5, 85.2], [53.35, 83.95], [55.03, 82.7], [56.3, 84.0], [58.3, 82.65], [60.9, 76.6], [61.1, 73.4], [61.0, 68.9], [63.9, 65.35], [66.45, 66.5], [66.7, 69.5]]],
  ['Yenisei', [[51.72, 94.3], [53.0, 91.5], [53.7, 91.65], [56.0, 93.1], [58.45, 92.35], [61.6, 90.1], [65.8, 88.15], [67.45, 86.6], [69.4, 86.2], [70.5, 83.2]]],
  ['Angara', [[51.85, 104.85], [52.3, 104.15], [53.15, 103.1], [56.15, 101.85], [58.0, 102.7], [58.4, 98.0], [58.1, 93.0]]],
  ['Syr Darya', [[40.9, 71.8], [40.3, 69.6], [41.0, 68.7], [42.3, 68.2], [44.8, 65.65], [45.75, 62.1], [46.1, 61.0]]],
  ['Amu Darya', [[36.7, 71.6], [37.5, 71.5], [38.3, 71.3], [38.45, 70.8], [37.9, 70.2], [37.55, 69.5], [37.2, 69.0], [37.2, 67.3], [37.5, 65.7], [39.1, 63.6], [41.2, 61.4], [41.55, 60.6], [42.5, 59.6], [43.7, 59.2]]],
  ['Shatt al-Arab', [[32.0, 47.0], [31.0, 47.4], [30.55, 47.85], [29.95, 48.55]]],
  ['Indus', [[34.05, 77.6], [35.3, 75.6], [35.75, 74.6], [35.2, 73.2], [34.0, 72.3], [32.9, 71.5], [31.8, 70.9], [30.2, 70.8], [28.9, 70.5], [27.7, 69.05], [26.2, 68.1], [25.4, 68.3], [24.0, 67.7]]],
  ['Ganges', [[30.0, 78.2], [28.9, 78.1], [27.4, 79.6], [26.45, 80.35], [25.43, 81.88], [25.22, 83.0], [25.7, 85.14], [25.25, 87.0], [24.8, 87.95], [24.05, 89.0], [23.8, 89.8], [23.2, 90.6], [22.3, 91.0]]],
  ['Brahmaputra', [[27.9, 95.6], [27.6, 94.9], [26.8, 93.5], [26.25, 91.74], [26.0, 90.0], [25.0, 89.7], [24.0, 89.75], [23.8, 89.8]]],
  // Africa. The Nile above the old sheet's edge, to the two lakes it rises from.
  ['Upper Nile', [[28.6, 30.85], [27.2, 31.3], [26.15, 32.75], [25.7, 32.65], [24.1, 32.95], [21.9, 31.3], [21.0, 30.6], [19.6, 30.4], [18.0, 31.0], [18.5, 31.8], [19.5, 33.3], [17.7, 33.9], [16.7, 33.4], [15.65, 32.5]]],
  ['White Nile', [[15.65, 32.5], [13.2, 32.7], [11.0, 32.7], [9.5, 31.5], [9.5, 30.5], [7.0, 30.3], [6.2, 31.55], [4.85, 31.75], [3.6, 32.05], [2.3, 31.4], [1.7, 32.1], [0.45, 33.2]]],
  ['Blue Nile', [[15.65, 32.5], [14.4, 33.5], [13.55, 33.6], [11.85, 34.4], [11.2, 35.0], [10.35, 35.6], [9.95, 36.5], [9.9, 37.6], [10.07, 38.2], [10.5, 38.45], [11.0, 38.25], [11.4, 37.7], [11.6, 37.4]]],
  ['Senegal', [[14.45, -11.45], [14.9, -12.45], [15.65, -13.25], [16.15, -13.5], [16.4, -14.2], [16.65, -14.95], [16.5, -15.6], [16.03, -16.5]]],
  ['Niger', [[9.1, -10.7], [10.65, -9.9], [11.4, -9.2], [12.6, -7.95], [13.45, -6.25], [14.45, -4.1], [16.0, -3.5], [16.65, -3.0], [16.9, -0.6], [16.25, -0.15], [15.1, 0.8], [13.5, 2.1], [11.88, 3.45], [10.3, 4.7], [9.1, 4.8], [7.8, 6.75], [6.1, 6.75], [5.3, 6.4], [4.4, 6.1]]],
  ['Congo', [[-8.7, 26.0], [-5.4, 27.0], [-2.95, 25.9], [0.45, 25.2], [1.2, 23.6], [2.1, 21.5], [1.2, 19.0], [0.05, 18.25], [-0.5, 17.7], [-2.16, 16.24], [-3.3, 16.2], [-4.3, 15.3], [-4.9, 14.4], [-5.82, 13.45], [-6.05, 12.4]]],
  ['Ubangi', [[5.0, 27.4], [4.75, 24.6], [4.1, 22.4], [4.3, 21.2], [4.95, 19.9], [4.35, 18.55], [3.6, 18.6], [1.2, 18.0], [-0.5, 17.7]]],
  ['Zambezi', [[-11.4, 24.3], [-13.5, 23.1], [-15.25, 23.1], [-16.1, 23.3], [-17.5, 24.3], [-17.93, 25.85], [-17.0, 27.9], [-15.6, 30.4], [-15.7, 31.5], [-16.1, 33.6], [-17.4, 35.1], [-18.3, 36.0], [-18.8, 36.3]]],
  ['Limpopo', [[-24.2, 26.9], [-22.9, 28.0], [-22.2, 29.7], [-22.4, 31.3], [-23.9, 32.6], [-25.2, 33.5]]],
  ['Orange', [[-28.9, 28.9], [-30.3, 27.4], [-30.7, 26.7], [-30.6, 25.5], [-29.65, 24.2], [-29.2, 23.4], [-28.75, 22.3], [-28.4, 21.25], [-28.7, 20.0], [-28.9, 18.5], [-28.6, 16.45]]],
];
/** Crossings the geometry finds that are not there: [region, region]. */
export const FAR_RIVER_CUT = [
  // Both stand on the Soviet bank: the straight road between them only clips Manchukuo's northern corner.
  ['Skovorodino', 'Dauria'],
  // The globe slice. Aswan and Wadi Halfa both stand on the Nile's east bank; the Niger and the Zambezi are brooks
  // where the roads from Kankan to Freetown and from Luso to Katanga and the Copperbelt pass their springs.
  ['Aswan', 'Wadi Halfa'], ['Kankan', 'Freetown'], ['Elisabethville', 'Luso'], ['Luso', 'Ndola'],
  // The Sudan's frontier with the Belgian Congo is the watershed east of the Mbomou's springs.
  ['Wau', 'Buta'],
];
/** Crossings the geometry misses: [region, region, river]. */
export const FAR_RIVER_ADD = [
  // The whole frontier of Manchukuo and the Soviet Union but for the hills behind Vladivostok is a river, and so
  // is Korea's with Manchukuo.
  ['Heiho', 'Skovorodino', 'Amur'], ['Hailar', 'Skovorodino', 'Argun'], ['Kiamusze', 'Blagoveshchensk', 'Amur'], ['Kiamusze', 'Iman', 'Ussuri'], ['Mutankiang', 'Iman', 'Ussuri'],
  ['Chongjin', 'Mutankiang', 'Tumen'], ['Chongjin', 'Tunghwa', 'Tumen'],
  // The Yangtze's cities stand on its banks, so the roads between them cross it at one end: Pukow lies over the
  // river from Nanking, Anking and Hankow on the north bank, Chungking between its two rivers.
  ['Nanking', 'Pengpu', 'Yangtze'], ['Nanking', 'Anking', 'Yangtze'], ['Anking', 'Hangchow', 'Yangtze'], ['Anking', 'Chuchow', 'Yangtze'], ['Anking', 'Nanchang', 'Yangtze'],
  ['Hankow', 'Nanchang', 'Yangtze'], ['Hankow', 'Changsha', 'Yangtze'], ['Ichang', 'Changsha', 'Yangtze'], ['Chungking', 'Changsha', 'Yangtze'], ['Chungking', 'Kweiyang', 'Yangtze'],
  // The flood of 1938 lay between Kaifeng and the country to its south-west.
  ['Kaifeng', 'Nanyang', 'Yellow River'],
  // The Salween gorge was the front in Yunnan from 1942; the Chindwin and the Irrawaddy lie between Mandalay
  // and the hills to its west.
  ['Paoshan', 'Lashio', 'Salween'], ['Paoshan', 'Myitkyina', 'Salween'], ['Mandalay', 'Kalewa', 'Chindwin'], ['Yenangyaung', 'Akyab', 'Irrawaddy'],
  // The Mekong is the frontier of Laos and Thailand, and lies between Phnom Penh and Saigon.
  ['Vientiane', 'Udon', 'Mekong'], ['Pakse', 'Udon', 'Mekong'], ['Pakse', 'Korat', 'Mekong'], ['Phnom Penh', 'Saigon', 'Mekong'],
  // The globe slice: rivers that are a frontier, or the edge of a country, where a chief place stands on the bank.
  // The Shatt al-Arab between Iraq and Persia; the Panj below the Pamirs; the Padma between Calcutta and Dacca.
  ['Basra', 'Abadan', 'Shatt al-Arab'], ['Khorog', 'Faizabad', 'Amu Darya'], ['Calcutta', 'Dacca', 'Ganges'],
  // Khartoum stands in the fork of the two Niles; the Blue Nile's gorge rings Gojjam; the Victoria Nile parts
  // Buganda from the north.
  ['Khartoum', 'El Obeid', 'White Nile'], ['Khartoum', 'Kassala', 'Blue Nile'], ['Khartoum', 'Atbara', 'Blue Nile'],
  ['Wad Medani', 'El Obeid', 'White Nile'], ['Wad Medani', 'Kassala', 'Blue Nile'], ['Wad Medani', 'Gondar', 'Blue Nile'],
  ['Debra Markos', 'Addis Ababa', 'Blue Nile'], ['Debra Markos', 'Dessie', 'Blue Nile'], ['Debra Markos', 'Lekemti', 'Blue Nile'], ['Kampala', 'Gulu', 'White Nile'],
  // The Senegal is Mauritania's frontier, the Niger Dahomey's with Niger and the edge of Nigeria's east, the Ubangi
  // the Belgian Congo's with Ubangi-Shari, the Zambezi the two Rhodesias', the Orange the Cape's northern edge.
  ['Boutilimit', 'Saint-Louis', 'Senegal'], ['Niamey', 'Cotonou', 'Niger'], ['Ibadan', 'Enugu', 'Niger'], ['Lagos', 'Enugu', 'Niger'], ['Lagos', 'Port Harcourt', 'Niger'],
  ['Bangui', 'Buta', 'Ubangi'], ['Lusaka', 'Bulawayo', 'Zambezi'], ['Kimberley', 'Beaufort West', 'Orange'],
];

/**
 * The railways of the far side, laid by tools/build-rails.mjs (run with 'far') as the new world's are: a line to
 * every province but these, main lines between the cities. No line ran into these in 1939: Alaska off its one
 * railway; China's west and south-west, where the war was fed by road and river; Mongolia but for the army's line
 * to Choibalsan; Siberia off the Trans-Siberian; Laos, the Arakan and Tenasserim; Borneo, Celebes and the islands
 * east of Java; Mindanao; New Guinea; Australia's north-west, its centre north of Alice Springs, and Cape York.
 */
export const FAR_NO_RAIL = [
  'Copper River', 'Nome', 'Kuskokwim', 'Yukon Flats', 'Arctic Slope', 'Bristol Bay',
  'Silingol', 'Siangyang', 'Ichang', 'Anking', 'Kanchow', 'Foochow', 'Amoy', 'Swatow', 'Pakhoi', 'Hainan', 'Nanning',
  'Chungking', 'Chengtu', 'Kweiyang', 'Paoshan', 'Sikang', 'Yenan', 'Hanchung', 'Lanchow', 'Ningsia', 'Etsin Gol', 'Sining', 'Yumen', 'Tsaidam',
  'Tihwa', 'Hami', 'Kuldja', 'Kashgar', 'Khotan', 'Charkhlik', 'Lhasa', 'Chamdo', 'Gartok', 'Changtang',
  'Ulan Bator', 'Sain Shand', 'Gobi', 'Uliassutai', 'Kobdo',
  'Nikolayevsk', 'North Sakhalin', 'Bodaibo', 'Aldan', 'Yakutsk', 'Vilyuisk', 'Tiksi', 'Verkhoyansk', 'Kolyma', 'Okhotsk', 'Magadan', 'Koryak', 'Anadyr', 'Providenya', 'Ust-Kamchatsk', 'Petropavlovsk',
  'Pakse', 'Vientiane', 'Luang Prabang', 'Mergui', 'Akyab', 'Kalewa',
  'Kuching', 'Brunei', 'Jesselton', 'Sandakan', 'Djambi', 'Pakanbaru', 'Bali', 'Flores', 'Koepang', 'Dili',
  'Pontianak', 'Bandjermasin', 'Balikpapan', 'Tarakan', 'Makassar', 'Kendari', 'Palu', 'Menado', 'Ambon',
  'Vogelkop', 'Sarmi', 'Hollandia', 'Mimika', 'Merauke',
  'Aparri', 'Bataan', 'Del Monte', 'Davao', 'Zamboanga',
  'Port Moresby', 'Milne Bay', 'Buna', 'Lae', 'Madang', 'Wewak', 'Kerema', 'Rabaul', 'Cape Gloucester',
  'Wyndham', 'Cape York', 'Broome', 'Port Hedland', 'Carnarvon', 'Tennant Creek',
];
/** Lines the tool leaves out that history built: [region, region]. */
export const FAR_RAIL_ADD = [
  // Korea's main line from Pusan to Seoul and on to Wonsan, and the line over the Tumen to Mutankiang.
  ['Seoul', 'Pusan'], ['Seoul', 'Hamhung'], ['Chongjin', 'Mutankiang'],
  // Peiping to Mukden by Shanhaikwan; Peiping to Hankow by Shihkiachwang, Chengchow and Sinyang; the line down
  // Shansi; Hengyang to Kweilin; Hanoi to Kunming; the Manchurian line from Ssupingkai to Tsitsihar.
  ['Tientsin', 'Jehol'], ['Peiping', 'Shihkiachwang'], ['Shihkiachwang', 'Kaifeng'], ['Kaifeng', 'Nanyang'], ['Nanyang', 'Hankow'],
  ['Tatung', 'Taiyuan'], ['Taiyuan', 'Linfen'], ['Hengyang', 'Kweilin'], ['Hanoi', 'Kunming'], ['Tungliao', 'Tsitsihar'],
  // The Trans-Siberian between Chita and Skovorodino.
  ['Skovorodino', 'Chita'],
  // The coast line of Annam; Siam's northern line; the line to Kota Bharu up the middle of Malaya; Burma's main
  // line to Mandalay.
  ['Hue', 'Qui Nhon'], ['Qui Nhon', 'Cam Ranh'], ['Bangkok', 'Phitsanulok'], ['Phitsanulok', 'Chiang Mai'], ['Singora', 'Kota Bharu'], ['Kota Bharu', 'Kuantan'], ['Kuantan', 'Johore'], ['Ipoh', 'Kuala Lumpur'],
  ['Toungoo', 'Mandalay'],
  // Australia's lines across the desert: Perth to Port Augusta, the Ghan to Alice Springs, Broken Hill's two
  // lines; and Brisbane's western line.
  ['Perth', 'Kalgoorlie'], ['Kalgoorlie', 'Nullarbor'], ['Nullarbor', 'Adelaide'], ['Adelaide', 'Oodnadatta'], ['Oodnadatta', 'Alice Springs'], ['Adelaide', 'Broken Hill'], ['Broken Hill', 'Sydney'],
  ['Brisbane', 'Charleville'],
];
/** Lines the tool lays that were never there: [region, region]. */
export const FAR_RAIL_CUT = [
  // No bridge crossed the Amur, the Ussuri or the Tumen into the Soviet Union; the only lines over that frontier
  // ran by Manchouli and by Suifenho.
  ['Chongjin', 'Vladivostok'], ['Mutankiang', 'Iman'], ['Kiamusze', 'Khabarovsk'], ['Heiho', 'Blagoveshchensk'], ['Skovorodino', 'Dauria'],
  // China's lines ran north and south: none joined Tientsin to Shihkiachwang, Hsuchow to Nanking but by Pengpu,
  // Pengpu or Nanchang to Hankow, or Kukong to Kweilin; the Shansi line stopped at the Yellow River; the line
  // from Burma to Yunnan was never finished.
  ['Tientsin', 'Shihkiachwang'], ['Hsuchow', 'Nanking'], ['Pengpu', 'Hankow'], ['Hankow', 'Nanchang'], ['Kukong', 'Kweilin'], ['Linfen', 'Loyang'], ['Kunming', 'Lashio'],
  // No line ran from Annam or Cambodia into Siam's north-east, from Saigon to Phnom Penh, or from Siam into Burma
  // before 1943.
  ['Vinh', 'Udon'], ['Saigon', 'Phnom Penh'], ['Battambang', 'Korat'], ['Bangkok', 'Moulmein'], ['Chiang Mai', 'Toungoo'], ['Phitsanulok', 'Moulmein'], ['Phitsanulok', 'Udon'], ['Phitsanulok', 'Korat'],
  ['Prome', 'Toungoo'], ['Prome', 'Yenangyaung'], ['Kota Bharu', 'Penang'], ['Ipoh', 'Kuantan'], ['Kuantan', 'Kuala Lumpur'], ['Loyang', 'Nanyang'], ['Nanyang', 'Sian'], ['Rockhampton', 'Charleville'],
  // The Ghan and the Trans-Australian met only at Port Augusta; no line ran from the Ghan to Broken Hill or on
  // from Alice Springs to Queensland.
  ['Nullarbor', 'Oodnadatta'], ['Oodnadatta', 'Broken Hill'], ['Alice Springs', 'Cloncurry'],
];

/**
 * The railways of the globe slice (Africa, India, Persia, Central Asia, Siberia west of Lake Baikal) are laid by
 * hand, line by line as they stood in 1939: a net grown as the Pacific's is would join a dozen colonial lines that
 * never met. Each list is one line through neighbouring provinces. A province of today's map at the head of a list
 * (Gorky, Baghdad) carries the line across the old sheet's edge; the pair must border ('joins').
 */
export const FAR_RAIL_LINES = [
  // The Trans-Siberian: the northern line from Moscow by Kirov and Molotov, over the Urals to Sverdlovsk, by Tyumen
  // and Ishim (Tobolsk's country) to Omsk, and on by Novosibirsk, Taiga (Tomsk's) and Krasnoyarsk to Irkutsk and
  // round Lake Baikal. The lines from Yaroslavl and from Kotlas join it at Kirov.
  ['Gorky', 'Kirov', 'Molotov', 'Sverdlovsk', 'Tyumen', 'Tobolsk', 'Omsk', 'Novosibirsk', 'Tomsk', 'Krasnoyarsk', 'Nizhneudinsk', 'Irkutsk', 'Ulan-Ude'],
  ['Kostroma', 'Kirov'], ['Kotlas', 'Kirov'],
  // The Kazan line to the Urals; the southern line by Syzran, Kuibyshev, Ufa and Chelyabinsk to Omsk.
  ['Gorky', 'Kazan', 'Izhevsk', 'Molotov'],
  ['Penza', 'Ulyanovsk', 'Kuibyshev', 'Ufa', 'Chelyabinsk', 'Kurgan', 'Omsk'],
  // The lines of the Urals: the mining line from Molotov by Nizhny Tagil, Sverdlovsk to Serov and to Chelyabinsk,
  // Magnitogorsk's two lines, Kustanay's.
  ['Molotov', 'Nizhny Tagil', 'Sverdlovsk', 'Chelyabinsk', 'Magnitogorsk', 'Chkalov'], ['Nizhny Tagil', 'Serov'], ['Chelyabinsk', 'Kustanay'],
  // Over the Volga from Saratov: to Uralsk and Chkalov, and down the east bank to Astrakhan in the delta.
  ['Saratov', 'Pugachev', 'Uralsk', 'Chkalov'], ['Pugachev', 'Urda', 'Volga Delta'],
  // The Orenburg line to Tashkent; the Turk-Sib from Novosibirsk by Semipalatinsk and Alma-Ata, with the branch to
  // Frunze; the coal line to Karaganda and Lake Balkhash; the Kuzbass and Abakan branches.
  ['Kuibyshev', 'Chkalov', 'Aktyubinsk', 'Aralsk', 'Kzyl-Orda', 'Tashkent'],
  ['Novosibirsk', 'Barnaul', 'Semipalatinsk', 'Alma-Ata', 'Frunze', 'Dzhambul', 'Tashkent'],
  ['Kurgan', 'Akmolinsk', 'Karaganda', 'Balkhash'], ['Novosibirsk', 'Stalinsk'], ['Krasnoyarsk', 'Abakan'],
  // The Trans-Caspian from Krasnovodsk to Tashkent, the Fergana valley, the line to Stalinabad by Termez.
  ['Krasnovodsk', 'Ashkhabad', 'Merv', 'Bukhara', 'Samarkand', 'Tashkent', 'Fergana'], ['Samarkand', 'Stalinabad'],
  // The Caspian shore line from Grozny to Baku, and the line from Tbilisi.
  ['Grozny', 'Makhachkala', 'Baku'], ['Kirovabad', 'Baku'],
  // The Trans-Iranian, opened in 1938, from the Gulf to the Caspian; Iraq's line to Basra; the line from Quetta
  // over the desert to Zahedan.
  ['Abadan', 'Ahwaz', 'Hamadan', 'Tehran', 'Bandar Shah'], ['Baghdad', 'Basra'], ['Quetta', 'Kalat', 'Zahedan'],
  // India. The North Western: Karachi up the Indus to Lahore and Peshawar, the Bolan line to Quetta, the desert
  // line to Jodhpur.
  ['Karachi', 'Sukkur', 'Multan', 'Lahore', 'Rawalpindi', 'Peshawar'], ['Sukkur', 'Quetta'], ['Karachi', 'Jodhpur'],
  // Delhi's lines: to Lahore, to Simla, down the Ganges by Lucknow, Benares and Patna to Calcutta, and the
  // Rajputana lines.
  ['Lahore', 'Delhi', 'Lucknow', 'Benares', 'Patna', 'Calcutta'], ['Delhi', 'Simla'], ['Delhi', 'Agra', 'Lucknow', 'Patna'],
  ['Delhi', 'Bikaner', 'Jodhpur', 'Jaipur', 'Delhi'], ['Jaipur', 'Agra'], ['Jodhpur', 'Ahmedabad', 'Rajkot'],
  // The two main lines from Bombay to the north, by Baroda and Kotah and by Bhopal and Jhansi; the line to
  // Calcutta by Nagpur.
  ['Bombay', 'Ahmedabad', 'Bhopal', 'Jaipur'], ['Agra', 'Jhansi', 'Bhopal', 'Nagpur'], ['Jhansi', 'Lucknow'],
  ['Bombay', 'Nagpur', 'Raipur', 'Jamshedpur', 'Calcutta'], ['Benares', 'Jubbulpore', 'Bhopal'], ['Jubbulpore', 'Raipur'], ['Benares', 'Jamshedpur'],
  // The Deccan and the south: Bombay to Madras by Poona, the Nizam's line, the east coast line to Calcutta, the
  // Southern Mahratta lines to Goa and Bangalore, and the lines of the far south.
  ['Bombay', 'Poona', 'Bellary', 'Madras'], ['Poona', 'Hyderabad', 'Bezwada'], ['Nagpur', 'Hyderabad'], ['Bellary', 'Goa'], ['Bellary', 'Bangalore'], ['Bellary', 'Bezwada'],
  ['Madras', 'Bezwada', 'Vizagapatam', 'Cuttack', 'Jamshedpur'], ['Vizagapatam', 'Raipur'],
  ['Madras', 'Bangalore', 'Madura', 'Madras'], ['Madura', 'Cochin', 'Calicut', 'Mangalore'], ['Calicut', 'Madura'],
  // Bengal and Assam: the ferry line to Dacca and on to Chittagong, the line to the foot of the hills, and the
  // Assam line by Gauhati and Dimapur (Kohima's railhead) to Dibrugarh. No line ran to Imphal or into Burma.
  ['Calcutta', 'Dacca', 'Chittagong'], ['Patna', 'Darjeeling', 'Gauhati', 'Kohima', 'Dibrugarh'], ['Dacca', 'Darjeeling'], ['Dacca', 'Gauhati'],
  ['Colombo', 'Trincomalee'],
  // Egypt's line up the Nile to Aswan; the Sudan's from Wadi Halfa over the desert to Atbara and Khartoum, to
  // Port Sudan, to Karima, and up the Blue Nile to El Obeid and round by Kassala.
  ['Faiyum', 'Minya', 'Asyut', 'Qena', 'Aswan'],
  ['Wadi Halfa', 'Atbara', 'Khartoum', 'Wad Medani', 'El Obeid'], ['Dongola', 'Atbara', 'Port Sudan', 'Kassala', 'Wad Medani'],
  // Eritrea's mountain line; the French line from Djibouti to Addis Ababa by Dire Dawa.
  ['Massawa', 'Asmara', 'Keren'], ['Djibouti', 'Harar', 'Addis Ababa'],
  // The Uganda Railway from Mombasa, with its link to the Tanga line; Tanganyika's central line and the branch
  // to Mwanza.
  ['Mombasa', 'Nairobi', 'Kisumu', 'Kampala'], ['Mombasa', 'Arusha', 'Tanga'], ['Dar es Salaam', 'Dodoma', 'Tabora', 'Mwanza'],
  // West Africa's lines run inland from the ports and do not meet: Dakar to the Niger at Bamako and to Saint-Louis;
  // Conakry to Kankan; Abidjan to Bobo-Dioulasso; the Gold Coast's lines by Kumasi; Lagos to Kano and Port
  // Harcourt to Kaduna; Douala to Yaounde.
  ['Saint-Louis', 'Dakar', 'Tambacounda', 'Kayes', 'Bamako'], ['Conakry', 'Kankan'], ['Abidjan', 'Bouake', 'Bobo-Dioulasso'], ['Takoradi', 'Accra'],
  ['Lagos', 'Ibadan', 'Kaduna', 'Kano'], ['Port Harcourt', 'Enugu', 'Kaduna'], ['Douala', 'Yaounde'],
  // The Congo's portage lines round the rapids, on both banks; the Katanga line from the Kasai to the Copperbelt;
  // the Benguela Railway from Lobito to Katanga; Luanda's line.
  ['Matadi', 'Leopoldville'], ['Pointe-Noire', 'Brazzaville'], ['Luluabourg', 'Kamina', 'Elisabethville', 'Ndola'],
  ['Lobito', 'Nova Lisboa', 'Luso', 'Elisabethville'], ['Luanda', 'Malange'],
  // The line from the Cape to the Copperbelt by Kimberley, Mafeking, Bulawayo and the Victoria Falls; Rhodesia's
  // line to the sea at Beira, and Nyasaland's over the Zambezi bridge at Sena.
  ['Cape Town', 'Beaufort West', 'Kimberley', 'Klerksdorp', 'Gaberones', 'Francistown', 'Bulawayo', 'Lusaka', 'Ndola'],
  ['Bulawayo', 'Salisbury', 'Beira', 'Tete', 'Blantyre'],
  // South Africa's net: the Rand, Natal, the Free State, the ports, Lourenco Marques, the Namaqualand line; and
  // South West Africa's, joined to the Union's by Upington.
  ['Klerksdorp', 'Johannesburg', 'Pretoria', 'Pietersburg'], ['Pretoria', 'Lourenco Marques'], ['Johannesburg', 'Ladysmith', 'Durban'],
  ['Kimberley', 'Bloemfontein', 'Ladysmith'], ['Klerksdorp', 'Bloemfontein', 'Beaufort West', 'Port Elizabeth', 'East London', 'Bloemfontein', 'Maseru'],
  ['Cape Town', 'Springbok'], ['Beaufort West', 'Upington', 'Keetmanshoop', 'Windhoek', 'Walvis Bay'], ['Keetmanshoop', 'Luderitz'], ['Windhoek', 'Tsumeb'],
  // Madagascar's line from the capital to the coast.
  ['Tananarive', 'Tamatave'],
];

/** How large an islet is drawn: half its width and half its height in map units (a unit is 2.8 km north to south). */
export const ISLET_SIZE = {
  _default: [5, 5],
  // The eastern Marshalls are four atolls 300 km from end to end; Truk's lagoon is 60 km across; Saipan stands with
  // Tinian south of it; Guam and Babeldaob are long islands.
  'Eastern Marshalls': [6, 9], Truk: [6, 7], Saipan: [4, 7], Guam: [5, 6], Palau: [4, 7],
};
