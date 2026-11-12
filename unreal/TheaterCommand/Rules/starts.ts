// Where a campaign can begin, and as whom.
//
// The list and its wording are the browser prototype's opening screen (src/campaign/ui/main.ts), set
// down here as data so the Unreal game can show the same choices. `from` is the month the player takes
// over (1 is September 1939): for a later start the computer plays history up to it. `view` is 'open'
// for free play, where no nation follows history's plan, and 'italy' to open on the Italian front.

export interface Side { nat: string | null; name: string; blurb: string }
export interface StartGroup { title: string; from: number; view: string; sides: Side[] }

export const STARTS: StartGroup[] = [
  {
    title: 'September 1939 · The war begins', from: 1, view: '', sides: [
      { nat: 'GER', name: 'Germany', blurb: 'On the attack from the first day. The hardest plan to pull off on schedule.' },
      { nat: 'FRA', name: 'France', blurb: 'The Maginot Line, a large army, and a slow command. Can you keep Paris?' },
      { nat: 'UK', name: 'United Kingdom', blurb: 'A small army, the largest navy in the world, and convoys to keep alive.' },
      { nat: 'POL', name: 'Poland', blurb: 'Outnumbered and nearly surrounded. You place your army on the eve of war, then hold as long as you can.' },
      { nat: null, name: 'Watch history unfold', blurb: 'Every nation follows its plan. End the month and watch Poland, then the West, then the East.' },
    ],
  },
  {
    title: 'September 1939 · Free play: make your own history', from: 1, view: 'open', sides: [
      { nat: 'GER', name: 'Germany', blurb: 'At war with Poland, Britain and France. Where you go next, and when, is up to you.' },
      { nat: 'UK', name: 'United Kingdom', blurb: 'Nobody knows what Germany will do this time. Neither do you.' },
      { nat: 'FRA', name: 'France', blurb: 'The blow may fall in the spring, in a year, or somewhere else first. Be ready, or strike first.' },
      { nat: 'POL', name: 'Poland', blurb: 'The first month is the one history wrote. Survive it and the rest is yours.' },
      { nat: 'ITA', name: 'Italy', blurb: 'Neutral, with an army in Libya and Albania. Pick your moment and your side of the Mediterranean.' },
      { nat: 'SOV', name: 'Soviet Union', blurb: 'At peace behind a pact. Build the Red Army and wait, or march before Germany does.' },
      { nat: null, name: 'Watch a war nobody has seen', blurb: 'The war begins as it did. After that no nation follows history: each one\'s staff decides for itself, and no two wars come out the same.' },
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
    title: 'January 1944 · Anzio and the road to Rome', from: 53, view: 'italy', sides: [
      { nat: 'UK', name: 'United Kingdom', blurb: 'The Eighth Army holds the Adriatic end of a front that has not moved since October, and the Americans are about to land behind it at Anzio. Monte Cassino bars the one good road to Rome. Decide when to break yourself against it, and when to wait for the spring.' },
      { nat: null, name: 'Watch Anzio, Cassino and Rome', blurb: 'The computer plays the first 52 months, then the landing at Anzio, the four battles for Monte Cassino, the fall of Rome in June, the pursuit to Florence and the autumn battles on the Gothic Line.' },
    ],
  },
];
