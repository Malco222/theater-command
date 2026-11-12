// Where a campaign can begin, and as whom.
//
// The grand campaign comes first: the player picks any country and the year to take it over. History is
// played up to that day, and from it no nation follows history's plan. GRAND_YEARS lists the days on offer
// and GRAND_BLURBS says a word about each great power; the countries themselves are read from the war as
// it stands on the day, so a new nation in the data is on the list without a change here.
//
// The scenarios after it are the browser prototype's opening screen (src/campaign/ui/main.ts), set down
// here as data: single campaigns in which the computer's nations keep to history's plan. `from` is the
// month the player takes over (1 is September 1939): for a later start the computer plays history up to
// it. `view` is 'open' for free play, and 'italy', 'west' or 'east' to open on that front.

export interface GrandYear { year: string; title: string; from: number; note: string }

export const GRAND_YEARS: GrandYear[] = [
  { year: '1939', title: 'September 1939', from: 1, note: 'Germany has attacked Poland, and Britain and France have declared war. Everything after that is still to be decided.' },
  { year: '1940', title: 'May 1940', from: 9, note: 'Poland has fallen and Norway is being fought over. The armies in the West have watched each other all winter.' },
  { year: '1941', title: 'June 1941', from: 22, note: 'France has fallen and Britain fights on. Three German army groups stand on the Soviet frontier.' },
  { year: '1942', title: 'June 1942', from: 34, note: 'The United States is in the war. Germany gathers its tanks in the south of Russia, and Rommel is at the gates of Tobruk.' },
  { year: '1943', title: 'July 1943', from: 47, note: 'Stalingrad and Africa are lost to the Axis. The tanks face each other round Kursk, and an invasion fleet waits at Malta.' },
  { year: '1944', title: 'June 1944', from: 58, note: 'The Allies are about to land in France, and four Soviet fronts are ready in Byelorussia.' },
];

/** The great powers, in the order they are offered, with a word about each that holds in any year. */
export const GRAND_BLURBS: Record<string, string> = {
  GER: 'The best army in Europe, and enemies gathering on every side. Where it strikes next, and when, is up to you.',
  UK: 'A small army, the largest navy in the world, and an empire to hold together. Outlast them, then come back.',
  FRA: 'A large army behind strong works, and a slow command. Hold the line, or do what history did not.',
  SOV: 'Endless country, endless men, and an army still learning its trade. Bide your time, or march first.',
  USA: 'The greatest industry on earth, an ocean away from the war. Its armies come by convoy.',
  ITA: 'An army spread from the Alps to Africa, and a sea to make your own. Pick your moment and your side.',
  JAP: 'An empire of islands with a great fleet and a long war in China. Strike before the oil runs out.',
  CHN: 'A vast country, half of it lost, with an army that will not give in. Hold on, and wear the invader down.',
  POL: 'Outnumbered and nearly surrounded. Hold on long enough and the rest of the war is yours to write.',
};

export interface Side { nat: string | null; name: string; blurb: string }
export interface StartGroup { title: string; from: number; view: string; sides: Side[] }

export const STARTS: StartGroup[] = [
  {
    title: 'September 1939 · The war begins, as history planned it', from: 1, view: '', sides: [
      { nat: 'GER', name: 'Germany', blurb: 'On the attack from the first day. The hardest plan to pull off on schedule.' },
      { nat: 'FRA', name: 'France', blurb: 'The Maginot Line, a large army, and a slow command. Can you keep Paris?' },
      { nat: 'UK', name: 'United Kingdom', blurb: 'A small army, the largest navy in the world, and convoys to keep alive.' },
      { nat: 'POL', name: 'Poland', blurb: 'Outnumbered and nearly surrounded. You place your army on the eve of war, then hold as long as you can.' },
      { nat: null, name: 'Watch history unfold', blurb: 'Every nation follows its plan. End the month and watch Poland, then the West, then the East.' },
    ],
  },
  {
    title: 'July 1940 · The Battle of Britain', from: 11, view: '', sides: [
      { nat: 'GER', name: 'Germany', blurb: 'Five air fleets and a summer of flying weather. History split its effort and lost. Can you win the sky over Kent?' },
      { nat: 'UK', name: 'United Kingdom', blurb: 'Radar, four fighter groups and the Channel. Keep Fighter Command in being until the autumn.' },
      { nat: null, name: 'Watch the Battle of Britain', blurb: 'France has fallen. The computer flies history\'s plan: the Channel, the airfields, London, then the night Blitz.' },
    ],
  },
  {
    title: 'September 1940 · The desert war', from: 13, view: '', sides: [
      { nat: 'UK', name: 'United Kingdom', blurb: 'Two divisions against an army. Throw the Italians out of Egypt, then decide how far from Alexandria you dare to chase them.' },
      { nat: 'ITA', name: 'Italy', blurb: 'A large army on foot, a long way from Tripoli. Take Egypt before the British tanks find your open flank.' },
      { nat: null, name: 'Watch the desert war', blurb: 'One road, no water, and a front that swings a thousand kilometres each way: Egypt, Operation Compass, Rommel, Tobruk.' },
    ],
  },
  {
    title: 'October 1940 · Greece and the Balkans', from: 14, view: '', sides: [
      { nat: 'GRE', name: 'Greece', blurb: 'A small army that knows its mountains. Hold the Italians in Epirus all winter, then decide what can be saved when Germany comes.' },
      { nat: 'ITA', name: 'Italy', blurb: 'Mussolini expects Athens in two weeks. You have a mountain wall in front of you, and an army in Libya that needs the same replacements.' },
      { nat: null, name: 'Watch the Balkans', blurb: 'Italy marches into the Greek mountains in the autumn rain. In April the panzers come through Bulgaria, and in May the parachutes open over Crete.' },
    ],
  },
  {
    title: 'June 1941 · Barbarossa', from: 22, view: '', sides: [
      { nat: 'GER', name: 'Germany', blurb: 'Three army groups on the frontier. Reach Moscow before the snow, or outrun your supplies trying.' },
      { nat: 'SOV', name: 'Soviet Union', blurb: 'The frontier armies are about to be hit. Trade space for time until the reserves and the winter arrive.' },
      { nat: null, name: 'Watch Barbarossa unfold', blurb: 'The computer plays the first 21 months, then the invasion of the Soviet Union and the first winter.' },
    ],
  },
  {
    title: 'May 1942 · Gazala and El Alamein', from: 33, view: '', sides: [
      { nat: 'UK', name: 'United Kingdom', blurb: 'Rommel strikes at the Gazala line as you take over, and Tobruk is not the fortress it was. Save the army, hold El Alamein, and strike back when the armour arrives.' },
      { nat: 'ITA', name: 'Italy', blurb: 'Most of the Axis army in Africa is yours, and so is the convoy route that feeds it. Tobruk is in reach. Decide how far past it you dare to go.' },
      { nat: null, name: 'Watch the desert in 1942', blurb: 'The computer plays the first 32 months, then Gazala, the fall of Tobruk, the stand at El Alamein and the long pursuit back across Libya.' },
    ],
  },
  {
    title: 'June 1942 · Case Blue and Stalingrad', from: 34, view: '', sides: [
      { nat: 'GER', name: 'Germany', blurb: 'Your tanks are gathered in the south and your allies will hold the Don behind you. The oil and the Volga are both in reach. Decide how much of the army goes into the city.' },
      { nat: 'SOV', name: 'Soviet Union', blurb: 'Give ground all summer and save the army. Hold Stalingrad, watch who is guarding the Don, and strike when the snow comes.' },
      { nat: null, name: 'Watch the road to Stalingrad', blurb: 'The computer plays the first 33 months, then the drive for the Don, the Volga and the Caucasus oil, and the winter that turned the war.' },
    ],
  },
  {
    title: 'November 1942 · Torch and Tunisia', from: 39, view: '', sides: [
      { nat: 'UK', name: 'United Kingdom', blurb: 'Montgomery is chasing Rommel across Libya, and a second army waits at Gibraltar beside the Americans. Get it ashore in Algeria, try for Tunis before the Germans do, and close the trap from both ends.' },
      { nat: 'ITA', name: 'Italy', blurb: 'Your army is falling back from El Alamein and a new enemy is about to land behind it. Hold Tunisia, bring the army home to it, and keep the sea road to Sicily open for as long as you can.' },
      { nat: null, name: 'Watch Torch and Tunisia', blurb: 'The computer plays the first 38 months, then the American and British landings in Morocco and Algeria, the race for Tunis, Kasserine, the Mareth Line and the end in Africa.' },
    ],
  },
  {
    title: 'July 1943 · Kursk and the Dnieper', from: 47, view: '', sides: [
      { nat: 'GER', name: 'Germany', blurb: 'Your tanks are gathered on both shoulders of the Kursk salient, and the Red Army has had three months to dig in there. Strike or hold them back: either way the weight is about to fall on Orel and Kharkov, and the Dnieper is a long way behind you.' },
      { nat: 'SOV', name: 'Soviet Union', blurb: 'You know where the blow will fall and you have dug in to meet it. Break the panzers on the lines round Kursk, then send the reserve fronts at Orel and Kharkov and do not stop until you are over the Dnieper.' },
      { nat: null, name: 'Watch Kursk and the road to the Dnieper', blurb: 'The computer plays the first 46 months, then the last German summer offensive in the east, the Soviet strokes at Orel and Kharkov, the race for the Dnieper and the freeing of Kiev.' },
    ],
  },
  {
    title: 'July 1943 · Sicily and Italy', from: 47, view: 'italy', sides: [
      { nat: 'UK', name: 'United Kingdom', blurb: 'Two armies wait at Malta and your air force has spent a month over Sicily. Keep the sky, put the Eighth Army ashore, and follow it across the strait before the Germans have dug in across the mountains.' },
      { nat: 'ITA', name: 'Italy', blurb: 'Africa is gone, the army has lost heart and the invasion fleet is gathering at Malta. If Sicily falls the King will dismiss Mussolini, and an armistice follows. Hold the island and Italy stays in the war.' },
      { nat: null, name: 'Watch Sicily and Italy', blurb: 'The computer plays the first 46 months, then the landing in Sicily, the fall of Mussolini, the Italian armistice, Salerno and Naples, and the German winter line across the mountains south of Rome.' },
    ],
  },
  {
    title: 'January 1944 · The eastern front, from Leningrad to the Danube', from: 53, view: 'east', sides: [
      { nat: 'SOV', name: 'Soviet Union', blurb: 'You have the men, the guns and the initiative on every part of the front. History freed Leningrad in January, the Ukraine by April, Minsk in July and Bucharest in August. Choose where each blow falls, and do not let the armies you cut off get away.' },
      { nat: 'GER', name: 'Germany', blurb: 'A front of two thousand kilometres, too few divisions to hold it, and orders to give up nothing. Italy has to be held as well, and the landing in France is coming in the summer. Decide what to give up while it is still yours to give.' },
      { nat: null, name: 'Watch the Red Army\'s year', blurb: 'The computer plays the first 52 months, then the relief of Leningrad, the spring offensive through the mud of the Ukraine, the Crimea, the destruction of Army Group Centre in the summer, Romania and Finland leaving the war, and the road to Budapest.' },
    ],
  },
  {
    title: 'January 1944 · Anzio and the road to Rome', from: 53, view: 'italy', sides: [
      { nat: 'UK', name: 'United Kingdom', blurb: 'The Eighth Army holds the Adriatic end of a front that has not moved since October, and the Americans are about to land behind it at Anzio. Monte Cassino bars the one good road to Rome. Decide when to break yourself against it, and when to wait for the spring.' },
      { nat: null, name: 'Watch Anzio, Cassino and Rome', blurb: 'The computer plays the first 52 months, then the landing at Anzio, the four battles for Monte Cassino, the fall of Rome in June, the pursuit to Florence and the autumn battles on the Gothic Line.' },
    ],
  },
  {
    title: 'May 1944 · Overlord and the liberation of France', from: 57, view: 'west', sides: [
      { nat: 'UK', name: 'United Kingdom', blurb: 'Two corps wait on the quays in Hampshire and the Americans beside you have a beach of their own. Win the sky over Normandy in May, land in June, and hold the German armour on your front while the Americans break out. Then see how far a single road to Arnhem will take you.' },
      { nat: 'GER', name: 'Germany', blurb: 'Some fifty divisions stand behind the Atlantic Wall and nobody can tell you which beach. The panzers can wait close to the coast or well back from it, not both. And every division kept in France is one the eastern front will not have when the Soviet summer offensive opens.' },
      { nat: null, name: 'Watch D-Day and the drive to the Rhine', blurb: 'The computer plays the first 56 months, then the air battle over the beaches, D-Day in June, the breakout and the liberation of Paris in August, Brussels and Antwerp, Market Garden, the West Wall, and the Ardennes offensive in December.' },
    ],
  },
  {
    title: 'June 1944 · Operation Bagration', from: 58, view: 'east', sides: [
      { nat: 'SOV', name: 'Soviet Union', blurb: 'The landing in France is days away and the German tanks have gone south, where they expect you. Four fronts are ready in Byelorussia. Break the fortified places at Vitebsk and Mogilev, close the ring at Minsk, and see how far the summer will carry you.' },
      { nat: 'GER', name: 'Germany', blurb: 'Army Group Centre holds a great bulge round Minsk, its tanks have been sent south, and the fortified places are to be held to the last man. The Allies are about to land in France. You have a few weeks to decide what can be saved.' },
      { nat: null, name: 'Watch Bagration', blurb: 'The computer plays the first 57 months, then the blow that destroyed Army Group Centre: Vitebsk and Minsk in June, the Bug and the Vistula in July, the Warsaw rising, and Romania changing sides as the summer ends.' },
    ],
  },
];
