// The air battle's numbers, in one place, so they can be tuned without touching the sim.
//
// The air battle is counted, not flown. An air group is a block on the board with a count of aircraft, a morale
// meter, minutes of fuel and one of three heights. While two hostile groups are in contact they wear each other
// down at a rate taken from the campaign's own numbers (assessAir and forecastAir in campaign/air.ts), so that a
// raid fought with the computer on both sides costs what the raid card forecast. There is no flight model, no
// aiming and no gunnery anywhere in it (project memory air-battle-paused; the design director's ruling 217).
//
// Units: metres on the board, "air seconds" for anything that happens in the sky. The sky runs faster than the
// battle clock (SKY.scale air seconds to one battle second). A fight between two groups lasts about two battle
// minutes, long enough for three or four choices (ruling 226, mark 2, in spirit); the page runs the clock fast
// while nobody is in a fight on the way in and on the way home (mark 1).
//
// TODO(polish): move these to data/battle/air.json once the calibration settles.

export const SKY = {
  /** Air seconds that pass in one battle second (6 until ruling 226: a fight then lasted some 45 battle seconds). */
  scale: 2,
  /** The board: 100 km square. The raid comes on from the south edge (y = 0), as the attacker does in every battle. */
  w: 100_000,
  h: 100_000,
  /** The target, ringed: bombers inside this circle on a steady course are on their run. */
  target: { x: 50_000, y: 70_000, r: 6_000 },
  /** Flak hangs in a ring round the target out to this radius. */
  flakR: 12_000,
  /** The three heights. alt: the middle of each band in metres (for the 3D side); climb and dive: air seconds per band. */
  bands: { names: ['LOW', 'MED', 'HIGH'] as const, alt: [1_800, 4_500, 7_200], climb: 120, dive: 25 },
  /** Cruising speed, metres per air second. A climbing group makes this share of it. */
  speed: { ftr: 120, bmr: 85, climbing: 0.6 },
  /** Air seconds a bomber group must hold its run inside the target ring before it drops. */
  run: 45,
  /** Fighter fuel over the board, in air seconds, for a group well inside its range; burnt this much faster in a fight. */
  fuel: { full: 1_800, fight: 1.5, home: 99_999 },
  /** How close two groups must be to be in contact, by day and by night. */
  contact: { day: 3_000, night: 1_000 },
  /** How far a group can see another, by day and by night. */
  sight: { day: 30_000, night: 3_000 },
  /** Without radar, the defenders are scrambled when a raider comes this close to the target. */
  scramble: 50_000,
  /** Defending airfields stand this far from the target on the board: near + the real distance x squeeze, at most far. */
  fields: { near: 15_000, far: 40_000, squeeze: 0.2 },
  /** Where a close escort flies beside its bombers, and how far ahead a loose escort sweeps. */
  escort: { close: 1_500, ahead: 7_000, reach: 12_000, near: 4_000, cover: [0.4, 2.5] as [number, number] },
  /** Battle seconds before the battle is called (an hour in the air), and the battle seconds the computer waits between rethinks (12 air seconds). */
  limit: 1_800,
  think: 6,
  /** Once every bomber has dropped or turned back and nobody has been in contact for this many air seconds, it is over. */
  quiet: 120,

  /**
   * The wear. The campaign settles a raid in one throw (fly() in campaign/air.ts): the raid card's forecast says how
   * many defending fighters, escorts and bombers fall. The board hands each fighter group its share of what its side
   * should bring down (AirSim.arm) and lets it spend that while in contact, so the sums are the card's however the
   * fight goes; where and how it goes decides who pays. The guns' share is spread over the time a box spends in the
   * flak ring. Checked by tests/battle-air-parity.test.ts: the computer on both sides averages the forecast within a
   * tenth. luck: each group's luck in its fight, drawn as fly()'s own dice are.
   */
  wear: { luck: 0.15 },
  /** A group in contact spends its fight by e every this many air seconds; it breaks off with 'spent' of it left. */
  tire: 120,
  spent: 0.12,
  /** Bomber boxes fly as one stream, side by side this many metres apart. */
  stream: 1_500,
  /** Flak by height: the low band takes the most, the high the least. Fighters take this share of what bombers do. */
  flak: { band: [1.5, 1, 0.55], fighters: 0 },
  /** A defending fighter caught between escorts and bombers gives the escorts this share of its effort. */
  tied: 0.6,
  /** Fighting edges: diving from a higher band (for this many air seconds), caught climbing, wavering. A routing group does not fight back. */
  edge: { bounce: 1.5, bounceFor: 60, climbing: 0.7, wavering: 0.8 },
  /** How near a side is to breaking off (AirSim.nearBreak) when the player is warned: three quarters of the way (ruling 226, mark 3). */
  warn: 0.75,
  /** Morale: where it starts (from readiness), what knocks it and what restores it; the marks for wavering and routing. */
  morale: { base: 0.55, ready: 0.45, perLoss: 1.1, bounced: 0.08, alone: 0.0005, mend: 0.001, wavering: 0.35, routing: 0.17 },

  /**
   * How a group is drawn (Brian, 17:50Z: our own 3D models, animated). A group shows one model for every 'per'
   * aircraft, at most 'most', in a box 'gap' metres between models; a loss is one model falling out of it ('air:lost').
   */
  look: { most: { ftr: 12, bmr: 12 }, gap: { ftr: 140, bmr: 110 }, rows: { ftr: 3, bmr: 3 } },
  /**
   * The sounds that answer what happens: every name is a file in the approved pack (/mnt/project-files/sound/battle-pack/wav/),
   * as the sound director's mix renamed them (18 November, /mnt/project-files/sound/mix/mix.json "rename"). How loud, how
   * far and how dull each one plays is the mix's, read from mix.json, never copied here.
   */
  cues: {
    dogfight: 'dogfight', onBombers: 'fighter_pass', scramble: 'aircraft_takeoff', siren: 'air_raid_siren', bombs: 'bomb_stick_01',
    explode: 'plane_explode', down: 'plane_down', chutes: 'parachute_open', bingo: 'fighter_pass',
    /** Far takes: past the family's farTakesFrom (mix.json "distance", explosions) a blast plays one of these instead. */
    far: { plane_explode: ['explosion_far_01', 'explosion_far_02'] },
    /**
     * A fighter group's own pass, by the model the 3D game draws for it (FMiniatureMesh::MainFighterOf): a 'fighter_pass'
     * cue on a fighter group, or on a bomber group the fighters reached (the contact's extra.with), plays its model's
     * take; a model with no recording of its own keeps 'fighter_pass'. No Thunderbolt is drawn yet.
     */
    pass: { Spitfire: 'spitfire_pass', Fw190: 'fw190_pass', Mustang: 'p51_pass', P47: 'p47_pass' } as Record<string, string>,
    /** Engines, one per formation while it flies (the bridge's 'loop'). */
    loops: { bmr: 'bomber_heavy_loop', ftr: 'fighter_engine_loop' },
    /**
     * The flak ring has no loop: inside it the guns fire one of 'drops' every 'every' seconds (at random between the two),
     * and each shot bursts as one of 'then.takes' 'then.after' seconds later, among the bombers. The timings are the
     * sound director's (mix.json "rename", air) as of 18 November: if mix.json moves them, mix.json wins.
     */
    flak: { drops: ['flak_heavy_fire_01', 'flak_light_fire'], every: [0.8, 2.5], then: { after: [1.5, 4], takes: ['flak_burst_01'] } },
  },

  /**
   * A loss, as the 3D side shows it (Brian's ask of 2026-11-17: it explodes, or an engine burns and it falls, and
   * sometimes the crew bails out). Cosmetic only: the count has already dropped; this picks how it looks. Whether a
   * fighter pilot comes back is drawn at the rules' own rate (AIR.pilots.home / away), so every pilot the rules give
   * back was seen under a parachute.
   */
  fall: {
    /** A fighter whose pilot is lost: explode, burn, spin. */
    lost: [0.35, 0.45, 0.2],
    /** A fighter whose pilot is saved: burn, spin (never an explosion: nobody gets out of one). */
    saved: [0.55, 0.45],
    /** A lost pilot over enemy ground may still be seen to bail out, and is taken prisoner: chance by burn and spin. */
    captured: [0.5, 0.6],
    /** Bombers: explode, burn, spin; and the chance each of the crew gets out of a burning or spinning one. */
    bomber: [0.15, 0.6, 0.25],
    bomberOut: [0.55, 0.35],
    /** Crew of a bomber, for the parachutes. TODO(polish): by the type each country flies. */
    crew: 5,
  },
};

export type Band = 0 | 1 | 2;
