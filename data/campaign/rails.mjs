// The railway net of the campaign map. tools/build-rails.mjs lays it: a line to every province outside the
// desert, by the easiest country, main lines between the cities that lie near each other, and each country's
// own net joined up where its provinces touch. The lists
// below correct what the tool cannot know. Run "npm run geo" after changing them.

/** What a kilometre of track costs to lay through each kind of country, against open plains. */
export const GROUND = { plains: 1, urban: 0.8, hills: 1.35, forest: 1.3, marsh: 1.9, mountain: 2.4, desert: 9 };
/** A city is joined by a main line to this many of the cities nearest it. */
export const NEAREST = 3;
/** Lines the tool leaves out that history built: [region, region]. */
export const ADD = [
  // Berlin to Warsaw, and on through Brest and Minsk towards Moscow.
  ['Neumark', 'Poznan'], ['Poznan', 'Lodz'], ['Warsaw', 'Brest'], ['Brest', 'Polesie'], ['Polesie', 'Minsk'],
  // Moscow to Leningrad, and the northern line that fed Leningrad through Tikhvin.
  ['Kalinin', 'Valdai'], ['Leningrad', 'Tikhvin'], ['Tikhvin', 'Vologda'],
  // Moscow to Bryansk and Gomel, to Kharkov by Kursk, and to Ryazan.
  ['Moscow', 'Kaluga'], ['Kaluga', 'Bryansk'], ['Bryansk', 'Gomel'], ['Kursk', 'Kharkov'], ['Moscow', 'Ryazan'],
  // Rostov to the Kuban, and the line into the Crimea.
  ['Rostov', 'Kuban'], ['Zaporozhye', 'Crimea'],
  // Paris to Lyon by Dijon, Dresden to Breslau, Hamburg to Hanover, Bucharest to the sea.
  ['Paris', 'Aube'], ['Aube', 'Burgundy'], ['Burgundy', 'Lyon'], ['Dresden', 'Lower Silesia'], ['Hamburg', 'Hanover'],
  ['Bucharest', 'Dobruja'],
];
/** Lines the tool lays that were never there: [region, region]. */
export const CUT = [
  // In 1939 no railway ran north of Trondheim's country. Narvik's only line was the ore railway to Sweden.
  ['Nordland', 'Narvik'], ['Narvik', 'Finnmark'],
];
