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
  /**
   * Where a close escort flies beside its bombers, how far ahead a loose escort sweeps, and how far behind its box top
   * cover keeps station, a band above it (1 to 2 km, a placeholder; the design's 1.8); and the first escort group, by
   * count from 0, that may fly top cover (ruling 237.2's fallback: the third).
   */
  escort: { close: 1_500, ahead: 7_000, behind: 1_500, reach: 12_000, near: 4_000, cover: [0.4, 2.5] as [number, number], topFrom: 2 },
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
  /**
   * Flak by height: the low band takes the most, the high the least. Fighters take this share of what bombers do.
   * kinds (shown only): the guns that reach each band, for the effects kit's AirFlak bursts round a box on its run in
   * (the art director's flak brief, sections 3 and 4): light guns reach the low band only, medium the low and middle,
   * heavy all three.
   * hold (Brian, 20 November 2026: "flak should stop in the air battles when their own fighters are in the air. for
   * example german flak 88s would stop when their bf 109s would arrive to attack the bombers to prevent taking down
   * their own planes"): while a defending fighter group is going in on a box inside the ring, the guns do not fire on
   * that box. They fire in the gaps between its passes, while the fighters stand off to form up again, and once the
   * fighters have gone (the coordinator's ruling, 20 November: the first build held the guns for as long as fighters
   * were anywhere on a box, and a defended raid had no flak left in it). Nothing is counted differently (ruling 237:
   * the battle's sums are the raid card's): what the guns would have brought down while they hold is the fighters'
   * doing instead, so the bombers lost, and when each falls, are the same with this on or off
   * (tests/battle-air-flak-hold.test.ts).
   * pass (shown only): the beat of the passes on a box, in air seconds. The fighters on it go in together for 'go',
   * by the pass the group pressing it hardest is flying, then stand off for 'off' to form up for the next. A head-on
   * pass is over in moments and the climb back ahead of the bombers is long; fighters sitting astern stay on a box
   * far longer. Each length varies by 'vary' either way. A box is in the ring some 115 air seconds on its way in, so
   * it sees two or three passes there. Every 'off' at 0 is the first build's rule: the guns silent all the while.
   */
  flak: {
    band: [1.5, 1, 0.55], fighters: 0, kinds: [['light', 'medium', 'heavy'], ['medium', 'heavy'], ['heavy']] as string[][], hold: true,
    pass: {
      go: { headon: 14, astern: 26, close: 26, quarter: 18, beam: 18, below: 18 } as Record<string, number>,
      off: { headon: 34, astern: 18, close: 18, quarter: 24, beam: 24, below: 24 } as Record<string, number>,
      vary: 0.25,
    },
  },
  /**
   * Bombers by name (AirPlace.bombers; Brian, 20 November 2026: "i would like the game to keep track of real bombers
   * names and how many missions they have completed and fighters shot down"). 'lines': when one of a group's
   * longest-serving bombers falls, a line names it, and only for this many of each group, its first seats; an air
   * fleet that loses forty bombers in a raid does not fill the strip with forty names. Shown, never counted.
   */
  named: { lines: 2 },
  /**
   * How good a fighter group's pilots are, 0 to 1, for the 3D game's pilots (shown, never counted: the campaign has
   * already weighed readiness, leaders and aircraft into each group's worth, and the battle's sums are the raid
   * card's): a base, its leader's experience (the ticket's xp: his level over 3, none with no named leader) and its
   * readiness. A group at full readiness under a level 3 leader comes to 0.85; a tired one with no leader, under a half.
   */
  pilots: { base: 0.3, leader: 0.25, ready: 0.3 },
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
   * aircraft, in a box 'gap' metres between models; a loss is one model falling out of it ('air:lost').
   * 'per' is one number for the whole battle (Brian, 21 November 2026: the numbers "proportionate and realistic";
   * "i dont want the defenders to go smaller to be proportionate"): all the aircraft sent up over 'all', rounded up,
   * so about 'all' models fly at the start whatever the battle's size and every model stands for the same number of
   * aircraft. No side's numbers are ever cut to make a battle look even: the count is the campaign's.
   * 'all' is the 3D game's budget (the production lead's scale plan of 22 November, item AR4: its first step, S1; the
   * next, 400, waits on the flight core's speed work, AR3, and a warm Mac reading). 'page' is the browser page's own,
   * which draws every model on a flat map and keeps the old 60.
   * A model falls only for its share of the losses (AR1, Brian's ask of 22 November 04:07Z: "the bombers get destroyed
   * a lot"): its group's losses stand for round(lost / per) models, so falls x per = losses, and every aircraft lost is
   * still told in the dispatches. At one model an aircraft, every loss is a fall, as before.
   * 'box' is the most models drawn in one box (AR2, the production lead's boxes of 18 to 24 models): a bomber group's
   * box of 24, the flight core's eight vics of three, and fighters in boxes of 20, five fours (a squadron of 16 and its
   * spares, or two squadrons' worth at a coarse scale). A group with more models flies more boxes, in trail
   * ('boxes' in the view's extra), and its box is never drawn wider than one: a big raid is a stream, not a wall.
   */
  look: { all: 150, page: 60, gap: { ftr: 140, bmr: 110 }, rows: { ftr: 3, bmr: 3 }, box: { ftr: 20, bmr: 24 } },
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
    /**
     * Engines, one per formation while it flies (the bridge's 'loop'): its kind's plain loop, mix.json "air"."loops"
     * twinEngined for bombers (every bomber drawn before the B-17 and the Lancaster has two engines) and fighter for
     * fighters. The 3D game plays its model's own instead, by 'engines'.
     */
    loops: { bmr: 'bomber_medium_loop', ftr: 'fighter_engine_loop' },
    /**
     * Which mix.json "air"."loops" entry a formation's engines take, by the model the 3D game draws for it
     * (FMiniatureMesh::MainBomberOf, MainFighterOf; '*' is any other, such as the plain twin-engined 'Bomber:<tag>').
     */
    engines: { bmr: { B17: 'B-17', Lancaster: 'fourEngined', He111: 'twinEngined', '*': 'twinEngined' }, ftr: { '*': 'fighter' } } as Record<'ftr' | 'bmr', Record<string, string>>,
    /**
     * What the mix plays with no event of its own, as paths into mix.json (the sound director's, never copied here):
     * the flak ring has no loop, its guns fire and their shells burst by "air"."flak"; the formations' engines are
     * "air"."loops", by 'engines'; each gun as it fires is "air"."gunsByCalibre", by 'calibre'; a lost engine and a
     * windmilling airscrew are "air"."engineLost" and "air"."windmilling".
     */
    mix: { flak: 'air.flak', engines: 'air.loops', guns: 'air.gunsByCalibre', engineLost: 'air.engineLost', windmilling: 'air.windmilling' },
    /**
     * A gun's kind (guns.ts, the effects kit's fx.json air_guns rows) to its entry in the mix's "air"."gunsByCalibre":
     * a heavy gun plays that entry's wing, turret or handHeld take by its mount; a single hand-held rifle-calibre gun
     * plays handHeldRifle. A bomber's lost engine and a windmilling airscrew play nothing of their own: the mix drops
     * the formation's drone by "air"."engineLost".
     */
    calibre: { rifle: 'rifle', hmg: 'heavy', cannon20: 'cannon' } as Record<'rifle' | 'hmg' | 'cannon20', string>,
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
    /**
     * The men whose dice are thrown for a bomber on the look streams whatever its type: five, as when every bomber
     * carried five. The crew aboard ('crews') says how many of them count, and a sixth man and after are thrown on a
     * stream of their own (fates.ts fateOf), so the crew aboard never moves a count or any other look.
     */
    crew: 5,
    /**
     * Crew aboard, by the model the 3D game draws (the historian, air-fates-check.md section 6; Brian's yes of 20
     * November 2026 to a dive bomber showing its real two parachutes, not up to five). 'Dive', 'Torpedo' and 'Bomber'
     * are a nation's plain carrier bombers and its plain twin; Japan's plain twin is read as the G4M, as its tanks are,
     * and the Soviet one as the SB, as its engines are. Any other model is the plain twin's.
     */
    crews: {
      D3A: 2, D4Y: 2, SBD: 2, SB2C: 2, Skua: 2, Stuka: 2, Dive: 2,
      B5N: 3, B6N: 3, TBD: 3, TBF: 3, Swordfish: 3, Barracuda: 3, Torpedo: 3,
      He111: 5, Bomber: 5, 'Bomber:SOV': 3, 'Bomber:JAP': 7, 'Bomber:JPN': 7, G4M: 7,
      Nell: 7, Betty: 7, SM79: 6, Wellington: 6, Pe2: 3, LeO451: 4, // the new models (23 Nov; crews as built, unchecked)
      Lancaster: 7, B17: 10,
    } as Record<string, number>,
    /**
     * Crews that changed, by the month ('from' and 'to' as year + month / 12, months from 0): the He 111 flew with four
     * until an extra gunner joined in 1941; the B-17 often with nine from mid-1944, one waist gunner dropped.
     */
    crewsDated: [
      { model: 'He111', to: 1941, crew: 4 },
      { model: 'B17', from: 1944 + 6 / 12, crew: 9 },
    ] as { model: string; from?: number; to?: number; crew: number }[],
  },

  /**
   * How a lost aircraft is seen to go, from its situation (fates.ts; Brian, 20 November: many ways down, a falling plane
   * hitting another, a wing blown off, never the same each time, "variables and different situations"). Shown only:
   * 'fall' above still draws the rules' pilots back and is kept as it was. Weights in the order explode, flamer (the
   * whole aircraft ablaze), burn (an engine on fire), wing (shot or burnt off), tail (shot away), pilot (hit, it noses
   * over and goes straight in), spin, glide (engines dead, it glides down to ditch or crash-land), breakup (it comes
   * apart). Each factor multiplies the row; a missing way is 1. Placeholders for the historian's review.
   */
  fate: {
    base: { ftr: [0.13, 0.1, 0.22, 0.1, 0.07, 0.12, 0.16, 0.05, 0.05], bmr: [0.11, 0.1, 0.26, 0.12, 0.07, 0.06, 0.12, 0.1, 0.06] },
    /** By the heaviest gun of the fighter that brought it down: a rifle-calibre gun kills men and starts fires; cannon shells take wings and tails off. */
    gun: {
      rifle: { pilot: 1.4, burn: 1.2, wing: 0.4, tail: 0.6, explode: 0.7, breakup: 0.4 },
      hmg: { burn: 1.1, wing: 1.1 },
      cannon20: { wing: 2, tail: 1.6, explode: 1.4, breakup: 1.6, pilot: 0.8 },
    },
    /** By the heaviest gun round the target firing at its height: a heavy shell close by blows it apart or takes a wing. */
    flak: {
      light: { burn: 1.3, pilot: 1.2, glide: 1.3, explode: 0.6 },
      medium: { burn: 1.3, wing: 1.2 },
      heavy: { explode: 2, wing: 1.5, breakup: 1.6, pilot: 0.3 },
    },
    /** By the way the fighter came at the box: head-on hits the cockpit and the engines; from astern the tail and the tanks; from below the tanks and the bomb bay. */
    pass: {
      headon: { pilot: 5, burn: 1.3, tail: 0.3 },
      astern: { tail: 2, flamer: 1.3, pilot: 0.7 },
      quarter: { wing: 1.3, burn: 1.2 },
      beam: { wing: 1.4, flamer: 1.2 },
      below: { flamer: 1.5, explode: 1.3, tail: 0.6 },
      close: {},
    },
    /** A bomber with its bombs aboard blows up far more often than one that has dropped. */
    loaded: { explode: 1.8 },
    dropped: { explode: 0.7 },
    /**
     * No self-sealing tanks, no armour (fates.ts tanksOf): Japan's aircraft until mid-1944, the G4M until November 1944;
     * and 'part', Japan's after that, with CO2 bottles and some armour but still no self-sealing tanks in most (the
     * historian, 20 November: the Zero's flamers cut to about two times, not to none). 'boom' is how much more often
     * a burning one blows apart on the way down.
     */
    fragile: { flamer: 3, explode: 1.9, burn: 1.3, glide: 0.6 },
    frail: { flamer: 2, explode: 1.4, burn: 1.15, glide: 0.8 },
    boomBare: { bare: 1.5, part: 1.25 },
    /**
     * The four-engined heavies by day took terrible punishment and often came down under control (the historian: the
     * Eighth Air Force had more men taken prisoner than killed, so its lost heavies glided or burned down slowly at
     * least two in five times, and blew up fewer than one in ten).
     */
    sturdy: { glide: 2.6, burn: 1.3, spin: 1.1, explode: 0.45, flamer: 0.7, breakup: 0.7 },
    /**
     * A bomber a night fighter brings down: from below and astern into the wing tanks, it goes up in flames (the
     * historian: about three in five flamers or blown up; a Lancaster's crew the least likely of the heavies to get out).
     */
    night: { flamer: 4.5, explode: 3, burn: 0.8, wing: 0.7, glide: 0.3, spin: 0.5, pilot: 0.4, tail: 0.6 },
    /** A straggler already on fewer engines: the dead engine's fire spreads, the wing burns through, the rest give out. */
    strag: { burn: 2.5, wing: 1.6, glide: 2.5, explode: 0.6, pilot: 0.5 },
    /** By height: low down it has no room to spin long, and glides in; high up it spins. */
    band: [{ glide: 1.8, spin: 0.5 }, {}, { spin: 1.3 }] as Partial<Record<string, number>>[],
    /** The last four ways shown, newest first, are down-weighted by these: never the same twice running (unless it is the only way left). */
    again: [0, 0.35, 0.6, 0.85],
    /** The ways a fighter pilot the rules save can get out of under a parachute. */
    saved: ['flamer', 'burn', 'wing', 'tail', 'spin'] as const,
    /** A lost fighter pilot over enemy ground seen to get out (taken prisoner), by the way down. */
    captured: { flamer: 0.4, burn: 0.5, wing: 0.45, tail: 0.5, spin: 0.6 },
    /** A burning one blows apart on the way down: the chance, times 'loaded' with bombs aboard, and when, as a share of its fall. */
    boom: { burn: 0.12, flamer: 0.35, loaded: 2, at: [0.2, 0.65] as [number, number] },
    /** Air seconds after the hit before the fire shows: an engine smokes first; a flamer goes up at once. */
    fire: { burn: [0.5, 3] as [number, number], flamer: [0, 0.5] as [number, number] },
    /** Metres a second it comes down, by the way. */
    fallV: {
      explode: [40, 70], flamer: [70, 110], burn: [60, 110], wing: [50, 80], tail: [60, 90], pilot: [120, 170], spin: [45, 70],
      glide: [12, 25], breakup: [40, 70], struck: [45, 80], collide: [45, 80],
    } as Record<string, [number, number]>,
    /** Each of a bomber's crew seen to get out, by the way down, times 'outBand' by height (low down there is no time). */
    out: { explode: 0, flamer: 0.2, burn: 0.55, wing: 0.2, tail: 0.3, pilot: 0.35, spin: 0.35, glide: 0, breakup: 0.1, struck: 0.25, collide: 0.1 } as Record<string, number>,
    outBand: [0.3, 1, 1.1],
    /** Air seconds after the hit the first canopy opens (later out of a spin, held there by the forces), and between canopies. */
    chute: { first: [2, 6] as [number, number], spin: [4, 10] as [number, number], gap: [1.5, 4] as [number, number] },
    /**
     * A loss of the same tight box within 't' battle seconds may strike this one: its wreckage after it blew up or
     * came apart ('debris'), the aircraft itself falling out of control ('falling'), or burning (or diving in, its pilot
     * hit); and a fighter shot down while it attacked the box, falling through it, at 'foe' times those (it is smaller).
     * Each loss strikes at most one other.
     */
    near: { t: 4, min: 1, debris: 0.3, falling: 0.2, burning: 0.08, foe: 0.5 },
    /**
     * A fighter and the bomber it was attacking lost in the same moment: the chance they collided by accident, head-on
     * and otherwise, and two fighters locked in a fight (the historian: about 2 to 3 in a hundred; closing at 900 km/h
     * the fighters broke away under or rolled off). 'meant': a ram on purpose, its own situation by nation and month
     * (the historian: Soviet pilots, most in 1941 to 1943; Japan's ramming units against the B-29s from late 1944;
     * Germany's armoured Sturm groups closing from astern in 1944 and to April 1945). The pilot who rams may get out
     * and fly again. The shares are the historian's judgement (historian/nose-art-rams-check.md, 20 November): Soviet
     * 20 in 100 from June 1941 to the end of 1942 and 10 in 1943; Japan's one-offs 5 from June 1944 and 12 across all
     * its fighters on the B-29s from November 1944; Germany's 8. 'same': two of one group lost in the same moment, the
     * chance they collided, bombers of a tight box by day (one swerving from its hit into the next; the UI director's
     * line of 15:38Z) and fighters pressing the same attack. The historian finds both shares fair (air-fates-check.md
     * section 8): it happened inside the tight boxes, though most of the Eighth's collisions came while forming up,
     * not under fire. By night the stream did collide, most over the target: if night raids are ever fought on the
     * field and can lose two in the same moment there, 10 in 100 would carry it; today it is never.
     */
    ram: {
      headon: 0.03, other: 0.01, tangle: 0.02,
      same: { bmr: 0.3, ftr: 0.1 },
      meant: [
        { nat: 'SOV', from: 1941 + 5 / 12, to: 1943, p: 0.2 },
        { nat: 'SOV', from: 1943, to: 1944, p: 0.1 },
        { nat: 'JAP', from: 1944 + 5 / 12, to: 1944 + 10 / 12, p: 0.05, four: true },
        { nat: 'JAP', from: 1944 + 10 / 12, to: 1946, p: 0.12, four: true },
        { nat: 'GER', from: 1944, to: 1945 + 4 / 12, p: 0.08, four: true, pass: 'close' },
      ] as { nat: string; from: number; to: number; p: number; four?: boolean; pass?: string }[],
    },
    /** How many looks each way down has on the 3D side (smoke, debris, the way it rolls); never the last one again. */
    looks: 4,
  },

  /**
   * A hit that does not bring it down (Brian: "not all planes that get hit go down"). Shown only: while a group is
   * being hit, a chance of k times what it was owed each tick of one of its aircraft taking a hit it survives, at most
   * one each 'gap' battle seconds a group: holes punched through it, a puff and trail of smoke, pieces torn off, or (a
   * bomber) an engine fire the crew puts out. 'for': air seconds it shows.
   */
  hit: {
    k: 3,
    gap: 6,
    marks: {
      ftr: [['holes', 0.45], ['smoke', 0.3], ['pieces', 0.25]],
      bmr: [['holes', 0.35], ['smoke', 0.25], ['pieces', 0.2], ['fireout', 0.2]],
    } as Record<'ftr' | 'bmr', ['holes' | 'smoke' | 'pieces' | 'fireout', number][]>,
    for: { holes: [0, 0], smoke: [3, 8], pieces: [1, 2], fireout: [4, 12] } as Record<string, [number, number]>,
  },

  /**
   * The straggler (the design's 1.6; the design director's ruling 237.1: shown, never counted twice). While a box is
   * hit, each tick one of its bombers may lose an engine and fall out, at k times what the box was owed that tick, at
   * most 'most' at a time and never its last aircraft in formation. The box's next loss is its oldest straggler that
   * has been out at least 'seen' air seconds (so it is seen to leave before it falls) and has no escort over it. A twin
   * on one engine makes three quarters of its speed or less and cannot hold height (B39): it drops back at 'twin.back'
   * of the box's speed and sinks. A four-engined straggler has one engine windmilling or two out (B38, B39), or, still
   * loaded, one cleanly feathered (the historian, question 6: loaded, it seldom held the box; after the drop most
   * could, FB12): it drops back 4.5 to 9 m/s. A single-engined carrier or dive bomber has its one engine hit, running
   * rough with its airscrew turning, and falls back and sinks as a twin does (a placeholder). The historian: about
   * half is a fair figure for 1943 (low), less in 1944, when escorts covered the stragglers (as 'cover' does here).
   * Before its box bombs it keeps within 'held' metres (its bombs are in the box's drop); after, it falls back freely
   * and is clear of the fight, and gets home, past 'clear' metres. k is tuned so that about half the bombers lost were stragglers first, until the historian
   * answers (ruling 237.1). Sink rates are placeholders.
   */
  strag: {
    k: 1, most: 2, seen: 10, twin: { back: 0.25, sink: 2 }, four: { back: [4.5, 9] as [number, number], sink: 1 }, held: 1_500, clear: 3_000,
    /**
     * Shown only (Brian, 20 November: "some bombers flew on 2 engines after they put out the fire"). The chance its dead
     * engine was burning as it fell out, and the air seconds before the crew put the fire out ('stays': the share where
     * it never goes out; the historian, 20 November: an engine fire was put out about 55 to 60 times in a hundred with
     * the propeller feathered and the CO2 bottle, a wing-tank fire almost never, so about half overall). A fire put out
     * leaves its propeller feathered. After its box has dropped, and its fire is out, a heavy with one engine out
     * regains the box: most with the dead engine's propeller feathered, few with it windmilling, for the drag held it
     * back (the historian: about 60 to 65 and about 10 in a hundred). It closes at 'close' metres an air second and
     * climbs back at 'climb'; at night it stays with the stream. A twin on one engine, or a carrier or dive bomber with
     * its one engine hit, never does.
     */
    fire: { k: 0.4, out: [4, 15] as [number, number], stays: 0.5 },
    join: { feather: 0.62, windmill: 0.1, close: 3, climb: 1.5 },
  },
};

export type Band = 0 | 1 | 2;
