// The land battle rules, bundled from the browser prototype (src/battle, data/battle) as of 91a49214 2026-11-26. Do not edit: run Tools/build_land_rules.sh.
"use strict";
(() => {
  var __defProp = Object.defineProperty;
  var __defNormalProp = (obj, key, value) => key in obj ? __defProp(obj, key, { enumerable: true, configurable: true, writable: true, value }) : obj[key] = value;
  var __publicField = (obj, key, value) => __defNormalProp(obj, typeof key !== "symbol" ? key + "" : key, value);

  // ../../src/battle/core/types.ts
  var DT = 0.1;

  // ../../src/battle/core/cues.ts
  var LOOPS = {
    // Men in a fight carry no loop of their own: their rifles and machine guns, by nation, and the shared beds are the fight.
    foot: { march: "marching_loop", run: "marching_loop" },
    // A tank in a fight keeps its engine idling under its gun, as where it stands with nothing to do.
    tracks: { idle: "tank_idle_loop", march: "tank_loop", run: "tank_loop", turn: "tank_loop", fight: "tank_idle_loop" },
    // The pack's lorry: the engine holding its revs on the move, idling where it stands or fights.
    wheels: { idle: "lorry_idle_loop", march: "lorry_drive_loop", run: "lorry_drive_loop", fight: "lorry_idle_loop" },
    // Guns in action have no loop: they fire one far salvo at a time (DROPS), as the pack asks.
    guns: {}
  };
  var SALVOS = { takes: ["far_guns_01", "far_guns_02", "far_guns_03"], every: [6, 20] };
  var DROPS = {
    guns: { fight: SALVOS }
  };
  var TOWN = { loop: "street_battle", drops: { takes: ["mg_town_01", "mg_town_02", "mg_town_03"], every: [4, 12] } };
  var ONE_SHOTS = {
    contact: "whistle_long"
  };

  // ../../src/core/rng.ts
  var Rng = class _Rng {
    constructor(seed) {
      __publicField(this, "s");
      this.s = seed >>> 0 || 2654435769;
    }
    /** Uniform in [0, 1). mulberry32. */
    next() {
      let t = this.s = this.s + 1831565813 >>> 0;
      t = Math.imul(t ^ t >>> 15, t | 1);
      t ^= t + Math.imul(t ^ t >>> 7, t | 61);
      return ((t ^ t >>> 14) >>> 0) / 4294967296;
    }
    range(a, b) {
      return a + (b - a) * this.next();
    }
    int(n) {
      return Math.floor(this.next() * n);
    }
    chance(p) {
      return this.next() < p;
    }
    pick(arr) {
      return arr[this.int(arr.length)];
    }
    /** Roughly normal, mean 0, sd 1 (sum of uniforms; cheap and deterministic). */
    normal() {
      return (this.next() + this.next() + this.next() + this.next() - 2) * 1.7320508;
    }
    /** Turns an expected count into a whole number of events. */
    roll(expected) {
      const whole = Math.floor(expected);
      return whole + (this.next() < expected - whole ? 1 : 0);
    }
    /** An independent stream, so adding rolls in one system does not shift another. */
    fork(label) {
      return new _Rng(Math.imul(this.s ^ 2246822507, 3266489909) + label * 668265263 >>> 0);
    }
    get state() {
      return this.s;
    }
  };
  function hashString(str) {
    let h = 2166136261;
    for (let i = 0; i < str.length; i++) {
      h ^= str.charCodeAt(i);
      h = Math.imul(h, 16777619);
    }
    return h >>> 0;
  }

  // ../../data/campaign/ground.json
  var ground_default = {
    _note: "How the ground suits each arm, on top of the classic battle rules, and what tanks and infantry do for each other. The grand campaign only: the historical scenarios are settled by the classic rules alone. attack and defence are multipliers by unit kind and terrain; march is extra moves to enter that terrain on the advance (not along roads through ground held since before the month began).",
    attack: {
      arm: { forest: 0.8, hills: 0.9 },
      mot: { mountain: 0.75, marsh: 0.8, forest: 0.9 },
      art: { mountain: 0.8, marsh: 0.8 },
      cav: { urban: 0.7 },
      mtn: { mountain: 1.3, hills: 1.15 }
    },
    defence: {
      arm: { mountain: 0.75, marsh: 0.75, forest: 0.85 },
      mot: { mountain: 0.9, marsh: 0.9 },
      cav: { urban: 0.85 },
      mtn: { mountain: 1.25, hills: 1.1 }
    },
    march: {
      arm: { mountain: 1, forest: 1, marsh: 1 },
      mot: { mountain: 1, marsh: 1 },
      art: { mountain: 1, marsh: 1 }
    },
    steep: {
      regions: ["Graubunden", "Tyrol", "Alto Adige", "Savoy", "Nordland", "Narvik", "Sukhumi", "Montenegro", "Epirus", "Van"],
      world: ["Gilgit", "Leh", "Khorog", "Faizabad", "Gartok", "Lhasa", "Shigatse", "Chamdo", "Sikang", "Sungpan", "Likiang", "Fort Hertz", "Kathmandu", "Nagano", "Copper River", "Cordova", "Aysen", "Cuzco", "Ayacucho", "Cerro de Pasco", "Maseru", "Greenland", "Sanaa", "Simla"],
      _world: "The steep ground of the wider world, to Europe's bar (the historian's ruling of 17 November 2026, /mnt/project-files/world-map-builder/steep-candidates.md). A name not yet on the chart in play waits for it: Aysen, Cuzco, Ayacucho, Cerro de Pasco and Greenland are on the Americas' chart; the rest come with the far side.",
      barred: ["arm"],
      attack: { mot: 0.7, art: 0.7 },
      march: { mot: 1, art: 1 }
    },
    says: {
      plains: "Open country: the best ground there is for tanks",
      desert: "Open desert: tank country, with nothing to hide behind",
      forest: "Forest: tanks cannot manoeuvre among the trees",
      hills: "Hill country: slow going for armour, good ground for mountain troops",
      mountain: "Mountains: made for mountain troops, hard on anything with wheels or tracks",
      marsh: "Marsh: the ground will not bear tanks, lorries or guns",
      urban: "City streets: a fight for infantry, and no place for horses",
      steep: "High mountains: too steep for tanks, and little better for lorries and guns"
    },
    arms: {
      foot: ["inf", "mtn", "mot", "cav"],
      perTank: 3,
      infantry: 0.15,
      armour: 0.1,
      alone: { urban: 0.75, forest: 0.85 },
      defence: { infantry: 0.08, alone: { urban: 0.8, forest: 0.85 } }
    }
  };

  // ../../data/campaign/support.json
  var support_default = {
    _about: "What you bring and what you face (the design director's rulings 146, 150 and 152; the Historian's anti-tank-and-support-weapons.md). The grand campaign only: a historical scenario is worked out as it always was. Every defence has an anti-tank strength from 0 to 1, and the tanks' edge (the +25% against a defence with no armour, and the break-in into a dug-in line) shrinks by it. Machine guns, mortars, assault rifles and snipers each have a job in place of 6% on everything. And a battle's losses fall on what the defence can kill.",
    antiTank: {
      _byYear: "The infantry's own anti-tank guns of the day: [in the open, behind finished fieldworks]. The Historian's starting shares (1 less the share of the edge kept). Between the two by how far the works have got. After 1944 the last row stands.",
      byYear: { "1939": [0, 0.2], "1940": [0, 0.3], "1941": [0.2, 0.5], "1942": [0.4, 0.7], "1943": [0.5, 0.75], "1944": [0.55, 0.8] },
      _without: "A nation that has not researched Anti-tank guns has the day's guns at this share of their worth.",
      without: 0.5,
      tankDestroyers: 0.15,
      _handHeld: "Hand-held anti-tank weapons, with Assault rifles: worth more in close country.",
      handHeld: { open: 0.1, close: 0.2, ground: ["forest", "hills"] },
      _armourStep: "Better armour wins the edge back: each tank technology the attacker has beyond those of the day (Medium tanks 1941, Heavy tanks 1942, Late-war armour 1944) takes this much off, and each one it lacks adds it.",
      armourStep: 0.08,
      _guns: "Artillery in the line, firing over open sights: a division of guns.",
      guns: { perDiv: 0.04, most: 0.15 },
      _mines: "A level of minefields, less what the attacker's engineers lift.",
      mines: 0.08,
      _works: "Fortress works by level: the anti-tank ditch, then dragon's teeth. Concrete adds no more.",
      works: [0, 0.05, 0.1, 0.1],
      _tanks: "The defence's own tanks: their share of its strength, times this.",
      tanks: 2,
      _alone: "Tanks that go in without infantry beside them are picked off by the guns (ruling 152(3): two tank corps alone at a gunned line of 1943 have no better than an even chance). Against a defence whose guns (the infantry's own, tank destroyers, hand-held weapons and guns in the line, less the attacker's armour ahead of its day) add up to the whole, tanks with no infantry at all lose 'cut' of their attack, and less as the infantry covers them: one division of infantry covers 'cover' divisions of tanks. Not where the ground has already stopped the tanks (mountain, marsh, city streets).",
      alone: { cut: 0.7, cover: 2 }
    },
    _machineGuns: "Defence of troops with Submachine guns and mortars against men on foot: this much against an attack that is all on foot, in finished fieldworks; 'open' is the share of it they have with no works.",
    machineGuns: { defence: 0.12, open: 0.4 },
    _mortars: "Attackers on foot with Submachine guns and mortars take this much of the worth out of fieldworks (never out of fortress works), in proportion to their share of the attack.",
    mortars: { works: 0.15 },
    _assault: "Assault rifles: men on foot attack this much stronger.",
    assault: { attack: 0.08 },
    _snipers: "Snipers, from the first month and with no research: a defence in this ground, or in the streets of a town, costs the attacker's men on foot this much more of the usual share.",
    snipers: { toll: 0.1, ground: ["forest", "urban"] },
    _toll: "Who pays. An attacking tank corps loses the usual share times 'tanks' (from the first number at no anti-tank strength to the second at full). A corps on foot loses the usual share, more by 'machineGuns' against a defence all of machine-gunners in finished works, by 'guns' for each division of guns in the defence and by the snipers' toll, and less by 'shield' where tanks make up half the attack and the anti-tank guns let them work. The attackers' own guns stand behind and lose the 'behind' share.",
    toll: { tanks: [0.7, 1.4], machineGuns: 0.25, guns: 0.05, gunsMost: 0.15, shield: 0.15, behind: 0.5 },
    _staff: "The computer's staffs: a line whose anti-tank strength is 'gunned' or more is not attacked by tanks alone when infantry can go in with them, and at 'never' or more not at all without infantry.",
    staff: { gunned: 0.3, never: 0.5 },
    tech: { antiTank: "antitank", tankDestroyers: "tankdestroyers", handHeld: "assaultrifles", support: "smg", armour: ["mediums", "heavies", "latetanks"] }
  };

  // ../../src/battle/land/rules.ts
  var ARMS = ground_default.arms;
  var SUPPORT_COVER = support_default.antiTank.alone.cover;
  var TEAMS = ["mg", "mor"];
  var MOTION = { inf: "foot", mtn: "foot", cav: "foot", eng: "foot", mot: "wheels", arm: "tracks", art: "guns", fort: "fixed" };
  var HEDGE_NEAR = 12;
  var LOCATE = { secs: 25, fire: 0.5 };
  var LAND = {
    /** Metres a second over open ground, battle time (the field is 3 km by 2 km; contact comes in a few minutes at x1). */
    speed: { inf: 6, mtn: 5.5, cav: 9, eng: 5.5, mot: 11, arm: 10, art: 4, fort: 0 },
    /** Radians a second a block can turn. */
    turn: { inf: 1.1, mtn: 1.1, cav: 1.4, eng: 1.1, mot: 0.9, arm: 0.8, art: 0.5, fort: 0 },
    /**
     * How far a block's blows carry, metres between edges. Guns fire over the line from well back. Tanks reach no further
     * than lorried infantry: at 380 they stood off out of the infantry's reach and struck without reply, pricing armour
     * again on top of the card, which counts it already (b418, b420: the desert villages ran 38 points over their card).
     */
    reach: { inf: 200, mtn: 200, cav: 140, eng: 140, mot: 220, arm: 220, art: 1500, fort: 300 },
    /** Frontage and depth in metres. */
    size: { inf: [150, 40], mtn: [140, 40], cav: [150, 50], eng: [120, 40], mot: [140, 60], arm: [130, 60], art: [110, 50], fort: [140, 50] },
    /** Models on a full-strength tray (the art director, 17 November): one fades off for each share lost. */
    figures: { inf: 12, mtn: 12, cav: 12, eng: 12, fort: 12, mot: 6, arm: 4, art: 3 },
    /**
     * The share of a block's own power it takes off its target each second, at full strength and in the open. Slow
     * enough that a close fight runs two or three minutes from the first contact, time for three or four choices
     * (ruling 226(2); tests/battle-land.test.ts). A sixth slower since tanks close to their reach (b418), which had cut
     * the quickest fights under a minute and a half.
     */
    rate: 125e-5,
    /** Blows struck on the march, or on a block giving ground at the brink (b496(1)), and by a block that is wavering. */
    moving: 0.4,
    wavering: 0.7,
    /** Guns: their share against what they hit, the extra they take off nerve, and what they can do when the enemy is on top of them. */
    guns: { punch: 0.35, nerve: 1.5, close: 0.3, closeAt: 160 },
    /**
     * Support teams (release 2; Brian, 21 Nov 03:49Z: "units you can place at the begining of a battle on your side and
     * they can then attack from the back of the battle"; 03:57Z: built in the campaign apart from the infantry). A team
     * is a gun block ('art', so it stays back, fires on and is brought up into reach as guns are) with its own numbers:
     *   reach: metres between edges its fire carries. A machine-gun team's tripod guns reach over the heads of its own
     *     line; a mortar team's tubes lob further, short of the guns'.
     *   punch, nerve: as the guns' (above): its share against what it hits, and the extra it takes off nerve. Machine
     *     guns harm little and pin hard; mortars fall between.
     *   close, closeAt: its share when the enemy is within closeAt (a mortar cannot drop its bombs on a man beside it;
     *     a machine gun fights best close).
     *   armour: its share against tanks (bullets and light bombs do little to armour).
     *   speed (m/s, battle time, as LAND.speed), size (frontage and depth, m), figures (models on a full tray).
     * First guesses; the campaign's card prices the teams (stage 2), and the field only shares that out (b418).
     */
    /** The setup step (release 2): metres a placed gun or team keeps clear of every other block. */
    placeClear: 12,
    /**
     * Direct command (release 2; Brian, 22 November: "I want to be able to click a unit and point where I want them to
     * go"). At the double: a 'move' with run goes LAND.double.pace faster by its motion (foot run; lorries and tanks at
     * their best; guns are towed as fast as they go), is struck 'exposed' harder while on the run (upright men in the
     * open), and strikes 'blows' as hard for 'winded' seconds after (men getting their breath). Orders by radio: an order
     * of the player's reaches a block 'delay' seconds after he gives it, 'cut' times that when its headquarters is hit
     * (wavering, or under 'hqAt' of its strength: the runners go instead of the wireless). Neither is felt unless the
     * player orders a run, or the battle is opened with radio on. 'nations': each army's wait as a multiple of 'delay',
     * by the last year it held ([through year, times], the first that covers the battle's year; 1 where not listed):
     * the historian's (22 November 05:20Z, 'L as held'): Soviet 1941-42 twice (few sets below battalion: telephone and
     * runners), Japanese twice, Italian one and a half, German infantry a quarter more (telephone and runners more than
     * wireless); 1943 on, the Soviet a quarter more as the German (ours). A tank block waits as its model's share with a
     * set talks (data/battle/armour.json radio, the modeller's sheet: a radio in every German, American and British
     * tank; two T-34s in five to 1943) and the rest as its army's foot.
     */
    double: { pace: { foot: 1.5, wheels: 1.2, tracks: 1.15, guns: 1, fixed: 0 }, exposed: 1.25, blows: 0.75, winded: 20 },
    radio: {
      delay: 5,
      cut: 3.5,
      hqAt: 0.5,
      nations: { SOV: [[1942, 2], [9999, 1.25]], JAP: [[9999, 2]], ITA: [[9999, 1.5]], GER: [[9999, 1.25]] }
    },
    /**
     * Measured armour (release 2; Brian, 22 November 04:03Z to 04:16Z; armour.ts): off unless the battle is opened with it
     * (LandBattle.measured). A tank block's blow on a tank block, or a gun's over open sights, weighs the chance its
     * model's gun gets through the face it strikes at that range against the field's par for that face, held within
     * [lo, hi] (a Sherman's 75 on a Tiger's front all but nothing, a Tiger's 88 on a Sherman twice par and more). The
     * computer's tank blocks go round to the flank or rear of a tank block whose front they weigh under 'flankBelow' at
     * 'flankAt' metres, where her side or rear weighs 'flankBetter' times her front or more ('flankFor': the most battle
     * seconds they spend getting there before they go in anyway).
     */
    armour: { lo: 0.15, hi: 3, flankBelow: 0.35, flankBetter: 2, flankAt: 200, flankFor: 150 },
    /**
     * Hidden in the woods (release 2; Brian, 22 November 04:18Z: "if you put units in the forest then they aren't visible
     * to the enemy until you get close or they start shooting. this allows you to do ambushes"). Off unless the battle is
     * opened with it (LandBattle.hideOn). A block standing in woods when the clock starts is hidden from the other side:
     * its blocks never strike it nor are sent at it, and its staff never plans against it. It is found when an enemy
     * block's edge comes within 'sight' metres of its own (by its motion: men lying still under the trees are seen last,
     * engines and tracks are heard first), when it opens fire, when it is struck, or when it leaves the woods; moving
     * inside them keeps it hidden. Until then it holds its fire till an enemy is within 'ambush' metres (by its motion; a
     * machine-gun team's 'mg'; 0: it never holds, as tanks fire at their reach and guns as they always do, and so gives
     * itself away) unless it is ordered to attack or fire. Its first 'surprise.secs' seconds of fire strike 'punch' times as hard and shake 'nerve' times as much: the
     * first volley from close cover at men in the open, the shock of an ambush. Infantry ambushes in close country were
     * sprung at 50 to 150 metres (the US Army's FM 7-10, 1944; German forest-fighting notes, 1943), and troops still and
     * camouflaged under trees were seldom seen beyond 50 to 100. The computer's staff, defending, puts up to
     * 'staff.blocks' foot blocks of its line into woods on the enemy's way in before the clock starts (up to 'staff.ahead'
     * metres forward or 'staff.back' back, 'staff.across' to either side, inside its own ground as the player's are), and
     * keeps every hidden block of its line where it is, holding its fire, until it is found. An attack that sees none of
     * the enemy goes forward 'staff.ahead' metres at a time until it finds them.
     *
     * The historian's check (22 November 05:20Z; the Normandy hedgerows, 1944: "they could not see the Germans, but the
     * Germans could see them"):
     * - A block on a hedge line (within 'hedge.near' metres of one: hedgerow, lane bank, wood edge, garden hedge or wall,
     *   where the 3D game has handed the field's hedges over) hides as in woods. In woods themselves (true forest, sight
     *   25 to 75 m) hidden foot and machine guns hold their fire to 'forest' metres; on a hedge line, to 'ambush'. The men
     *   drawn on foot cross a hedge at its nearest gate (within 'hedge.gap' metres of the gap's middle; looks only).
     * - A hidden block that opens fire is found but not yet located: the fire back at it strikes 'locate.fire' as hard for
     *   'locate.secs' seconds, or until an enemy comes within its 'sight'.
     * - A tank or lorry moving inside cover is heard at 'heard' metres (by its motion; 0: as 'sight').
     * - Its ambush shakes a block caught moving in the open 'surprise.open' times (not 'nerve'), and its side's mortar
     *   teams strike what the ambush is firing on 'surprise.mortar' times as hard in those seconds (pre-sighted on the gaps).
     * - Reconnaissance by fire: a block told to fire on a stretch of cover (the player's land:fire at a point; the
     *   computer's attack once it has been ambushed, at the cover ahead in its lane) strikes a block hidden within
     *   'search.reach' metres of it at 'search.blow', and finds it about 'search.odds' times in 'search.every' seconds.
     */
    hide: {
      sight: { foot: 60, wheels: 150, tracks: 150, guns: 100, fixed: 0 },
      heard: { foot: 0, wheels: 0, tracks: 350, guns: 0, fixed: 0 },
      ambush: { foot: 150, wheels: 150, tracks: 0, guns: 0, fixed: 0, mg: 200 },
      forest: 100,
      surprise: { secs: 20, punch: 1.5, nerve: 2, open: 2.5, mortar: 1.25 },
      locate: LOCATE,
      search: { reach: 60, every: 10, odds: 1 / 3, blow: 0.5, ahead: 300, lane: 150 },
      hedge: { near: HEDGE_NEAR, gap: 3 },
      staff: { blocks: 2, ahead: 400, back: 100, across: 200 }
    },
    /**
     * Night patrol (release 2; Brian, 22 November 17:33Z: "lets do all three but make it where we can toggle it off if
     * someone doesnt want it"; the design director, 18:36Z, Q6 to Q8; LandBattle.patrol): off unless the battle is opened
     * with it, and it needs hiding on (LAND.hide): with hiding off nothing is hidden to find, so no patrol goes out and
     * nothing is drawn. The night before the battle the attack sends one reconnaissance patrol from a block of its line
     * on foot (the computer's always, from its most forward; the player's only by his order while he places his army):
     * settled once as the clock starts, on its own stream ('patrol:' and the seed), never the battle's dice. Two draws,
     * always both, found first: with chance 'found' the defence's most forward hidden post is found before a shot is
     * fired (it springs no ambush, and the attack's first plan counts it); with chance 'caught' the patrol is caught and
     * the defence is alerted, ready but not stronger (the card prices the digging already, b418): one more of its blocks
     * takes a hidden post by the computer's own ambush rule (LAND.hide.staff), a computer defence's reserve starts
     * toward the attack at once as a bold defence's does (styles defShift), and the patrol's block loses the patrol's men
     * from its strength (its nerve untouched). Both can happen: the rear man gets the report home (Infantry Training
     * 1937, sec. 35).
     *   found, caught: the design director's knobs (18:36Z, Q6). No army's rate is sourced; the only tally is approximate,
     *     the 38th (Irish) Brigade's war diary, Italy, December 1944, about 106 patrols: about 24% saw or found something
     *     and about 42% made contact (the tactics thread's readings, 22 November 18:40Z).
     *   size: men in a reconnaissance patrol, by army. USA 4 (FM 21-75, 1944, ¶86g: "three to six men"); the UK and her
     *     kin 3 ("an officer or sergeant and two men who go out at night": a British junior officer, Intelligence Bulletin,
     *     August 1943); SOV 4 ("three- to five-man teams": Sasso, Leavenworth Paper 6, p. 12); JAP 10 ("approximately one
     *     squad (10 men)": Intelligence Bulletin, July 1944); GER 10 (patrols "not less strong than a squad": German
     *     Motorized Infantry Regiment, MIS 1942, ¶40); FRA and VIC 2 (night patrols "by riflemen alone": Passmore 2025;
     *     the count a knob); ITA 4 (unsourced: knob); every other army '_' 4 (knob). Kin follow Britain, Vichy France.
     *     Its pictures, and the men its block loses when it is caught.
     *   night: battle seconds the patrol is out, before the clock starts (negative: the night before). Looks only, a knob.
     */
    patrol: {
      found: 0.4,
      caught: 0.3,
      size: { USA: 4, UK: 3, CAN: 3, AUS: 3, NZL: 3, SAF: 3, IND: 3, SOV: 4, JAP: 10, GER: 10, FRA: 2, VIC: 2, ITA: 4, _: 4 },
      night: { t0: -2400, t1: -600 }
    },
    teams: {
      mg: { reach: 800, punch: 0.35, nerve: 2.5, close: 1, closeAt: 0, armour: 0.15, speed: 5, size: [70, 30], figures: 3 },
      mor: { reach: 1200, punch: 0.4, nerve: 1.8, close: 0.3, closeAt: 100, armour: 0.5, speed: 4.5, size: [80, 40], figures: 3 }
    },
    /** Damage and nerve by where a blow lands: [damage, nerve]. */
    aspect: { front: [1, 1], flank: [1.5, 2], rear: [2, 3] },
    /**
     * The damage a block takes on this ground (cover), and the speed it keeps on it, by how it moves. Fieldworks
     * ('works', dug where a block stood dug in) give no cover of their own: a blow on them takes the cover of the ground
     * they were dug into (field.dugInto; 'works' here is only for works with no record). The card counts them in the
     * defence's strength already, and the field shares out the card's own strength (ruling 217; b418: what the card
     * prices, the field draws but never prices again). Counted twice, the plain attack carried 23 points less often than
     * the card said in the campaign's dug-in battles, four in five of all it fights; counted once it lands within 3 (b407).
     */
    cover: { open: 1, field: 0.95, woods: 0.7, rough: 0.85, marsh: 1, water: 1.25, road: 1.05, bridge: 1.2, buildings: 0.55, rock: 0.75, bunker: 0.4, works: 1, blown: 1.25, suburb: 0.75, industry: 0.6, park: 0.9 },
    pace: {
      foot: { open: 1, field: 0.9, woods: 0.6, rough: 0.7, marsh: 0.35, water: 0.35, road: 1.1, bridge: 1, buildings: 0.7, rock: 0.4, bunker: 0.5, works: 0.8, blown: 0.15, suburb: 0.85, industry: 0.75, park: 1 },
      wheels: { open: 0.9, field: 0.8, woods: 0.3, rough: 0.4, marsh: 0, water: 0, road: 1.5, bridge: 1.2, buildings: 0.5, rock: 0, bunker: 0, works: 0.5, blown: 0, suburb: 0.7, industry: 0.55, park: 0.85 },
      tracks: { open: 1, field: 0.9, woods: 0.4, rough: 0.6, marsh: 0.12, water: 0, road: 1.3, bridge: 1, buildings: 0.5, rock: 0, bunker: 0, works: 0.6, blown: 0, suburb: 0.75, industry: 0.55, park: 0.95 },
      guns: { open: 1, field: 0.9, woods: 0.4, rough: 0.5, marsh: 0.15, water: 0, road: 1.4, bridge: 1, buildings: 0.6, rock: 0, bunker: 0, works: 0.7, blown: 0, suburb: 0.75, industry: 0.6, park: 0.95 },
      fixed: { open: 0, field: 0, woods: 0, rough: 0, marsh: 0, water: 0, road: 0, bridge: 0, buildings: 0, rock: 0, bunker: 0, works: 0, blown: 0, suburb: 0, industry: 0, park: 0 }
    },
    /**
     * Tanks are blind in the streets: what they strike and what they take there. Not in woods: the card prices tanks in
     * forest and on hills already (the campaign's ground rules), so the field draws them slow there but never prices them
     * again (b418). Priced twice, the panzers at Sedan carried the Meuse 21 points less often than their card said.
     */
    tanksClose: { punch: 0.6, take: 1.3, ground: ["buildings", "industry"] },
    /**
     * Tanks that leave their infantry on close ground (the tactics panel, item 7; the design director, ruling 239(5)):
     * tanks see little in woods and streets, and hidden guns and close-assault teams kill those that come on alone. A
     * tank block in contact with enemy foot (the card's foot: 'foot'), with no friendly foot block within 'near' metres
     * (edge to edge), standing in woods or streets or fighting foot in fieldworks, takes that foot's blows times 'take',
     * the card's own figure for tanks alone there (1 over ground.json's arms.alone: forest for woods and fieldworks,
     * urban for streets; a defending tank the card's defence figures), on top of tanksClose. Not where that foot is
     * already in contact with friendly foot: the enemy is fixed, and a tank on its flank is the hook.
     * Never twice (b418): only the tank divisions the card counted as covered by their foot, and paid the attack for
     * fighting together, can be bitten: the card's foot divisions times 'cover' (1 on forest and urban cards, where
     * ground.ts covers a tank division with one of foot; support.json's 2 elsewhere; a defence 1), a regiment a third,
     * the first blocks first. A side with no foot has none covered: the card charged it already. On (b647): the plain
     * order lands within 0.1 point of 8df23b3 on every ground; the computer's tanks are bitten for 0.6% of their contact.
     */
    alone: {
      on: true,
      near: 200,
      foot: ARMS.foot,
      woods: ["woods"],
      streets: ["buildings", "industry", "suburb"],
      works: ["works", "bunker"],
      take: {
        att: { woods: 1 / ARMS.alone.forest, streets: 1 / ARMS.alone.urban, works: 1 / ARMS.alone.forest },
        def: { woods: 1 / ARMS.defence.alone.forest, streets: 1 / ARMS.defence.alone.urban, works: 1 / ARMS.defence.alone.forest }
      },
      cover: { close: 1, open: SUPPORT_COVER, def: 1, cards: ["forest", "urban"] }
    },
    /**
     * Going in together (item 1; 7b): whether the player's group attack carries 'together', holding its tanks and lorries
     * to its foot's pace. Off (the design director, ruling 239's rider, 23:13Z, amending 239(5)): held to the slowest foot
     * of the whole order, the plain order lost 6 points on its card (15 on mountains), as LAND.hold was fitted with its
     * tanks reaching the line first. The player keeps his tanks with his foot by his own orders; the computer's groups
     * keep together by its drill (ai.ts).
     */
    together: { player: false },
    /** Metres a second two blocks standing in each other's ground are eased apart, and the metres enemies keep between their fronts. */
    spread: 4,
    apart: 10,
    /**
     * Overrun (release 2; Brian, 23 November 05:05Z: "vechiles can run over enemy soldiers though which would kill them";
     * the design director, 05:48Z): with LandBattle.escort on, a tank block fighting a foot block in the open, with its own
     * foot within LAND.alone.near and no enemy weapon that can hurt a tank by direct fire in its own reach of the way,
     * drives through that block's ground to its far edge and backs out to its place: once per foot block per contact
     * (until the block breaks or no tank of the enemy is in reach of it), every side, staffs and player alike. Not where the
     * foot carries its own anti-tank weapon. Harm unchanged (b418): the overrun is a picture of losses the rules already
     * take (the men taken from that block while a tank is on its ground die under her tracks, Drill.thin); the blows
     * between the two are struck as between blocks standing face to face. 'reverse': its pace backing out, a share of its
     * pace forward. 'at': from when to when a nation's foot carries its own anti-tank weapon, [from, to) as the year and
     * twelfths (the design director: looks gating only until the historian checks the dates on resumption; the
     * Commonwealth's foot carries Britain's): the US bazooka from November 1942; Britain's Boys rifle 1939-42, then the
     * PIAT; Germany's anti-tank rifles 1939-42, the Panzerschreck and Panzerfaust from late 1943; the Soviet anti-tank
     * rifles from 1941. Japan, Italy and the rest none until the historian names one.
     */
    overrun: {
      reverse: 0.5,
      at: {
        USA: [[1942 + 10 / 12, 9999]],
        UK: [[1939, 9999]],
        GER: [[1939, 1943], [1943 + 9 / 12, 9999]],
        SOV: [[1941, 9999]]
      },
      commonwealth: ["CAN", "AUS", "NZL", "NZ", "SAF", "IND"]
    },
    /**
     * A queue, not a pile (the art director, b418): within 'near' metres of ground a block keeps 'hard' of its pace on or
     * less (at a bridge, a pass), a friend in the way of a marching block makes way for it rather than both being eased
     * apart, so a jam reads as one block behind another, each on its own ground. In the open, blocks are eased apart as before.
     */
    queue: { hard: 0.2, near: 75 },
    /** Metres between two friends' boxes where an attack's block takes its own stretch beside a friend standing there (b439). */
    room: 10,
    /** Metres off either end of a bridge kept clear: no attack has a block stand there, so those coming over can (b439). */
    mouth: 50,
    /** Mountain troops on rough ground and rock keep this much more of their pace. */
    mountainFoot: 1.6,
    /** Higher ground: blows from above strike harder, blows from below land softer, past this many metres of rise. */
    height: { over: 8, up: 1.15, down: 0.9 },
    /**
     * Holding its own ground: a defending block within 'within' metres of where it was drawn up takes this share of the
     * damage. Set by the fairness table, ground by ground, so that the plain attack (select all, right-click the nearest,
     * Space) carries the ground about as often as the card says at the telling odds, 1.1 to 1.4 (ruling 226, mark 4), and
     * two computer generals stay fair to it too (ruling 217(3)). Above 1 where the field's own cover (woods, marsh, rock)
     * already gives the defence more than the campaign's odds, which count the ground already, mean it to have. 'river'
     * multiplies it where the defence stands behind a river, 'works' by the works' level: the card counts both already.
     * Level 2 sits between the telling-odds works (an infantry line that keeps a reserve back out of its bunkers) and the
     * practice works (motorized, no reserve): each lands within a tenth of its card (b407). Re-set for b418 (tanks no
     * longer out-reach infantry nor pay twice in woods, works give their own ground's cover, regiments hold a division's
     * frontage, blows a sixth slower, and tanks and lorries find their way round rivers, rock and marsh: mountains held
     * at 1.43 only because the tanks there stood for good at the first outcrop): over five sets of 160 battles a ground,
     * every ground lands within 4 points of its card, about 2 in the attack's favour; the test's own seeds run about 5
     * under that. 'river' 1.51 to 1.5 for b439, once blocks at a bridge made way for each other rather than shove at its
     * foot to a standstill (Sedan's panzers ran out the clock there one fight in eleven). 1.4 to 1.3 for b464(5), once foot
     * waded out of the river at the wading pace instead of crawling up its bank under fire: the practice river and the
     * telling odds behind a river land where they did (+5 and 0 against +6 and -2, the tests' seeds). 1.3 to 1.2, and
     * counted only as far as the attack's tanks and lorries cross, for b494: a river is only ever a cost, so every river
     * attack lands at or below the same attack with no river. Men on foot cross the field's rivers at no cost the field
     * needs to make up (the campaign's river attacks on foot, with no hold, land level with their no-river twins), while
     * an attack whose armour must cross pays dearly at the bridges; 1.3 lifted the telling odds 7.5 points and the
     * practice river 5 over their no-river twins, 1.2 leaves them level (+0.6) and below (-5.8).
     */
    hold: {
      take: { plains: 0.97, forest: 1.035, hills: 0.99, mountain: 1.12, marsh: 1.32, urban: 1.025, desert: 1.045 },
      within: 300,
      river: 1.2,
      works: [1, 1.02, 1.06, 1.07],
      // A capital's and a city's streets (field.ts streetsOf): the card prices the place's ground, not the cover of its
      // houses, so the field takes that cover back off the held defence (b418; b511). With none, cities fell 4 points
      // under the card and capitals 17, most to the clock. A kind of ground may have its own, else the size's (b515(f)):
      // capital 1.1 is the plains capital's fit (48 attacks, captured and hand-made), and the hills' and forest's; an
      // urban capital's core is deepest; a mountain capital, a desert city and a marsh city hold on their own ground.
      // Fitted 18 November, 10 fights each, on 555 capital and city attacks captured from the campaign and hand-made on
      // every kind of ground: capitals -1.0 against the card, cities +0.2, every kind within a tenth (furthest: plains
      // cities captured -9.5, plains capitals captured +9.0 on 8); the practice city +2.0; every other place unchanged.
      // Next (b515(g)): a city's clock follows its built share, and these come back toward 1.
      streets: { city: 1.05, capital: 1.1, "urban capital": 1.5, "mountain capital": 1, "desert city": 1, "marsh city": 1 },
      // A city's own river up the field beside the heart (b526): where the whole defence stands on one bank its line ends
      // on the river, a flank the attack cannot turn, and the card never priced the river, so the field takes that back
      // off the held defence too. An army broad enough to stand on both banks has no such flank. Fitted 19 November, 10
      // fights each on the 555 capital and city attacks with a river forced up every field the card left dry: 1 cost the
      // attack 2.2 points (mountain cities at close odds 10.7); 1.06 leaves it -0.1 against the same attacks with no river,
      // every kind within a tenth both ways (furthest: plains cities at close odds -6.7), fewer fights to the clock.
      // Re-measured with bridges by the record (b596, three): +0.25, every kind within a tenth, 58 fights to the clock
      // against 75 (1.045 would leave -0.4 but more capitals to the clock, 0 to 5 of 2,420).
      stream: 1.06,
      // A second river along the defence's flank, meeting one across the field behind it (b596(2), the Sava at Belgrade):
      // the defence drawn up beside it has its line's end on the water as up the field. Fitted 19 November on the same 555
      // attacks with the river across and a second one from the west edge on every field the card left dry, bridges 3
      // and 2: 1 left the attack where it was (+0.05) but 18 more fights of 5,550 ran to the clock (75 to 93); 1.06 (as up
      // the field) took the clock back (76) but gave the attack +2.3, two kinds at close odds over a tenth (forest capitals
      // +11.4, hills cities +13.4); 1.045 leaves +1.7, every kind within a tenth (furthest: hills cities at close odds +10.0), 81 fights to the clock.
      join: 1.045
    },
    /** Dug in: a block is drawn in its works while it stays within this many metres of where it began. */
    dug: { keep: 30 },
    nerve: {
      /** Below 'waver' it fights worse; below 'rout' it runs; a runner rallies at 'rally' when clear of the enemy for 'quiet' seconds. */
      waver: 0.4,
      rout: 0.2,
      rally: 0.35,
      quiet: 15,
      clear: 450,
      /**
       * Nerve lost for each share of strength lost, before where the blow landed. Divisions break on their nerve long
       * before they are worn away, so a battle fought to its end costs about what the campaign bills for its result
       * (the design director, b356); an attack's nerve goes faster than a defence's on its own ground.
       */
      perLoss: { att: 0.5, def: 0.5 },
      /** A friend breaking within 'near' metres. */
      friendRuns: 0.03,
      near: 400,
      /** Coming back while not being hit for 'still' seconds, a second; faster for a runner who is clear. */
      recover: 0.01,
      still: 5,
      runnerRecover: 0.02,
      /** Shattered: below this much strength a block runs whatever its nerve. */
      shattered: 0.12,
      /** Experience and a general: experience takes this share off every loss of nerve at full; each level of the general 0.03. */
      xp: 0.35,
      general: 0.03
    },
    /**
     * Blocks a side: one a division. A side with fewer than 'least' has divisions split into their three regiments until
     * it has enough to handle (ruling 217(c)). No division is ever folded into another (ruling 226(10)).
     */
    blocks: { least: 6, regiments: 3 },
    /**
     * The battle's scale (the true-scale plan, GR1; Brian's standing rule of 22 November 04:14Z: on-screen numbers reflect
     * the units each side brings): one N a battle, the men both sides bring over the figure budget, rounded up to a
     * multiple of step (1 when they fit the budget whole). Looks only: the drill reads it (no block draws more men than
     * its men0 over N) and the recording states it; nothing in the fight does. Tanks and guns join the count with GR2's
     * table; lorries never do (GR6). page: the 2D field page's own budget (GR5, the production lead's option A of 22
     * November 08:03Z): every block's men as plain dots, its men0 over the page's one N, thinning with its strength, with
     * no drill; the Kursk salient's 1,050,000 men draw about 23,400 dots at 1 to 45.
     */
    scale: { budget: 8500, step: 5, page: 24e3 },
    /** The army breaks when this share of its power is running or gone. */
    breaks: 0.5,
    /**
     * Or when this share of its power is worn away: what an attack will pay before it is called off, and a defence
     * before it gives up the ground. Set so that a battle fought to its end costs each side about what the campaign's
     * bill charges for the result it hands back (the design director, b356; tests/battle-land.test.ts).
     */
    spent: { att: 0.13, def: 0.18 },
    /**
     * But not while more than this share of its power stands fresh or has not fought: one worn block never takes a fresh
     * army with it (ruling 226(2), b476(1)).
     */
    fought: 0.5,
    /**
     * A block at this share of its strength or more, and steady, is fresh, in contact or not; in an army of this many
     * blocks or fewer (one block a fifth of it or more), the fresh hold the army at the brink as those not yet in the
     * fight do. Larger armies hold by those not yet in the fight alone: the fresh rule there moves the field 8 points off
     * its card, and at six blocks the hills-and-works practice ground 10 (the design director, b476(1) and b480; 356
     * campaign attacks and the nine practice grounds, 18 November). Never a defence behind fortress works or in a fort,
     * whose works the card prices already (b488: Verdun's three divisions behind works ran 8.6 points under their card).
     * Held there, such an army's blocks no longer fresh give ground rather than stand to be ground down (the design
     * director, b496(1): fought out, the Libyan Group at Mechili booked 1.27 of the staff's bill; now 1.06).
     */
    fresh: { str: 0.9, upTo: 5 },
    /**
     * Burning wrecks stay (b477, Brian's pick): every tank a block loses leaves a wreck where it stood, to the end of the
     * fight. Men on foot beside one take light cover, either army's, at the field's own figure for broken ground: a block
     * takes it for the share of its tray that wrecks within its frontage, or 'beside' metres of it, can shelter (one wreck,
     * one model's worth), never better than its ground gives already. 'r' is how far a wreck's shelter reaches, for the 3D
     * game's mark.
     */
    wreck: { cover: "rough", beside: 10, r: 12 },
    /**
     * What a block that leaves the field still loses on the way (stragglers, wounded, guns and lorries left behind), as a
     * share of the strength it had left. A defence driven off its own ground leaves far more behind than an attack that
     * falls back to the lines it came from: the campaign's own bill has the same shape (resolveBattle in game.ts).
     */
    count: { routing: { att: 0.05, def: 0.05 }, gone: { att: 0.1, def: 0.15 } },
    /** Randomness of each blow, so near-even fights do not always go the same way: +- this share. */
    jitter: 0.1,
    /**
     * How much of the card's own run of luck the day carries (ruling 226(4)): 1 draws the day's luck across the whole run,
     * as the card's chance does, so a plain attack at the card's odds wins about as often as the card says.
     */
    fortune: 1,
    /**
     * A stretch of the defence's line is quiet when no enemy stands within this many metres of a block's own lane, left or
     * right, anywhere on the field: that block closes on the fight once the line is struck elsewhere (b407).
     */
    quiet: 350,
    /** Metres a block pressing an attack will go to take the next enemy when its own breaks. */
    press: 600,
    /** Seconds of battle time before the defender is counted as having held. */
    limit: 360,
    /**
     * Neighbours join the fight (release 2): metres a corps from next door walks onto the field from its edge before it
     * stands as a reserve; how far in from a back edge it comes on; metres between its blocks; and how far it looks for
     * clear dry ground to come on at, in field cells in from the edge and along it either way.
     */
    join: { walk: 200, edge: 40, gap: 40, inward: 24, across: 20 },
    /**
     * A capital's or a city's streets (field.ts layCity, streetsOf). inside: the metres inside the ragged edge of its core
     * that the defence's line stands, in the outer streets and never in front of the city (the design director, b529).
     * clock: a city's day is longer by this share of its field that is built (b515(g)): limit * (1 + clock * built), so
     * the march in through the streets and the fight in them do not end on the clock. Measured with the line inside the
     * edge over 555 city and capital fields x 10 seeds (b529's bars: every kind within a tenth of its card, close odds
     * too, and none on the clock more than 1 fight in 10): 0.15 holds both, the widest forest cities at close odds +8.9
     * and marsh cities at close odds 8.7% on the clock; 0.25 and 0.5 put the forest and marsh cities' close odds over
     * a tenth, 0 put marsh and mountain cities on the clock 1 fight in 8.
     */
    city: { inside: 100, clock: 0.15 },
    /** The computer general thinks once a second. */
    think: 1,
    /**
     * Air support (Brian, 2026-11-18 18:58Z; the design director, b533): a side's fighters on call strike when called,
     * the card having counted them already. While a side has strikes left, its blows hold back the card's share for them
     * (SideTicket.air.share), owed to the block each fell on; a strike pays what is owed where its run lands (sim.ts,
     * "Air support"). Seconds are battle time, metres field metres.
     *   first, every: the first strike is ready this long after the armies meet, each next this long after the last call.
     *   wait: a strike left ready this long, the staff call it themselves, for either side (b533(c): the enemy's too,
     *     never aimed sharper).
     *   eta: from the call to the run; speed: metres a second the flight flies, so the run takes its length over it;
     *     out: seconds it is seen pulling away after the run.
     *   len, minRun, wide: the run's longest and shortest, and the strip its guns rake, each side of its line half of it.
     *   near: pointed at a place, the run takes the frontage of the enemy block nearest it within this.
     *   close: no gun fires within this of the caller's own men; a run with less than 'least' of its line clear is refused.
     *   armour: tanks take this much of a strafing run (guns made for men in the open).
     *   nerve: the run's weight on nerve, as a blow from the front.
     *   alt, low: the flight's height coming in and on the run, metres (pictures only). models: planes in a flight;
     *     legs: the staff's strike splits into pairs, a pair down the frontage of each of the blocks owed most.
     *   toss: seconds from a bomb let go to its burst ('land:bomb-drop' then the 'shell'), pictures only.
     *   bombs: how many fall on the run's far third, pictures only (b533(f)): from the nation's first year of
     *     fighter-bombers, before it the run strafes alone. bombsFrom: that year by nation, '_' for every other, from the
     *     historian's list (/mnt/project-files/historian/fighter-bombers.md, 19:35Z): Germany from the Balkans and Russia,
     *     1941; Britain and America 1942; the Soviets from June 1941 (the certain date); Italy 1941; Japan 1943; France
     *     only with the Free French, 1943; Poland never in 1939; the generic fighter 1942.
     */
    air: {
      first: 40,
      every: 40,
      wait: 20,
      eta: 12,
      speed: 120,
      out: 6,
      len: 400,
      minRun: 100,
      wide: 60,
      near: 300,
      close: 60,
      least: 0.3,
      armour: 0.6,
      nerve: 1,
      alt: 150,
      low: 60,
      models: 4,
      legs: 2,
      bombs: 2,
      toss: 1.5,
      bombsFrom: { GER: 1941, UK: 1942, USA: 1942, SOV: 1941, ITA: 1941, JAP: 1943, FRA: 1943, POL: 9999, _: 1942 }
    },
    /**
     * Hedge cutters (release 2; Brian, 22 November 14:19Z: "bladed Shermans punch a gap through a hedgerow where the player
     * picks"; the design director's ruling 242, C1-A to C4-A; LandBattle.cutters): off unless the battle is opened with
     * them. A tank block carries cutters from turn 'from' on a field with hedges, whatever its army (C2-A: Brian's standing
     * rule that the battle's mechanics are every playable nation's, 21 November 06:54Z). Told to cut where a hedge line
     * runs (within 'near' metres of one), it drives for that point and on through; as its front reaches the hedge the cut
     * is begun (t0), and 'secs' battle seconds later a gap 'width' metres wide stands open there (t1, a Gate of kind
     * 'cut'), which the drawn men of every block on either side walk through from then. The staffs' tank blocks cut
     * wherever their own way crosses a hedge line with no gap near (C4-A). Looks only (C1-A): nothing in the fight reads
     * a gap, so the fight is the same with the switch on or off. The bocage (C1-B, built as the ruling's 17:45Z amendment
     * asks, "unfitted tanks cross only at gaps"; its own switch, LandBattle.bocage, measured against off): a tank block
     * without cutters crosses a hedge line only at a gap, a gateway or a cut once open, and goes round to one; this is
     * where cutters (and a cut's gap) change the fight, and only with that switch on.
     *   from: turn 59, July 1944. Bradley saw the Culin cutter on 14 July and held it back until Cobra, 25 July 1944; by
     *     then three of every five First Army tanks carried one (Blumenson, Breakout and Pursuit, ch. XI).
     *   share: three tanks in five carried one at Cobra (Blumenson; Zaloga, Operation Cobra 1944): how many of a block's
     *     drawn tanks wear the blades, sent to the 3D game as the block's cutter flag (the bridge's unitOut). Looks only.
     *   secs: battle seconds a cut takes, about 25 real seconds at LAND.smoke.perMin. No source gives one (the historian,
     *     H1, 22 November: not found in ten sources; the one timed sortie bounds a breach and the drive to the next bank at
     *     about 3 real minutes): the design director's looks-only knob (ruling 242 C3), until H1 is sourced.
     *   width: the gap, metres: the 3D game's own gateway width (an M4 is about 2.6 m wide; no source gives a gap's width:
     *     ruling 242 C3).
     *   near: how near a hedge line the point picked must lie, metres: the hedge line's own (LAND.hide.hedge.near).
     */
    cutters: { from: 59, share: 0.6, secs: 6, width: 3.2, near: HEDGE_NEAR },
    /**
     * Smoke on call (release 2; Brian, 22 November 14:19Z: "a section lays a smoke line to cover a crossing, when and where
     * the player orders it"; the design director's ruling 242, S1-A to S6, amended 17:45Z from the historian's answers;
     * LandBattle.smoke): off unless the battle is opened with it. A block on foot, or lorried infantry, whose army has the
     * means lays a screen across the ground before it: a line no longer than its own front (S2), its middle within its
     * army's 'reach' of its front, standing 'hang' battle seconds; one at a time, and 'count' a battle (S3). While a screen
     * stands, any blow struck straight at its target (direct fire: foot, tanks, a machine-gun team, a gun over open
     * sights) whose line from the shooter to the target crosses it strikes at 'share' (S1-A); fire from far back (guns
     * beyond LAND.guns.closeAt, mortar teams, the air, on fire plans laid before the smoke: the German 1940 handbook) is
     * not touched. It gives no cover and does no harm. With hiding on, nobody is seen through it (the build spec's reading
     * of S1-A; it draws nothing of the dice itself, but a block it keeps hidden is not fired on till found, so the draws
     * after it change as the fight does: LandBattle's Smoke on call). Both staffs lay one as a
     * foot block of theirs goes in to the assault (S5-A); never a defence in its works.
     *   share: fire at a place, not a seen target: the rules' existing share for fire back at a block found but not
     *     located (LAND.hide.locate.fire; the German squad manual: if tanks lay smoke, "fire into the smoke"). Never twice
     *     on one blow: a blow already that vague, or fire on cover at a block still hidden (already a place, not a seen
     *     target: LAND.hide.search.blow), is not halved again. Only the ground's part of a blow: the share a side holds
     *     back for its air strikes is held back whole (the air is not touched).
     *   count: screens a block lays in a battle where its army's means gives none: the design director's knob, pending H7
     *     (ruling 242 S3; the historian found no section load for any army but Britain's).
     *   perMin: battle seconds to a real minute, turning the sources' burn times into battle time (ruling 242 S2, answering
     *     H6): foot moves at 6 m/s of battle time (LAND.speed.inf) against US quick time of 120 steps of 30 inches a minute,
     *     1.52 m/s (FM 22-5).
     *   front: an army that sets its smoke down along its own front (reach 0) lays it there for a line drawn on its own
     *     ground or within this many metres of its front, and refuses one drawn further off (129(d)): a throw, the thrown
     *     reach of the others' grenades (the historian, D: the Italian pot "is thrown or set down, so the 30 m thrown reach
     *     fits").
     *   means: by army: reach, metres beyond its front the line's middle may lie (0: it is laid along its own front);
     *     hang, battle seconds it stands (real minutes times perMin); count, where the army's own load is sourced.
     *     USA: the M19A1 WP rifle grenade to about 195 m (OP 1664); the AN-M8 HC grenade burns 105 to 150 s (FAS), about
     *       2 real minutes. (The 60 mm WP shell came only in January 1945.)
     *     UK: the platoon's 2-inch mortar, smoke to 500 yd, 457 m (canadiansoldiers.com), about 2 real minutes a bomb (the
     *       1966 pamphlet); 6 smoke bombs and 4 smoke grenades a platoon (Memorandum on Platoon Battle Drill, 1942), about
     *       3 a section (the historian, D; ruling 242 as amended: Britain and her kin 3 screens).
     *     GER: smoke hand grenades and candles, thrown (Nebelhandgranate 39, 1 to 2 minutes: Ordnance Bulletin 59,
     *       Intelligence Bulletin January 1943); the 5 cm mortar had no smoke round (MIS Special Series 14).
     *     SOV: the RDG-1 grenade, about 1.5 minutes, about 30 yd of screen, thrown (Intelligence Bulletin, June 1946).
     *     JAP: the Type 89 discharger's smoke round, 45 to 206 yd (Intelligence Bulletin, June 1943); no burn time found
     *       (the historian, H5): 22 s is the design director's knob.
     *     ITA: the 2-kg smoke pot, set down or thrown, burning 1 min 15 s (TTT 38, November 1943): laid along its own
     *       front (ruling 242 as amended).
     *     France 1940 has none (no section smoke in the 1940 platoon or weapons lists: the historian, H5), and no army
     *     not listed has any: no stand-in until a means is found (ruling 242 S2).
     *   kin: a nation that takes its senior partner's means (ruling 242 S2, "the 240 form", as plans.kin on the air
     *     branch's arsenal.json): Canada, Australia, New Zealand, South Africa and India Britain's; Vichy and Free France
     *     France's (none today; theirs too once H5 finds France a means).
     */
    smoke: {
      share: LOCATE.fire,
      count: 2,
      perMin: 15,
      front: 30,
      means: {
        USA: { reach: 195, hang: 30 },
        UK: { reach: 457, hang: 30, count: 3 },
        GER: { reach: 30, hang: 22 },
        SOV: { reach: 30, hang: 22 },
        JAP: { reach: 188, hang: 22 },
        ITA: { reach: 0, hang: 19 }
      },
      kin: { CAN: "UK", AUS: "UK", NZL: "UK", SAF: "UK", IND: "UK", VIC: "FRA", FFR: "FRA" }
    },
    /**
     * Field telephone lines (release 2; Brian, 22 November 17:33Z: "lets do all three but make it where we can toggle it
     * off if someone doesnt want it"; the design director's rulings, 18:36Z, PHONES Q1 to Q3; LandBattle.phones): off
     * unless the battle is opened with them. Wire was the defence's own means ("Telephones are usually installed down to
     * companies along the main line of resistance", FM 7-25 ¶52; Cherbourg 1944, "Approximately 95% of communications was
     * by wire", Signal Corps Report 63), so as the clock starts every block of the defence on the field has a wire from its
     * headquarters at the middle of its zone's back edge; a corps that comes up from next door has none. The wire head is
     * "pushed close" (FM 7-24 ¶68h): it follows its block, and moving never cuts it. A blow on the block from an enemy gun
     * block or mortar team cuts a line at 'cut' (Q1; machine guns never), and an enemy tank block whose step crosses the
     * wire cuts a line outright (FM 24-20 ¶32d: surface lines "often are broken by passing troops and vehicles"). Once its
     * lines are all cut the wire is down, and every order to that block, the player's or a staff's, waits its army's
     * fallback from when it was given: the wireless (LAND.radio's wait, as the radio switch reckons it) or a runner (the
     * metres from the headquarters over 'runner'). A lineman leaves the headquarters at the first cut, walks the wire to it
     * at foot pace (LAND.speed.inf), splices it in 'splice' seconds and the wire is up again: what was waiting goes at once.
     *   cut: the chance a line is cut by each such blow (each tick's blow of a gun block or a mortar team on the block at
     *     the wire's end): knob (DD 18:36Z). No source counts breaks: the sources say lines were "repeatedly cut by enemy
     *     artillery" (84th Division, Battle Experiences 1945) and "Heavy fire ... was constantly cutting our lines" (100th
     *     Division, 1945).
     *   splice: battle seconds a splice takes: FM 24-20 (1944) ¶15c(3) and ¶15c(5), preparing the splice "should not take
     *     more than 1 minute" and tying it "should be about 1 minute": about 2 real minutes, at LAND.smoke.perMin 15
     *     battle seconds a real minute. Finding the break is not counted (no source).
     *   runner: metres a battle second a runner goes: the rules' own foot speed (LAND.speed.inf); knob (DD 18:36Z).
     *   lines: lines to each block, by army. Germany 2 (DD 18:36Z, Q3, P6): "in the danger zone, therefore, double lines and
     *     heavy insulated wires are used" (German Tactical Doctrine, MIS): a German wire needs two cuts before orders wait.
     *     Every other army 1 ('_').
     *   fallback: by army, [through year, 'radio' or 'runner'], the first that covers the battle's year (as LAND.radio's
     *     nations): how orders go once the wire is down.
     *     USA: runners to 1942, the wireless from 1943: the SCR-536 and SCR-300 "became available in substantial quantities"
     *       only "well into 1943" (Tactical Notebook); FM 7-25 ¶49b, "radio and messengers are the primary means".
     *     UK and her kin (CAN, AUS, NZL, SAF, IND) the wireless, every year: No. 18 sets at company and platoon headquarters
     *       (the Calgary Highlanders' signals platoon, canadiansoldiers.com).
     *     GER: runners to 1942, the wireless from 1943 (the Feldfu b/c pack sets "in the forward echelon", TTT 46, 1944):
     *       "When wire connections fail or are not functioning steadily, the radio is indispensable" (German Tactical Doctrine).
     *     SOV runners: 9,586 sets made in 1941 (SPbGUT); "a broken line often meant losing touch with subordinate troops"
     *       (3DNews). JAP runners: 0 to 4 sets a battalion (TTT 46), "a pool of runners" at company (MIS Bulletin 16).
     *     ITA runners (nothing sourced but the telephone). FRA and VIC runners: "only French heavy tanks had radios" in 1940
     *       (GlobalSecurity). Every other army runners ('_'): knob (DD 18:36Z).
     */
    phones: {
      cut: 0.12,
      splice: 30,
      runner: 6,
      lines: { USA: 1, UK: 1, CAN: 1, AUS: 1, NZL: 1, SAF: 1, IND: 1, GER: 2, SOV: 1, JAP: 1, ITA: 1, FRA: 1, VIC: 1, _: 1 },
      fallback: {
        USA: [[1942, "runner"], [9999, "radio"]],
        UK: [[9999, "radio"]],
        CAN: [[9999, "radio"]],
        AUS: [[9999, "radio"]],
        NZL: [[9999, "radio"]],
        SAF: [[9999, "radio"]],
        IND: [[9999, "radio"]],
        GER: [[1942, "runner"], [9999, "radio"]],
        SOV: [[9999, "runner"]],
        JAP: [[9999, "runner"]],
        ITA: [[9999, "runner"]],
        FRA: [[9999, "runner"]],
        VIC: [[9999, "runner"]],
        _: [[9999, "runner"]]
      }
    },
    /**
     * Stretcher bearers (release 2; Brian, 22 November 17:33Z: "lets do all three but make it where we can toggle it off";
     * the design director's rulings Q4 and Q5, 18:36Z, and 18:38Z on the light cases; LandBattle.bearers): off unless the
     * battle is opened with them. Of each loss of strength a block suffers, 'killed' are dead and the rest wounded; of the
     * wounded, light.share are dressed and back in the line light.dress seconds later (the only same-battle return in the
     * record, Q5), and the rest are litter cases waiting for the block's bearers. Its bearers ('share' of its strength at the
     * start, counted in its strength) carry them back 'litter' to a case, out to the aid post 'aid' metres behind where it
     * started and back at 'pace': while they are out the block's own blows weaken by the men away (never its strength, its
     * nerve, its count or its size). Nothing here draws on the dice.
     *   killed: the share of each loss killed, by army (the design director, 19:14Z, from the historian's
     *     /mnt/project-files/historian/three-ideas-readings-1122.md; died of wounds counted as killed). The drill draws
     *     the rest of the men who fall as wounded. Sourced, no knob of ours:
     *     USA 0.192: 4.22 wounded to 1 killed (Dupuy).
     *     UK and her kin (CAN, AUS, NZL, SAF, IND) 0.230: 3.35 to 1 (Hansard, 27 June 1944, Italy to Rome).
     *     GER 0.279: 2.58 to 1 (OKW to January 1945, all fronts).
     *     JAP 0.271: 2.69 to 1 (China, 1937 to 1941).
     *     '_', every other army (SOV, ITA, FRA, VIC and the rest): 0.227, the middle of the laid ratios, 3.40 to 1.
     *   share: bearers as a share of a block's men, by army, sourced in kind and rounded:
     *     UK and her kin (CAN, AUS, NZL, SAF, IND) 0.025: a corporal and 19 privates a battalion, "all of these were
     *       infantrymen" (bayonetstrength.uk, The Organisation of the British Infantry Battalion 1938-45, p.10;
     *       britishmilitaryhistory.co.uk, the battalion HQ's medical list).
     *     GER 0.03: a Krankenträger in each platoon headquarters and the company's medical NCO (KStN 131n, 1 May 1944,
     *       battleorder.org): 4 of a company of 142.
     *     SOV 0.035: a medical instructor and 4 orderlies a company of 143 (shtat 04/550, 10 December 1942, battleorder.org).
     *     USA 0.02: two company aid men a company (FM 8-10, 1942, para 28d(1), 33b) and the battalion's 12-man litter
     *       squad (med-dept.com, The WW2 Aid Station).
     *     JAP 0.02: a medical orderly a platoon (TM-E 30-480, Handbook on Japanese Military Forces, evacuation).
     *     ITA, FRA and VIC 0.0275: unsourced; knob (DD 18:36Z), the middle of the sourced 2 to 3.5% (Q4).
     *     '_', every other army: 0.0275, the same knob (DD 18:36Z).
     *   litter: bearers to a stretcher (FM 8-10 para 32a: "Litter squads consist ordinarily of four bearers each").
     *   aid: metres from the block's start place to its aid post, toward its own back edge, kept on the field: the middle
     *     of FM 8-10's 300 to 800 yards (para 31b(3)), 550 yards (the design director, Q4, not the 350 proposed).
     *   pace: metres a battle second out and back: FM 8-10 para 32c, a round trip of 500 yards each way "at least twenty
     *     minutes", 914.4 m in 300 battle seconds (15 battle seconds a real minute). A round trip to 503 m: 330 s.
     *   light: the lightly wounded, back in the same battle (Q5: the only same-battle return; no bigger share for feel):
     *     share: of the wounded, the US "carded for record only", 17.1% (123,836 of 723,560 wounded: the Dupuy Institute,
     *       Carded for Record in WWII): knob (DD 18:36Z).
     *     at: metres from the block where they are dressed. The design director, 18:38Z: "Light cases are dressed at the
     *       block by the company's aid man, or by themselves, and are back in the line after the 30 s dressing knob, with
     *       no walk ... every soldier carried a field dressing." Kept as a knob (DD 18:36Z): 0, no walk.
     *     out, back: their pace going and coming back, times the foot's (LAND.speed.inf), read only where at > 0:
     *       knobs (DD 18:36Z).
     *     dress: battle seconds to dress a wound: knob (DD 18:36Z).
     */
    bearers: {
      killed: { USA: 0.192, UK: 0.23, CAN: 0.23, AUS: 0.23, NZL: 0.23, SAF: 0.23, IND: 0.23, GER: 0.279, JAP: 0.271, _: 0.227 },
      share: { USA: 0.02, UK: 0.025, CAN: 0.025, AUS: 0.025, NZL: 0.025, SAF: 0.025, IND: 0.025, GER: 0.03, SOV: 0.035, JAP: 0.02, ITA: 0.0275, FRA: 0.0275, VIC: 0.0275, _: 0.0275 },
      litter: 4,
      aid: 503,
      pace: 3.048,
      light: { share: 0.17, at: 0, out: 0.5, back: 1, dress: 30 }
    }
  };
  var phoneLines = (nat) => LAND.phones.lines[nat] ?? LAND.phones.lines._;
  var phoneFallback = (nat, year) => (LAND.phones.fallback[nat] ?? LAND.phones.fallback._).find(([to]) => year <= to)?.[1] ?? "runner";
  var reachOf = (u) => u.team ? LAND.teams[u.team].reach : LAND.reach[u.kind];
  var patrolSize = (nat) => LAND.patrol.size[nat] ?? LAND.patrol.size._;
  var gunsOf = (u) => u.team ? LAND.teams[u.team] : LAND.guns;
  var smokeMeans = (nat) => LAND.smoke.means[LAND.smoke.kin[nat] ?? nat] ?? null;
  var bearerShare = (nat) => LAND.bearers.share[nat] ?? LAND.bearers.share._;
  var killedShare = (nat) => LAND.bearers.killed[nat] ?? LAND.bearers.killed._;

  // ../../data/battle/city-plans.json
  var city_plans_default = {
    _doc: [
      "Each real capital's and city's kept street plan (the design director, b618 and b625). A city's plan is laid from its own name: plan 1 from 'streets:<town>', plan n from 'streets:<town>#n'. Each city keeps the first plan of its own sequence that, fought as it will be fought (its own river or shore in the lie of each of its real attacks, on its kind's 24 equal tickets), sits within 7 points of its kind and runs no more than one fight in ten to the clock. Picked by that rule, never by eye. A city not listed keeps plan 1.",
      "Two real cities share a town's name (Birmingham in the Midlands and in Alabama): 'shared' gives each its own key by its province's name, so no two cities share a plan. A plan is listed by that key.",
      "Stalingrad (b625(2)): its strip on, tested by the bars (within a tenth of its kind, no more than one fight in ten to the clock); its plan 2 is the first to meet them.",
      "Checked again on fresh fights against the kind's own cities on the same fights; a city that read past 7 there walked its sequence again on that test and keeps the first plan that passes.",
      "Where a city's own river or shore moves every one of its first ten plans the same way past 7, that is the place's water, not chance: the city takes the bars, the first plan within a tenth of its kind and no more than one fight in ten to the clock (the design director, b638). Belgrade (the Danube across with the Sava on the flank) keeps plan 1 so.",
      "Every attack on a city as the open game fights it: 115 of 683 cross a river on the map, where the card's river is the field's and the city's own water is not drawn. Each city with such an attack was fought on its kind's crossing tickets too, against the kind on the same fights; 13 past 7 there walked their sequence on with every lie, crossings included (b638). Lyon's only miss on plan 1 was a lie the open game fights as a crossing, so it keeps plan 1."
    ],
    plans: {
      Amsterdam: 2,
      Asuncion: 4,
      Athens: 2,
      Atlanta: 3,
      Balboa: 6,
      Birmingham: 5,
      "Birmingham, Alabama": 2,
      Bogota: 2,
      Bordeaux: 2,
      Bratislava: 4,
      Budapest: 2,
      "Buenos Aires": 2,
      Buffalo: 3,
      Cairo: 3,
      Chicago: 2,
      Cincinnati: 2,
      Cologne: 5,
      Columbus: 2,
      Denver: 2,
      Detroit: 4,
      Dnepropetrovsk: 5,
      Dublin: 6,
      Essen: 2,
      Glasgow: 4,
      Gorky: 2,
      "Guatemala City": 5,
      Halifax: 3,
      Hamburg: 6,
      Hartford: 4,
      Havana: 4,
      Houston: 2,
      Indianapolis: 2,
      Istanbul: 2,
      Katowice: 3,
      Kaunas: 5,
      Kharkov: 2,
      Kiev: 3,
      Leeds: 8,
      Leipzig: 3,
      Lille: 6,
      Lisbon: 4,
      Liverpool: 6,
      London: 2,
      Luxembourg: 3,
      Madrid: 2,
      Marseille: 2,
      "Mexico City": 2,
      Milan: 2,
      Milwaukee: 2,
      Minneapolis: 3,
      Montevideo: 4,
      Montreal: 2,
      Moscow: 2,
      Munich: 4,
      "New York": 2,
      Ottawa: 2,
      "Panama City": 2,
      Paris: 2,
      Philadelphia: 2,
      Pittsburgh: 2,
      Ploiesti: 3,
      Prague: 8,
      Rome: 3,
      Rostov: 3,
      Rotterdam: 5,
      "San Antonio": 2,
      Santiago: 2,
      "Sao Paulo": 2,
      Saratov: 3,
      Seattle: 2,
      Smolensk: 3,
      "St Louis": 5,
      Stalingrad: 2,
      Stockholm: 2,
      Tbilisi: 4,
      Tegucigalpa: 2,
      Tehran: 5,
      Toledo: 4,
      Toronto: 4,
      Vienna: 3,
      Voronezh: 3,
      Voroshilovgrad: 2
    },
    shared: {
      Birmingham: {
        Midlands: "Birmingham",
        Birmingham: "Birmingham, Alabama"
      }
    }
  };

  // ../../src/battle/land/field.ts
  var GROUNDS = ["open", "field", "woods", "rough", "marsh", "water", "road", "bridge", "buildings", "rock", "bunker", "works", "blown", "suburb", "industry", "park"];
  var G = Object.fromEntries(GROUNDS.map((k, i) => [k, i]));
  var FIELD = { w: 3e3, h: 2e3, cell: 25 };
  var zonesOf = (w, h) => ({ att: [w * 0.12, 120, w * 0.88, 480], def: [w * 0.12, h * 0.62, w * 0.88, h * 0.9] });
  var BEND = { r: 180, swing: 30, wave: 520, clear: 130 };
  var STRIP = { core: 380, terraces: 170, suburbs: 250 };
  var STREAM = { great: 55, lesser: 35, swing: 45, shore: 230, behind: 330, joinBehind: 420, far: 60, clear: 75, road: 90, off: 60, wobble: 0.12 };
  var RECIPE = {
    plains: { relief: 14, woods: 0.14, fields: 0.38, rough: 0.03, marsh: 0, rock: 0 },
    forest: { relief: 22, woods: 0.55, fields: 0.12, rough: 0.05, marsh: 0.02, rock: 0 },
    hills: { relief: 70, woods: 0.26, fields: 0.2, rough: 0.12, marsh: 0, rock: 0.02 },
    mountain: { relief: 190, woods: 0.22, fields: 0.04, rough: 0.22, marsh: 0, rock: 0.16 },
    marsh: { relief: 5, woods: 0.16, fields: 0.1, rough: 0.02, marsh: 0.42, rock: 0 },
    urban: { relief: 12, woods: 0.06, fields: 0.18, rough: 0.04, marsh: 0, rock: 0 },
    desert: { relief: 18, woods: 0, fields: 0, rough: 0.14, marsh: 0, rock: 0.03 }
  };
  function noise(rng, cols, rows, scale) {
    const gx = Math.ceil(cols / scale) + 2, gy = Math.ceil(rows / scale) + 2;
    const lat = new Float32Array(gx * gy);
    for (let i = 0; i < lat.length; i++) lat[i] = rng.next();
    const out = new Float32Array(cols * rows);
    const s = (t) => t * t * (3 - 2 * t);
    for (let r = 0; r < rows; r++) for (let c = 0; c < cols; c++) {
      const fx = c / scale, fy = r / scale, x0 = Math.floor(fx), y0 = Math.floor(fy), tx = s(fx - x0), ty = s(fy - y0);
      const a = lat[y0 * gx + x0], b = lat[y0 * gx + x0 + 1], d = lat[(y0 + 1) * gx + x0], e = lat[(y0 + 1) * gx + x0 + 1];
      out[r * cols + c] = (a + (b - a) * tx) * (1 - ty) + (d + (e - d) * tx) * ty;
    }
    return out;
  }
  function fractal(rng, cols, rows, scale, octaves = 3) {
    const out = new Float32Array(cols * rows);
    let amp = 1, total = 0;
    for (let o = 0; o < octaves; o++) {
      const n = noise(rng, cols, rows, Math.max(2, scale / 2 ** o));
      for (let i = 0; i < out.length; i++) out[i] += n[i] * amp;
      total += amp;
      amp *= 0.5;
    }
    for (let i = 0; i < out.length; i++) out[i] /= total;
    return out;
  }
  function cut(v, share) {
    if (share <= 0) return -1;
    if (share >= 1) return 2;
    const s = Array.from(v).sort((a, b) => a - b);
    return s[Math.min(s.length - 1, Math.floor(share * s.length))];
  }
  function buildField(place, seed, bend) {
    const { w, h, cell } = FIELD;
    const cols = Math.round(w / cell), rows = Math.round(h / cell), n = cols * rows;
    const rng = new Rng(seed ^ 24301);
    const R = RECIPE[place.terrain] ?? RECIPE.plains;
    const at = (c, r) => r * cols + c;
    const ground = new Array(n).fill(G.open);
    const height = new Array(n).fill(0);
    const lift = fractal(rng, cols, rows, 28, 4);
    for (let r = 0; r < rows; r++) for (let c = 0; c < cols; c++) {
      let v = lift[at(c, r)];
      if (place.terrain === "mountain") {
        const pass = Math.exp(-(((c - cols / 2) / (cols * 0.14)) ** 2));
        v = Math.max(0, v - 0.45 * pass) * 1.25;
      }
      if (place.terrain === "desert") {
        v = v * 0.5 + (r > rows * 0.62 ? 0.5 : r > rows * 0.55 ? (r - rows * 0.55) / (rows * 0.07) * 0.5 : 0);
      }
      height[at(c, r)] = v * R.relief;
    }
    const coverOf = (share, scale, code, where = () => true) => {
      if (share <= 0) return;
      const f = fractal(rng, cols, rows, scale, 3), k = cut(f, 1 - share);
      for (let r = 0; r < rows; r++) for (let c = 0; c < cols; c++) if (f[at(c, r)] > k && where(c, r) && ground[at(c, r)] === G.open) ground[at(c, r)] = code;
    };
    coverOf(R.marsh, 12, G.marsh, (c, r) => height[at(c, r)] < R.relief * 0.55);
    coverOf(R.woods, 10, G.woods);
    coverOf(R.fields, 7, G.field);
    coverOf(R.rough, 9, G.rough);
    if (R.rock > 0) {
      const slope = new Float32Array(n);
      for (let r = 1; r < rows - 1; r++) for (let c = 1; c < cols - 1; c++) slope[at(c, r)] = Math.hypot(height[at(c + 1, r)] - height[at(c - 1, r)], height[at(c, r + 1)] - height[at(c, r - 1)]) / (2 * cell);
      const k = cut(slope, 1 - R.rock);
      for (let i = 0; i < n; i++) if (slope[i] > k && slope[i] > 0.12) ground[i] = G.rock;
    }
    let town = null;
    const size = streetsOf(place) ?? place.townSize;
    const plan = planSeed(place, seed), own = plan !== seed && (size === "capital" || size === "city"), pr = new Rng(plan ^ 31249);
    let built = {};
    if (size) {
      const rad = size === "capital" ? 520 : size === "city" ? 430 : size === "town" ? 260 : 150;
      const s = place.stream, strip = size === "city" && s?.lie === "shore" && s.side === "back" && s.strip ? h - (s.great ? STREAM.shore : STREAM.shore * 0.7) : 0;
      const ux = rng.next(), uy = rng.next();
      const tx2 = w * (0.4 + 0.2 * (own ? pr.next() : ux)), ty0 = h * (0.7 + 0.06 * (own ? pr.next() : uy)), ty2 = strip ? strip - STRIP.core / 2 : ty0;
      town = { x: tx2, y: ty2, r: rad };
      const dense = fractal(rng, cols, rows, 3, 2);
      if (size === "capital" || size === "city") built = layCity(size, tx2, ty2, cols, rows, ground, height, plan, strip);
      else {
        for (let r = 0; r < rows; r++) for (let c = 0; c < cols; c++) {
          const d = Math.hypot(c * cell + cell / 2 - tx2, r * cell + cell / 2 - ty2) / rad;
          if (d < 1 && dense[at(c, r)] > 0.25 + 0.45 * d) ground[at(c, r)] = G.buildings;
        }
        for (let r = 0; r < rows; r++) for (let c = 0; c < cols; c++) {
          const d = Math.hypot(c * cell - tx2, r * cell - ty2) / (rad * 1.3);
          if (d < 1) height[at(c, r)] *= 0.4 + 0.6 * d;
        }
      }
    }
    const road = (path, wide = 1) => {
      for (let i = 0; i <= 600; i++) {
        const [x, y] = path(i / 600);
        const c0 = Math.floor(x / cell), r0 = Math.floor(y / cell);
        for (let dc = 0; dc < wide; dc++) {
          const c = c0 + dc;
          if (c >= 0 && c < cols && r0 >= 0 && r0 < rows && ground[at(c, r0)] !== G.water) ground[at(c, r0)] = G.road;
        }
      }
    };
    const tx = town?.x ?? w / 2, ty = town?.y ?? h * 0.72;
    const wob0 = rng.range(-1, 1), wob = own ? pr.range(-1, 1) : wob0;
    const flow = place.river ? { base: h * (0.46 + 0.04 * rng.next()), amp: 60 + 50 * rng.next(), ph: rng.range(0, 6) } : null;
    const midAt = (c) => flow.base + Math.sin(c / cols * 7 + flow.ph) * flow.amp + Math.sin(c / cols * 17 + flow.ph * 2) * flow.amp * 0.3;
    const halfAt = (c) => 28 + 10 * Math.sin(c * 0.3 + flow.ph);
    const away = bend?.dry === "east" ? -1 : 1, turnX = bend ? bend.x + away * BEND.r : 0;
    const straight = (c) => !bend || (c * cell + cell / 2 - turnX) * away >= 0;
    if (flow && bend) {
      const z = zonesOf(w, h).att, mid = (bend.x + (away > 0 ? z[2] : z[0])) / 2;
      const rx = away > 0 ? Math.max(mid, turnX + 130) : Math.min(mid, turnX - 130), yr = midAt(rx / cell - 0.5) + 150, yt = h * 0.71;
      road((t) => [rx + (tx - rx) * Math.min(1, Math.max(0, (t * h - yr) / (yt - yr))) + Math.sin(t * 5 + wob) * 60, t * h]);
    } else road((t) => [w / 2 + (tx - w / 2) * Math.min(1, t * 1.4) + Math.sin(t * 5 + wob) * 90, t * h]);
    road((t) => [t * w, ty + Math.sin(t * 4 + wob) * 70]);
    let riverMid, course;
    if (flow) {
      riverMid = [];
      const over = new Set((place.crossings ?? []).map((x) => Math.min(cols - 1, Math.max(0, Math.floor(x * w / cell)))));
      for (let c = 0; c < cols; c++) {
        const yc = midAt(c);
        riverMid.push(yc);
        if (!straight(c)) continue;
        const half = halfAt(c);
        for (let r = 0; r < rows; r++) {
          const y = r * cell + cell / 2;
          if (Math.abs(y - yc) <= half) {
            const i = at(c, r);
            ground[i] = ground[i] === G.road || over.has(c) ? place.bridgeDown ? G.blown : G.bridge : G.water;
            height[i] = Math.min(height[i], 0) - 2;
          } else if (over.has(c) && Math.abs(y - yc) <= half + 200) ground[at(c, r)] = G.road;
          else if (Math.abs(y - yc) <= half + 40 && ground[at(c, r)] === G.open && place.terrain !== "desert") ground[at(c, r)] = G.field;
        }
      }
      if (bend) {
        const R2 = BEND.r, c0 = turnX / cell - 0.5, y0 = midAt(c0), slope = (midAt(c0 + 0.5) - midAt(c0 - 0.5)) / cell;
        const tl = Math.hypot(1, slope), k = 0.55 * R2, top = y0 - R2;
        const P = [[turnX, y0], [turnX - away * k / tl, y0 - away * slope * k / tl], [bend.x, top + k], [bend.x, top]];
        const bez = (u, i) => (1 - u) ** 3 * P[0][i] + 3 * (1 - u) ** 2 * u * P[1][i] + 3 * (1 - u) * u ** 2 * P[2][i] + u ** 3 * P[3][i];
        const arc = Array.from({ length: 49 }, (_, i) => [bez(i / 48, 0), bez(i / 48, 1)]);
        const sw = flow.ph > 3 ? 1 : -1, leg = [];
        for (let d = 10, y = top - d; ; d += 10, y = Math.max(-60, top - d)) {
          leg.push([bend.x + sw * BEND.swing * Math.sin(2 * Math.PI * (top - y) / BEND.wave) * Math.min(1, (top - y) / 150), y]);
          if (y <= -60) break;
        }
        const line = [...arc, ...leg];
        const along = [0];
        for (let i = 1; i < line.length; i++) along.push(along[i - 1] + Math.hypot(line[i][0] - line[i - 1][0], line[i][1] - line[i - 1][1]));
        const arcLen = along[arc.length - 1];
        const lo = Math.min(bend.x - away * 160, turnX + away * 60), hi = Math.max(bend.x - away * 160, turnX + away * 60);
        for (let r = 0; r < rows; r++) for (let c = 0; c < cols; c++) {
          const x = c * cell + cell / 2, y = r * cell + cell / 2;
          if (x < lo || x > hi || y > y0 + 120) continue;
          let best = Infinity, sAt = 0;
          for (let i2 = 1; i2 < line.length; i2++) {
            const [ax, ay] = line[i2 - 1], [bx, by2] = line[i2], vx = bx - ax, vy = by2 - ay;
            const q = Math.max(0, Math.min(1, ((x - ax) * vx + (y - ay) * vy) / (vx * vx + vy * vy || 1)));
            const dd = Math.hypot(x - ax - q * vx, y - ay - q * vy);
            if (dd < best) {
              best = dd;
              sAt = along[i2 - 1] + q * (along[i2] - along[i2 - 1]);
            }
          }
          const half = halfAt(c0 - away * sAt / cell), i = at(c, r), g0 = ground[i];
          if (g0 === G.water || g0 === G.bridge || g0 === G.blown) continue;
          if (best <= half) {
            ground[i] = g0 === G.road && sAt <= arcLen ? place.bridgeDown ? G.blown : G.bridge : G.water;
            height[i] = Math.min(height[i], 0) - 2;
          } else if (best <= half + 40 && g0 === G.open && place.terrain !== "desert") ground[i] = G.field;
        }
        for (let c = 0; c < cols; c++) {
          if (straight(c)) continue;
          const x = c * cell + cell / 2;
          riverMid[c] = (x - bend.x) * away < 0 ? 0 : arc.reduce((m, p) => Math.abs(p[0] - x) <= cell / 2 ? Math.max(m, p[1]) : m, top);
        }
        course = [];
        for (let x = away > 0 ? w : 0; (x - turnX) * away > 0; x -= away * cell) course.push([x, midAt(x / cell - 0.5)]);
        course.push(...line);
      }
    }
    let stream, cityWater;
    if (place.stream && !flow && town && built.landmark) {
      const roadX = (y) => w / 2 + (tx - w / 2) * Math.min(1, y / h * 1.4) + Math.sin(y / h * 5 + wob) * 90;
      const roadY = (x) => ty + Math.sin(x / w * 4 + wob) * 70;
      const dry = ground.map((k) => k !== G.water);
      stream = layStream(place.stream, seed, place.terrain, ground, height, built.landmark, town, built.streetEdge, roadX, roadY);
      cityWater = dry.map((d, i) => d && ground[i] === G.water);
    }
    const span2 = place.bridgeDown ? G.blown : G.bridge;
    const deck = (k) => k === G.road || k === G.bridge || k === G.blown;
    const hard = (k) => k === G.rock || k === G.marsh || k === G.water || k === G.bunker;
    for (let r = 0; r < rows - 1; r++) for (let c = 0; c < cols; c++) {
      if (!deck(ground[at(c, r)])) continue;
      for (const dc of [-1, 1]) {
        if (c + dc < 0 || c + dc >= cols || !deck(ground[at(c + dc, r + 1)])) continue;
        const p = at(c + dc, r), q = at(c, r + 1);
        if (!hard(ground[p]) || !hard(ground[q])) continue;
        const k = ground[p] === G.water && ground[q] !== G.water ? q : p;
        if (cityWater?.[k] && ![-1, 0, 1].some((a) => [-1, 0, 1].some((b) => ground[at(Math.min(cols - 1, Math.max(0, k % cols + a)), Math.min(rows - 1, Math.max(0, Math.floor(k / cols) + b)))] === G.bridge))) continue;
        ground[k] = ground[k] === G.water ? span2 : G.road;
      }
    }
    const beneath = {};
    if (place.fort > 0) {
      const fy = h * 0.62 + 40 - 20 - cell, every = Math.round(10 / place.fort);
      for (let c = Math.floor(w * 0.1 / cell); c < Math.min(cols - 2, Math.ceil(w * 0.9 / cell)); c += every) {
        const r = Math.floor(fy / cell);
        for (const [dc, dr] of [[0, 0], [1, 0], [0, 1], [1, 1]]) {
          const i = at(c + dc, r + dr);
          if (ground[i] !== G.water && ground[i] !== G.bridge && ground[i] !== G.blown) {
            if (ground[i] !== G.bunker) beneath[i] = ground[i];
            ground[i] = G.bunker;
          }
        }
      }
    }
    let storeys = built.storeys;
    if (ground.some((k) => k === G.buildings || k === G.suburb || k === G.industry)) {
      storeys ?? (storeys = new Array(n).fill(0));
      for (let i = 0; i < n; i++) {
        const k = ground[i];
        if (k !== G.buildings && k !== G.suburb && k !== G.industry) storeys[i] = 0;
        else if (!storeys[i]) storeys[i] = 1 + (Math.imul(i + 1, 2654435761) >>> 31);
      }
    } else storeys = void 0;
    return {
      w,
      h,
      cell,
      cols,
      rows,
      ground,
      height,
      beneath,
      riverMid,
      zones: zonesOf(w, h),
      town,
      ...storeys ? { storeys } : {},
      ...built.landmark ? { landmark: built.landmark } : {},
      ...built.streetEdge ? { streetEdge: built.streetEdge } : {},
      ...course ? { bend, course } : {},
      ...stream ? { stream } : {}
    };
  }
  function bezier(a, k, b, steps = 40) {
    return Array.from({ length: steps + 1 }, (_, i) => {
      const u = i / steps, v = 1 - u;
      return [v * v * a[0] + 2 * v * u * k[0] + u * u * b[0], v * v * a[1] + 2 * v * u * k[1] + u * u * b[1]];
    });
  }
  function layStream(s, seed, terrain, ground, height, heart, town, streetEdge, roadX, roadY) {
    const { w, h, cell } = FIELD, cols = Math.round(w / cell), rows = Math.round(h / cell), z = zonesOf(w, h);
    const rng = new Rng(seed ^ 22506);
    const half = s.great ? STREAM.great : STREAM.lesser, jh = STREAM.lesser;
    const dcx = Math.min(z.def[2] - 300, Math.max(z.def[0] + 300, town.x));
    const edge = (streetEdge ?? []).filter((y, c) => y >= 0 && Math.abs(c * cell + cell / 2 - dcx) <= 700).sort((a, b) => a - b);
    const front = Math.max(z.def[1] + 40, edge.length ? edge[Math.floor(edge.length * 0.75)] + LAND.city.inside : 0);
    const L2 = rng.range(1500, 2300), p1 = rng.range(0, Math.PI * 2), p2 = rng.range(0, Math.PI * 2);
    const wave = (v) => 0.5 + 0.35 * Math.sin(2 * Math.PI * v / L2 + p1) + 0.15 * Math.sin(2 * Math.PI * v / (L2 * 0.37) + p2);
    const far = (hw) => h - hw - STREAM.far;
    const jside = s.join?.side === "east" ? 1 : -1;
    const course = [];
    let lie = s.lie, join;
    if (lie === "shore" && s.side === "back") {
      const width = s.great ? STREAM.shore : STREAM.shore * 0.7;
      const bank = (x) => Math.min(h - STREAM.far, Math.max(front + STREAM.behind, h - width + 35 * (2 * wave(x) - 1)));
      for (let x = -50; x <= w + 50; x += cell) course.push([x, bank(x)]);
      if (s.flows === "west") course.reverse();
      for (let r = 0; r < rows; r++) for (let c = 0; c < cols; c++) {
        const x = c * cell + cell / 2, y = r * cell + cell / 2, i = r * cols + c, d = y - bank(x);
        if (d >= 0) {
          ground[i] = G.water;
          height[i] = Math.min(height[i], 0) - 2;
        } else if (d > -40 && ground[i] === G.open && terrain !== "desert") ground[i] = G.field;
      }
      return { name: s.name, lie, side: "back", ...s.strip ? { strip: true } : {}, great: s.great, half: 0, course, front };
    }
    if (lie === "shore") {
      const west = s.side !== "east", width = s.great ? STREAM.shore : STREAM.shore * 0.7;
      const bank = (y) => width + 35 * (2 * wave(y) - 1);
      for (let y = -50; y <= h + 50; y += cell) course.push([west ? bank(y) : w - bank(y), y]);
      if (s.flows === "south") course.reverse();
      for (let r = 0; r < rows; r++) for (let c = 0; c < cols; c++) {
        const x = c * cell + cell / 2, y = r * cell + cell / 2, i = r * cols + c, d = west ? bank(y) - x : x - (w - bank(y));
        if (d >= 0) {
          ground[i] = G.water;
          height[i] = Math.min(height[i], 0) - 2;
        } else if (d > -40 && ground[i] === G.open && terrain !== "desert") ground[i] = G.field;
      }
      return { name: s.name, lie, side: west ? "west" : "east", great: s.great, half: 0, course };
    }
    const drowns = (f, up) => {
      const hit = /* @__PURE__ */ new Set(), k = Math.ceil(half / cell);
      for (let v = -50; v <= (up ? h : w) + 50; v += cell / 2) {
        const x = up ? f(v) : v, y = up ? v : f(v), c0 = Math.floor(x / cell), r0 = Math.floor(y / cell);
        for (let dr = -k; dr <= k; dr++) for (let dc = -k; dc <= k; dc++) {
          const c = c0 + dc, r = r0 + dr;
          if (c < 0 || r < 0 || c >= cols || r >= rows || Math.hypot(c * cell + cell / 2 - x, r * cell + cell / 2 - y) > half) continue;
          if (ground[r * cols + c] === G.road) hit.add(r * cols + c);
        }
      }
      return hit.size;
    };
    const drownsPts = (pts, hw) => {
      const hit = /* @__PURE__ */ new Set(), k = Math.ceil(hw / cell);
      for (let n = 1; n < pts.length; n++) for (let t = 0; t < 1; t += 0.25) {
        const x = pts[n - 1][0] + (pts[n][0] - pts[n - 1][0]) * t, y = pts[n - 1][1] + (pts[n][1] - pts[n - 1][1]) * t, c0 = Math.floor(x / cell), r0 = Math.floor(y / cell);
        for (let dr = -k; dr <= k; dr++) for (let dc = -k; dc <= k; dc++) {
          const c = c0 + dc, r = r0 + dr;
          if (c < 0 || r < 0 || c >= cols || r >= rows || Math.hypot(c * cell + cell / 2 - x, r * cell + cell / 2 - y) > hw) continue;
          if (ground[r * cols + c] === G.road) hit.add(r * cols + c);
        }
      }
      return hit.size;
    };
    const fewest = (ways) => ways.reduce((a, b) => drownsPts(b, jh) < drownsPts(a, jh) ? b : a);
    if (lie === "up") {
      const pref = town.x > w / 2 ? 1 : -1;
      const line = (x0) => (y) => x0 + STREAM.swing * 1.2 * (2 * wave(y) - 1);
      let xr = null, least = Infinity;
      for (const side of [pref, -pref]) for (let off = 200; off <= 400; off += 25) {
        const f = line(heart.x + side * off);
        let ok2 = true;
        for (let y = -50; y <= h + 50 && ok2; y += cell) {
          const x = f(y);
          ok2 = Math.abs(x - roadX(Math.min(h, Math.max(0, y)))) >= half + STREAM.road && x >= z.att[0] + 200 && x <= z.att[2] - 200;
        }
        const n = ok2 ? drowns(f, true) : Infinity;
        if (n < least) {
          least = n;
          xr = f;
        }
      }
      if (xr) {
        for (let y = -50; y <= h + 50; y += cell) course.push([xr(y), y]);
        if (s.flows === "south") course.reverse();
        if (s.join) {
          const yc = Math.min(far(jh), front + STREAM.joinBehind + jh), xc = xr(yc), ex = jside < 0 ? -50 : w + 50;
          const da = rng.range(40, 160), dk = rng.range(0, 80);
          const way = (a, k) => bezier([ex, Math.min(h + 50, yc + a)], [(ex + xc) / 2, Math.min(h + 50, yc + k)], [xc, yc]);
          join = { name: s.join.name, half: jh, course: fewest([way(da, dk), ...[40, 100, 160, 220].flatMap((a) => [way(a, 0), way(a, a / 2)])]) };
        }
      } else lie = "across";
    }
    if (lie === "across") {
      const line = (base) => (x) => Math.min(far(half), Math.max(base, roadY(Math.min(w, Math.max(0, x))) + STREAM.road + half) + STREAM.swing * wave(x));
      let yr = line(front + STREAM.behind + half), least = drowns(yr, false);
      for (let k = 1; k <= 6; k++) {
        const f = line(front + STREAM.behind + half + k * cell), n = drowns(f, false);
        if (n < least) {
          least = n;
          yr = f;
        }
      }
      for (let x = -50; x <= w + 50; x += cell) course.push([x, yr(x)]);
      if (s.flows === "west") course.reverse();
      if (s.join) {
        const d0 = rng.range(260, 420), e0 = rng.range(60, 200), k0 = rng.range(0, 120);
        const way = (d, e, k) => {
          const xc = heart.x + jside * d, yc = yr(xc), ye = Math.min(yc - 2 * jh - 2 * cell, front + e);
          return bezier([jside < 0 ? -50 : w + 50, ye], [xc + jside * k, ye + (yc - ye) * 0.1], [xc, yc]);
        };
        join = { name: s.join.name, half: jh, course: fewest([way(d0, e0, k0), ...[260, 340, 420].flatMap((d) => [way(d, 80, 40), way(d, 140, 80), way(d, 200, 0)])]) };
      }
    }
    const lay = (pts, hw, cap) => {
      const road0 = ground.map((k) => k === G.road);
      const wob = rng.range(0, Math.PI * 2), m = hw * 1.15 + 40;
      const along = [0];
      for (let k = 1; k < pts.length; k++) along.push(along[k - 1] + Math.hypot(pts[k][0] - pts[k - 1][0], pts[k][1] - pts[k - 1][1]));
      const xs = pts.map((p) => p[0]), ys = pts.map((p) => p[1]);
      const c0 = Math.max(0, Math.floor((Math.min(...xs) - m) / cell)), c1 = Math.min(cols - 1, Math.ceil((Math.max(...xs) + m) / cell));
      const r0 = Math.max(0, Math.floor((Math.min(...ys) - m) / cell)), r13 = Math.min(rows - 1, Math.ceil((Math.max(...ys) + m) / cell));
      const dist2 = /* @__PURE__ */ new Map(), bank = /* @__PURE__ */ new Map(), sta = /* @__PURE__ */ new Map();
      const wet = /* @__PURE__ */ new Set();
      for (let r = r0; r <= r13; r++) for (let c = c0; c <= c1; c++) {
        const x = c * cell + cell / 2, y = r * cell + cell / 2, i = r * cols + c;
        if (ground[i] === G.water) continue;
        let best = Infinity, sAt = 0, sd = 0;
        for (let k = 1; k < pts.length; k++) {
          const [ax, ay] = pts[k - 1], vx = pts[k][0] - ax, vy = pts[k][1] - ay;
          const q = Math.max(0, Math.min(1, ((x - ax) * vx + (y - ay) * vy) / (vx * vx + vy * vy || 1)));
          const dd = Math.hypot(x - ax - q * vx, y - ay - q * vy);
          if (dd < best) {
            best = dd;
            sAt = along[k - 1] + q * (along[k] - along[k - 1]);
            sd = vx * (y - ay) - vy * (x - ax) >= 0 ? 1 : -1;
          }
        }
        if (best > m) continue;
        dist2.set(i, best);
        bank.set(i, sd);
        sta.set(i, sAt);
        if (ground[i] === G.bridge) continue;
        if (best <= hw * (1 + STREAM.wobble * Math.sin(sAt / 170 + wob))) wet.add(i);
        else if (best <= hw + 40 && ground[i] === G.open && terrain !== "desert") ground[i] = G.field;
      }
      for (const i of wet) {
        ground[i] = G.water;
        height[i] = Math.min(height[i], 0) - 2;
      }
      const mid = (j) => [j % cols * cell + cell / 2, Math.floor(j / cols) * cell + cell / 2];
      const cellOf = (x, y) => {
        const c = Math.floor(x / cell), r = Math.floor(y / cell);
        return c < 0 || r < 0 || c >= cols || r >= rows ? -1 : r * cols + c;
      };
      const pave = (x0, y0, x1, y1, land = false) => {
        const steps = Math.max(1, Math.ceil(Math.hypot(x1 - x0, y1 - y0) / (cell / 4)));
        for (let t = 0; t <= steps; t++) {
          const j = cellOf(x0 + (x1 - x0) * t / steps, y0 + (y1 - y0) * t / steps);
          if (j < 0) continue;
          if (wet.has(j)) {
            if (!land) ground[j] = G.bridge;
          } else if (ground[j] !== G.road && ground[j] !== G.bridge && ground[j] !== G.water) ground[j] = G.road;
        }
      };
      const span2 = (sAt, ends) => {
        let k = 1;
        while (k < pts.length - 1 && along[k] < sAt) k++;
        const q = Math.max(0, Math.min(1, (sAt - along[k - 1]) / (along[k] - along[k - 1] || 1)));
        const [ax, ay] = pts[k - 1], [bx, by2] = pts[k], len = Math.hypot(bx - ax, by2 - ay) || 1;
        const ux = (bx - ax) / len, uy = (by2 - ay) / len, px = ax + (bx - ax) * q, py = ay + (by2 - ay) * q, reach = hw * 1.15 + cell;
        pave(px + uy * reach, py - ux * reach, px - uy * reach, py + ux * reach);
        if (ends) for (const sd of [1, -1]) {
          const ex = px - uy * reach * sd, ey = py + ux * reach * sd;
          const to = ends[sd].reduce((a, j) => Math.hypot(mid(j)[0] - ex, mid(j)[1] - ey) < Math.hypot(mid(a)[0] - ex, mid(a)[1] - ey) ? j : a, ends[sd][0]);
          pave(ex, ey, ...mid(to), true);
        }
      };
      const plans = [];
      const plan = (sAt, ends) => {
        const near = plans.find((p) => Math.abs(p.s - sAt) < 2 * cell);
        if (near) near.ends.push(ends);
        else plans.push({ s: sAt, ends: [ends] });
      };
      const seen = /* @__PURE__ */ new Set();
      const pieces = [];
      for (const i0 of wet) {
        if (!road0[i0] || seen.has(i0)) continue;
        const ends = { 1: /* @__PURE__ */ new Set(), [-1]: /* @__PURE__ */ new Set() };
        const stack = [i0];
        seen.add(i0);
        while (stack.length) {
          const i = stack.pop();
          for (let dr = -1; dr <= 1; dr++) for (let dc = -1; dc <= 1; dc++) {
            const c = i % cols + dc, r = Math.floor(i / cols) + dr, j = r * cols + c;
            if (!dc && !dr || c < 0 || r < 0 || c >= cols || r >= rows) continue;
            if (wet.has(j)) {
              if (road0[j] && !seen.has(j)) {
                seen.add(j);
                stack.push(j);
              }
            } else if ((ground[j] === G.road || ground[j] === G.bridge) && bank.has(j)) ends[bank.get(j)].add(j);
          }
        }
        const runs = (cs) => {
          const out = [];
          for (const j of [...cs].sort((p, q) => sta.get(p) - sta.get(q) || p - q)) {
            const last = out[out.length - 1];
            if (last && sta.get(j) - sta.get(last.cells[last.cells.length - 1]) <= 2 * cell) last.cells.push(j);
            else out.push({ s: 0, cells: [j] });
          }
          for (const o of out) o.s = o.cells.reduce((a, j) => a + sta.get(j), 0) / o.cells.length;
          return out;
        };
        const right2 = runs(ends[-1]), left = runs(ends[1]);
        pieces.push([...right2, ...left].map((o) => o.cells));
        const pairs2 = [];
        right2.forEach((a, ia) => left.forEach((b, ib) => {
          const d = Math.abs(a.s - b.s);
          if (d <= 6 * hw + 2 * cell) pairs2.push([d, ia, ib]);
        }));
        pairs2.sort((p, q) => p[0] - q[0] || p[1] - q[1] || p[2] - q[2]);
        const usedR = /* @__PURE__ */ new Set(), usedL = /* @__PURE__ */ new Set();
        for (const [, ia, ib] of pairs2) {
          if (usedR.has(ia) || usedL.has(ib)) continue;
          usedR.add(ia);
          usedL.add(ib);
          plan((right2[ia].s + left[ib].s) / 2, { [-1]: right2[ia].cells, 1: left[ib].cells });
        }
        for (const [mine, other2, used, sd] of [[right2, left, usedR, -1], [left, right2, usedL, 1]]) mine.forEach((o, io) => {
          if (used.has(io) || !other2.length) return;
          const p = other2.reduce((a, b) => Math.abs(b.s - o.s) < Math.abs(a.s - o.s) ? b : a);
          if (Math.abs(p.s - o.s) > 6 * hw + 2 * cell) return;
          used.add(io);
          plan((o.s + p.s) / 2, { [sd]: o.cells, [-sd]: p.cells });
        });
        if (!usedR.size && !usedL.size && right2.length && left.length) {
          let ia = 0, ib = 0;
          right2.forEach((a, i) => left.forEach((b, j) => {
            if (Math.abs(a.s - b.s) < Math.abs(right2[ia].s - left[ib].s)) {
              ia = i;
              ib = j;
            }
          }));
          usedR.add(ia);
          usedL.add(ib);
          plan((right2[ia].s + left[ib].s) / 2, { [-1]: right2[ia].cells, 1: left[ib].cells });
        }
        for (const [side, used] of [[right2, usedR], [left, usedL]]) {
          for (const o of side) for (let k = 1; k < o.cells.length; k++) pave(...mid(o.cells[k - 1]), ...mid(o.cells[k]), true);
          side.forEach((o, io) => {
            if (used.has(io)) return;
            let a0 = -1, b0 = -1, bd = Infinity;
            side.forEach((p, ip) => {
              if (ip !== io) for (const a of o.cells) for (const b of p.cells) {
                const d = Math.hypot(mid(a)[0] - mid(b)[0], mid(a)[1] - mid(b)[1]);
                if (d < bd) {
                  bd = d;
                  a0 = a;
                  b0 = b;
                }
              }
            });
            if (a0 >= 0 && bd <= 4 * cell) pave(...mid(a0), ...mid(b0), true);
          });
        }
      }
      const pointAt = (sv) => {
        let k = 1;
        while (k < pts.length - 1 && along[k] < sv) k++;
        const q = Math.max(0, Math.min(1, (sv - along[k - 1]) / (along[k] - along[k - 1] || 1)));
        return [pts[k - 1][0] + (pts[k][0] - pts[k - 1][0]) * q, pts[k - 1][1] + (pts[k][1] - pts[k - 1][1]) * q];
      };
      const fromHeart = (sv) => {
        const [x, y] = pointAt(sv);
        return Math.hypot(x - heart.x, y - heart.y);
      };
      for (const p of [...plans].sort((a, b) => fromHeart(a.s) - fromHeart(b.s) || a.s - b.s).slice(0, cap)) for (const e of p.ends) span2(p.s, e);
      const deck = (i) => ground[i] === G.road || ground[i] === G.bridge;
      const label = () => {
        const lab2 = new Int32Array(cols * rows).fill(-1);
        let n = 0;
        for (let i0 = 0; i0 < cols * rows; i0++) {
          if (lab2[i0] >= 0 || !deck(i0)) continue;
          lab2[i0] = n;
          const todo = [i0];
          while (todo.length) {
            const i = todo.pop();
            for (let dr = -1; dr <= 1; dr++) for (let dc = -1; dc <= 1; dc++) {
              const c = i % cols + dc, r = Math.floor(i / cols) + dr, j = r * cols + c;
              if (c < 0 || r < 0 || c >= cols || r >= rows || lab2[j] >= 0 || !deck(j)) continue;
              lab2[j] = n;
              todo.push(j);
            }
          }
          n++;
        }
        return lab2;
      };
      let lab = null;
      for (const piece of pieces) {
        if (piece.length < 2) continue;
        for (let tries = 0; tries < piece.length; tries++) {
          lab ?? (lab = label());
          const L3 = lab, own = new Set(piece.map((cs) => L3[cs[0]]));
          if (own.size < 2) break;
          const from = L3[piece[0][0]], goal = new Set([...own].filter((k) => k !== from));
          const prev = /* @__PURE__ */ new Map(), todo = [];
          for (let i = 0; i < cols * rows; i++) if (L3[i] === from) {
            prev.set(i, -1);
            todo.push(i);
          }
          let hit = -1;
          for (let q = 0, depth = 0; q < todo.length && hit < 0; depth++) {
            const end = todo.length;
            if (depth > Math.ceil((6 * hw + 2 * cell) / cell)) break;
            for (; q < end && hit < 0; q++) {
              const i = todo[q];
              for (let dr = -1; dr <= 1 && hit < 0; dr++) for (let dc = -1; dc <= 1; dc++) {
                const c = i % cols + dc, r = Math.floor(i / cols) + dr, j = r * cols + c;
                if (c < 0 || r < 0 || c >= cols || r >= rows || prev.has(j) || ground[j] === G.water || ground[j] === G.blown) continue;
                prev.set(j, i);
                if (goal.has(L3[j])) {
                  hit = j;
                  break;
                }
                todo.push(j);
              }
            }
          }
          if (hit < 0) break;
          for (let i = prev.get(hit); i >= 0 && L3[i] !== from; i = prev.get(i)) if (!deck(i)) ground[i] = G.road;
          lab = null;
        }
      }
      if (!plans.length && cap > 0 && pts.length > 1) {
        let s1 = 0, d1 = Infinity;
        const dry = (x, y) => {
          const j = cellOf(x, y);
          return j >= 0 && !wet.has(j) && ground[j] !== G.water;
        };
        for (let k = 0; k < pts.length; k++) {
          const d = Math.hypot(pts[k][0] - heart.x, pts[k][1] - heart.y), j = cellOf(pts[k][0], pts[k][1]);
          const [ax, ay] = pts[Math.max(0, k - 1)], [bx, by2] = pts[Math.max(1, k)], len = Math.hypot(bx - ax, by2 - ay) || 1, reach = hw * 1.15 + cell;
          const nx = (by2 - ay) / len, ny = -(bx - ax) / len;
          if (d < d1 && j >= 0 && wet.has(j) && dry(pts[k][0] + nx * reach, pts[k][1] + ny * reach) && dry(pts[k][0] - nx * reach, pts[k][1] - ny * reach)) {
            d1 = d;
            s1 = along[k];
          }
        }
        if (d1 < Infinity) span2(s1);
      }
    };
    lay(course, half, s.bridges ?? Infinity);
    if (join) lay(join.course, jh, s.join?.bridges ?? Infinity);
    return { name: s.name, lie, great: s.great, half, course, ...join ? { join } : {}, ...lie === "across" ? { front } : {} };
  }
  function builtShare(f) {
    let n = 0;
    for (const k of f.ground) {
      const g = GROUNDS[k];
      if (g === "buildings" || g === "suburb" || g === "industry") n++;
    }
    return n / f.ground.length;
  }
  var CITY_PLANS = { ...city_plans_default.plans };
  function planKey(place) {
    return city_plans_default.shared[place.town ?? ""]?.[place.name] ?? place.town ?? "";
  }
  function planSeed(place, seed) {
    if (!(place.region >= 0 && place.town)) return seed;
    const key = planKey(place), n = CITY_PLANS[key] ?? 1;
    return hashString(n > 1 ? `streets:${key}#${n}` : `streets:${key}`);
  }
  function streetsOf(place) {
    if (place.townSize === "capital") return "capital";
    return place.terrain === "urban" || place.townSize === "city" ? "city" : null;
  }
  function layCity(size, tx, ty, cols, rows, ground, height, seed, strip = 0) {
    const { w, h, cell } = FIELD, n = cols * rows;
    const cr = new Rng(seed ^ 49534);
    const edge = fractal(cr, cols, rows, 6, 2), fill = fractal(cr, cols, rows, 2, 2);
    const storeys = new Array(n).fill(0);
    const core = new Uint8Array(n);
    const capital = size === "capital", front = h * 0.6;
    const wave = (a, l) => {
      const p1 = cr.range(0, Math.PI * 2), p2 = cr.range(0, Math.PI * 2), l2 = l * cr.range(0.3, 0.45);
      return (s) => a * (0.7 * Math.sin(s / l * Math.PI * 2 + p1) + 0.3 * Math.sin(s / l2 * Math.PI * 2 + p2));
    };
    const coreFront = wave(170, 1300), termFront = wave(130, 1e3), westSide = wave(110, 700), eastSide = wave(110, 700);
    const ringAt = new Uint8Array(n);
    for (let r = 0; r < rows; r++) for (let c = 0; c < cols; c++) {
      const i = r * cols + c, x = c * cell + cell / 2, y = r * cell + cell / 2, e = (edge[i] - 0.5) * 120;
      let ring = 0;
      if (capital) {
        const west = w * 0.05 + westSide(y) + e * 0.6, east = w - w * 0.05 - eastSide(y) - e * 0.6;
        ring = y >= front + coreFront(x) + e && x > west && x < east ? 3 : y >= front - 350 + termFront(x) + e ? 2 : y >= 90 ? 1 : 0;
      } else if (strip) {
        const cf = strip - STRIP.core + coreFront(x) * 0.4 + e;
        ring = y >= cf ? 3 : y >= cf - STRIP.terraces + termFront(x) * 0.4 ? 2 : y >= cf - STRIP.terraces - STRIP.suburbs + termFront(x) * 0.6 + e ? 1 : 0;
      } else {
        const box = (a, b) => ((x - tx) / a) ** 4 + ((y - ty) / b) ** 4;
        const back = Math.max(300, ty - h * 0.5 + e);
        ring = box(400 + e, 300 + e) < 1 ? 3 : box(580 + e, 480 + e) < 1 ? 2 : ((x - tx) / (1150 + e)) ** 2 + ((y - ty) / back) ** 2 < 1 || y > ty && Math.abs(x - tx) < 1150 + e ? 1 : 0;
      }
      ringAt[i] = ring;
      if (ring === 3) {
        ground[i] = G.buildings;
        core[i] = 1;
        storeys[i] = 3 + (fill[i] > 0.55 ? 1 : 0);
      } else if (ring === 2 && fill[i] > 0.28) {
        ground[i] = G.buildings;
        storeys[i] = 2;
      } else if (ring === 1 && fill[i] > (capital ? 0.4 : 0.3)) {
        ground[i] = G.suburb;
        storeys[i] = 1 + (fill[i] > 0.7 ? 1 : 0);
      }
      if (ring) height[i] *= ring === 1 ? 0.7 : 0.4;
    }
    const inCore = (c, r) => c >= 0 && r >= 0 && c < cols && r < rows && core[r * cols + c] === 1;
    const c0 = Math.round(tx / cell);
    let hub = -1, hd = Infinity;
    for (let i = 0; i < n; i++) {
      if (!core[i]) continue;
      const d = Math.hypot(i % cols * cell + cell / 2 - tx, Math.floor(i / cols) * cell + cell / 2 - ty);
      if (d < hd) {
        hd = d;
        hub = i;
      }
    }
    if (hub < 0) return { storeys };
    const hc = hub % cols, hr = Math.floor(hub / cols), hx = hc * cell + cell / 2, hy = hr * cell + cell / 2;
    const lay = (i) => {
      if (i !== hub) {
        ground[i] = G.road;
        storeys[i] = 0;
      }
    };
    const diag = (p, q) => Math.abs(p % cols - q % cols) === 1 && Math.abs(Math.floor(p / cols) - Math.floor(q / cols)) === 1;
    const layPath = (path, where) => {
      let last = path[0];
      for (let j = 1; j < path.length; j++) {
        const q = path[j + 1];
        if (q !== void 0 && diag(last, q)) continue;
        if (where(path[j])) lay(path[j]);
        last = path[j];
      }
    };
    const round = (radius) => {
      const p2 = cr.range(0, Math.PI * 2), p3 = cr.range(0, Math.PI * 2);
      const at = (a) => radius * (1 + 0.16 * Math.sin(2 * a + p2) + 0.1 * Math.sin(3 * a + p3));
      return { at, inside: (i) => {
        const x = i % cols * cell + cell / 2 - hx, y = Math.floor(i / cols) * cell + cell / 2 - hy;
        return Math.hypot(x, y) < at(Math.atan2(y, x));
      } };
    };
    const old = round(capital ? 300 : 210);
    for (let i = 0; i < n; i++) if (core[i] && old.inside(i)) storeys[i] = 4 + (fill[i] > 0.5 ? 1 : 0);
    storeys[hub] = 5;
    for (const { at } of capital ? [old, round(720), round(1150)] : [old]) {
      const path = [];
      const steps = Math.ceil(at(0) * Math.PI * 2 / (cell / 2));
      for (let t = 0; t <= steps; t++) {
        const a = t / steps * Math.PI * 2, c = Math.floor((hx + Math.cos(a) * at(a)) / cell), r = Math.floor((hy + Math.sin(a) * at(a)) / cell);
        const i = c >= 0 && r >= 0 && c < cols && r < rows ? r * cols + c : -1;
        if (i >= 0 && i !== path[path.length - 1]) path.push(i);
      }
      layPath(path, (i) => core[i] === 1);
    }
    const spokes = capital ? 8 : 5, a0 = -Math.PI / 2 + cr.range(-0.15, 0.15);
    for (let k = 0; k < spokes; k++) {
      const a = a0 + k * Math.PI * 2 / spokes + (k ? cr.range(-0.22, 0.22) : 0);
      const bend = cr.range(-0.35, 0.35), bl = cr.range(500, 900), bp = cr.range(0, Math.PI * 2);
      const path = [hub];
      let x = hx, y = hy;
      for (let s = 0; s < 3e3; s += cell / 2) {
        const dir = a + bend * (Math.sin(s / bl + bp) - Math.sin(bp));
        x += Math.cos(dir) * (cell / 2);
        y += Math.sin(dir) * (cell / 2);
        const c = Math.floor(x / cell), r = Math.floor(y / cell);
        if (c < 0 || r < 0 || c >= cols || r >= rows || ringAt[r * cols + c] < 2) break;
        const i = r * cols + c;
        if (i !== path[path.length - 1]) path.push(i);
      }
      layPath(path, () => true);
    }
    for (let dr = -1; dr <= 1; dr++) for (let dc = -1; dc <= 1; dc++) if (inCore(hc + dc, hr + dr)) lay((hr + dr) * cols + hc + dc);
    for (let k = 0; k < (capital ? 3 : 2); k++) {
      for (let tries = 0; tries < 20; tries++) {
        const pc = Math.floor(cr.next() * cols), pr = Math.floor(cr.next() * rows), pw = 3 + Math.floor(cr.next() * 2), ph = 3 + Math.floor(cr.next() * 2);
        if (!inCore(pc, pr) || !inCore(pc + pw - 1, pr + ph - 1) || old.inside(pr * cols + pc) || old.inside((pr + ph - 1) * cols + pc + pw - 1)) continue;
        for (let r = pr; r < pr + ph; r++) for (let c = pc; c < pc + pw; c++) if (ground[r * cols + c] === G.buildings) {
          ground[r * cols + c] = G.park;
          storeys[r * cols + c] = 0;
        }
        break;
      }
    }
    const iw = capital ? 12 : 8, ih = capital ? 6 : 5, side = cr.next() < 0.5 ? -1 : 1;
    for (let c = c0 + side * (capital ? 30 : 16) - (side < 0 ? iw : 0), k = 0; k < iw; k++, c++) {
      for (let r = Math.round((capital ? front : ty - 300) / cell) - 2, j = 0; j < ih; j++, r++) {
        if (c < 0 || r < 0 || c >= cols || r >= rows) continue;
        const i = r * cols + c;
        if (ground[i] !== G.road) {
          ground[i] = G.industry;
          storeys[i] = 2;
        }
      }
    }
    const kinds = ["spire", "dome", "station"];
    const landmark = { x: hx, y: hy, kind: kinds[Math.floor(cr.next() * 3)] };
    const streetEdge = new Array(cols).fill(-1);
    for (let c = 0; c < cols; c++) for (let r = 0; r < rows; r++) if (core[r * cols + c]) {
      streetEdge[c] = r * cell;
      break;
    }
    return { storeys, landmark, streetEdge };
  }
  var UNDUG = ["water", "bridge", "blown", "buildings", "industry", "bunker", "works"];
  var diggable = (g) => !UNDUG.includes(g);
  function digWorks(f, x, y, w, d) {
    for (let yy = y - d / 2 - 15; yy <= y + d / 2 + 15; yy += f.cell / 2) for (let xx = x - w / 2; xx <= x + w / 2; xx += f.cell / 2) {
      const c = Math.floor(xx / f.cell), r = Math.floor(yy / f.cell);
      if (c < 0 || r < 0 || c >= f.cols || r >= f.rows) continue;
      const i = r * f.cols + c, k = GROUNDS[f.ground[i]];
      if (diggable(k)) {
        (f.dugInto ?? (f.dugInto = {}))[i] = f.ground[i];
        f.ground[i] = G.works;
      }
    }
  }
  function coverAt(f, x, y) {
    const c = Math.min(f.cols - 1, Math.max(0, Math.floor(x / f.cell))), r = Math.min(f.rows - 1, Math.max(0, Math.floor(y / f.cell)));
    const i = r * f.cols + c, k = f.ground[i];
    return GROUNDS[k === G.works && f.dugInto?.[i] !== void 0 ? f.dugInto[i] : k];
  }
  function groundAt(f, x, y) {
    const c = Math.min(f.cols - 1, Math.max(0, Math.floor(x / f.cell))), r = Math.min(f.rows - 1, Math.max(0, Math.floor(y / f.cell)));
    return GROUNDS[f.ground[r * f.cols + c]];
  }
  function heightAt(f, x, y) {
    const c = Math.min(f.cols - 1, Math.max(0, Math.floor(x / f.cell))), r = Math.min(f.rows - 1, Math.max(0, Math.floor(y / f.cell)));
    return f.height[r * f.cols + c];
  }

  // ../../src/battle/core/geometry.ts
  var TAU = Math.PI * 2;
  var bearing = (x0, y0, x1, y1) => Math.atan2(x1 - x0, y1 - y0);
  var turn = (a, b) => {
    let d = (b - a) % TAU;
    if (d > Math.PI) d -= TAU;
    if (d < -Math.PI) d += TAU;
    return d;
  };
  var dist = (ax, ay, bx, by2) => {
    const dx = bx - ax, dy = by2 - ay;
    return Math.sqrt(dx * dx + dy * dy);
  };
  function aspectOf(face, x, y, fx, fy) {
    const off = Math.abs(turn(face, bearing(x, y, fx, fy)));
    return off <= Math.PI / 3 ? "front" : off <= 2 * Math.PI / 3 ? "flank" : "rear";
  }
  var ahead = (face) => [Math.sin(face), Math.cos(face)];
  var clamp = (v, lo, hi) => v < lo ? lo : v > hi ? hi : v;

  // ../../src/battle/core/result.ts
  var RATIO = {
    /** The campaign's line between taking the ground and being thrown back. */
    carry: 1.2,
    /** The furthest a played battle can go either way. */
    most: 4,
    least: 0.3,
    /** Added to both sides' losses before they are compared, so a fight with almost no losses reads as even. */
    floor: 0.05,
    /** How hard the exchange pushes R from the line: R = carry * exchange ^ power. */
    power: 0.5
  };
  function fieldRatio(winner, lost) {
    const ex = (lost.def + RATIO.floor) / (lost.att + RATIO.floor);
    const r = RATIO.carry * Math.pow(ex, RATIO.power);
    return winner === "att" ? Math.min(RATIO.most, Math.max(RATIO.carry, r)) : Math.max(RATIO.least, Math.min(RATIO.carry - 0.01, r));
  }
  function levelOf(R, attLostMore) {
    return R >= 3 ? "Heroic victory" : R >= 2 ? "Decisive victory" : R >= RATIO.carry ? attLostMore ? "Pyrrhic victory" : "Close victory" : R >= 0.8 ? "Close defeat" : "Decisive defeat";
  }
  var CLAMP = { mode: "across" };
  function clampToDice(R, odds, dice, winner) {
    const lo = odds * dice[0], hi = odds * dice[1];
    let r = Math.min(hi, Math.max(lo, R));
    if (CLAMP.mode === "across") {
      if (winner === "att" && r < RATIO.carry) r = RATIO.carry;
      if (winner === "def" && r >= RATIO.carry) r = RATIO.carry - 0.01;
    }
    return r;
  }
  function diceNow(odds, dice, standing, roll2) {
    const now = odds * (Math.max(1e-6, standing.att) / Math.max(1e-6, standing.def));
    const R = now * (dice[0] + (dice[1] - dice[0]) * roll2);
    return Math.min(odds * dice[1], Math.max(odds * dice[0], R));
  }

  // ../../src/battle/core/select.ts
  function lineUp(units, side, x0, y0, x1, y1, gap = 120) {
    if (!units.length) return [];
    let len = dist(x0, y0, x1, y1);
    if (len < 30) {
      const w = gap * (units.length - 1);
      x0 -= w / 2;
      x1 = x0 + w;
      y1 = y0;
      len = w;
    }
    let face = bearing(x0, y0, x1, y1) - Math.PI / 2;
    const toward = side === "att" ? 0 : Math.PI;
    if (Math.cos(face - toward) < 0) face += Math.PI;
    const ux = len > 0 ? (x1 - x0) / len : 1, uy = len > 0 ? (y1 - y0) / len : 0;
    const order = [...units].sort((a, b) => a.x * ux + a.y * uy - (b.x * ux + b.y * uy));
    const n = order.length;
    return order.map((u, i) => {
      const k = n === 1 ? 0.5 : i / (n - 1);
      return { id: u.id, x: x0 + (x1 - x0) * k, y: y0 + (y1 - y0) * k, face };
    });
  }

  // ../../data/battle/land-styles.json
  var land_styles_default = {
    _doc: [
      "How the computer general fights on the land field (the tactics panel, item 1; Brian, 19 Nov: 'more general themes', 'loosely to history or not at all', 'a sandbox to play and make your own history').",
      "Every army attacks on one plan: guns open first on a chosen main effort; the main effort's foot waits at a start line under its guns, then goes in with its tanks beside it; the rest pin the line in front of them; once the main target is fixed the tanks hook round a flank. An attack holds no reserve back: the card prices the whole army, so a style moves where the weight falls, never how much of it strikes (the design director, ruling 239's rider, 21:28Z; one comes back only when the card prices one). A defence holds its ground and sends its reserve where it is struck hardest.",
      "A style changes only how long, how deep and how much: numbers on that one plan, never a script. Never the player's: a side the player commands has no style (styles.ts styleFor).",
      "styles: wait, seconds the main effort waits at its start line under its guns; main, the share of the side's non-gun power at the main effort; stretch, the enemy line blocks in the main effort; hook, the tanks leave the foot once the main target's nerve is under this share of its start (null: never); deep, the hook goes behind the target or onto guns within 400 m behind it, else onto its flank; plan false, no main effort at all: every block its own lane at once, tanks with the line. defLeash and defFinish, metres a defending block goes from home, and the gap at which it finishes a wavering attacker (today 300 and 150). No leash is longer than LAND.hold.within (300 m): a block past it loses its held ground's take (ruling 239's rider): a bold defence chases a wavering attacker further (250 m), never off its prepared ground. Every defence, of every style, keeps the fitted quarter of its line back as its reserve (units.ts drawUp, b407): the fit's reserve is part of what the card priced (the design director, ruling 239's rider, 21:57Z), so a mass differs from the others in its attack only. defShift (the design director, ruling 239's rider, 03:20Z): while its line is not yet struck, a defence's held block walks along its own row, behind its line, toward the stretch the attack is coming at, once an enemy block on the march comes within this many metres of its line: at most LAND.hold.within less 10 m from home, striking nothing, then sent as every reserve is once the line is struck (0: it stands in the middle, as today). The gamble is reading the wrong stretch, drawn to a pin (Bagration, June 1944; the historian, H).",
      "Bold and methodical put the same weight on the same width (main 0.6, stretch 2; ruling 239's rider, 21:57Z): bold differs by going in sooner, its early and deep hook and its 250 m finish, never by striking its point harder. No card, dispatch or codex line may say it concentrates more (the design director, 20 Nov 05:00Z).",
      "Methodical's defence is today's exactly, so every fight against a methodical defence replays as it did before styles, and the plain order's fit (LAND.hold) stands.",
      "shared: startLine, metres short of the enemy front the main effort waits (outside its foot's, tanks' and fort's reach); latest, the main effort goes in by this share of the limit whatever its wait (0.35: on plains its foot reaches the start line at about 95 s, not the brief's 82, and 0.3 clipped the wait the brief meant methodical to keep; the design director, 20 Nov 05:00Z); lead, metres a tank or lorry block going in together keeps at most ahead of its group's slowest foot; raw, a side whose experience is under this fights as a mass; near, metres between edges within which friendly foot is beside a tank block; deepBehind, metres behind the main target the bold hook looks for guns.",
      "lean (history dial stops 1 to 3): the land column of data/battle/lean.json, the one table every arm reads; '_' every other nation, or mass where its experience is under raw. path: a nation that grows out of its leaning, by its army's experience or its research, never by date (ruling 239(2), (8): the dated Soviet reform is no trigger): step one at side experience grow.xp[0] or the doctrine grow.tech[0]; step two at grow.xp[1] or grow.tech[1]. The Soviets grow from mass to methodical to bold; America from methodical to bold at the second step (the design director's rider, 20:58Z). Japan attacks by night and defends to the last, fought as bold until item 6 (ruling 239(3)).",
      "pick (stops 4 and 5): no leaning, the situation picks, on ticket facts only, never the nation (styles.ts pickFor)."
    ],
    styles: {
      bold: { plan: true, wait: 5, main: 0.6, stretch: 2, hook: 0.9, deep: true, defLeash: 300, defFinish: 250, defShift: 600 },
      methodical: { plan: true, wait: 30, main: 0.6, stretch: 2, hook: 0.7, deep: false, defLeash: 300, defFinish: 150, defShift: 0 },
      mass: { plan: false, wait: 0, main: 0, stretch: 0, hook: null, deep: false, defLeash: 300, defFinish: 150, defShift: 0 }
    },
    shared: { startLine: 350, latest: 0.35, lead: 100, raw: 0.1, near: 200, deepBehind: 400 },
    path: { SOV: ["mass", "methodical", "bold"], USA: ["methodical", "methodical", "bold"] },
    grow: { xp: [0.25, 0.45], tech: ["combined", "deep"] },
    pick: { attOdds: 1.25, defOdds: 1.2, armour: 0.25, open: ["plains", "desert"] },
    dial: { absent: 3, pickFrom: 4 }
  };

  // ../../data/battle/lean.json
  var lean_default = {
    _doc: [
      "Where each nation's staffs start at history dial stops 1 to 3: one table, a column per arm (the design director, ruling 239(1) and its rider of 20:58Z, on the historian's check). Each field reads its own column; the campaign will read the land column after release 1. Nations are named here and nowhere else in the styles (Brian, 19 Nov: a sandbox to make your own history).",
      "Each arm keeps its own style numbers and grows by its own experience and research (land: data/battle/land-styles.json). '_' is every nation not listed.",
      "A leaning not built yet (night, last-stand: item 6, after release 1) is fought as its stand-in. Japan's land column attacks by night and defends to the last; until item 6 every arm fights those as bold (ruling 239(3)).",
      "Germany and Britain differ at sea (historian, H): Germany's heavy ships broke off when pressed, the Royal Navy sought action."
    ],
    lean: {
      GER: { land: "bold", sea: "methodical", air: "methodical" },
      UK: { land: "methodical", sea: "bold", air: "methodical" },
      USA: { land: "methodical", sea: "methodical", air: "methodical" },
      ITA: { land: "methodical", sea: "methodical", air: "methodical" },
      FRA: { land: "methodical", sea: "methodical", air: "methodical" },
      SOV: { land: "mass", sea: "mass", air: "mass" },
      JAP: { land: { att: "night", def: "last-stand" }, sea: "night", air: "bold" },
      _: { land: "methodical", sea: "methodical", air: "methodical" }
    },
    standIn: { night: "bold", "last-stand": "bold" }
  };

  // ../../src/battle/land/styles.ts
  var D = land_styles_default;
  var STYLES = Object.fromEntries(
    Object.keys(D.styles).map((id) => [id, { id, ...D.styles[id] }])
  );
  var STYLE = D.shared;
  var PATH = D.path;
  var L = lean_default;
  var LEAN = Object.fromEntries(Object.entries(L.lean).map(([n, row]) => [n, row.land]));
  function leaningOf(nat, side) {
    const row = LEAN[nat];
    if (row === void 0) return null;
    const s = typeof row === "string" ? row : row[side];
    const id = isStyle(s) ? s : L.standIn[s];
    return isStyle(id) ? id : "methodical";
  }
  var isStyle = (s) => !!s && s in STYLES;
  function sideXp(s) {
    const tot = s.forces.reduce((a, f) => a + (f.share ?? 0), 0);
    if (tot > 0) return s.forces.reduce((a, f) => a + (f.share ?? 0) * f.xp, 0) / tot;
    return s.forces.length ? s.forces.reduce((a, f) => a + f.xp, 0) / s.forces.length : 0;
  }
  function kindShare(s, kinds) {
    const tot = s.forces.reduce((a, f) => a + (f.share ?? 0), 0) || 1;
    return s.forces.filter((f) => kinds.includes(f.kind)).reduce((a, f) => a + (f.share ?? 0), 0) / tot;
  }
  function stepsOf(s) {
    const xp = sideXp(s), G2 = D.grow, has = (id) => !!s.doctrine?.includes(id);
    let n = 0;
    if (xp >= G2.xp[0] || has(G2.tech[0])) n = 1;
    if (xp >= G2.xp[1] || has(G2.tech[1])) n = 2;
    return n;
  }
  function leanFor(s, side) {
    const path = PATH[s.nat];
    if (path) return path[Math.min(path.length - 1, stepsOf(s))];
    const own = s.nat !== "_" ? leaningOf(s.nat, side) : null;
    if (own) return own;
    return sideXp(s) < STYLE.raw ? "mass" : leaningOf("_", side) ?? "methodical";
  }
  function pickFor(t, side) {
    const s = t[side], P = D.pick, p = t.place, odds = t.reckoning.odds;
    if (sideXp(s) < STYLE.raw) return "mass";
    const open = P.open.includes(p.terrain);
    if (side === "att") {
      const armouredCrossing = !!p.river && s.forces.some((f) => (f.kind === "arm" || f.kind === "mot") && f.cross !== false);
      if (p.fort >= 1 || streetsOf(p) || armouredCrossing || odds < P.attOdds) return "methodical";
      if (open && kindShare(s, ["arm", "mot"]) >= P.armour) return "bold";
      return "methodical";
    }
    if (open && kindShare(t.att, ["arm"]) >= P.armour) return "methodical";
    if (odds <= P.defOdds) return "bold";
    return "methodical";
  }
  var stopOfTicket = (t) => t.hist ?? D.dial.absent;
  function styleFor(t, side) {
    const s = t[side];
    if (s.human) return null;
    if (isStyle(s.style)) return STYLES[s.style];
    return STYLES[stopOfTicket(t) >= D.dial.pickFrom ? pickFor(t, side) : leanFor(s, side)];
  }

  // ../../src/battle/land/ai.ts
  var ok = (u) => u.state !== "gone" && u.state !== "withdrawn" && u.state !== "routing";
  var isFoot = (u) => MOTION[u.kind] === "foot";
  var isMobile = (u) => u.kind === "arm" || u.kind === "mot";
  var ownArm = (u) => u.team === "mg" || u.team === "mor";
  var CLOSE = ["woods", "buildings", "industry", "suburb"];
  var MAIN = 1e5;
  function thinkFor(b, side) {
    const round = b.measured ? flanksNow(b, side) : null;
    const mine = b.units.filter((u) => u.side === side && ok(u) && u.kind !== "fort" && !u.taken && !round?.has(u.id));
    const foes = b.units.filter((u) => u.side !== side && ok(u) && !u.hid);
    if (!foes.length) {
      if (side === "att" && b.units.some((u) => u.side !== side && ok(u))) probe(b, side, mine);
      if (side === "att" && b.hide && b.ambushed.has(side)) searchCover(b, side, mine);
      return;
    }
    const style = b.style[side];
    const stand = b.relief && b.hasJoiners(side) ? relieve(b, side, mine, foes) : null;
    if (side === "att") {
      sendOn(b, side, mine, foes, stand);
      const line = mine.some((u) => u.reserve === "sent" || stand?.has(u.id)) ? mine.filter((u) => u.reserve !== "sent" && !stand?.has(u.id)) : mine;
      if (style) planned(b, side, style, line, foes);
      else plain(b, side, line, foes);
      if (b.measured) seekFlanks(b, side, mine);
      if (b.hide && b.ambushed.has(side)) searchCover(b, side, mine);
      if (b.smoke) smokeIn(b, side, mine, foes);
      return;
    }
    defend(b, side, style, mine, foes, stand);
    if (b.measured) seekFlanks(b, side, mine.filter((u) => u.reserve === "sent"));
    if (b.smoke) smokeIn(b, side, mine, foes);
  }
  function smokeIn(b, side, mine, foes) {
    const P = b.plan[side];
    if (side === "att" && P && P.went === null) return;
    for (const u of mine) {
      if (!hasTarget(u, foes) || u.waits && !u.waits.free) continue;
      if (side === "def" && u.reserve !== "sent" && dist(u.x, u.y, u.home[0], u.home[1]) < LAND.hold.within) continue;
      if (b.smokeWhy(u) !== null) continue;
      const t = foes.find((f) => f.id === u.order.target), gap = b.gap(u, t);
      if (gap > reachOf(u)) continue;
      const m = b.smokeOf(u), k = Math.hypot(t.x - u.x, t.y - u.y);
      if (k < 1e-6) continue;
      const dx = (t.x - u.x) / k, dy = (t.y - u.y) / k, [fx, fy] = [Math.sin(u.face), Math.cos(u.face)];
      const on = Math.min(m.reach, gap / 2), mx = u.x + fx * u.d / 2 + dx * on, my = u.y + fy * u.d / 2 + dy * on, h = u.w / 2;
      b.command({ do: "land:smoke", units: [u.id], x0: mx - dy * h, y0: my + dx * h, x1: mx + dy * h, y1: my - dx * h }, side);
    }
  }
  function searchCover(b, side, mine) {
    const F = LAND.hide.search, dir = side === "att" ? 1 : -1, step = b.hedges.length ? 10 : b.field.cell;
    for (const u of mine) {
      if (u.waits && !u.waits.free) continue;
      if (u.kind === "art" && !u.team || u.engaged !== void 0 || u.order?.kind === "withdraw") {
        if (u.engaged === void 0) {
          delete u.search;
          delete u.searchR;
        }
        continue;
      }
      const ahead2 = Math.min(F.ahead, reachOf(u)), r = ahead2 / 2;
      const x = u.x, y = u.y + dir * (u.d / 2 + r);
      let cover = false;
      for (let dy = -r; dy <= r + F.reach && !cover; dy += step) for (let dx = -F.lane; dx <= F.lane && !cover; dx += step) {
        const px = x + dx, py = y + dy;
        if (px >= 0 && py >= 0 && px < b.field.w && py < b.field.h && (coverAt(b.field, px, py) === "woods" || b.onHedge(px, py))) cover = true;
      }
      if (cover) {
        u.search = [x, y];
        u.searchR = Math.max(r, F.lane);
      } else {
        delete u.search;
        delete u.searchR;
      }
    }
  }
  function seekFlanks(b, side, mine) {
    const A = LAND.armour, dir = side === "att" ? 1 : -1;
    for (const u of mine) {
      if (u.kind !== "arm" || u.order?.kind !== "attack" || u.order.target === void 0 || b.flanking.has(u.id)) continue;
      const t = b.unit(u.order.target);
      if (!t || t.kind !== "arm" || !ok(t) || t.hid) continue;
      const front = b.armourAt(u, t, "front", A.flankAt), sideW = b.armourAt(u, t, "side", A.flankAt), rear = b.armourAt(u, t, "rear", A.flankAt);
      if (front === null || sideW === null || rear === null || front >= A.flankBelow || Math.max(sideW, rear) < A.flankBetter * front) continue;
      const flank = Math.sign(u.x - t.x) || 1, deep = rear > sideW ? dir * (t.d / 2 + u.d / 2 + 60) : 0;
      const x = Math.min(b.field.w - u.w / 2 - 10, Math.max(u.w / 2 + 10, t.x + flank * (t.w / 2 + u.w / 2 + 80)));
      const y = Math.min(b.field.h - u.d / 2 - 10, Math.max(u.d / 2 + 10, t.y + deep));
      if (CLOSE.includes(groundAt(b.field, x, y))) continue;
      b.flanking.set(u.id, { target: t.id, at: [x, y], since: b.t });
      b.command({ do: "move", units: [u.id], x, y }, side);
    }
  }
  function flanksNow(b, side) {
    const busy = /* @__PURE__ */ new Set();
    for (const [id, f] of b.flanking) {
      const u = b.unit(id), t = b.unit(f.target);
      if (!u || u.side !== side) continue;
      if (!ok(u) || u.taken || u.order?.kind === "withdraw") {
        b.flanking.delete(id);
        continue;
      }
      if (!t || !ok(t)) {
        b.flanking.delete(id);
        continue;
      }
      if (!u.order || dist(u.x, u.y, f.at[0], f.at[1]) < 40 || b.t - f.since > LAND.armour.flankFor) {
        b.flanking.delete(id);
        by(b, side, u, t);
        continue;
      }
      busy.add(id);
    }
    return busy;
  }
  function probe(b, side, mine) {
    const dir = side === "att" ? 1 : -1, step = LAND.hide.staff.ahead;
    for (const u of mine) {
      if (u.kind === "art" || u.order) continue;
      const y = Math.min(b.field.h - u.d / 2 - 10, Math.max(u.d / 2 + 10, u.y + dir * step));
      if (Math.abs(y - u.y) > 20) b.command({ do: "move", units: [u.id], x: u.x, y }, side);
    }
  }
  var by = (b, side, u, t, together) => {
    b.command({ do: "attack", units: [u.id], target: t.id, ...together !== void 0 ? { together } : {} }, side);
  };
  var hasTarget = (u, foes) => u.order?.kind === "attack" && foes.some((f) => f.id === u.order.target);
  var inLane = (u, foes) => foes.reduce((best, f) => Math.abs(f.x - u.x) + 0.3 * Math.abs(f.y - u.y) < Math.abs(best.x - u.x) + 0.3 * Math.abs(best.y - u.y) ? f : best, foes[0]);
  function gunsUp(b, side, u, foes) {
    const near = foes.reduce((m, f) => Math.min(m, b.gap(u, f)), Infinity);
    const reach = reachOf(u);
    if (near > reach * 0.9 && !u.order) {
      const cy = foes.reduce((s, f) => s + f.y, 0) / foes.length;
      b.command({ do: "move", units: [u.id], x: u.x, y: Math.min(cy - reach * 0.75, u.y + 400) }, side);
    }
  }
  function plain(b, side, mine, foes) {
    const sorted = [...foes].sort((a, c) => a.x - c.x);
    for (const u of mine) {
      if (u.order?.kind === "withdraw") continue;
      if (u.kind === "art") {
        gunsUp(b, side, u, foes);
        continue;
      }
      if (hasTarget(u, foes)) continue;
      let t;
      if (isMobile(u)) {
        const left = sorted[0], right2 = sorted[sorted.length - 1];
        t = dist(u.x, u.y, left.x, left.y) < dist(u.x, u.y, right2.x, right2.y) ? left : right2;
      } else t = inLane(u, foes);
      by(b, side, u, t);
    }
  }
  var struckOf = (mine, foes) => mine.filter((m) => m.kind !== "art" && m.reserve !== "held" && m.engaged !== void 0 && foes.some((f) => f.id === m.engaged)).sort((a, c) => a.str / a.str0 - c.str / c.str0);
  function sendOn(b, side, mine, foes, skip = null) {
    const sent = mine.filter((u) => u.reserve === "sent" && u.kind !== "art" && u.order?.kind !== "withdraw" && !hasTarget(u, foes) && !skip?.has(u.id));
    if (!sent.length) return;
    const struck = struckOf(mine, foes);
    if (!struck.length) return;
    sent.forEach((u, i) => by(b, side, u, foes.find((f) => f.id === struck[i % struck.length].engaged)));
  }
  function relieve(b, side, mine, foes) {
    const skip = /* @__PURE__ */ new Set();
    for (const u of struckOf(mine, foes)) if (b.waitWhy(u) === null) b.command({ do: "land:wait", units: [u.id] }, side);
    const waiting = b.units.filter((u) => u.side === side && ok(u) && u.waits && !u.waits.free).sort((a, c) => a.str / a.str0 - c.str / c.str0 || a.id - c.id);
    for (const w of waiting) {
      skip.add(w.id);
      const near = (r4) => dist(r4.x, r4.y, w.x, w.y);
      const rs = mine.filter((r4) => r4.corps === w.waits.corps && r4.joined !== void 0 && r4.kind !== "art" && r4.order?.kind !== "withdraw" && !(r4.waits && !r4.waits.free) && !skip.has(r4.id));
      if (!rs.length) continue;
      const r = rs.reduce((m, x) => near(x) < near(m) - 1e-9 || Math.abs(near(x) - near(m)) <= 1e-9 && x.id < m.id ? x : m);
      skip.add(r.id);
      const foe = w.engaged !== void 0 ? foes.find((f) => f.id === w.engaged) : void 0;
      if (foe) {
        if (!(r.order?.kind === "attack" && r.order.target === foe.id)) by(b, side, r, foe);
      } else if (!(r.order?.kind === "move" && dist(r.order.x, r.order.y, w.x, w.y) <= LAND.alone.near)) b.command({ do: "move", units: [r.id], x: w.x, y: w.y }, side);
    }
    return skip;
  }
  var weight = (u) => u.power0 * (0.35 + 0.65 * (u.str / u.str0));
  function makePlan(b, side, s, mine, foes) {
    const P = { phase: s.plan ? "guns" : "assault", main: [], startY: null, role: {}, group: {}, spot: {}, arrived: null, went: null, hook: {} };
    const rest = mine.filter((u) => u.kind !== "art" && u.reserve !== "sent");
    if (!s.plan || !rest.length) {
      for (const u of rest) P.role[u.id] = "pin";
      P.went = 0;
      return P;
    }
    let line = foes.filter((f) => f.kind !== "fort" && f.kind !== "art" && f.reserve !== "held");
    if (!line.length) line = foes.filter((f) => f.kind !== "art");
    if (!line.length) line = foes;
    line = [...line].sort((a, c) => a.x - c.x || a.id - c.id);
    const foeWeight = (f) => weight(f) * (b.teamsRule ? 1 + b.ownTeamsPower(f) / Math.max(1e-9, f.power0) : 1);
    const score = line.map((f, i2) => LAND.cover[coverAt(b.field, f.x, f.y)] / Math.max(1e-6, foeWeight(f)) * (i2 === 0 || i2 === line.length - 1 ? 1.25 : 1) * (f.dug ? 0.8 : 1));
    let k = 0;
    for (let i2 = 1; i2 < line.length; i2++) if (score[i2] > score[k]) k = i2;
    P.main = [line[k].id];
    if (s.stretch >= 2 && line.length > 1) {
      const l = k > 0 ? k - 1 : -1, r = k < line.length - 1 ? k + 1 : -1;
      const n = l < 0 ? r : r < 0 ? l : score[l] >= score[r] ? l : r;
      P.main.push(line[n].id);
    }
    const stretch = P.main.map((id) => b.unit(id));
    const cx = stretch.reduce((a, f) => a + f.x, 0) / stretch.length;
    const order = [...rest].sort((a, c) => Math.abs(a.x - cx) - Math.abs(c.x - cx) || a.id - c.id);
    const withTeams2 = (u) => u.power0 + (b.teamsRule ? b.ownTeamsPower(u) : 0);
    const total = rest.reduce((a, u) => a + withTeams2(u), 0);
    let got = 0, i = 0;
    for (; i < order.length && (got < s.main * total || i === 0); i++) {
      P.role[order[i].id] = "main";
      got += withTeams2(order[i]);
    }
    for (const u of order.slice(i)) P.role[u.id] = "pin";
    const mainUs = () => order.filter((u) => P.role[u.id] === "main");
    if (!mainUs().some(isFoot)) {
      const foot = order.find((u) => isFoot(u) && P.role[u.id] !== "main");
      if (foot) {
        const out = mainUs()[mainUs().length - 1];
        P.role[out.id] = P.role[foot.id];
        P.role[foot.id] = "main";
      }
    }
    if (!order.some((u) => P.role[u.id] === "pin" && !isMobile(u))) {
      for (const u of order) if (P.role[u.id] === "pin") P.role[u.id] = "main";
    }
    const dir = side === "att" ? 1 : -1;
    const front = dir > 0 ? Math.min(...line.map((f) => f.y - f.d / 2)) : Math.max(...line.map((f) => f.y + f.d / 2));
    let y = front - dir * STYLE.startLine;
    if (b.ticket.place.river) {
      const near = nearBank(b, cx, dir);
      if (near !== null) y = dir > 0 ? Math.min(y, near - 60) : Math.max(y, near + 60);
    }
    P.startY = y;
    const mainFoot = mainUs().filter((u) => !isMobile(u)), mainMobile = mainUs().filter(isMobile);
    const clampX = (u, x) => Math.min(b.field.w - u.w / 2 - 10, Math.max(u.w / 2 + 10, x));
    for (const u of mainFoot) P.spot[u.id] = [u.x, dir > 0 ? Math.max(u.y, y) : Math.min(u.y, y)];
    const behind = {};
    for (const u of [...mainMobile].sort((a, c) => a.x - c.x || a.id - c.id)) {
      const f = mainFoot.reduce((m, x2) => !m || Math.abs(x2.x - u.x) < Math.abs(m.x - u.x) ? x2 : m, null);
      const n = f ? behind[f.id] = (behind[f.id] ?? -1) + 1 : 0;
      const x = f ? f.x + (n % 2 ? 1 : -1) * Math.ceil(n / 2) * (u.w + 20) : u.x;
      P.spot[u.id] = [clampX(u, x), dir > 0 ? Math.max(u.y, y - 90) : Math.min(u.y, y + 90)];
    }
    for (const u of mainUs()) P.group[u.id] = MAIN;
    return P;
  }
  function nearBank(b, x, dir) {
    const f = b.field, wet = (g) => g === "water" || g === "blown" || g === "bridge";
    let best = null;
    for (const dx of [-150, 0, 150]) {
      const cxx = Math.min(f.w - 1, Math.max(0, x + dx));
      for (let y = dir > 0 ? 0 : f.h - 1; dir > 0 ? y < f.h : y >= 0; y += dir * f.cell) {
        if (wet(groundAt(f, cxx, y))) {
          best = best === null ? y : dir > 0 ? Math.min(best, y) : Math.max(best, y);
          break;
        }
      }
    }
    return best;
  }
  var mainNow = (b, P) => P.main.map((id) => b.unit(id)).find((f) => !!f && ok(f)) ?? null;
  function mainTarget(b, P, u, foes) {
    const stretch = P.main.map((id) => b.unit(id)).filter((f) => !!f && ok(f));
    if (stretch.length) return stretch.reduce((m, f) => Math.abs(f.x - u.x) < Math.abs(m.x - u.x) ? f : m, stretch[0]);
    return inLane(u, foes);
  }
  function planned(b, side, s, mine, foes) {
    var _a;
    const P = (_a = b.plan)[side] ?? (_a[side] = makePlan(b, side, s, mine, foes));
    const limit = b.ticket.limit;
    const guns = mine.filter((u) => u.kind === "art"), rest = mine.filter((u) => u.kind !== "art");
    const role = (u) => P.role[u.id] ?? "pin";
    for (const g of guns) {
      if (b.teamsRule && ownArm(g)) delete g.aim;
      else if (s.plan) {
        const t2 = mainNow(b, P);
        if (t2) g.aim = t2.id;
        else delete g.aim;
      }
      if (g.order?.kind !== "withdraw") gunsUp(b, side, g, foes);
    }
    if (P.went === null) {
      const us = rest.filter((u) => role(u) === "main");
      const stood = us.filter((u) => !isMobile(u)).length ? us.filter((u) => !isMobile(u)) : us;
      for (const u of us) {
        const at = P.spot[u.id];
        if (!at || u.order?.kind === "withdraw") continue;
        if (!u.order && dist(u.x, u.y, at[0], at[1]) > 20 && u.engaged === void 0) b.command({ do: "move", units: [u.id], x: at[0], y: at[1], face: side === "att" ? 0 : Math.PI }, side);
      }
      const there = (u) => !u.order && (!P.spot[u.id] || dist(u.x, u.y, P.spot[u.id][0], P.spot[u.id][1]) <= 25);
      const withTeams2 = (u) => u.power0 + (b.teamsRule ? b.ownTeamsPower(u) : 0);
      const all = stood.reduce((a, u) => a + withTeams2(u), 0);
      if (P.arrived === null && stood.filter(there).reduce((a, u) => a + withTeams2(u), 0) >= 0.75 * all) P.arrived = b.t;
      const wait = guns.some((g) => !(b.teamsRule && ownArm(g))) ? s.wait : 0;
      if (P.arrived !== null && b.t >= P.arrived + wait || b.t >= STYLE.latest * limit || !us.length) {
        P.went = b.t;
        P.phase = "assault";
      }
    }
    const going = P.went !== null;
    const pinFoot = rest.filter((u) => role(u) === "pin" && !isMobile(u));
    for (const u of pinFoot) {
      if (u.order?.kind === "withdraw" || hasTarget(u, foes)) continue;
      by(b, side, u, inLane(u, foes), u.id);
      P.group[u.id] = u.id;
    }
    for (const u of rest) {
      if (u.order?.kind === "withdraw") continue;
      const r = role(u);
      if (r === "pin" && isMobile(u)) {
        if (hasTarget(u, foes)) continue;
        const partner = pinFoot.filter((f) => ok(f) && f.order?.kind === "attack").reduce((m, f) => !m || Math.abs(f.x - u.x) < Math.abs(m.x - u.x) ? f : m, null);
        const t2 = partner ? foes.find((f) => f.id === partner.order.target) : void 0;
        if (partner && t2) {
          P.group[u.id] = partner.id;
          by(b, side, u, t2, partner.id);
        } else by(b, side, u, inLane(u, foes));
        continue;
      }
      if (r === "main" && going) {
        if (P.hook[u.id]) continue;
        if (hasTarget(u, foes)) continue;
        if (isMobile(u)) {
          const foot = rest.filter((f) => role(f) === "main" && !isMobile(f) && ok(f));
          const partner = foot.reduce((m, f) => !m || Math.abs(f.x - u.x) < Math.abs(m.x - u.x) ? f : m, null);
          const t2 = partner?.order?.kind === "attack" ? foes.find((f) => f.id === partner.order.target) : void 0;
          by(b, side, u, t2 ?? mainTarget(b, P, partner ?? u, foes), foot.length ? MAIN : void 0);
        } else by(b, side, u, mainTarget(b, P, u, foes), MAIN);
      }
    }
    if (!s.plan) return;
    const t = mainNow(b, P);
    if (going && t && s.hook !== null) {
      const fixed = rest.some((f) => isFoot(f) && f.engaged === t.id) || t.engaged !== void 0 && rest.some((f) => f.id === t.engaged && isFoot(f));
      if (fixed && t.mor < s.hook * t.mor0 && !CLOSE.includes(coverAt(b.field, t.x, t.y))) {
        const dir = side === "att" ? 1 : -1;
        for (const u of rest) {
          if (role(u) !== "main" || !isMobile(u) || P.hook[u.id] || u.order?.kind === "withdraw") continue;
          const flank = Math.sign(u.x - t.x) || 1;
          let target = t;
          if (s.deep) {
            const gun = foes.filter((g) => g.kind === "art" && dir * (g.y - t.y) > 0 && dir * (g.y - t.y) <= STYLE.deepBehind && Math.abs(g.x - t.x) <= STYLE.deepBehind).reduce((m, g) => !m || dist(g.x, g.y, t.x, t.y) < dist(m.x, m.y, t.x, t.y) ? g : m, null);
            if (gun) target = gun;
          }
          const at = [t.x + flank * (t.w / 2 + u.w / 2 + 80), t.y + (s.deep ? dir * (t.d / 2 + 60) : 0)];
          const x = Math.min(b.field.w - u.w / 2 - 10, Math.max(u.w / 2 + 10, at[0]));
          if (CLOSE.includes(groundAt(b.field, x, at[1]))) continue;
          P.hook[u.id] = { stage: "out", at: [x, at[1]], target: target.id };
          b.command({ do: "move", units: [u.id], x, y: at[1] }, side);
          P.phase = "hook";
        }
      }
    }
    for (const [id, h] of Object.entries(P.hook)) {
      const u = b.unit(Number(id));
      if (!u || !ok(u) || u.order?.kind === "withdraw") continue;
      const tg = b.unit(h.target);
      if (!tg || !ok(tg)) {
        if (!hasTarget(u, foes)) by(b, side, u, mainTarget(b, P, u, foes));
        continue;
      }
      if (h.stage === "out" && (!u.order || dist(u.x, u.y, h.at[0], h.at[1]) < 40)) {
        h.stage = "in";
        by(b, side, u, tg);
      } else if (h.stage === "in" && !hasTarget(u, foes)) by(b, side, u, mainTarget(b, P, u, foes));
    }
  }
  function coming(b, mine, foes, within) {
    let best = null, gap = within;
    for (const f of foes) {
      if (f.kind === "art" || f.kind === "fort" || f.speed <= 0.5) continue;
      for (const l of mine) {
        if (l.kind === "art" || l.reserve === "held") continue;
        const g = b.gap(f, l);
        if (g < gap) {
          gap = g;
          best = { line: l, foe: f };
        }
      }
    }
    return best;
  }
  function walk(b, side, u, near) {
    const most = LAND.hold.within - 10;
    const want = Math.round(u.home[0] + Math.max(-most, Math.min(most, near.line.home[0] - u.home[0])));
    if (b.shifted.get(u.id) === want) return;
    b.shifted.set(u.id, want);
    b.command({ do: "move", units: [u.id], x: want, y: u.home[1], face: Math.PI }, side);
    b.reserveSeen(side, near.foe);
  }
  function defend(b, side, style, mine, foes, skip = null) {
    const leash = style?.defLeash ?? 300, finish = style?.defFinish ?? 150;
    const edge = b.field.streetEdge, cell = b.field.cell;
    const outside = (f) => {
      if (!edge) return false;
      const y = edge[Math.min(edge.length - 1, Math.max(0, Math.floor(f.x / cell)))];
      return y < 0 || f.y < y;
    };
    const struck = struckOf(mine, foes);
    let next = 0;
    const sendTo = (u) => {
      const at = struck[next++ % struck.length];
      u.reserve = "sent";
      by(b, side, u, foes.find((f) => f.id === at.engaged));
    };
    const shift = b.alertedStaff(side) ? Infinity : style?.defShift;
    const near = !struck.length && shift ? coming(b, mine, foes, shift) : null;
    for (const u of mine) {
      if (u.kind === "art" || u.order?.kind === "withdraw" || skip?.has(u.id)) continue;
      if (u.hid && u.reserve !== "held") continue;
      if (u.reserve === "held") {
        if (struck.length) sendTo(u);
        else if (near) walk(b, side, u, near);
        continue;
      }
      if (u.reserve === "sent") {
        if (!hasTarget(u, foes) && struck.length) sendTo(u);
        continue;
      }
      if (struck.length && !hasTarget(u, foes) && u.engaged === void 0 && !foes.some((f) => Math.abs(f.x - u.home[0]) < LAND.quiet)) {
        sendTo(u);
        continue;
      }
      const away = dist(u.x, u.y, u.home[0], u.home[1]);
      if (hasTarget(u, foes)) {
        const t = foes.find((f) => f.id === u.order.target);
        if (away > leash || u.engaged !== t.id && outside(t)) b.command({ do: "move", units: [u.id], x: u.home[0], y: u.home[1], face: Math.PI }, side);
        continue;
      }
      if (u.engaged !== void 0) continue;
      if (away > 40 && !u.order) {
        b.command({ do: "move", units: [u.id], x: u.home[0], y: u.home[1], face: Math.PI }, side);
        continue;
      }
      const shaky = foes.find((f) => f.state === "wavering" && b.gap(u, f) < finish && !outside(f));
      if (shaky) {
        by(b, side, u, shaky);
        continue;
      }
      const round = foes.find((f) => dist(f.x, f.y, u.home[0], u.home[1]) < leash && !outside(f) && mine.some((m) => !(b.teamsRule && ownArm(m)) && m.engaged === f.id && m !== u));
      if (round) by(b, side, u, round);
    }
  }

  // ../../src/battle/land/crew-names.ts
  var same = (xs) => xs.map((name) => ({ name, en: name }));
  var pairs = (xs) => xs.map(([name, en]) => ({ name, en }));
  var CREW_NAMES = {
    USA: same(["Alabama Belle", "Ace High", "Atlas", "Buckeye", "Blue Ridge Rambler", "Brass Hat", "Cactus Kate", "Cornhusker", "Cyclone", "Dakota Dust", "Desert Rose", "Dixie Doodle", "Erie Lass", "Eight Ball", "Jersey Lightning", "Lone Star Lady", "Lucky Seven", "Ozark Queen", "Prairie Fire", "Texas Twister"]),
    UK: same(["Agincourt", "Ardent", "Aberdour", "Badger", "Bramble", "Kent Lass", "Wolfhound", "Cheshire Cat", "Clydesdale", "Cormorant", "Doughty", "Dorset Lad", "Hambledon", "Heather", "Kestrel", "Moorhen", "Wayfarer", "Stalwart", "Tynesider", "Moorland"]),
    FRA: same(["Aunis", "Quercy", "Vivarais", "Rouergue", "Limousin", "Morvan", "Durance", "Ardèche", "Vézère", "Loing", "Autan", "Bise", "Orage", "Grêle", "Sologne", "Camargue", "Cévennes", "Margeride", "Bigorre", "Dombes"]),
    SOV: pairs([["Za Rodinu", "For the Motherland"], ["Mstitel", "Avenger"], ["Smelyy", "Bold"], ["Vpered na zapad", "Forward to the West"], ["Za nashu Moskvu", "For our Moscow"], ["Za Odessu", "For Odessa"], ["Bogatyr", "Warrior of the old tales"], ["Sokol", "Falcon"], ["Burya", "Storm"], ["Molniya", "Lightning"], ["Podarok frontu", "A gift to the front"], ["Kolkhoznik", "The collective farmer"], ["Ne otstupim", "We will not retreat"], ["Pobeda", "Victory"], ["Sibiryak", "The Siberian"], ["Volga", "Volga"], ["Groza", "Thunderstorm"], ["Druzhba", "Friendship"], ["Za Tulu", "For Tula"], ["Ural", "Ural"]]),
    GER: pairs([["Liesel", "Liesel"], ["Gretl", "Gretl"], ["Annemie", "Annemie"], ["Bärbel", "Bärbel"], ["Rübezahl", "Mountain spirit of the old tales"], ["Gewitter", "Thunderstorm"], ["Hagelschlag", "Hailstorm"], ["Stier", "Bull"], ["Igel", "Hedgehog"], ["Kuckuck", "Cuckoo"], ["Heimweh", "Homesick"], ["Schwarzwald", "Black Forest"], ["Allgäu", "Allgäu"], ["Waldschrat", "Forest goblin"], ["Kleiner Bär", "Little bear"], ["Spätzle", "Noodles"], ["Wetterhexe", "Weather witch"], ["Dickhäuter", "Thick-skin"], ["Feldmaus", "Field mouse"], ["Nebelkrähe", "Hooded crow"]]),
    ITA: pairs([["Rosina", "Rosina"], ["Ninetta", "Ninetta"], ["Vesuvio", "Vesuvius"], ["Etna", "Etna"], ["Tuono", "Thunder"], ["Leone", "Lion"], ["Cicala", "Cicada"], ["Grillo", "Cricket"]]),
    JAP: pairs([["Inoshishi", "Boar"], ["Washi", "Eagle"], ["Hōō", "Phoenix"], ["Fūjin", "Wind spirit"], ["Raijin", "Thunder spirit"], ["Shishi", "Lion"], ["Ōkami", "Wolf"], ["Kiji", "Pheasant"], ["Fuji", "Mount Fuji"], ["Taka", "Hawk"]]),
    POL: pairs([["Ryś", "Lynx"], ["Kania", "Kite"], ["Dzik", "Boar"], ["Smok", "Dragon"], ["Wisła", "Vistula"], ["Warta", "Warta"], ["Mazur", "The Masurian"], ["Kurpie", "Kurpie"], ["Wawel", "Wawel"], ["Jadzia", "Jadzia"], ["Halinka", "Halinka"], ["Kasia", "Kasia"], ["Śmiały", "Bold"], ["Wierny", "Faithful"], ["Bystry", "Swift"], ["Sokół", "Falcon"], ["Góral", "Highlander"]])
  };

  // ../../src/battle/land/crews.ts
  var CREWS = {
    /** A unit that has come through this many battles is a veteran: its crews have named their tanks. */
    veteran: 3,
    /** Japan, Italy and Poland named few: about one tank in this many of a veteran unit (b477(3)). */
    few: 0.25,
    /** Germany's kill rings: one to five a tank, more with the unit's battles, varying tank to tank (b477(4)). */
    rings: { most: 5, per: 0.5 }
  };
  var COMMONWEALTH = ["CAN", "AUS", "NZL", "NZ", "SAF", "IND"];
  function steady(s) {
    let h = 2166136261;
    for (let i = 0; i < s.length; i++) {
      h ^= s.charCodeAt(i);
      h = Math.imul(h, 16777619);
    }
    h ^= h >>> 16;
    h = Math.imul(h, 2246822507);
    h ^= h >>> 13;
    h = Math.imul(h, 3266489909);
    h ^= h >>> 16;
    return (h >>> 0) / 4294967296;
  }
  function army(nat) {
    if (nat === "VIC" || nat === "FFR") return "FRA";
    if (COMMONWEALTH.includes(nat)) return "UK";
    return nat in CREW_NAMES ? nat : null;
  }
  function crewMarks(nat, kind, battles, tanks, seed) {
    if (kind !== "arm" || tanks <= 0) return null;
    const a = army(nat);
    if (!a) return null;
    const vet = battles >= CREWS.veteran;
    if (a === "GER") {
      if (!vet) return null;
      const base = 1 + (battles - CREWS.veteran) * CREWS.rings.per;
      const rings = Array.from({ length: tanks }, (_, i) => Math.max(1, Math.min(CREWS.rings.most, Math.round(base + (steady(`${seed}#r${i}`) * 3 - 1.5)))));
      return { mark: "rings", rings };
    }
    const all = a === "FRA" || (a === "USA" || a === "UK" || a === "SOV") && vet;
    const few = (a === "JAP" || a === "ITA" || a === "POL") && vet;
    if (!all && !few) return null;
    const list = CREW_NAMES[a];
    const start = Math.floor(steady(seed) * list.length);
    const names = Array.from({ length: tanks }, (_, i) => all || steady(`${seed}#n${i}`) < CREWS.few ? list[(start + i) % list.length] : null);
    return names.some((n) => n) ? { mark: "name", names } : null;
  }
  function crewFor(u, battles) {
    return crewMarks(u.nat, u.kind, battles, u.figures0, `${u.corps}:${u.name}`);
  }

  // ../../src/core/ticket.ts
  function turnToDate(turn3) {
    const idx = 8 + (turn3 - 1);
    const year = 1939 + Math.floor(idx / 12);
    const month = idx % 12;
    const names = ["January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"];
    return { year, month, label: `${names[month]} ${year}` };
  }

  // ../../src/battle/land/shells.ts
  var REACH = { light: 4, mortar: 9, field: 11, howitzer: 15, heavy: 23, bomb: 45 };
  var CRATER = { light: 0.5, mortar: 0.8, field: 1, howitzer: 1.5, heavy: 3, bomb: 7 };
  var SHELLS_FROM = ["art", "arm"];
  var MORTARS = ["inf", "mtn", "mot"];
  var MODEL_CAL = {
    Gun75: "field",
    ZiS3: "field",
    "25pdr": "field",
    Pak40: "field",
    AntiTankGun: "light",
    leFH18: "howitzer",
    Howitzer105: "howitzer",
    PanzerIV: "field",
    Panther: "field",
    StuG3: "field",
    Tiger: "field",
    T34: "field",
    "T34-85": "field",
    Sherman: "field",
    CharB1: "field",
    MediumTank: "field",
    SU76: "field",
    SU85: "field",
    M10: "field",
    Sexton: "field",
    LightTank: "light",
    "7TP": "light",
    Matilda: "light",
    M13: "light",
    Type97: "light",
    M7: "howitzer",
    Hummel: "heavy"
  };
  var GENERIC = { gun: "Gun75", tank: [["LightTank", 0], ["MediumTank", 1942]] };
  var BRITISH = { gun: "25pdr", tank: [["Matilda", 0], ["Sherman", 1943]] };
  var FRENCH = { gun: "Gun75", tank: [["CharB1", 0]] };
  var DRAWN = {
    GER: { gun: "leFH18", tank: [["PanzerIV", 0], ["Panther", 1944]] },
    UK: BRITISH,
    CAN: BRITISH,
    NZL: BRITISH,
    SAF: BRITISH,
    IND: BRITISH,
    AUS: { gun: "25pdr", tank: [["Matilda", 0]] },
    USA: { gun: "Howitzer105", tank: [["LightTank", 0], ["Sherman", 1942]] },
    SOV: { gun: "ZiS3", tank: [["LightTank", 0], ["T34", 1941], ["T34-85", 1944]] },
    FRA: FRENCH,
    VIC: FRENCH,
    FFR: FRENCH,
    ITA: { gun: "Gun75", tank: [["M13", 0]] },
    JAP: { gun: "Gun75", tank: [["Type97", 0]] },
    POL: { gun: "Gun75", tank: [["7TP", 0]] }
  };
  function modelOf(kind, nat, year) {
    const d = DRAWN[nat] ?? GENERIC;
    if (kind === "art") return d.gun;
    if (kind !== "arm") return null;
    let m = d.tank[0][0];
    for (const [name, from] of d.tank) if (year >= from) m = name;
    return m;
  }
  function calFor(kind, nat, turn3, armour) {
    if (MORTARS.includes(kind)) return "mortar";
    const year = kind === "arm" && armour ? armour : turnToDate(turn3).year;
    const m = modelOf(kind, nat, year);
    if (!m) return void 0;
    return MODEL_CAL[m] ?? MODEL_CAL[modelOf(kind, "", year)];
  }

  // ../../data/battle/armour.json
  var armour_default = { _about: ["Measured armour and guns (release 2; Brian, 22 November 04:03Z to 04:16Z: 'the named tanks have real advantages from their measured performance against their peers; German tanks are far weaker at the back, so hits there pay'). Built by tools/build-armour.mjs from the land vehicle modeller's sheet (performance-land.json); the historian checks the figures (sure: high, medium, low, or design for our own inventions). Do not edit by hand: rebuild.", "ranges: metres. duels[shooter][target][face]: the chance a hit on that face comes through at each range (before: the same before the shooter had its hollow-charge round, from the month models[shooter].heat). models[name]: kills, the share of the hits that come through that put her out of the fight (the shooter's gun); fire, how often one that comes through sets her burning, by face; explodes, how often a fire reaches her ammunition; radio, the share of her kind with a set that talks (orders reach her the sooner)."], source: "performance-land.json", ranges: [100, 500, 1e3, 1500, 2e3], models: { Tiger: { kills: 0.85, fire: { front: 0.45, side: 0.65, rear: 0.75 }, explodes: 0.25, radio: 1, sure: "high" }, Panther: { kills: 0.75, fire: { front: 0.45, side: 0.65, rear: 0.8 }, explodes: 0.3, radio: 1, sure: "high" }, PanzerIV: { kills: 0.75, fire: { front: 0.5, side: 0.7, rear: 0.75 }, explodes: 0.3, radio: 1, sure: "high" }, "PanzerIV:short": { kills: 0.75, fire: { front: 0.5, side: 0.7, rear: 0.75 }, explodes: 0.3, heat: "1942-03", radio: 1, sure: "high" }, StuG3: { kills: 0.75, fire: { front: 0.5, side: 0.65, rear: 0.75 }, explodes: 0.3, radio: 1, sure: "high" }, Hummel: { kills: 0.95, fire: { front: 0.6, side: 0.7, rear: 0.75 }, explodes: 0.45, radio: 1, sure: "medium" }, Hanomag: { kills: 0.28, fire: { front: 0.5, side: 0.6, rear: 0.6 }, explodes: 0.1, radio: 0.8, sure: "high" }, SdKfz222: { kills: 0.28, fire: { front: 0.5, side: 0.6, rear: 0.6 }, explodes: 0.1, radio: 1, sure: "medium" }, FlakHalftrack: { kills: 0.28, fire: { front: 0.6, side: 0.6, rear: 0.6 }, explodes: 0.1, radio: 0.5, sure: "medium" }, Pak40: { kills: 0.75, fire: { front: 0, side: 0, rear: 0 }, explodes: 0, radio: 0, sure: "high" }, leFH18: { kills: 0.95, fire: { front: 0, side: 0, rear: 0 }, explodes: 0, heat: "1942-01", radio: 0, sure: "high" }, T34: { kills: 0.75, fire: { front: 0.45, side: 0.65, rear: 0.55 }, explodes: 0.35, radio: 0.4, sure: "high" }, "T34-85": { kills: 0.85, fire: { front: 0.45, side: 0.65, rear: 0.55 }, explodes: 0.35, radio: 1, sure: "high" }, KV1: { kills: 0.75, fire: { front: 0.25, side: 0.4, rear: 0.45 }, explodes: 0.35, radio: 0.4, sure: "medium" }, SU85: { kills: 0.85, fire: { front: 0.3, side: 0.5, rear: 0.45 }, explodes: 0.45, radio: 1, sure: "medium" }, SU76: { kills: 0.75, fire: { front: 0.55, side: 0.65, rear: 0.65 }, explodes: 0.35, radio: 0.6, sure: "medium" }, ZiS3: { kills: 0.75, fire: { front: 0, side: 0, rear: 0 }, explodes: 0, radio: 0, sure: "high" }, Sherman: { kills: 0.75, fire: { front: 0.6, side: 0.8, rear: 0.7 }, explodes: 0.25, radio: 1, sure: "high" }, M10: { kills: 0.75, fire: { front: 0.35, side: 0.5, rear: 0.45 }, explodes: 0.25, radio: 1, sure: "high" }, M7: { kills: 0.95, fire: { front: 0.55, side: 0.65, rear: 0.65 }, explodes: 0.35, heat: "1942-06", radio: 1, sure: "high" }, M3Halftrack: { kills: 0.28, fire: { front: 0.5, side: 0.6, rear: 0.6 }, explodes: 0.1, radio: 1, sure: "high" }, M16: { kills: 0.28, fire: { front: 0.5, side: 0.6, rear: 0.6 }, explodes: 0.1, radio: 1, sure: "high" }, M8: { kills: 0.38, fire: { front: 0.5, side: 0.6, rear: 0.6 }, explodes: 0.2, radio: 1, sure: "high" }, Howitzer105: { kills: 0.95, fire: { front: 0, side: 0, rear: 0 }, explodes: 0, heat: "1942-06", radio: 0, sure: "high" }, Matilda: { kills: 0.38, fire: { front: 0.2, side: 0.3, rear: 0.4 }, explodes: 0.2, radio: 1, sure: "medium" }, Sexton: { kills: 0.73, fire: { front: 0.55, side: 0.65, rear: 0.65 }, explodes: 0.35, radio: 1, sure: "medium" }, "25pdr": { kills: 0.73, fire: { front: 0, side: 0, rear: 0 }, explodes: 0, radio: 0, sure: "high" }, CharB1: { kills: 0.6, fire: { front: 0.4, side: 0.55, rear: 0.6 }, explodes: 0.25, radio: 0.7, sure: "medium" }, Gun75: { kills: 0.75, fire: { front: 0, side: 0, rear: 0 }, explodes: 0, radio: 0, sure: "medium" }, M13: { kills: 0.6, fire: { front: 0.35, side: 0.45, rear: 0.5 }, explodes: 0.25, radio: 0.5, sure: "medium" }, Semovente: { kills: 0.75, fire: { front: 0.35, side: 0.45, rear: 0.5 }, explodes: 0.25, heat: "1942-04", radio: 0.7, sure: "medium" }, L3: { kills: 0.28, fire: { front: 0.5, side: 0.6, rear: 0.6 }, explodes: 0.1, radio: 0.1, sure: "medium" }, Type97: { kills: 0.6, fire: { front: 0.3, side: 0.4, rear: 0.45 }, explodes: 0.25, radio: 0.3, sure: "medium" }, "Type97:47": { kills: 0.6, fire: { front: 0.3, side: 0.4, rear: 0.45 }, explodes: 0.25, radio: 0.3, sure: "low" }, "7TP": { kills: 0.38, fire: { front: 0.3, side: 0.4, rear: 0.45 }, explodes: 0.2, radio: 0.3, sure: "medium" }, LightTank: { kills: 0.38, fire: { front: 0.45, side: 0.55, rear: 0.65 }, explodes: 0.2, radio: 0.4, sure: "design" }, MediumTank: { kills: 0.6, fire: { front: 0.45, side: 0.6, rear: 0.7 }, explodes: 0.25, radio: 0.8, sure: "design" }, AssaultGun: { kills: 0.75, fire: { front: 0.45, side: 0.6, rear: 0.7 }, explodes: 0.25, radio: 0.8, sure: "design" }, SelfPropelledGun: { kills: 0.75, fire: { front: 0.55, side: 0.65, rear: 0.7 }, explodes: 0.35, radio: 0.6, sure: "design" }, ArmouredCar: { kills: 0.38, fire: { front: 0.5, side: 0.6, rear: 0.6 }, explodes: 0.15, radio: 0.6, sure: "design" }, AntiTankGun: { kills: 0.48, fire: { front: 0, side: 0, rear: 0 }, explodes: 0, radio: 0, sure: "medium" }, Truck: { kills: 0, fire: { front: 0.4, side: 0.4, rear: 0.4 }, explodes: 0.05, radio: 0, sure: "design" }, FlakTruck: { kills: 0.28, fire: { front: 0.5, side: 0.5, rear: 0.5 }, explodes: 0.1, radio: 0.3, sure: "design" }, WhatIf_BreakthroughTank: { kills: 0.75, fire: { front: 0.4, side: 0.55, rear: 0.7 }, explodes: 0.25, radio: 1, sure: "design" }, WhatIf_TankHunterCar: { kills: 0.75, fire: { front: 0.5, side: 0.6, rear: 0.6 }, explodes: 0.25, radio: 1, sure: "design" }, WhatIf_FlameTank: { kills: 0.28, fire: { front: 0.55, side: 0.75, rear: 0.85 }, explodes: 0.35, radio: 0.8, sure: "design" }, WhatIf_AmphibiousTank: { kills: 0.38, fire: { front: 0.45, side: 0.55, rear: 0.65 }, explodes: 0.2, radio: 0.8, sure: "design" }, WhatIf_SnowTank: { kills: 0.38, fire: { front: 0.45, side: 0.55, rear: 0.65 }, explodes: 0.2, radio: 0.8, sure: "design" }, WhatIf_GliderTank: { kills: 0.38, fire: { front: 0.5, side: 0.6, rear: 0.65 }, explodes: 0.2, radio: 0.8, sure: "design" } }, duels: { Tiger: { Tiger: { front: [0.95, 0.95, 0.85, 0.53, 0.21], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Panther: { front: [0.53, 0.5, 0.46, 0.3, 0.13], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, PanzerIV: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "PanzerIV:short": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, StuG3: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hummel: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hanomag: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SdKfz222: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, T34: { front: [1, 1, 1, 1, 0.93], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "T34-85": { front: [1, 1, 1, 1, 0.87], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, KV1: { front: [1, 1, 1, 1, 0.93], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SU85: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SU76: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Sherman: { front: [1, 1, 1, 1, 0.84], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M10: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M7: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M8: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Matilda: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Sexton: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, CharB1: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M13: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Semovente: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, L3: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Type97: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "Type97:47": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "7TP": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, LightTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, MediumTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, AssaultGun: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SelfPropelledGun: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, ArmouredCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_BreakthroughTank: { front: [1, 0.92, 0.56, 0.17, 0.02], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_TankHunterCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_FlameTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_AmphibiousTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_SnowTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_GliderTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, Panther: { Tiger: { front: [0.95, 0.95, 0.95, 0.81, 0.42], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Panther: { front: [0.72, 0.52, 0.5, 0.42, 0.22], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, PanzerIV: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "PanzerIV:short": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, StuG3: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hummel: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hanomag: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SdKfz222: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, T34: { front: [1, 1, 1, 1, 0.97], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "T34-85": { front: [1, 1, 1, 1, 0.97], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, KV1: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SU85: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SU76: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Sherman: { front: [1, 1, 1, 1, 0.94], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M10: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M7: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M8: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Matilda: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Sexton: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, CharB1: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M13: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Semovente: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, L3: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Type97: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "Type97:47": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "7TP": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, LightTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, MediumTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, AssaultGun: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SelfPropelledGun: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, ArmouredCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_BreakthroughTank: { front: [1, 1, 0.91, 0.47, 0.09], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_TankHunterCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_FlameTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_AmphibiousTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_SnowTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_GliderTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, PanzerIV: { Tiger: { front: [0.93, 0.72, 0.24, 0.01, 0], side: [1, 1, 1, 0.96, 0.48], rear: [1, 1, 1, 0.92, 0.26] }, Panther: { front: [0.48, 0.37, 0.13, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, PanzerIV: { front: [1, 1, 1, 0.91, 0.47], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "PanzerIV:short": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, StuG3: { front: [1, 1, 1, 0.74, 0.13], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hummel: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hanomag: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SdKfz222: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, T34: { front: [1, 1, 0.89, 0.57, 0.38], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "T34-85": { front: [1, 1, 0.85, 0.34, 0.08], side: [1, 1, 1, 0.97, 0.73], rear: [1, 1, 1, 1, 1] }, KV1: { front: [1, 1, 0.95, 0.42, 0.02], side: [1, 1, 1, 1, 0.54], rear: [1, 1, 1, 1, 0.62] }, SU85: { front: [1, 1, 1, 1, 0.79], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 0.95] }, SU76: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Sherman: { front: [1, 1, 0.81, 0.34, 0.22], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M10: { front: [1, 1, 1, 0.89, 0.68], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M7: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M8: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Matilda: { front: [1, 1, 1, 0.93, 0.4], side: [1, 1, 1, 1, 0.81], rear: [1, 1, 1, 1, 0.87] }, Sexton: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, CharB1: { front: [1, 1, 0.99, 0.78, 0.59], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M13: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Semovente: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, L3: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Type97: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "Type97:47": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "7TP": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, LightTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, MediumTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, AssaultGun: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SelfPropelledGun: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, ArmouredCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_BreakthroughTank: { front: [0.77, 0.34, 0.03, 0, 0], side: [1, 1, 1, 0.96, 0.67], rear: [1, 1, 1, 1, 0.95] }, WhatIf_TankHunterCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_FlameTank: { front: [1, 1, 1, 1, 0.9], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_AmphibiousTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_SnowTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_GliderTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, "PanzerIV:short": { Tiger: { front: [0, 0, 0, 0, 0], side: [0.4, 0.4, 0, 0, 0], rear: [0.17, 0.17, 0, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, Panther: { front: [0, 0, 0, 0, 0], side: [1, 1, 0.26, 0.16, 0.03], rear: [1, 1, 0.05, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0.47, 0.41, 0.26, 0.16, 0.03], rear: [0.64, 0.4, 0.05, 0, 0] } }, PanzerIV: { front: [0.42, 0.42, 0, 0, 0], side: [1, 1, 1, 1, 0.96], rear: [1, 1, 1, 1, 1], before: { front: [0.1, 0.04, 0, 0, 0], side: [1, 1, 1, 1, 0.96], rear: [1, 1, 1, 1, 1] } }, "PanzerIV:short": { front: [1, 1, 0, 0, 0], side: [1, 1, 1, 1, 0.96], rear: [1, 1, 1, 1, 1], before: { front: [0.3, 0.12, 0, 0, 0], side: [1, 1, 1, 1, 0.96], rear: [1, 1, 1, 1, 1] } }, StuG3: { front: [0.07, 0.07, 0, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 0.3, 0.3, 0.3], before: { front: [0, 0, 0, 0, 0], side: [1, 1, 1, 1, 1], rear: [0.54, 0.4, 0.3, 0.3, 0.3] } }, Hummel: { front: [1, 1, 1, 1, 0.99], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 0.99], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, Hanomag: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, SdKfz222: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, T34: { front: [0.35, 0.35, 0, 0, 0], side: [1, 1, 0.07, 0.01, 0], rear: [1, 1, 0, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0.36, 0.25, 0.07, 0.01, 0], rear: [0.01, 0, 0, 0, 0] } }, "T34-85": { front: [0.05, 0.05, 0, 0, 0], side: [0.7, 0.7, 0.07, 0.01, 0], rear: [1, 1, 0, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0.32, 0.24, 0.07, 0.01, 0], rear: [0.06, 0.01, 0, 0, 0] } }, KV1: { front: [0.01, 0.01, 0, 0, 0], side: [0.43, 0.43, 0, 0, 0], rear: [0.51, 0.51, 0, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, SU85: { front: [0.7, 0.7, 0, 0, 0], side: [1, 1, 0.1, 0.01, 0], rear: [0.9, 0.9, 0, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0.76, 0.53, 0.1, 0.01, 0], rear: [0, 0, 0, 0, 0] } }, SU76: { front: [1, 1, 0.58, 0.53, 0.46], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [0.8, 0.72, 0.58, 0.53, 0.46], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, Sherman: { front: [0.22, 0.22, 0, 0, 0], side: [1, 1, 0.66, 0.49, 0.17], rear: [1, 1, 0.62, 0.44, 0.13], before: { front: [0, 0, 0, 0, 0], side: [0.79, 0.73, 0.66, 0.49, 0.17], rear: [0.8, 0.74, 0.62, 0.44, 0.13] } }, M10: { front: [0.64, 0.64, 0, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [0, 0, 0, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, M7: { front: [1, 1, 0.5, 0.5, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [0.5, 0.5, 0.5, 0.5, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, M8: { front: [1, 1, 1, 1, 0.99], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 0.99], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, Matilda: { front: [0.3, 0.3, 0, 0, 0], side: [0.71, 0.71, 0, 0, 0], rear: [0.83, 0.83, 0, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0.04, 0, 0, 0, 0] } }, Sexton: { front: [1, 1, 0.5, 0.5, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 0.97, 0.85, 0.62], before: { front: [0.5, 0.5, 0.5, 0.5, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 0.97, 0.85, 0.62] } }, CharB1: { front: [0.55, 0.55, 0, 0, 0], side: [1, 1, 0, 0, 0], rear: [1, 1, 0, 0, 0], before: { front: [0.01, 0, 0, 0, 0], side: [0.04, 0, 0, 0, 0], rear: [0.32, 0.15, 0, 0, 0] } }, M13: { front: [1, 1, 0.97, 0.88, 0.72], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 0.97, 0.88, 0.72], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, Semovente: { front: [1, 1, 0.5, 0.41, 0.4], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [0.93, 0.8, 0.5, 0.41, 0.4], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, L3: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, Type97: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, "Type97:47": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, "7TP": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, LightTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, MediumTank: { front: [1, 1, 0, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [0.11, 0.04, 0, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, AssaultGun: { front: [1, 1, 0, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [0, 0, 0, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, SelfPropelledGun: { front: [1, 1, 0.5, 0.5, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [0.5, 0.5, 0.5, 0.5, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, ArmouredCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, WhatIf_BreakthroughTank: { front: [0, 0, 0, 0, 0], side: [0.59, 0.59, 0, 0, 0], rear: [0.91, 0.91, 0, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0.22, 0.09, 0, 0, 0] } }, WhatIf_TankHunterCar: { front: [1, 1, 1, 1, 0.95], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 0.95], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, WhatIf_FlameTank: { front: [0.84, 0.84, 0, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [0, 0, 0, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, WhatIf_AmphibiousTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, WhatIf_SnowTank: { front: [1, 1, 0.64, 0.45, 0.18], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 0.96, 0.64, 0.45, 0.18], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, WhatIf_GliderTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } } }, StuG3: { Tiger: { front: [0.93, 0.72, 0.24, 0.01, 0], side: [1, 1, 1, 0.96, 0.48], rear: [1, 1, 1, 0.92, 0.26] }, Panther: { front: [0.48, 0.37, 0.13, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, PanzerIV: { front: [1, 1, 1, 0.91, 0.47], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "PanzerIV:short": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, StuG3: { front: [1, 1, 1, 0.74, 0.13], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hummel: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hanomag: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SdKfz222: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, T34: { front: [1, 1, 0.89, 0.57, 0.38], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "T34-85": { front: [1, 1, 0.85, 0.34, 0.08], side: [1, 1, 1, 0.97, 0.73], rear: [1, 1, 1, 1, 1] }, KV1: { front: [1, 1, 0.95, 0.42, 0.02], side: [1, 1, 1, 1, 0.54], rear: [1, 1, 1, 1, 0.62] }, SU85: { front: [1, 1, 1, 1, 0.79], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 0.95] }, SU76: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Sherman: { front: [1, 1, 0.81, 0.34, 0.22], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M10: { front: [1, 1, 1, 0.89, 0.68], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M7: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M8: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Matilda: { front: [1, 1, 1, 0.93, 0.4], side: [1, 1, 1, 1, 0.81], rear: [1, 1, 1, 1, 0.87] }, Sexton: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, CharB1: { front: [1, 1, 0.99, 0.78, 0.59], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M13: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Semovente: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, L3: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Type97: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "Type97:47": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "7TP": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, LightTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, MediumTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, AssaultGun: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SelfPropelledGun: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, ArmouredCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_BreakthroughTank: { front: [0.77, 0.34, 0.03, 0, 0], side: [1, 1, 1, 0.96, 0.67], rear: [1, 1, 1, 1, 0.95] }, WhatIf_TankHunterCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_FlameTank: { front: [1, 1, 1, 1, 0.9], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_AmphibiousTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_SnowTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_GliderTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, SdKfz222: { Tiger: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, Panther: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, PanzerIV: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0.65, 0, 0, 0, 0] }, "PanzerIV:short": { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0.65, 0, 0, 0, 0] }, StuG3: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, Hummel: { front: [0.5, 0.5, 0, 0, 0], side: [1, 0.6, 0.41, 0, 0], rear: [1, 0.5, 0.4, 0, 0] }, Hanomag: { front: [1, 0.4, 0, 0, 0], side: [1, 1, 0.94, 0, 0], rear: [1, 1, 1, 0, 0] }, SdKfz222: { front: [1, 0.13, 0, 0, 0], side: [1, 1, 0.94, 0, 0], rear: [1, 1, 0.94, 0, 0] }, T34: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, "T34-85": { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, KV1: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, SU85: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, SU76: { front: [0, 0, 0, 0, 0], side: [1, 0.99, 0.33, 0, 0], rear: [1, 0.98, 0.34, 0, 0] }, Sherman: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, M10: { front: [0, 0, 0, 0, 0], side: [0.2, 0, 0, 0, 0], rear: [0.26, 0, 0, 0, 0] }, M7: { front: [0.5, 0.46, 0, 0, 0], side: [1, 1, 0, 0, 0], rear: [1, 1, 0, 0, 0] }, M8: { front: [0.35, 0, 0, 0, 0], side: [1, 0.65, 0.48, 0, 0], rear: [1, 0.66, 0.65, 0, 0] }, Matilda: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, Sexton: { front: [0.48, 0, 0, 0, 0], side: [1, 1, 0, 0, 0], rear: [0.5, 0.5, 0, 0, 0] }, CharB1: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, M13: { front: [0, 0, 0, 0, 0], side: [0.6, 0, 0, 0, 0], rear: [0.38, 0, 0, 0, 0] }, Semovente: { front: [0, 0, 0, 0, 0], side: [0.75, 0, 0, 0, 0], rear: [0.58, 0, 0, 0, 0] }, L3: { front: [1, 1, 0, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0] }, Type97: { front: [0.09, 0, 0, 0, 0], side: [0.36, 0, 0, 0, 0], rear: [0.75, 0, 0, 0, 0] }, "Type97:47": { front: [0.09, 0, 0, 0, 0], side: [0.36, 0, 0, 0, 0], rear: [0.75, 0, 0, 0, 0] }, "7TP": { front: [1, 0.4, 0, 0, 0], side: [1, 0.98, 0, 0, 0], rear: [1, 0.98, 0.6, 0, 0] }, LightTank: { front: [0.15, 0, 0, 0, 0], side: [1, 0.96, 0, 0, 0], rear: [1, 0.98, 0, 0, 0] }, MediumTank: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, AssaultGun: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0.11, 0, 0, 0, 0] }, SelfPropelledGun: { front: [0.5, 0.5, 0, 0, 0], side: [0.6, 0.6, 0.48, 0, 0], rear: [0.5, 0.5, 0.43, 0, 0] }, ArmouredCar: { front: [1, 0.34, 0, 0, 0], side: [1, 1, 0.52, 0, 0], rear: [1, 1, 0.65, 0, 0] }, WhatIf_BreakthroughTank: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, WhatIf_TankHunterCar: { front: [0, 0, 0, 0, 0], side: [1, 0.99, 0, 0, 0], rear: [1, 1, 0.57, 0, 0] }, WhatIf_FlameTank: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, WhatIf_AmphibiousTank: { front: [0, 0, 0, 0, 0], side: [1, 0.97, 0.05, 0, 0], rear: [1, 0.97, 0.56, 0, 0] }, WhatIf_SnowTank: { front: [0, 0, 0, 0, 0], side: [1, 0, 0, 0, 0], rear: [1, 0.63, 0, 0, 0] }, WhatIf_GliderTank: { front: [1, 0.59, 0, 0, 0], side: [1, 1, 0.61, 0, 0], rear: [1, 1, 0.66, 0, 0] } }, FlakHalftrack: { Tiger: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, Panther: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, PanzerIV: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0.65, 0, 0, 0, 0] }, "PanzerIV:short": { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0.65, 0, 0, 0, 0] }, StuG3: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, Hummel: { front: [0.5, 0.5, 0, 0, 0], side: [1, 0.6, 0.41, 0, 0], rear: [1, 0.5, 0.4, 0, 0] }, Hanomag: { front: [1, 0.4, 0, 0, 0], side: [1, 1, 0.94, 0, 0], rear: [1, 1, 1, 0, 0] }, SdKfz222: { front: [1, 0.13, 0, 0, 0], side: [1, 1, 0.94, 0, 0], rear: [1, 1, 0.94, 0, 0] }, T34: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, "T34-85": { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, KV1: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, SU85: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, SU76: { front: [0, 0, 0, 0, 0], side: [1, 0.99, 0.33, 0, 0], rear: [1, 0.98, 0.34, 0, 0] }, Sherman: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, M10: { front: [0, 0, 0, 0, 0], side: [0.2, 0, 0, 0, 0], rear: [0.26, 0, 0, 0, 0] }, M7: { front: [0.5, 0.46, 0, 0, 0], side: [1, 1, 0, 0, 0], rear: [1, 1, 0, 0, 0] }, M8: { front: [0.35, 0, 0, 0, 0], side: [1, 0.65, 0.48, 0, 0], rear: [1, 0.66, 0.65, 0, 0] }, Matilda: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, Sexton: { front: [0.48, 0, 0, 0, 0], side: [1, 1, 0, 0, 0], rear: [0.5, 0.5, 0, 0, 0] }, CharB1: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, M13: { front: [0, 0, 0, 0, 0], side: [0.6, 0, 0, 0, 0], rear: [0.38, 0, 0, 0, 0] }, Semovente: { front: [0, 0, 0, 0, 0], side: [0.75, 0, 0, 0, 0], rear: [0.58, 0, 0, 0, 0] }, L3: { front: [1, 1, 0, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0] }, Type97: { front: [0.09, 0, 0, 0, 0], side: [0.36, 0, 0, 0, 0], rear: [0.75, 0, 0, 0, 0] }, "Type97:47": { front: [0.09, 0, 0, 0, 0], side: [0.36, 0, 0, 0, 0], rear: [0.75, 0, 0, 0, 0] }, "7TP": { front: [1, 0.4, 0, 0, 0], side: [1, 0.98, 0, 0, 0], rear: [1, 0.98, 0.6, 0, 0] }, LightTank: { front: [0.15, 0, 0, 0, 0], side: [1, 0.96, 0, 0, 0], rear: [1, 0.98, 0, 0, 0] }, MediumTank: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, AssaultGun: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0.11, 0, 0, 0, 0] }, SelfPropelledGun: { front: [0.5, 0.5, 0, 0, 0], side: [0.6, 0.6, 0.48, 0, 0], rear: [0.5, 0.5, 0.43, 0, 0] }, ArmouredCar: { front: [1, 0.34, 0, 0, 0], side: [1, 1, 0.52, 0, 0], rear: [1, 1, 0.65, 0, 0] }, WhatIf_BreakthroughTank: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, WhatIf_TankHunterCar: { front: [0, 0, 0, 0, 0], side: [1, 0.99, 0, 0, 0], rear: [1, 1, 0.57, 0, 0] }, WhatIf_FlameTank: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, WhatIf_AmphibiousTank: { front: [0, 0, 0, 0, 0], side: [1, 0.97, 0.05, 0, 0], rear: [1, 0.97, 0.56, 0, 0] }, WhatIf_SnowTank: { front: [0, 0, 0, 0, 0], side: [1, 0, 0, 0, 0], rear: [1, 0.63, 0, 0, 0] }, WhatIf_GliderTank: { front: [1, 0.59, 0, 0, 0], side: [1, 1, 0.61, 0, 0], rear: [1, 1, 0.66, 0, 0] } }, Pak40: { Tiger: { front: [0.94, 0.72, 0.24, 0.01, 0], side: [1, 1, 1, 0.96, 0.48], rear: [1, 1, 1, 0.92, 0.26] }, Panther: { front: [0.49, 0.37, 0.13, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, PanzerIV: { front: [1, 1, 1, 0.91, 0.47], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "PanzerIV:short": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, StuG3: { front: [1, 1, 1, 0.74, 0.13], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hummel: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hanomag: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SdKfz222: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, T34: { front: [1, 1, 0.89, 0.57, 0.38], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "T34-85": { front: [1, 1, 0.85, 0.34, 0.08], side: [1, 1, 1, 0.97, 0.73], rear: [1, 1, 1, 1, 1] }, KV1: { front: [1, 1, 0.95, 0.42, 0.02], side: [1, 1, 1, 1, 0.54], rear: [1, 1, 1, 1, 0.62] }, SU85: { front: [1, 1, 1, 1, 0.79], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 0.95] }, SU76: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Sherman: { front: [1, 1, 0.81, 0.34, 0.22], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M10: { front: [1, 1, 1, 0.89, 0.68], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M7: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M8: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Matilda: { front: [1, 1, 1, 0.93, 0.4], side: [1, 1, 1, 1, 0.81], rear: [1, 1, 1, 1, 0.87] }, Sexton: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, CharB1: { front: [1, 1, 0.99, 0.78, 0.59], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M13: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Semovente: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, L3: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Type97: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "Type97:47": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "7TP": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, LightTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, MediumTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, AssaultGun: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SelfPropelledGun: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, ArmouredCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_BreakthroughTank: { front: [0.83, 0.34, 0.03, 0, 0], side: [1, 1, 1, 0.96, 0.67], rear: [1, 1, 1, 1, 0.95] }, WhatIf_TankHunterCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_FlameTank: { front: [1, 1, 1, 1, 0.9], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_AmphibiousTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_SnowTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_GliderTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, leFH18: { Tiger: { front: [0.19, 0.19, 0.19, 0, 0], side: [1, 1, 1, 0.03, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0.24, 0.17, 0.1, 0.03, 0], rear: [0, 0, 0, 0, 0] } }, Panther: { front: [0.13, 0.13, 0.13, 0, 0], side: [1, 1, 1, 0.73, 0.57], rear: [1, 1, 1, 0.98, 0.87], before: { front: [0, 0, 0, 0, 0], side: [1, 0.95, 0.85, 0.73, 0.57], rear: [1, 1, 1, 0.98, 0.87] } }, PanzerIV: { front: [1, 1, 1, 0.27, 0.17], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [0.33, 0.32, 0.3, 0.27, 0.17], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, "PanzerIV:short": { front: [1, 1, 1, 0.83, 0.53], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [0.98, 0.97, 0.95, 0.83, 0.53], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, StuG3: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 0.92, 0.7], before: { front: [0, 0, 0, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 0.92, 0.7] } }, Hummel: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, Hanomag: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, SdKfz222: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, T34: { front: [0.96, 0.96, 0.96, 0, 0], side: [1, 1, 1, 0.74, 0.53], rear: [1, 1, 1, 0.32, 0.09], before: { front: [0.23, 0.11, 0.04, 0, 0], side: [1, 0.99, 0.92, 0.74, 0.53], rear: [0.99, 0.86, 0.61, 0.32, 0.09] } }, "T34-85": { front: [0.9, 0.9, 0.9, 0, 0], side: [1, 1, 1, 0.41, 0.36], rear: [1, 1, 1, 0.41, 0.16], before: { front: [0.01, 0, 0, 0, 0], side: [0.65, 0.58, 0.5, 0.41, 0.36], rear: [0.99, 0.87, 0.68, 0.41, 0.16] } }, KV1: { front: [0.93, 0.93, 0.93, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0.04, 0, 0, 0, 0], rear: [0.1, 0, 0, 0, 0] } }, SU85: { front: [1, 1, 1, 0.03, 0], side: [1, 1, 1, 1, 0.92], rear: [1, 1, 1, 0, 0], before: { front: [0.42, 0.17, 0.08, 0.03, 0], side: [1, 1, 1, 1, 0.92], rear: [0.58, 0.24, 0.06, 0, 0] } }, SU76: { front: [1, 1, 1, 0.98, 0.88], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 0.98, 0.88], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, Sherman: { front: [0.87, 0.87, 0.87, 0.04, 0], side: [1, 1, 1, 0.96, 0.86], rear: [1, 1, 1, 0.96, 0.87], before: { front: [0.22, 0.17, 0.1, 0.04, 0], side: [1, 1, 1, 0.96, 0.86], rear: [1, 1, 1, 0.96, 0.87] } }, M10: { front: [1, 1, 1, 0.04, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [0.48, 0.27, 0.12, 0.04, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, M7: { front: [1, 1, 1, 0.59, 0.51], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 0.88, 0.73, 0.59, 0.51], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, M8: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, Matilda: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0.34, 0.14], before: { front: [0.02, 0, 0, 0, 0], side: [0.22, 0.03, 0, 0, 0], rear: [0.67, 0.65, 0.53, 0.34, 0.14] } }, Sexton: { front: [1, 1, 1, 0.59, 0.51], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 0.88, 0.73, 0.59, 0.51], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, CharB1: { front: [1, 1, 1, 0.11, 0.04], side: [1, 1, 1, 0.41, 0.16], rear: [1, 1, 1, 0.73, 0.49], before: { front: [0.37, 0.26, 0.19, 0.11, 0.04], side: [0.99, 0.9, 0.7, 0.41, 0.16], rear: [0.99, 0.91, 0.84, 0.73, 0.49] } }, M13: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, Semovente: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, L3: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, Type97: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, "Type97:47": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, "7TP": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, LightTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, MediumTank: { front: [1, 1, 1, 0.47, 0.22], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 0.9, 0.72, 0.47, 0.22], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, AssaultGun: { front: [1, 1, 1, 0.11, 0.02], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [0.84, 0.55, 0.3, 0.11, 0.02], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, SelfPropelledGun: { front: [1, 1, 1, 0.63, 0.52], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 0.92, 0.78, 0.63, 0.52], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, ArmouredCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, WhatIf_BreakthroughTank: { front: [0.02, 0.02, 0.02, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0.58, 0.38], before: { front: [0, 0, 0, 0, 0], side: [0.2, 0.03, 0, 0, 0], rear: [0.73, 0.66, 0.65, 0.58, 0.38] } }, WhatIf_TankHunterCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, WhatIf_FlameTank: { front: [1, 1, 1, 0.02, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [0.51, 0.23, 0.1, 0.02, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, WhatIf_AmphibiousTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, WhatIf_SnowTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, WhatIf_GliderTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } } }, T34: { Tiger: { front: [0, 0, 0, 0, 0], side: [0.59, 0.28, 0.13, 0, 0], rear: [0.4, 0.02, 0, 0, 0] }, Panther: { front: [0, 0, 0, 0, 0], side: [1, 1, 0.86, 0.55, 0.34], rear: [1, 1, 1, 0.84, 0.19] }, PanzerIV: { front: [0.56, 0.35, 0.31, 0.17, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "PanzerIV:short": { front: [1, 1, 0.96, 0.52, 0.01], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, StuG3: { front: [0.23, 0.01, 0, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 0.7, 0.31] }, Hummel: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hanomag: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SdKfz222: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, T34: { front: [0.41, 0.29, 0.05, 0, 0], side: [1, 1, 0.94, 0.5, 0.15], rear: [1, 1, 0.62, 0.07, 0] }, "T34-85": { front: [0.11, 0.02, 0, 0, 0], side: [0.78, 0.65, 0.49, 0.35, 0.15], rear: [1, 1, 0.68, 0.15, 0] }, KV1: { front: [0.05, 0, 0, 0, 0], side: [0.69, 0.18, 0, 0, 0], rear: [0.76, 0.25, 0, 0, 0] }, SU85: { front: [0.9, 0.48, 0.1, 0, 0], side: [1, 1, 1, 0.91, 0.29], rear: [0.99, 0.68, 0.05, 0, 0] }, SU76: { front: [1, 1, 1, 0.88, 0.65], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Sherman: { front: [0.22, 0.22, 0.11, 0, 0], side: [1, 1, 1, 0.86, 0.7], rear: [1, 1, 1, 0.87, 0.7] }, M10: { front: [0.72, 0.53, 0.12, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M7: { front: [1, 1, 0.75, 0.5, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M8: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Matilda: { front: [0.54, 0.12, 0, 0, 0], side: [0.91, 0.44, 0, 0, 0], rear: [0.92, 0.73, 0.58, 0.14, 0] }, Sexton: { front: [1, 1, 0.75, 0.5, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, CharB1: { front: [0.64, 0.44, 0.21, 0.04, 0], side: [1, 1, 0.78, 0.16, 0], rear: [1, 1, 0.85, 0.49, 0.03] }, M13: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Semovente: { front: [1, 1, 1, 1, 0.65], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, L3: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Type97: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "Type97:47": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "7TP": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, LightTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, MediumTank: { front: [1, 1, 0.74, 0.2, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, AssaultGun: { front: [1, 0.95, 0.34, 0.01, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SelfPropelledGun: { front: [1, 1, 0.8, 0.51, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, ArmouredCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_BreakthroughTank: { front: [0, 0, 0, 0, 0], side: [0.77, 0.37, 0, 0, 0], rear: [0.98, 0.81, 0.65, 0.37, 0.01] }, WhatIf_TankHunterCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_FlameTank: { front: [0.97, 0.65, 0.13, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_AmphibiousTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_SnowTank: { front: [1, 1, 1, 1, 0.82], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_GliderTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, "T34-85": { Tiger: { front: [0.77, 0.52, 0.17, 0.01, 0], side: [1, 1, 1, 0.99, 0.75], rear: [1, 1, 1, 0.96, 0.62] }, Panther: { front: [0.42, 0.29, 0.1, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, PanzerIV: { front: [1, 1, 1, 0.94, 0.7], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "PanzerIV:short": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, StuG3: { front: [1, 1, 1, 0.81, 0.43], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hummel: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hanomag: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SdKfz222: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, T34: { front: [1, 1, 0.89, 0.64, 0.48], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "T34-85": { front: [1, 1, 0.82, 0.43, 0.19], side: [1, 1, 1, 0.98, 0.86], rear: [1, 1, 1, 1, 1] }, KV1: { front: [1, 1, 0.9, 0.5, 0.17], side: [1, 1, 1, 1, 0.88], rear: [1, 1, 1, 1, 0.92] }, SU85: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SU76: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Sherman: { front: [1, 0.99, 0.78, 0.41, 0.24], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M10: { front: [1, 1, 1, 0.94, 0.8], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M7: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M8: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Matilda: { front: [1, 1, 1, 0.96, 0.74], side: [1, 1, 1, 1, 0.98], rear: [1, 1, 1, 1, 0.98] }, Sexton: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, CharB1: { front: [1, 1, 0.99, 0.84, 0.71], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M13: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Semovente: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, L3: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Type97: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "Type97:47": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "7TP": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, LightTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, MediumTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, AssaultGun: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SelfPropelledGun: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, ArmouredCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_BreakthroughTank: { front: [0.41, 0.16, 0.01, 0, 0], side: [1, 1, 1, 0.98, 0.85], rear: [1, 1, 1, 1, 1] }, WhatIf_TankHunterCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_FlameTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_AmphibiousTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_SnowTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_GliderTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, KV1: { Tiger: { front: [0, 0, 0, 0, 0], side: [0.59, 0.28, 0.13, 0, 0], rear: [0.4, 0.02, 0, 0, 0] }, Panther: { front: [0, 0, 0, 0, 0], side: [1, 1, 0.86, 0.55, 0.34], rear: [1, 1, 1, 0.84, 0.19] }, PanzerIV: { front: [0.56, 0.35, 0.31, 0.17, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "PanzerIV:short": { front: [1, 1, 0.96, 0.52, 0.01], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, StuG3: { front: [0.23, 0.01, 0, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 0.7, 0.31] }, Hummel: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hanomag: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SdKfz222: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, T34: { front: [0.41, 0.29, 0.05, 0, 0], side: [1, 1, 0.94, 0.5, 0.15], rear: [1, 1, 0.62, 0.07, 0] }, "T34-85": { front: [0.11, 0.02, 0, 0, 0], side: [0.78, 0.65, 0.49, 0.35, 0.15], rear: [1, 1, 0.68, 0.15, 0] }, KV1: { front: [0.05, 0, 0, 0, 0], side: [0.69, 0.18, 0, 0, 0], rear: [0.76, 0.25, 0, 0, 0] }, SU85: { front: [0.9, 0.48, 0.1, 0, 0], side: [1, 1, 1, 0.91, 0.29], rear: [0.99, 0.68, 0.05, 0, 0] }, SU76: { front: [1, 1, 1, 0.88, 0.65], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Sherman: { front: [0.22, 0.22, 0.11, 0, 0], side: [1, 1, 1, 0.86, 0.7], rear: [1, 1, 1, 0.87, 0.7] }, M10: { front: [0.72, 0.53, 0.12, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M7: { front: [1, 1, 0.75, 0.5, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M8: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Matilda: { front: [0.54, 0.12, 0, 0, 0], side: [0.91, 0.44, 0, 0, 0], rear: [0.92, 0.73, 0.58, 0.14, 0] }, Sexton: { front: [1, 1, 0.75, 0.5, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, CharB1: { front: [0.64, 0.44, 0.21, 0.04, 0], side: [1, 1, 0.78, 0.16, 0], rear: [1, 1, 0.85, 0.49, 0.03] }, M13: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Semovente: { front: [1, 1, 1, 1, 0.65], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, L3: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Type97: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "Type97:47": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "7TP": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, LightTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, MediumTank: { front: [1, 1, 0.74, 0.2, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, AssaultGun: { front: [1, 0.95, 0.34, 0.01, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SelfPropelledGun: { front: [1, 1, 0.8, 0.51, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, ArmouredCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_BreakthroughTank: { front: [0, 0, 0, 0, 0], side: [0.77, 0.37, 0, 0, 0], rear: [0.98, 0.81, 0.65, 0.37, 0.01] }, WhatIf_TankHunterCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_FlameTank: { front: [0.97, 0.65, 0.13, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_AmphibiousTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_SnowTank: { front: [1, 1, 1, 1, 0.82], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_GliderTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, SU85: { Tiger: { front: [0.77, 0.52, 0.17, 0.01, 0], side: [1, 1, 1, 0.99, 0.75], rear: [1, 1, 1, 0.96, 0.62] }, Panther: { front: [0.42, 0.29, 0.1, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, PanzerIV: { front: [1, 1, 1, 0.94, 0.7], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "PanzerIV:short": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, StuG3: { front: [1, 1, 1, 0.81, 0.43], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hummel: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hanomag: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SdKfz222: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, T34: { front: [1, 1, 0.89, 0.64, 0.48], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "T34-85": { front: [1, 1, 0.82, 0.43, 0.19], side: [1, 1, 1, 0.98, 0.86], rear: [1, 1, 1, 1, 1] }, KV1: { front: [1, 1, 0.9, 0.5, 0.17], side: [1, 1, 1, 1, 0.88], rear: [1, 1, 1, 1, 0.92] }, SU85: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SU76: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Sherman: { front: [1, 0.99, 0.78, 0.41, 0.24], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M10: { front: [1, 1, 1, 0.94, 0.8], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M7: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M8: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Matilda: { front: [1, 1, 1, 0.96, 0.74], side: [1, 1, 1, 1, 0.98], rear: [1, 1, 1, 1, 0.98] }, Sexton: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, CharB1: { front: [1, 1, 0.99, 0.84, 0.71], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M13: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Semovente: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, L3: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Type97: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "Type97:47": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "7TP": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, LightTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, MediumTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, AssaultGun: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SelfPropelledGun: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, ArmouredCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_BreakthroughTank: { front: [0.41, 0.16, 0.01, 0, 0], side: [1, 1, 1, 0.98, 0.85], rear: [1, 1, 1, 1, 1] }, WhatIf_TankHunterCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_FlameTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_AmphibiousTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_SnowTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_GliderTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, SU76: { Tiger: { front: [0, 0, 0, 0, 0], side: [0.7, 0.34, 0.17, 0.02, 0], rear: [0.55, 0.09, 0, 0, 0] }, Panther: { front: [0, 0, 0, 0, 0], side: [1, 1, 0.93, 0.65, 0.38], rear: [1, 1, 1, 0.95, 0.29] }, PanzerIV: { front: [0.65, 0.38, 0.32, 0.24, 0.02], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "PanzerIV:short": { front: [1, 1, 0.97, 0.73, 0.05], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, StuG3: { front: [0.35, 0.04, 0, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 0.85, 0.35] }, Hummel: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hanomag: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SdKfz222: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, T34: { front: [0.44, 0.33, 0.1, 0, 0], side: [1, 1, 0.99, 0.63, 0.2], rear: [1, 1, 0.79, 0.18, 0] }, "T34-85": { front: [0.14, 0.04, 0, 0, 0], side: [0.83, 0.68, 0.55, 0.37, 0.2], rear: [1, 1, 0.8, 0.27, 0] }, KV1: { front: [0.11, 0, 0, 0, 0], side: [0.82, 0.31, 0, 0, 0], rear: [0.87, 0.39, 0, 0, 0] }, SU85: { front: [0.97, 0.61, 0.14, 0.01, 0], side: [1, 1, 1, 0.99, 0.41], rear: [1, 0.82, 0.13, 0, 0] }, SU76: { front: [1, 1, 1, 0.95, 0.68], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Sherman: { front: [0.23, 0.22, 0.15, 0.02, 0], side: [1, 1, 1, 0.93, 0.71], rear: [1, 1, 1, 0.93, 0.72] }, M10: { front: [0.76, 0.6, 0.21, 0.02, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M7: { front: [1, 1, 0.85, 0.54, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M8: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Matilda: { front: [0.67, 0.21, 0, 0, 0], side: [0.96, 0.6, 0.03, 0, 0], rear: [0.96, 0.78, 0.65, 0.27, 0] }, Sexton: { front: [1, 1, 0.85, 0.54, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, CharB1: { front: [0.67, 0.5, 0.25, 0.08, 0], side: [1, 1, 0.89, 0.32, 0], rear: [1, 1, 0.9, 0.65, 0.08] }, M13: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Semovente: { front: [1, 1, 1, 1, 0.73], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, L3: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Type97: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "Type97:47": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "7TP": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, LightTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, MediumTank: { front: [1, 1, 0.86, 0.35, 0.02], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, AssaultGun: { front: [1, 0.99, 0.51, 0.05, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SelfPropelledGun: { front: [1, 1, 0.89, 0.57, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, ArmouredCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_BreakthroughTank: { front: [0, 0, 0, 0, 0], side: [0.82, 0.49, 0.03, 0, 0], rear: [1, 0.87, 0.66, 0.51, 0.04] }, WhatIf_TankHunterCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_FlameTank: { front: [1, 0.76, 0.21, 0.01, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_AmphibiousTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_SnowTank: { front: [1, 1, 1, 1, 0.9], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_GliderTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, ZiS3: { Tiger: { front: [0, 0, 0, 0, 0], side: [0.7, 0.34, 0.17, 0.02, 0], rear: [0.55, 0.09, 0, 0, 0] }, Panther: { front: [0, 0, 0, 0, 0], side: [1, 1, 0.93, 0.65, 0.38], rear: [1, 1, 1, 0.95, 0.29] }, PanzerIV: { front: [0.65, 0.38, 0.32, 0.24, 0.02], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "PanzerIV:short": { front: [1, 1, 0.97, 0.73, 0.05], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, StuG3: { front: [0.35, 0.04, 0, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 0.85, 0.35] }, Hummel: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hanomag: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SdKfz222: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, T34: { front: [0.44, 0.33, 0.1, 0, 0], side: [1, 1, 0.99, 0.63, 0.2], rear: [1, 1, 0.79, 0.18, 0] }, "T34-85": { front: [0.14, 0.04, 0, 0, 0], side: [0.83, 0.68, 0.55, 0.37, 0.2], rear: [1, 1, 0.8, 0.27, 0] }, KV1: { front: [0.11, 0, 0, 0, 0], side: [0.82, 0.31, 0, 0, 0], rear: [0.87, 0.39, 0, 0, 0] }, SU85: { front: [0.97, 0.61, 0.14, 0.01, 0], side: [1, 1, 1, 0.99, 0.41], rear: [1, 0.82, 0.13, 0, 0] }, SU76: { front: [1, 1, 1, 0.95, 0.68], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Sherman: { front: [0.23, 0.22, 0.15, 0.02, 0], side: [1, 1, 1, 0.93, 0.71], rear: [1, 1, 1, 0.93, 0.72] }, M10: { front: [0.76, 0.6, 0.21, 0.02, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M7: { front: [1, 1, 0.85, 0.54, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M8: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Matilda: { front: [0.67, 0.21, 0, 0, 0], side: [0.96, 0.6, 0.03, 0, 0], rear: [0.96, 0.78, 0.65, 0.27, 0] }, Sexton: { front: [1, 1, 0.85, 0.54, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, CharB1: { front: [0.67, 0.5, 0.25, 0.08, 0], side: [1, 1, 0.89, 0.32, 0], rear: [1, 1, 0.9, 0.65, 0.08] }, M13: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Semovente: { front: [1, 1, 1, 1, 0.73], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, L3: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Type97: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "Type97:47": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "7TP": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, LightTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, MediumTank: { front: [1, 1, 0.86, 0.35, 0.02], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, AssaultGun: { front: [1, 0.99, 0.51, 0.05, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SelfPropelledGun: { front: [1, 1, 0.89, 0.57, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, ArmouredCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_BreakthroughTank: { front: [0, 0, 0, 0, 0], side: [0.82, 0.49, 0.03, 0, 0], rear: [1, 0.87, 0.66, 0.51, 0.04] }, WhatIf_TankHunterCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_FlameTank: { front: [1, 0.76, 0.21, 0.01, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_AmphibiousTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_SnowTank: { front: [1, 1, 1, 1, 0.9], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_GliderTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, Sherman: { Tiger: { front: [0, 0, 0, 0, 0], side: [0.9, 0.53, 0.28, 0.22, 0.1], rear: [0.82, 0.33, 0.02, 0, 0] }, Panther: { front: [0, 0, 0, 0, 0], side: [1, 1, 1, 0.98, 0.82], rear: [1, 1, 1, 1, 1] }, PanzerIV: { front: [0.83, 0.51, 0.35, 0.33, 0.3], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "PanzerIV:short": { front: [1, 1, 1, 0.98, 0.95], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, StuG3: { front: [0.62, 0.18, 0.01, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hummel: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hanomag: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SdKfz222: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, T34: { front: [0.52, 0.39, 0.29, 0.15, 0.03], side: [1, 1, 1, 1, 0.89], rear: [1, 1, 1, 0.9, 0.53] }, "T34-85": { front: [0.25, 0.09, 0.01, 0, 0], side: [0.93, 0.76, 0.65, 0.6, 0.46], rear: [1, 1, 1, 0.9, 0.6] }, KV1: { front: [0.3, 0.03, 0, 0, 0], side: [0.98, 0.62, 0.18, 0, 0], rear: [0.98, 0.69, 0.25, 0.03, 0] }, SU85: { front: [1, 0.85, 0.47, 0.2, 0.08], side: [1, 1, 1, 1, 1], rear: [1, 0.98, 0.67, 0.26, 0.02] }, SU76: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Sherman: { front: [0.28, 0.22, 0.22, 0.19, 0.09], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M10: { front: [0.85, 0.7, 0.53, 0.3, 0.09], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M7: { front: [1, 1, 1, 0.93, 0.7], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M8: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Matilda: { front: [0.87, 0.47, 0.12, 0, 0], side: [1, 0.87, 0.44, 0.11, 0], rear: [1, 0.89, 0.73, 0.65, 0.53] }, Sexton: { front: [1, 1, 1, 0.93, 0.7], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, CharB1: { front: [0.74, 0.61, 0.44, 0.29, 0.19], side: [1, 1, 1, 0.94, 0.69], rear: [1, 1, 1, 0.94, 0.83] }, M13: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Semovente: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, L3: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Type97: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "Type97:47": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "7TP": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, LightTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, MediumTank: { front: [1, 1, 1, 0.95, 0.67], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, AssaultGun: { front: [1, 1, 0.95, 0.68, 0.26], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SelfPropelledGun: { front: [1, 1, 1, 0.96, 0.75], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, ArmouredCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_BreakthroughTank: { front: [0, 0, 0, 0, 0], side: [0.92, 0.72, 0.37, 0.1, 0], rear: [1, 0.97, 0.81, 0.68, 0.65] }, WhatIf_TankHunterCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_FlameTank: { front: [1, 0.94, 0.65, 0.33, 0.1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_AmphibiousTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_SnowTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_GliderTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, M10: { Tiger: { front: [0.79, 0.56, 0.33, 0.11, 0.01], side: [1, 1, 1, 1, 0.96], rear: [1, 1, 1, 1, 0.92] }, Panther: { front: [0.41, 0.3, 0.18, 0.07, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, PanzerIV: { front: [1, 1, 1, 1, 0.91], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "PanzerIV:short": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, StuG3: { front: [1, 1, 1, 0.98, 0.74], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hummel: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hanomag: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SdKfz222: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, T34: { front: [1, 1, 0.94, 0.78, 0.57], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "T34-85": { front: [1, 1, 0.93, 0.68, 0.34], side: [1, 1, 1, 1, 0.97], rear: [1, 1, 1, 1, 1] }, KV1: { front: [1, 1, 0.98, 0.81, 0.42], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SU85: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SU76: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Sherman: { front: [1, 0.99, 0.89, 0.64, 0.34], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M10: { front: [1, 1, 1, 1, 0.9], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M7: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M8: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Matilda: { front: [1, 1, 1, 1, 0.93], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Sexton: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, CharB1: { front: [1, 1, 1, 0.94, 0.79], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M13: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Semovente: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, L3: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Type97: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "Type97:47": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "7TP": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, LightTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, MediumTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, AssaultGun: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SelfPropelledGun: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, ArmouredCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_BreakthroughTank: { front: [0.43, 0.18, 0.06, 0, 0], side: [1, 1, 1, 1, 0.96], rear: [1, 1, 1, 1, 1] }, WhatIf_TankHunterCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_FlameTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_AmphibiousTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_SnowTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_GliderTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, M7: { Tiger: { front: [0.26, 0.26, 0.26, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, Panther: { front: [0.17, 0.17, 0.17, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, PanzerIV: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, "PanzerIV:short": { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, StuG3: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, Hummel: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, Hanomag: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, SdKfz222: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, T34: { front: [0.99, 0.99, 0.99, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, "T34-85": { front: [0.95, 0.95, 0.95, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, KV1: { front: [0.96, 0.96, 0.96, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, SU85: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, SU76: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, Sherman: { front: [0.93, 0.93, 0.93, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, M10: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, M7: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, M8: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, Matilda: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, Sexton: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, CharB1: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, M13: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, Semovente: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, L3: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, Type97: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, "Type97:47": { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, "7TP": { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, LightTank: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, MediumTank: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, AssaultGun: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, SelfPropelledGun: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, ArmouredCar: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, WhatIf_BreakthroughTank: { front: [0.03, 0.03, 0.03, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, WhatIf_TankHunterCar: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, WhatIf_FlameTank: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, WhatIf_AmphibiousTank: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, WhatIf_SnowTank: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, WhatIf_GliderTank: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } } }, M3Halftrack: { Tiger: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, Panther: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, PanzerIV: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0.65, 0.02, 0, 0, 0] }, "PanzerIV:short": { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0.65, 0.02, 0, 0, 0] }, StuG3: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, Hummel: { front: [0.5, 0.5, 0, 0, 0], side: [1, 0.63, 0.07, 0, 0], rear: [1, 0.51, 0.11, 0, 0] }, Hanomag: { front: [1, 0.6, 0, 0, 0], side: [1, 1, 0.31, 0, 0], rear: [1, 1, 0.61, 0, 0] }, SdKfz222: { front: [1, 0.23, 0, 0, 0], side: [1, 1, 0.31, 0, 0], rear: [1, 1, 0.31, 0, 0] }, T34: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, "T34-85": { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, KV1: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, SU85: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, SU76: { front: [0, 0, 0, 0, 0], side: [1, 1, 0.05, 0, 0], rear: [1, 1, 0.12, 0, 0] }, Sherman: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, M10: { front: [0, 0, 0, 0, 0], side: [0.14, 0, 0, 0, 0], rear: [0.21, 0, 0, 0, 0] }, M7: { front: [0.5, 0.5, 0, 0, 0], side: [1, 1, 0, 0, 0], rear: [1, 1, 0, 0, 0] }, M8: { front: [0.35, 0.03, 0, 0, 0], side: [1, 0.68, 0.1, 0, 0], rear: [1, 0.74, 0.36, 0, 0] }, Matilda: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, Sexton: { front: [0.45, 0, 0, 0, 0], side: [1, 1, 0, 0, 0], rear: [0.5, 0.5, 0, 0, 0] }, CharB1: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, M13: { front: [0, 0, 0, 0, 0], side: [0.59, 0, 0, 0, 0], rear: [0.36, 0, 0, 0, 0] }, Semovente: { front: [0, 0, 0, 0, 0], side: [0.75, 0, 0, 0, 0], rear: [0.56, 0, 0, 0, 0] }, L3: { front: [1, 1, 0, 0, 0], side: [1, 1, 0.81, 0, 0], rear: [1, 1, 0.81, 0, 0] }, Type97: { front: [0.09, 0, 0, 0, 0], side: [0.36, 0.02, 0, 0, 0], rear: [0.75, 0, 0, 0, 0] }, "Type97:47": { front: [0.09, 0, 0, 0, 0], side: [0.36, 0.02, 0, 0, 0], rear: [0.75, 0, 0, 0, 0] }, "7TP": { front: [1, 0.57, 0, 0, 0], side: [1, 1, 0, 0, 0], rear: [1, 1, 0.21, 0, 0] }, LightTank: { front: [0.14, 0, 0, 0, 0], side: [1, 1, 0, 0, 0], rear: [1, 1, 0, 0, 0] }, MediumTank: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, AssaultGun: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0.11, 0, 0, 0, 0] }, SelfPropelledGun: { front: [0.5, 0.5, 0, 0, 0], side: [0.6, 0.6, 0.14, 0, 0], rear: [0.5, 0.5, 0.15, 0, 0] }, ArmouredCar: { front: [1, 0.56, 0, 0, 0], side: [1, 1, 0.15, 0, 0], rear: [1, 1, 0.65, 0, 0] }, WhatIf_BreakthroughTank: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, WhatIf_TankHunterCar: { front: [0, 0, 0, 0, 0], side: [1, 1, 0, 0, 0], rear: [1, 1, 0.2, 0, 0] }, WhatIf_FlameTank: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, WhatIf_AmphibiousTank: { front: [0, 0, 0, 0, 0], side: [1, 1, 0, 0, 0], rear: [1, 1, 0.2, 0, 0] }, WhatIf_SnowTank: { front: [0, 0, 0, 0, 0], side: [1, 0.06, 0, 0, 0], rear: [1, 0.66, 0, 0, 0] }, WhatIf_GliderTank: { front: [1, 0.82, 0, 0, 0], side: [1, 1, 0.21, 0, 0], rear: [1, 1, 0.65, 0, 0] } }, M16: { Tiger: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, Panther: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, PanzerIV: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0.65, 0.02, 0, 0, 0] }, "PanzerIV:short": { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0.65, 0.02, 0, 0, 0] }, StuG3: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, Hummel: { front: [0.5, 0.5, 0, 0, 0], side: [1, 0.63, 0.07, 0, 0], rear: [1, 0.51, 0.11, 0, 0] }, Hanomag: { front: [1, 0.6, 0, 0, 0], side: [1, 1, 0.31, 0, 0], rear: [1, 1, 0.61, 0, 0] }, SdKfz222: { front: [1, 0.23, 0, 0, 0], side: [1, 1, 0.31, 0, 0], rear: [1, 1, 0.31, 0, 0] }, T34: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, "T34-85": { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, KV1: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, SU85: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, SU76: { front: [0, 0, 0, 0, 0], side: [1, 1, 0.05, 0, 0], rear: [1, 1, 0.12, 0, 0] }, Sherman: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, M10: { front: [0, 0, 0, 0, 0], side: [0.14, 0, 0, 0, 0], rear: [0.21, 0, 0, 0, 0] }, M7: { front: [0.5, 0.5, 0, 0, 0], side: [1, 1, 0, 0, 0], rear: [1, 1, 0, 0, 0] }, M8: { front: [0.35, 0.03, 0, 0, 0], side: [1, 0.68, 0.1, 0, 0], rear: [1, 0.74, 0.36, 0, 0] }, Matilda: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, Sexton: { front: [0.45, 0, 0, 0, 0], side: [1, 1, 0, 0, 0], rear: [0.5, 0.5, 0, 0, 0] }, CharB1: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, M13: { front: [0, 0, 0, 0, 0], side: [0.59, 0, 0, 0, 0], rear: [0.36, 0, 0, 0, 0] }, Semovente: { front: [0, 0, 0, 0, 0], side: [0.75, 0, 0, 0, 0], rear: [0.56, 0, 0, 0, 0] }, L3: { front: [1, 1, 0, 0, 0], side: [1, 1, 0.81, 0, 0], rear: [1, 1, 0.81, 0, 0] }, Type97: { front: [0.09, 0, 0, 0, 0], side: [0.36, 0.02, 0, 0, 0], rear: [0.75, 0, 0, 0, 0] }, "Type97:47": { front: [0.09, 0, 0, 0, 0], side: [0.36, 0.02, 0, 0, 0], rear: [0.75, 0, 0, 0, 0] }, "7TP": { front: [1, 0.57, 0, 0, 0], side: [1, 1, 0, 0, 0], rear: [1, 1, 0.21, 0, 0] }, LightTank: { front: [0.14, 0, 0, 0, 0], side: [1, 1, 0, 0, 0], rear: [1, 1, 0, 0, 0] }, MediumTank: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, AssaultGun: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0.11, 0, 0, 0, 0] }, SelfPropelledGun: { front: [0.5, 0.5, 0, 0, 0], side: [0.6, 0.6, 0.14, 0, 0], rear: [0.5, 0.5, 0.15, 0, 0] }, ArmouredCar: { front: [1, 0.56, 0, 0, 0], side: [1, 1, 0.15, 0, 0], rear: [1, 1, 0.65, 0, 0] }, WhatIf_BreakthroughTank: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, WhatIf_TankHunterCar: { front: [0, 0, 0, 0, 0], side: [1, 1, 0, 0, 0], rear: [1, 1, 0.2, 0, 0] }, WhatIf_FlameTank: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, WhatIf_AmphibiousTank: { front: [0, 0, 0, 0, 0], side: [1, 1, 0, 0, 0], rear: [1, 1, 0.2, 0, 0] }, WhatIf_SnowTank: { front: [0, 0, 0, 0, 0], side: [1, 0.06, 0, 0, 0], rear: [1, 0.66, 0, 0, 0] }, WhatIf_GliderTank: { front: [1, 0.82, 0, 0, 0], side: [1, 1, 0.21, 0, 0], rear: [1, 1, 0.65, 0, 0] } }, M8: { Tiger: { front: [0, 0, 0, 0, 0], side: [0.31, 0.17, 0.02, 0, 0], rear: [0.05, 0, 0, 0, 0] }, Panther: { front: [0, 0, 0, 0, 0], side: [1, 0.84, 0.57, 0.38, 0.21], rear: [1, 1, 0.87, 0.24, 0] }, PanzerIV: { front: [0.34, 0.3, 0.23, 0.04, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "PanzerIV:short": { front: [0.98, 0.95, 0.7, 0.1, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, StuG3: { front: [0.01, 0, 0, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 0.84, 0.38, 0.3] }, Hummel: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hanomag: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SdKfz222: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, T34: { front: [0.27, 0.07, 0, 0, 0], side: [1, 0.91, 0.48, 0.24, 0.03], rear: [0.89, 0.46, 0.08, 0, 0] }, "T34-85": { front: [0, 0, 0, 0, 0], side: [0.63, 0.43, 0.35, 0.24, 0.03], rear: [0.89, 0.49, 0.2, 0.01, 0] }, KV1: { front: [0, 0, 0, 0, 0], side: [0.24, 0, 0, 0, 0], rear: [0.29, 0, 0, 0, 0] }, SU85: { front: [0.2, 0.14, 0.01, 0, 0], side: [1, 1, 0.96, 0.48, 0.05], rear: [0.22, 0, 0, 0, 0] }, SU76: { front: [1, 1, 0.79, 0.64, 0.52], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Sherman: { front: [0.22, 0.1, 0, 0, 0], side: [1, 1, 0.93, 0.73, 0.59], rear: [1, 1, 0.93, 0.74, 0.53] }, M10: { front: [0.27, 0.1, 0, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M7: { front: [1, 0.72, 0.5, 0.5, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M8: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Matilda: { front: [0.16, 0, 0, 0, 0], side: [0.52, 0.03, 0, 0, 0], rear: [0.76, 0.65, 0.27, 0, 0] }, Sexton: { front: [1, 0.72, 0.5, 0.5, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 0.92] }, CharB1: { front: [0.39, 0.24, 0.08, 0, 0], side: [1, 0.86, 0.31, 0, 0], rear: [1, 0.87, 0.65, 0.15, 0] }, M13: { front: [1, 1, 1, 1, 0.92], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Semovente: { front: [1, 1, 1, 0.8, 0.45], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, L3: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Type97: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "Type97:47": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "7TP": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, LightTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, MediumTank: { front: [1, 0.71, 0.26, 0.03, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, AssaultGun: { front: [0.94, 0.37, 0.01, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SelfPropelledGun: { front: [1, 0.77, 0.51, 0.5, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, ArmouredCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_BreakthroughTank: { front: [0, 0, 0, 0, 0], side: [0.43, 0.03, 0, 0, 0], rear: [0.83, 0.65, 0.5, 0.08, 0] }, WhatIf_TankHunterCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_FlameTank: { front: [0.57, 0.19, 0, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_AmphibiousTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_SnowTank: { front: [1, 1, 1, 0.76, 0.39], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_GliderTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, Howitzer105: { Tiger: { front: [0.26, 0.26, 0.26, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, Panther: { front: [0.17, 0.17, 0.17, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, PanzerIV: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, "PanzerIV:short": { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, StuG3: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, Hummel: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, Hanomag: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, SdKfz222: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, T34: { front: [0.99, 0.99, 0.99, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, "T34-85": { front: [0.95, 0.95, 0.95, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, KV1: { front: [0.96, 0.96, 0.96, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, SU85: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, SU76: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, Sherman: { front: [0.93, 0.93, 0.93, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, M10: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, M7: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, M8: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, Matilda: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, Sexton: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, CharB1: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, M13: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, Semovente: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, L3: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, Type97: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, "Type97:47": { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, "7TP": { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, LightTank: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, MediumTank: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, AssaultGun: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, SelfPropelledGun: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, ArmouredCar: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, WhatIf_BreakthroughTank: { front: [0.03, 0.03, 0.03, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, WhatIf_TankHunterCar: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, WhatIf_FlameTank: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, WhatIf_AmphibiousTank: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, WhatIf_SnowTank: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, WhatIf_GliderTank: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } } }, Matilda: { Tiger: { front: [0, 0, 0, 0, 0], side: [0.2, 0.03, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, Panther: { front: [0, 0, 0, 0, 0], side: [0.89, 0.63, 0.36, 0.06, 0], rear: [1, 0.94, 0.17, 0, 0] }, PanzerIV: { front: [0.31, 0.26, 0.02, 0, 0], side: [1, 1, 1, 0.97, 0.36], rear: [1, 1, 1, 1, 0.76] }, "PanzerIV:short": { front: [0.96, 0.8, 0.04, 0, 0], side: [1, 1, 1, 0.97, 0.36], rear: [1, 1, 1, 1, 0.76] }, StuG3: { front: [0, 0, 0, 0, 0], side: [1, 1, 1, 1, 0.45], rear: [1, 0.9, 0.34, 0.3, 0.15] }, Hummel: { front: [1, 1, 1, 1, 0.59], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hanomag: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SdKfz222: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, T34: { front: [0.1, 0, 0, 0, 0], side: [0.97, 0.56, 0.2, 0, 0], rear: [0.57, 0.13, 0, 0, 0] }, "T34-85": { front: [0, 0, 0, 0, 0], side: [0.47, 0.35, 0.2, 0, 0], rear: [0.58, 0.25, 0, 0, 0] }, KV1: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, SU85: { front: [0.16, 0.03, 0, 0, 0], side: [1, 0.99, 0.37, 0, 0], rear: [0.01, 0, 0, 0, 0] }, SU76: { front: [1, 0.86, 0.63, 0.46, 0.34], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Sherman: { front: [0.13, 0.01, 0, 0, 0], side: [1, 0.95, 0.71, 0.28, 0], rear: [1, 0.96, 0.72, 0.21, 0] }, M10: { front: [0.13, 0.01, 0, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M7: { front: [0.79, 0.51, 0.5, 0.5, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M8: { front: [1, 1, 1, 1, 0.67], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Matilda: { front: [0, 0, 0, 0, 0], side: [0.06, 0, 0, 0, 0], rear: [0.65, 0.34, 0, 0, 0] }, Sexton: { front: [0.79, 0.51, 0.5, 0.5, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 0.7, 0.5] }, CharB1: { front: [0.25, 0.11, 0, 0, 0], side: [0.89, 0.4, 0, 0, 0], rear: [0.89, 0.71, 0.08, 0, 0] }, M13: { front: [1, 1, 1, 0.76, 0.41], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Semovente: { front: [1, 1, 0.72, 0.4, 0.26], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, L3: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Type97: { front: [1, 1, 1, 1, 0.98], side: [1, 1, 1, 1, 0.94], rear: [1, 1, 1, 1, 1] }, "Type97:47": { front: [1, 1, 1, 1, 0.98], side: [1, 1, 1, 1, 0.94], rear: [1, 1, 1, 1, 1] }, "7TP": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, LightTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, MediumTank: { front: [0.79, 0.34, 0.01, 0, 0], side: [1, 1, 1, 1, 0.44], rear: [1, 1, 1, 1, 0.37] }, AssaultGun: { front: [0.47, 0.03, 0, 0, 0], side: [1, 1, 1, 1, 0.46], rear: [1, 1, 1, 1, 0.62] }, SelfPropelledGun: { front: [0.84, 0.54, 0.5, 0.5, 0.5], side: [1, 1, 1, 1, 0.8], rear: [1, 1, 1, 1, 0.71] }, ArmouredCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_BreakthroughTank: { front: [0, 0, 0, 0, 0], side: [0.06, 0, 0, 0, 0], rear: [0.67, 0.56, 0.04, 0, 0] }, WhatIf_TankHunterCar: { front: [1, 1, 1, 0.96, 0.1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_FlameTank: { front: [0.22, 0.02, 0, 0, 0], side: [1, 1, 1, 1, 0.44], rear: [1, 1, 1, 1, 0.37] }, WhatIf_AmphibiousTank: { front: [1, 1, 1, 1, 0.59], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_SnowTank: { front: [1, 1, 0.71, 0.23, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_GliderTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, Sexton: { Tiger: { front: [0, 0, 0, 0, 0], side: [0.9, 0.59, 0.23, 0.03, 0], rear: [0.82, 0.4, 0, 0, 0] }, Panther: { front: [0, 0, 0, 0, 0], side: [1, 1, 1, 0.71, 0.38], rear: [1, 1, 1, 0.98, 0.31] }, PanzerIV: { front: [0.84, 0.56, 0.33, 0.26, 0.02], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "PanzerIV:short": { front: [1, 1, 0.98, 0.82, 0.05], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, StuG3: { front: [0.63, 0.24, 0, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 0.92, 0.35] }, Hummel: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hanomag: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SdKfz222: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, T34: { front: [0.57, 0.43, 0.19, 0, 0], side: [1, 1, 1, 0.72, 0.2], rear: [1, 1, 0.96, 0.29, 0] }, "T34-85": { front: [0.31, 0.13, 0, 0, 0], side: [0.93, 0.79, 0.63, 0.4, 0.2], rear: [1, 1, 0.96, 0.38, 0] }, KV1: { front: [0.33, 0.06, 0, 0, 0], side: [0.98, 0.69, 0.02, 0, 0], rear: [0.98, 0.77, 0.06, 0, 0] }, SU85: { front: [1, 0.95, 0.29, 0.03, 0], side: [1, 1, 1, 1, 0.41], rear: [1, 1, 0.41, 0, 0] }, SU76: { front: [1, 1, 1, 0.98, 0.69], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Sherman: { front: [0.31, 0.22, 0.21, 0.03, 0], side: [1, 1, 1, 0.96, 0.71], rear: [1, 1, 1, 0.96, 0.72] }, M10: { front: [0.88, 0.74, 0.4, 0.03, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M7: { front: [1, 1, 0.97, 0.58, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M8: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Matilda: { front: [0.89, 0.55, 0.01, 0, 0], side: [1, 0.91, 0.16, 0, 0], rear: [1, 0.92, 0.66, 0.34, 0] }, Sexton: { front: [1, 1, 0.97, 0.58, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, CharB1: { front: [0.77, 0.65, 0.33, 0.11, 0], side: [1, 1, 0.97, 0.41, 0], rear: [1, 1, 0.97, 0.73, 0.08] }, M13: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Semovente: { front: [1, 1, 1, 1, 0.73], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, L3: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Type97: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "Type97:47": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "7TP": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, LightTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, MediumTank: { front: [1, 1, 0.99, 0.45, 0.02], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, AssaultGun: { front: [1, 1, 0.77, 0.09, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SelfPropelledGun: { front: [1, 1, 0.99, 0.62, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, ArmouredCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_BreakthroughTank: { front: [0, 0, 0, 0, 0], side: [0.92, 0.77, 0.15, 0, 0], rear: [1, 0.98, 0.71, 0.57, 0.04] }, WhatIf_TankHunterCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_FlameTank: { front: [1, 0.98, 0.41, 0.02, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_AmphibiousTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_SnowTank: { front: [1, 1, 1, 1, 0.9], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_GliderTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, "25pdr": { Tiger: { front: [0, 0, 0, 0, 0], side: [0.9, 0.59, 0.23, 0.03, 0], rear: [0.82, 0.4, 0, 0, 0] }, Panther: { front: [0, 0, 0, 0, 0], side: [1, 1, 1, 0.71, 0.38], rear: [1, 1, 1, 0.98, 0.31] }, PanzerIV: { front: [0.84, 0.56, 0.33, 0.26, 0.02], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "PanzerIV:short": { front: [1, 1, 0.98, 0.82, 0.05], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, StuG3: { front: [0.63, 0.24, 0, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 0.92, 0.35] }, Hummel: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hanomag: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SdKfz222: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, T34: { front: [0.57, 0.43, 0.19, 0, 0], side: [1, 1, 1, 0.72, 0.2], rear: [1, 1, 0.96, 0.29, 0] }, "T34-85": { front: [0.31, 0.13, 0, 0, 0], side: [0.93, 0.79, 0.63, 0.4, 0.2], rear: [1, 1, 0.96, 0.38, 0] }, KV1: { front: [0.33, 0.06, 0, 0, 0], side: [0.98, 0.69, 0.02, 0, 0], rear: [0.98, 0.77, 0.06, 0, 0] }, SU85: { front: [1, 0.95, 0.29, 0.03, 0], side: [1, 1, 1, 1, 0.41], rear: [1, 1, 0.41, 0, 0] }, SU76: { front: [1, 1, 1, 0.98, 0.69], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Sherman: { front: [0.31, 0.22, 0.21, 0.03, 0], side: [1, 1, 1, 0.96, 0.71], rear: [1, 1, 1, 0.96, 0.72] }, M10: { front: [0.88, 0.74, 0.4, 0.03, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M7: { front: [1, 1, 0.97, 0.58, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M8: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Matilda: { front: [0.89, 0.55, 0.01, 0, 0], side: [1, 0.91, 0.16, 0, 0], rear: [1, 0.92, 0.66, 0.34, 0] }, Sexton: { front: [1, 1, 0.97, 0.58, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, CharB1: { front: [0.77, 0.65, 0.33, 0.11, 0], side: [1, 1, 0.97, 0.41, 0], rear: [1, 1, 0.97, 0.73, 0.08] }, M13: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Semovente: { front: [1, 1, 1, 1, 0.73], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, L3: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Type97: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "Type97:47": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "7TP": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, LightTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, MediumTank: { front: [1, 1, 0.99, 0.45, 0.02], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, AssaultGun: { front: [1, 1, 0.77, 0.09, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SelfPropelledGun: { front: [1, 1, 0.99, 0.62, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, ArmouredCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_BreakthroughTank: { front: [0, 0, 0, 0, 0], side: [0.92, 0.77, 0.15, 0, 0], rear: [1, 0.98, 0.71, 0.57, 0.04] }, WhatIf_TankHunterCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_FlameTank: { front: [1, 0.98, 0.41, 0.02, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_AmphibiousTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_SnowTank: { front: [1, 1, 1, 1, 0.9], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_GliderTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, CharB1: { Tiger: { front: [0, 0, 0, 0, 0], side: [0.02, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, Panther: { front: [0, 0, 0, 0, 0], side: [0.6, 0.44, 0.26, 0, 0], rear: [0.91, 0.52, 0.05, 0, 0] }, PanzerIV: { front: [0.23, 0.07, 0, 0, 0], side: [1, 1, 1, 0.1, 0], rear: [1, 1, 1, 0.66, 0.52] }, "PanzerIV:short": { front: [0.71, 0.2, 0, 0, 0], side: [1, 1, 1, 0.1, 0], rear: [1, 1, 1, 0.66, 0.52] }, StuG3: { front: [0, 0, 0, 0, 0], side: [1, 1, 1, 0.11, 0], rear: [0.84, 0.46, 0.3, 0.04, 0] }, Hummel: { front: [1, 1, 1, 0.5, 0.5], side: [1, 1, 1, 1, 0.94], rear: [1, 1, 1, 1, 0.9] }, Hanomag: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SdKfz222: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, T34: { front: [0, 0, 0, 0, 0], side: [0.53, 0.3, 0.07, 0, 0], rear: [0.11, 0, 0, 0, 0] }, "T34-85": { front: [0, 0, 0, 0, 0], side: [0.35, 0.28, 0.07, 0, 0], rear: [0.21, 0.03, 0, 0, 0] }, KV1: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, SU85: { front: [0.01, 0, 0, 0, 0], side: [0.97, 0.65, 0.1, 0, 0], rear: [0, 0, 0, 0, 0] }, SU76: { front: [0.88, 0.76, 0.58, 0.17, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Sherman: { front: [0, 0, 0, 0, 0], side: [0.93, 0.76, 0.66, 0, 0], rear: [0.93, 0.77, 0.62, 0, 0] }, M10: { front: [0, 0, 0, 0, 0], side: [1, 1, 1, 0.92, 0.03], rear: [1, 1, 1, 0.96, 0.03] }, M7: { front: [0.51, 0.5, 0.5, 0.5, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M8: { front: [1, 1, 1, 0.51, 0.32], side: [1, 1, 1, 1, 0.97], rear: [1, 1, 1, 1, 1] }, Matilda: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0.27, 0.01, 0, 0, 0] }, Sexton: { front: [0.51, 0.5, 0.5, 0.5, 0.32], side: [1, 1, 1, 1, 1], rear: [1, 1, 0.97, 0.5, 0.5] }, CharB1: { front: [0.08, 0, 0, 0, 0], side: [0.31, 0.01, 0, 0, 0], rear: [0.65, 0.23, 0, 0, 0] }, M13: { front: [1, 1, 0.97, 0.21, 0], side: [1, 1, 1, 1, 0.19], rear: [1, 1, 1, 1, 0.07] }, Semovente: { front: [1, 0.88, 0.5, 0.11, 0], side: [1, 1, 1, 1, 0.26], rear: [1, 1, 1, 1, 0.16] }, L3: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Type97: { front: [1, 1, 1, 0.74, 0], side: [1, 1, 1, 0.75, 0.26], rear: [1, 1, 1, 0.96, 0.5] }, "Type97:47": { front: [1, 1, 1, 0.74, 0], side: [1, 1, 1, 0.75, 0.26], rear: [1, 1, 1, 0.96, 0.5] }, "7TP": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, LightTank: { front: [1, 1, 1, 0.83, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, MediumTank: { front: [0.29, 0.07, 0, 0, 0], side: [1, 1, 1, 0.11, 0], rear: [1, 1, 1, 0.07, 0] }, AssaultGun: { front: [0.02, 0, 0, 0, 0], side: [1, 1, 1, 0.11, 0], rear: [1, 1, 1, 0.38, 0] }, SelfPropelledGun: { front: [0.53, 0.5, 0.5, 0.5, 0.5], side: [1, 1, 1, 0.66, 0.6], rear: [1, 1, 1, 0.54, 0.5] }, ArmouredCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_BreakthroughTank: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0.5, 0.15, 0, 0, 0] }, WhatIf_TankHunterCar: { front: [1, 1, 1, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_FlameTank: { front: [0.01, 0, 0, 0, 0], side: [1, 1, 1, 0.11, 0], rear: [1, 1, 1, 0.07, 0] }, WhatIf_AmphibiousTank: { front: [1, 1, 1, 0.26, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_SnowTank: { front: [1, 0.99, 0.64, 0, 0], side: [1, 1, 1, 1, 0.83], rear: [1, 1, 1, 1, 0.93] }, WhatIf_GliderTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, Gun75: { Tiger: { front: [0, 0, 0, 0, 0], side: [0.31, 0.17, 0.02, 0, 0], rear: [0.05, 0, 0, 0, 0] }, Panther: { front: [0, 0, 0, 0, 0], side: [1, 0.92, 0.65, 0.41, 0.16], rear: [1, 1, 0.95, 0.4, 0] }, PanzerIV: { front: [0.36, 0.32, 0.24, 0.04, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "PanzerIV:short": { front: [1, 0.97, 0.73, 0.12, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, StuG3: { front: [0.02, 0, 0, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 0.85, 0.4, 0.3] }, Hummel: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hanomag: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SdKfz222: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, T34: { front: [0.31, 0.1, 0, 0, 0], side: [1, 0.99, 0.63, 0.25, 0.01], rear: [1, 0.78, 0.18, 0, 0] }, "T34-85": { front: [0.02, 0, 0, 0, 0], side: [0.66, 0.55, 0.37, 0.24, 0.01], rear: [1, 0.8, 0.27, 0.01, 0] }, KV1: { front: [0, 0, 0, 0, 0], side: [0.24, 0, 0, 0, 0], rear: [0.32, 0, 0, 0, 0] }, SU85: { front: [0.54, 0.14, 0.01, 0, 0], side: [1, 1, 0.99, 0.53, 0.01], rear: [0.75, 0.13, 0, 0, 0] }, SU76: { front: [1, 1, 0.95, 0.72, 0.53], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Sherman: { front: [0.22, 0.15, 0.02, 0, 0], side: [1, 1, 0.93, 0.73, 0.49], rear: [1, 1, 0.93, 0.74, 0.44] }, M10: { front: [0.56, 0.2, 0.02, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M7: { front: [1, 0.84, 0.54, 0.5, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M8: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Matilda: { front: [0.16, 0, 0, 0, 0], side: [0.52, 0.03, 0, 0, 0], rear: [0.76, 0.65, 0.27, 0, 0] }, Sexton: { front: [1, 0.84, 0.54, 0.5, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 0.85] }, CharB1: { front: [0.47, 0.25, 0.08, 0, 0], side: [1, 0.89, 0.32, 0, 0], rear: [1, 0.9, 0.65, 0.15, 0] }, M13: { front: [1, 1, 1, 1, 0.88], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Semovente: { front: [1, 1, 1, 0.8, 0.41], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, L3: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Type97: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "Type97:47": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "7TP": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, LightTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, MediumTank: { front: [1, 0.86, 0.35, 0.04, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, AssaultGun: { front: [0.98, 0.51, 0.05, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SelfPropelledGun: { front: [1, 0.89, 0.56, 0.5, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, ArmouredCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_BreakthroughTank: { front: [0, 0, 0, 0, 0], side: [0.43, 0.03, 0, 0, 0], rear: [0.84, 0.66, 0.51, 0.09, 0] }, WhatIf_TankHunterCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_FlameTank: { front: [0.71, 0.21, 0.01, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_AmphibiousTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_SnowTank: { front: [1, 1, 1, 0.96, 0.45], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_GliderTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, M13: { Tiger: { front: [0, 0, 0, 0, 0], side: [0.03, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, Panther: { front: [0, 0, 0, 0, 0], side: [0.65, 0.34, 0, 0, 0], rear: [0.95, 0.12, 0, 0, 0] }, PanzerIV: { front: [0.26, 0, 0, 0, 0], side: [1, 1, 0.82, 0, 0], rear: [1, 1, 0.95, 0.65, 0.15] }, "PanzerIV:short": { front: [0.8, 0.01, 0, 0, 0], side: [1, 1, 0.82, 0, 0], rear: [1, 1, 0.95, 0.65, 0.15] }, StuG3: { front: [0, 0, 0, 0, 0], side: [1, 1, 0.94, 0, 0], rear: [0.91, 0.31, 0.29, 0, 0] }, Hummel: { front: [1, 1, 0.86, 0.5, 0.5], side: [1, 1, 1, 1, 0.72], rear: [1, 1, 1, 1, 0.62] }, Hanomag: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SdKfz222: { front: [1, 1, 1, 1, 0.88], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, T34: { front: [0, 0, 0, 0, 0], side: [0.6, 0.15, 0, 0, 0], rear: [0.15, 0, 0, 0, 0] }, "T34-85": { front: [0, 0, 0, 0, 0], side: [0.35, 0.15, 0, 0, 0], rear: [0.26, 0, 0, 0, 0] }, KV1: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, SU85: { front: [0.03, 0, 0, 0, 0], side: [1, 0.27, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, SU76: { front: [0.92, 0.62, 0.45, 0.02, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Sherman: { front: [0.01, 0, 0, 0, 0], side: [0.96, 0.7, 0.03, 0, 0], rear: [0.96, 0.7, 0.01, 0, 0] }, M10: { front: [0.01, 0, 0, 0, 0], side: [1, 1, 1, 0.56, 0], rear: [1, 1, 1, 0.6, 0] }, M7: { front: [0.53, 0.5, 0.5, 0.5, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M8: { front: [1, 1, 0.9, 0.4, 0.13], side: [1, 1, 1, 1, 0.78], rear: [1, 1, 1, 1, 0.84] }, Matilda: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0.34, 0, 0, 0, 0] }, Sexton: { front: [0.53, 0.5, 0.5, 0.5, 0.06], side: [1, 1, 1, 1, 1], rear: [1, 1, 0.52, 0.5, 0.5] }, CharB1: { front: [0.11, 0, 0, 0, 0], side: [0.4, 0, 0, 0, 0], rear: [0.71, 0.03, 0, 0, 0] }, M13: { front: [1, 1, 0.6, 0.05, 0], side: [1, 1, 1, 0.86, 0], rear: [1, 1, 1, 0.73, 0] }, Semovente: { front: [1, 0.65, 0.4, 0.01, 0], side: [1, 1, 1, 0.95, 0], rear: [1, 1, 1, 0.86, 0] }, L3: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Type97: { front: [1, 1, 1, 0.31, 0], side: [1, 1, 1, 0.47, 0.09], rear: [1, 1, 1, 0.83, 0.11] }, "Type97:47": { front: [1, 1, 1, 0.31, 0], side: [1, 1, 1, 0.47, 0.09], rear: [1, 1, 1, 0.83, 0.11] }, "7TP": { front: [1, 1, 1, 1, 0.86], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, LightTank: { front: [1, 1, 1, 0.39, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, MediumTank: { front: [0.36, 0, 0, 0, 0], side: [1, 1, 0.94, 0, 0], rear: [1, 1, 0.9, 0, 0] }, AssaultGun: { front: [0.04, 0, 0, 0, 0], side: [1, 1, 0.95, 0, 0], rear: [1, 1, 0.95, 0.19, 0] }, SelfPropelledGun: { front: [0.55, 0.5, 0.5, 0.5, 0.5], side: [1, 1, 0.99, 0.6, 0.6], rear: [1, 1, 0.96, 0.5, 0.5] }, ArmouredCar: { front: [1, 1, 1, 1, 0.88], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_BreakthroughTank: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0.56, 0.01, 0, 0, 0] }, WhatIf_TankHunterCar: { front: [1, 1, 0.61, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_FlameTank: { front: [0.02, 0, 0, 0, 0], side: [1, 1, 0.94, 0, 0], rear: [1, 1, 0.9, 0, 0] }, WhatIf_AmphibiousTank: { front: [1, 1, 0.97, 0.03, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_SnowTank: { front: [1, 0.69, 0.07, 0, 0], side: [1, 1, 1, 1, 0.28], rear: [1, 1, 1, 1, 0.73] }, WhatIf_GliderTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, Semovente: { Tiger: { front: [0, 0, 0, 0, 0], side: [0.26, 0.26, 0, 0, 0], rear: [0.01, 0.01, 0, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, Panther: { front: [0, 0, 0, 0, 0], side: [1, 1, 0.16, 0, 0], rear: [1, 1, 0, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0.51, 0.38, 0.16, 0, 0], rear: [0.75, 0.29, 0, 0, 0] } }, PanzerIV: { front: [0.34, 0.34, 0, 0, 0], side: [1, 1, 1, 0.91, 0.37], rear: [1, 1, 1, 0.99, 0.77], before: { front: [0.14, 0.02, 0, 0, 0], side: [1, 1, 1, 0.91, 0.37], rear: [1, 1, 1, 0.99, 0.77] } }, "PanzerIV:short": { front: [0.99, 0.99, 0, 0, 0], side: [1, 1, 1, 0.91, 0.37], rear: [1, 1, 1, 0.99, 0.77], before: { front: [0.41, 0.05, 0, 0, 0], side: [1, 1, 1, 0.91, 0.37], rear: [1, 1, 1, 0.99, 0.77] } }, StuG3: { front: [0, 0, 0, 0, 0], side: [1, 1, 1, 1, 0.45], rear: [1, 1, 0.3, 0.3, 0.15], before: { front: [0, 0, 0, 0, 0], side: [1, 1, 1, 1, 0.45], rear: [0.62, 0.34, 0.3, 0.3, 0.15] } }, Hummel: { front: [1, 1, 1, 0.95, 0.61], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 0.95, 0.61], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, Hanomag: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, SdKfz222: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, T34: { front: [0.27, 0.27, 0, 0, 0], side: [1, 1, 0.01, 0, 0], rear: [1, 1, 0, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0.43, 0.2, 0.01, 0, 0], rear: [0.03, 0, 0, 0, 0] } }, "T34-85": { front: [0.01, 0.01, 0, 0, 0], side: [0.65, 0.65, 0.01, 0, 0], rear: [1, 1, 0, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0.34, 0.2, 0.01, 0, 0], rear: [0.1, 0, 0, 0, 0] } }, KV1: { front: [0, 0, 0, 0, 0], side: [0.14, 0.14, 0, 0, 0], rear: [0.21, 0.21, 0, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] } }, SU85: { front: [0.43, 0.43, 0, 0, 0], side: [1, 1, 0.01, 0, 0], rear: [0.62, 0.62, 0, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0.85, 0.4, 0.01, 0, 0], rear: [0, 0, 0, 0, 0] } }, SU76: { front: [1, 1, 0.53, 0.45, 0.37], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [0.84, 0.68, 0.53, 0.45, 0.37], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, Sherman: { front: [0.22, 0.22, 0, 0, 0], side: [1, 1, 0.49, 0.09, 0], rear: [1, 1, 0.44, 0.05, 0], before: { front: [0, 0, 0, 0, 0], side: [0.83, 0.71, 0.49, 0.09, 0], rear: [0.83, 0.72, 0.44, 0.05, 0] } }, M10: { front: [0.5, 0.5, 0, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [0, 0, 0, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, M7: { front: [1, 1, 0.5, 0.5, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [0.5, 0.5, 0.5, 0.5, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, M8: { front: [1, 1, 1, 0.96, 0.67], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 0.96, 0.67], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, Matilda: { front: [0.09, 0.09, 0, 0, 0], side: [0.39, 0.39, 0, 0, 0], rear: [0.72, 0.72, 0, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0.08, 0, 0, 0, 0] } }, Sexton: { front: [1, 1, 0.5, 0.5, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 0.85, 0.56, 0.5], before: { front: [0.5, 0.5, 0.5, 0.5, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 0.85, 0.56, 0.5] } }, CharB1: { front: [0.41, 0.41, 0, 0, 0], side: [1, 1, 0, 0, 0], rear: [1, 1, 0, 0, 0], before: { front: [0.02, 0, 0, 0, 0], side: [0.09, 0, 0, 0, 0], rear: [0.4, 0.08, 0, 0, 0] } }, M13: { front: [1, 1, 0.88, 0.67, 0.44], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 0.88, 0.67, 0.44], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, Semovente: { front: [1, 1, 0.41, 0.4, 0.29], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [0.98, 0.73, 0.41, 0.4, 0.29], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, L3: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, Type97: { front: [1, 1, 1, 1, 0.99], side: [1, 1, 1, 1, 0.96], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 0.99], side: [1, 1, 1, 1, 0.96], rear: [1, 1, 1, 1, 1] } }, "Type97:47": { front: [1, 1, 1, 1, 0.99], side: [1, 1, 1, 1, 0.96], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 0.99], side: [1, 1, 1, 1, 0.96], rear: [1, 1, 1, 1, 1] } }, "7TP": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, LightTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, MediumTank: { front: [1, 1, 0, 0, 0], side: [1, 1, 1, 0.99, 0.45], rear: [1, 1, 1, 0.99, 0.39], before: { front: [0.15, 0.02, 0, 0, 0], side: [1, 1, 1, 0.99, 0.45], rear: [1, 1, 1, 0.99, 0.39] } }, AssaultGun: { front: [0.92, 0.92, 0, 0, 0], side: [1, 1, 1, 1, 0.46], rear: [1, 1, 1, 1, 0.63], before: { front: [0, 0, 0, 0, 0], side: [1, 1, 1, 1, 0.46], rear: [1, 1, 1, 1, 0.63] } }, SelfPropelledGun: { front: [1, 1, 0.5, 0.5, 0.5], side: [1, 1, 1, 1, 0.8], rear: [1, 1, 1, 1, 0.71], before: { front: [0.5, 0.5, 0.5, 0.5, 0.5], side: [1, 1, 1, 1, 0.8], rear: [1, 1, 1, 1, 0.71] } }, ArmouredCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, WhatIf_BreakthroughTank: { front: [0, 0, 0, 0, 0], side: [0.33, 0.33, 0, 0, 0], rear: [0.79, 0.79, 0, 0, 0], before: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0.29, 0.04, 0, 0, 0] } }, WhatIf_TankHunterCar: { front: [1, 1, 1, 0.84, 0.16], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 0.84, 0.16], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, WhatIf_FlameTank: { front: [0.61, 0.61, 0, 0, 0], side: [1, 1, 1, 0.99, 0.45], rear: [1, 1, 1, 0.99, 0.39], before: { front: [0, 0, 0, 0, 0], side: [1, 1, 1, 0.99, 0.45], rear: [1, 1, 1, 0.99, 0.39] } }, WhatIf_AmphibiousTank: { front: [1, 1, 1, 1, 0.65], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 0.65], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, WhatIf_SnowTank: { front: [1, 1, 0.45, 0.13, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 0.9, 0.45, 0.13, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, WhatIf_GliderTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1], before: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } } }, Type97: { Tiger: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, Panther: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, PanzerIV: { front: [0, 0, 0, 0, 0], side: [0.37, 0, 0, 0, 0], rear: [0.76, 0.65, 0.03, 0, 0] }, "PanzerIV:short": { front: [0, 0, 0, 0, 0], side: [0.37, 0, 0, 0, 0], rear: [0.76, 0.65, 0.03, 0, 0] }, StuG3: { front: [0, 0, 0, 0, 0], side: [0.45, 0, 0, 0, 0], rear: [0.15, 0, 0, 0, 0] }, Hummel: { front: [0.6, 0.5, 0.5, 0.5, 0], side: [1, 1, 0.63, 0.6, 0], rear: [1, 1, 0.52, 0.5, 0] }, Hanomag: { front: [1, 1, 0.84, 0, 0], side: [1, 1, 1, 1, 0], rear: [1, 1, 1, 1, 0] }, SdKfz222: { front: [1, 1, 0.58, 0, 0], side: [1, 1, 1, 1, 0], rear: [1, 1, 1, 1, 0] }, T34: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, "T34-85": { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, KV1: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, SU85: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, SU76: { front: [0.37, 0, 0, 0, 0], side: [1, 1, 1, 0.65, 0], rear: [1, 1, 1, 0.58, 0] }, Sherman: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, M10: { front: [0, 0, 0, 0, 0], side: [1, 0.33, 0, 0, 0], rear: [1, 0.37, 0, 0, 0] }, M7: { front: [0.5, 0.5, 0.5, 0.15, 0], side: [1, 1, 1, 0.95, 0], rear: [1, 1, 1, 0.95, 0] }, M8: { front: [0.67, 0.36, 0.04, 0, 0], side: [1, 1, 0.69, 0.65, 0], rear: [1, 1, 0.74, 0.65, 0] }, Matilda: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, Sexton: { front: [0.5, 0.5, 0, 0, 0], side: [1, 1, 1, 0.95, 0], rear: [0.5, 0.5, 0.5, 0.47, 0] }, CharB1: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, M13: { front: [0.43, 0.01, 0, 0, 0], side: [1, 0.66, 0, 0, 0], rear: [1, 0.51, 0, 0, 0] }, Semovente: { front: [0.28, 0, 0, 0, 0], side: [1, 0.77, 0, 0, 0], rear: [1, 0.65, 0, 0, 0] }, L3: { front: [1, 1, 1, 0.47, 0], side: [1, 1, 1, 1, 0], rear: [1, 1, 1, 1, 0] }, Type97: { front: [0.99, 0.13, 0, 0, 0], side: [0.95, 0.37, 0.02, 0, 0], rear: [1, 0.76, 0.01, 0, 0] }, "Type97:47": { front: [0.99, 0.13, 0, 0, 0], side: [0.95, 0.37, 0.02, 0, 0], rear: [1, 0.76, 0.01, 0, 0] }, "7TP": { front: [1, 1, 0.66, 0.08, 0], side: [1, 1, 1, 0.73, 0], rear: [1, 1, 1, 0.77, 0] }, LightTank: { front: [1, 0.19, 0, 0, 0], side: [1, 1, 1, 0.29, 0], rear: [1, 1, 1, 0.73, 0] }, MediumTank: { front: [0, 0, 0, 0, 0], side: [0.45, 0, 0, 0, 0], rear: [0.39, 0, 0, 0, 0] }, AssaultGun: { front: [0, 0, 0, 0, 0], side: [0.46, 0, 0, 0, 0], rear: [0.62, 0.11, 0, 0, 0] }, SelfPropelledGun: { front: [0.5, 0.5, 0.5, 0.44, 0], side: [0.8, 0.6, 0.6, 0.6, 0], rear: [0.71, 0.5, 0.5, 0.5, 0] }, ArmouredCar: { front: [1, 1, 0.69, 0.01, 0], side: [1, 1, 1, 0.94, 0], rear: [1, 1, 1, 0.96, 0] }, WhatIf_BreakthroughTank: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, WhatIf_TankHunterCar: { front: [0.15, 0, 0, 0, 0], side: [1, 1, 1, 0.76, 0], rear: [1, 1, 1, 1, 0] }, WhatIf_FlameTank: { front: [0, 0, 0, 0, 0], side: [0.45, 0, 0, 0, 0], rear: [0.39, 0, 0, 0, 0] }, WhatIf_AmphibiousTank: { front: [0.65, 0, 0, 0, 0], side: [1, 1, 1, 0.76, 0], rear: [1, 1, 1, 0.73, 0] }, WhatIf_SnowTank: { front: [0, 0, 0, 0, 0], side: [1, 1, 0.06, 0, 0], rear: [1, 1, 0.66, 0.2, 0] }, WhatIf_GliderTank: { front: [1, 1, 0.95, 0.04, 0], side: [1, 1, 1, 1, 0], rear: [1, 1, 1, 1, 0] } }, "Type97:47": { Tiger: { front: [0, 0, 0, 0, 0], side: [0.31, 0.17, 0, 0, 0], rear: [0.05, 0, 0, 0, 0] }, Panther: { front: [0, 0, 0, 0, 0], side: [1, 0.88, 0.42, 0.06, 0], rear: [1, 1, 0.42, 0, 0] }, PanzerIV: { front: [0.36, 0.31, 0.07, 0, 0], side: [1, 1, 1, 0.98, 0.21], rear: [1, 1, 1, 1, 0.7] }, "PanzerIV:short": { front: [1, 0.96, 0.19, 0, 0], side: [1, 1, 1, 0.98, 0.21], rear: [1, 1, 1, 1, 0.7] }, StuG3: { front: [0.01, 0, 0, 0, 0], side: [1, 1, 1, 1, 0.26], rear: [1, 1, 0.45, 0.3, 0.09] }, Hummel: { front: [1, 1, 1, 1, 0.53], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hanomag: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SdKfz222: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, T34: { front: [0.28, 0.08, 0, 0, 0], side: [1, 0.96, 0.28, 0, 0], rear: [0.97, 0.59, 0, 0, 0] }, "T34-85": { front: [0, 0, 0, 0, 0], side: [0.66, 0.48, 0.28, 0, 0], rear: [0.97, 0.61, 0.03, 0, 0] }, KV1: { front: [0, 0, 0, 0, 0], side: [0.24, 0, 0, 0, 0], rear: [0.29, 0, 0, 0, 0] }, SU85: { front: [0.28, 0.14, 0, 0, 0], side: [1, 1, 0.62, 0, 0], rear: [0.43, 0.02, 0, 0, 0] }, SU76: { front: [1, 1, 0.69, 0.46, 0.27], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Sherman: { front: [0.22, 0.12, 0, 0, 0], side: [1, 1, 0.76, 0.28, 0], rear: [1, 1, 0.77, 0.22, 0] }, M10: { front: [0.38, 0.12, 0, 0, 0], side: [1, 1, 1, 1, 0.99], rear: [1, 1, 1, 1, 1] }, M7: { front: [1, 0.77, 0.5, 0.5, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M8: { front: [1, 1, 1, 1, 0.59], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Matilda: { front: [0.16, 0, 0, 0, 0], side: [0.52, 0.03, 0, 0, 0], rear: [0.76, 0.65, 0.01, 0, 0] }, Sexton: { front: [1, 0.77, 0.5, 0.5, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 0.7, 0.5] }, CharB1: { front: [0.4, 0.24, 0, 0, 0], side: [1, 0.87, 0.01, 0, 0], rear: [1, 0.88, 0.23, 0, 0] }, M13: { front: [1, 1, 1, 0.77, 0.31], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Semovente: { front: [1, 1, 0.87, 0.4, 0.19], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, L3: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Type97: { front: [1, 1, 1, 1, 0.9], side: [1, 1, 1, 1, 0.87], rear: [1, 1, 1, 1, 0.99] }, "Type97:47": { front: [1, 1, 1, 1, 0.9], side: [1, 1, 1, 1, 0.87], rear: [1, 1, 1, 1, 0.99] }, "7TP": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, LightTank: { front: [1, 1, 1, 1, 0.97], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, MediumTank: { front: [1, 0.77, 0.06, 0, 0], side: [1, 1, 1, 1, 0.26], rear: [1, 1, 1, 1, 0.2] }, AssaultGun: { front: [0.95, 0.42, 0, 0, 0], side: [1, 1, 1, 1, 0.27], rear: [1, 1, 1, 1, 0.5] }, SelfPropelledGun: { front: [1, 0.82, 0.5, 0.5, 0.5], side: [1, 1, 1, 1, 0.72], rear: [1, 1, 1, 1, 0.62] }, ArmouredCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_BreakthroughTank: { front: [0, 0, 0, 0, 0], side: [0.43, 0.03, 0, 0, 0], rear: [0.83, 0.65, 0.14, 0, 0] }, WhatIf_TankHunterCar: { front: [1, 1, 1, 0.98, 0.03], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_FlameTank: { front: [0.6, 0.19, 0, 0, 0], side: [1, 1, 1, 1, 0.26], rear: [1, 1, 1, 1, 0.2] }, WhatIf_AmphibiousTank: { front: [1, 1, 1, 1, 0.44], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_SnowTank: { front: [1, 1, 0.92, 0.24, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_GliderTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, "7TP": { Tiger: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, Panther: { front: [0, 0, 0, 0, 0], side: [0.4, 0.03, 0, 0, 0], rear: [0.35, 0, 0, 0, 0] }, PanzerIV: { front: [0.06, 0, 0, 0, 0], side: [1, 0.93, 0.02, 0, 0], rear: [1, 1, 0.65, 0.02, 0] }, "PanzerIV:short": { front: [0.18, 0, 0, 0, 0], side: [1, 0.93, 0.02, 0, 0], rear: [1, 1, 0.65, 0.02, 0] }, StuG3: { front: [0, 0, 0, 0, 0], side: [1, 1, 0.02, 0, 0], rear: [0.45, 0.3, 0.01, 0, 0] }, Hummel: { front: [1, 0.98, 0.5, 0.5, 0.15], side: [1, 1, 1, 0.63, 0.6], rear: [1, 1, 1, 0.52, 0.5] }, Hanomag: { front: [1, 1, 1, 0.84, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SdKfz222: { front: [1, 1, 1, 0.58, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, T34: { front: [0, 0, 0, 0, 0], side: [0.28, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, "T34-85": { front: [0, 0, 0, 0, 0], side: [0.28, 0, 0, 0, 0], rear: [0.03, 0, 0, 0, 0] }, KV1: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, SU85: { front: [0, 0, 0, 0, 0], side: [0.6, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, SU76: { front: [0.66, 0.45, 0.07, 0, 0], side: [1, 1, 1, 1, 0.5], rear: [1, 1, 1, 1, 0.4] }, Sherman: { front: [0, 0, 0, 0, 0], side: [0.76, 0.17, 0, 0, 0], rear: [0.77, 0.12, 0, 0, 0] }, M10: { front: [0, 0, 0, 0, 0], side: [1, 1, 0.75, 0, 0], rear: [1, 1, 0.8, 0, 0] }, M7: { front: [0.5, 0.5, 0.5, 0.5, 0], side: [1, 1, 1, 1, 0.15], rear: [1, 1, 1, 1, 0.15] }, M8: { front: [1, 0.98, 0.43, 0.04, 0], side: [1, 1, 1, 0.69, 0.65], rear: [1, 1, 1, 0.74, 0.65] }, Matilda: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0.01, 0, 0, 0, 0] }, Sexton: { front: [0.5, 0.5, 0.5, 0, 0], side: [1, 1, 1, 1, 0.15], rear: [1, 0.62, 0.5, 0.5, 0.07] }, CharB1: { front: [0, 0, 0, 0, 0], side: [0.01, 0, 0, 0, 0], rear: [0.23, 0, 0, 0, 0] }, M13: { front: [1, 0.71, 0.11, 0, 0], side: [1, 1, 0.95, 0, 0], rear: [1, 1, 0.91, 0, 0] }, Semovente: { front: [0.87, 0.4, 0.04, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 0.98, 0, 0] }, L3: { front: [1, 1, 1, 1, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Type97: { front: [1, 1, 0.51, 0, 0], side: [1, 1, 0.59, 0.02, 0], rear: [1, 1, 0.89, 0.01, 0] }, "Type97:47": { front: [1, 1, 0.51, 0, 0], side: [1, 1, 0.59, 0.02, 0], rear: [1, 1, 0.89, 0.01, 0] }, "7TP": { front: [1, 1, 1, 0.65, 0], side: [1, 1, 1, 1, 0.1], rear: [1, 1, 1, 1, 0.7] }, LightTank: { front: [1, 1, 0.61, 0, 0], side: [1, 1, 1, 1, 0], rear: [1, 1, 1, 1, 0.1] }, MediumTank: { front: [0.06, 0, 0, 0, 0], side: [1, 1, 0.02, 0, 0], rear: [1, 1, 0, 0, 0] }, AssaultGun: { front: [0, 0, 0, 0, 0], side: [1, 1, 0.02, 0, 0], rear: [1, 1, 0.27, 0, 0] }, SelfPropelledGun: { front: [0.5, 0.5, 0.5, 0.5, 0.05], side: [1, 1, 0.61, 0.6, 0.6], rear: [1, 1, 0.5, 0.5, 0.5] }, ArmouredCar: { front: [1, 1, 1, 0.69, 0], side: [1, 1, 1, 1, 0.67], rear: [1, 1, 1, 1, 0.69] }, WhatIf_BreakthroughTank: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0.14, 0, 0, 0, 0] }, WhatIf_TankHunterCar: { front: [1, 0.85, 0, 0, 0], side: [1, 1, 1, 1, 0.14], rear: [1, 1, 1, 1, 0.8] }, WhatIf_FlameTank: { front: [0, 0, 0, 0, 0], side: [1, 1, 0.02, 0, 0], rear: [1, 1, 0, 0, 0] }, WhatIf_AmphibiousTank: { front: [1, 1, 0.07, 0, 0], side: [1, 1, 1, 1, 0.35], rear: [1, 1, 1, 1, 0.65] }, WhatIf_SnowTank: { front: [0.84, 0.18, 0, 0, 0], side: [1, 1, 1, 0.06, 0], rear: [1, 1, 1, 0.66, 0] }, WhatIf_GliderTank: { front: [1, 1, 1, 0.95, 0], side: [1, 1, 1, 1, 0.82], rear: [1, 1, 1, 1, 0.8] } }, LightTank: { Tiger: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, Panther: { front: [0, 0, 0, 0, 0], side: [0.3, 0, 0, 0, 0], rear: [0.03, 0, 0, 0, 0] }, PanzerIV: { front: [0, 0, 0, 0, 0], side: [1, 0.88, 0.02, 0, 0], rear: [1, 0.98, 0.65, 0.15, 0] }, "PanzerIV:short": { front: [0, 0, 0, 0, 0], side: [1, 0.88, 0.02, 0, 0], rear: [1, 0.98, 0.65, 0.15, 0] }, StuG3: { front: [0, 0, 0, 0, 0], side: [1, 1, 0.02, 0, 0], rear: [0.3, 0.3, 0.01, 0, 0] }, Hummel: { front: [1, 0.92, 0.5, 0.5, 0.5], side: [1, 1, 1, 0.72, 0.6], rear: [1, 1, 1, 0.62, 0.5] }, Hanomag: { front: [1, 1, 1, 1, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SdKfz222: { front: [1, 1, 1, 0.88, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, T34: { front: [0, 0, 0, 0, 0], side: [0.11, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, "T34-85": { front: [0, 0, 0, 0, 0], side: [0.11, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, KV1: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, SU85: { front: [0, 0, 0, 0, 0], side: [0.17, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, SU76: { front: [0.58, 0.45, 0.07, 0, 0], side: [1, 1, 1, 1, 0.65], rear: [1, 1, 1, 1, 0.58] }, Sherman: { front: [0, 0, 0, 0, 0], side: [0.7, 0.09, 0, 0, 0], rear: [0.67, 0.05, 0, 0, 0] }, M10: { front: [0, 0, 0, 0, 0], side: [1, 1, 0.75, 0, 0], rear: [1, 1, 0.8, 0, 0] }, M7: { front: [0.5, 0.5, 0.5, 0.5, 0.15], side: [1, 1, 1, 1, 0.95], rear: [1, 1, 1, 1, 0.95] }, M8: { front: [1, 0.94, 0.43, 0.13, 0], side: [1, 1, 1, 0.78, 0.65], rear: [1, 1, 1, 0.84, 0.65] }, Matilda: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, Sexton: { front: [0.5, 0.5, 0.5, 0.05, 0], side: [1, 1, 1, 1, 0.95], rear: [1, 0.56, 0.5, 0.5, 0.47] }, CharB1: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, M13: { front: [0.99, 0.65, 0.11, 0, 0], side: [1, 1, 0.95, 0, 0], rear: [1, 1, 0.91, 0, 0] }, Semovente: { front: [0.57, 0.4, 0.04, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 0.98, 0, 0] }, L3: { front: [1, 1, 1, 1, 0.47], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Type97: { front: [1, 1, 0.51, 0, 0], side: [1, 1, 0.59, 0.09, 0], rear: [1, 1, 0.89, 0.11, 0] }, "Type97:47": { front: [1, 1, 0.51, 0, 0], side: [1, 1, 0.59, 0.09, 0], rear: [1, 1, 0.89, 0.11, 0] }, "7TP": { front: [1, 1, 1, 0.86, 0.08], side: [1, 1, 1, 1, 0.73], rear: [1, 1, 1, 1, 0.77] }, LightTank: { front: [1, 1, 0.61, 0, 0], side: [1, 1, 1, 1, 0.29], rear: [1, 1, 1, 1, 0.73] }, MediumTank: { front: [0, 0, 0, 0, 0], side: [1, 0.99, 0.02, 0, 0], rear: [1, 0.98, 0, 0, 0] }, AssaultGun: { front: [0, 0, 0, 0, 0], side: [1, 1, 0.02, 0, 0], rear: [1, 1, 0.27, 0, 0] }, SelfPropelledGun: { front: [0.5, 0.5, 0.5, 0.5, 0.44], side: [1, 1, 0.61, 0.6, 0.6], rear: [1, 1, 0.5, 0.5, 0.5] }, ArmouredCar: { front: [1, 1, 1, 0.87, 0.01], side: [1, 1, 1, 1, 0.94], rear: [1, 1, 1, 1, 0.96] }, WhatIf_BreakthroughTank: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, WhatIf_TankHunterCar: { front: [1, 0.71, 0, 0, 0], side: [1, 1, 1, 1, 0.76], rear: [1, 1, 1, 1, 1] }, WhatIf_FlameTank: { front: [0, 0, 0, 0, 0], side: [1, 0.99, 0.02, 0, 0], rear: [1, 0.98, 0, 0, 0] }, WhatIf_AmphibiousTank: { front: [1, 1, 0.07, 0, 0], side: [1, 1, 1, 1, 0.76], rear: [1, 1, 1, 1, 0.73] }, WhatIf_SnowTank: { front: [0.51, 0.12, 0, 0, 0], side: [1, 1, 1, 0.28, 0], rear: [1, 1, 1, 0.73, 0.2] }, WhatIf_GliderTank: { front: [1, 1, 1, 1, 0.04], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, MediumTank: { Tiger: { front: [0, 0, 0, 0, 0], side: [0.22, 0.03, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, Panther: { front: [0, 0, 0, 0, 0], side: [0.95, 0.66, 0.3, 0, 0], rear: [1, 0.96, 0.07, 0, 0] }, PanzerIV: { front: [0.33, 0.26, 0, 0, 0], side: [1, 1, 1, 0.82, 0.02], rear: [1, 1, 1, 0.95, 0.65] }, "PanzerIV:short": { front: [0.98, 0.81, 0, 0, 0], side: [1, 1, 1, 0.82, 0.02], rear: [1, 1, 1, 0.95, 0.65] }, StuG3: { front: [0, 0, 0, 0, 0], side: [1, 1, 1, 0.95, 0.02], rear: [1, 0.91, 0.3, 0.29, 0.01] }, Hummel: { front: [1, 1, 1, 0.86, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hanomag: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SdKfz222: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, T34: { front: [0.13, 0, 0, 0, 0], side: [1, 0.62, 0.11, 0, 0], rear: [0.76, 0.16, 0, 0, 0] }, "T34-85": { front: [0, 0, 0, 0, 0], side: [0.55, 0.36, 0.11, 0, 0], rear: [0.76, 0.27, 0, 0, 0] }, KV1: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0.02, 0, 0, 0, 0] }, SU85: { front: [0.17, 0.03, 0, 0, 0], side: [1, 1, 0.17, 0, 0], rear: [0.08, 0, 0, 0, 0] }, SU76: { front: [1, 0.93, 0.59, 0.45, 0.09], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Sherman: { front: [0.17, 0.01, 0, 0, 0], side: [1, 0.96, 0.7, 0.03, 0], rear: [1, 0.96, 0.68, 0.01, 0] }, M10: { front: [0.18, 0.01, 0, 0, 0], side: [1, 1, 1, 1, 0.77], rear: [1, 1, 1, 1, 0.82] }, M7: { front: [0.88, 0.53, 0.5, 0.5, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M8: { front: [1, 1, 1, 0.9, 0.45], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Matilda: { front: [0, 0, 0, 0, 0], side: [0.11, 0, 0, 0, 0], rear: [0.65, 0.34, 0, 0, 0] }, Sexton: { front: [0.88, 0.53, 0.5, 0.5, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 0.52, 0.5] }, CharB1: { front: [0.26, 0.11, 0, 0, 0], side: [0.93, 0.4, 0, 0, 0], rear: [0.93, 0.71, 0, 0, 0] }, M13: { front: [1, 1, 0.99, 0.6, 0.12], side: [1, 1, 1, 1, 0.96], rear: [1, 1, 1, 1, 0.92] }, Semovente: { front: [1, 1, 0.57, 0.4, 0.05], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 0.99] }, L3: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Type97: { front: [1, 1, 1, 1, 0.53], side: [1, 1, 1, 1, 0.61], rear: [1, 1, 1, 1, 0.9] }, "Type97:47": { front: [1, 1, 1, 1, 0.53], side: [1, 1, 1, 1, 0.61], rear: [1, 1, 1, 1, 0.9] }, "7TP": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, LightTank: { front: [1, 1, 1, 1, 0.63], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, MediumTank: { front: [0.9, 0.37, 0, 0, 0], side: [1, 1, 1, 0.94, 0.02], rear: [1, 1, 1, 0.9, 0] }, AssaultGun: { front: [0.61, 0.05, 0, 0, 0], side: [1, 1, 1, 0.95, 0.02], rear: [1, 1, 1, 0.95, 0.27] }, SelfPropelledGun: { front: [0.92, 0.56, 0.5, 0.5, 0.5], side: [1, 1, 1, 0.99, 0.61], rear: [1, 1, 1, 0.96, 0.5] }, ArmouredCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_BreakthroughTank: { front: [0, 0, 0, 0, 0], side: [0.1, 0, 0, 0, 0], rear: [0.68, 0.57, 0, 0, 0] }, WhatIf_TankHunterCar: { front: [1, 1, 1, 0.63, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_FlameTank: { front: [0.28, 0.02, 0, 0, 0], side: [1, 1, 1, 0.94, 0.02], rear: [1, 1, 1, 0.9, 0] }, WhatIf_AmphibiousTank: { front: [1, 1, 1, 0.97, 0.12], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_SnowTank: { front: [1, 1, 0.62, 0.07, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_GliderTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, AssaultGun: { Tiger: { front: [0.05, 0, 0, 0, 0], side: [1, 0.81, 0.38, 0.23, 0.08], rear: [1, 0.69, 0.14, 0, 0] }, Panther: { front: [0.03, 0, 0, 0, 0], side: [1, 1, 1, 0.99, 0.79], rear: [1, 1, 1, 1, 1] }, PanzerIV: { front: [0.99, 0.74, 0.41, 0.33, 0.3], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "PanzerIV:short": { front: [1, 1, 1, 0.98, 0.94], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, StuG3: { front: [0.92, 0.49, 0.06, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 0.99] }, Hummel: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hanomag: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SdKfz222: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, T34: { front: [0.68, 0.47, 0.35, 0.18, 0.02], side: [1, 1, 1, 1, 0.83], rear: [1, 1, 1, 0.94, 0.43] }, "T34-85": { front: [0.52, 0.19, 0.05, 0, 0], side: [1, 0.88, 0.69, 0.62, 0.43], rear: [1, 1, 1, 0.94, 0.51] }, KV1: { front: [0.66, 0.19, 0, 0, 0], side: [1, 0.92, 0.39, 0.02, 0], rear: [1, 0.94, 0.47, 0.05, 0] }, SU85: { front: [1, 1, 0.67, 0.25, 0.06], side: [1, 1, 1, 1, 1], rear: [1, 1, 0.87, 0.34, 0.01] }, SU76: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Sherman: { front: [0.5, 0.24, 0.22, 0.2, 0.07], side: [1, 1, 1, 1, 0.99], rear: [1, 1, 1, 1, 1] }, M10: { front: [0.97, 0.8, 0.63, 0.35, 0.07], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M7: { front: [1, 1, 1, 0.96, 0.65], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M8: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Matilda: { front: [0.99, 0.79, 0.27, 0.01, 0], side: [1, 0.99, 0.68, 0.16, 0], rear: [1, 0.99, 0.81, 0.66, 0.47] }, Sexton: { front: [1, 1, 1, 0.96, 0.65], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, CharB1: { front: [0.88, 0.7, 0.53, 0.31, 0.16], side: [1, 1, 1, 0.96, 0.6], rear: [1, 1, 1, 0.96, 0.81] }, M13: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Semovente: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, L3: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Type97: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "Type97:47": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "7TP": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, LightTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, MediumTank: { front: [1, 1, 1, 0.98, 0.6], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, AssaultGun: { front: [1, 1, 1, 0.75, 0.19], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SelfPropelledGun: { front: [1, 1, 1, 0.99, 0.7], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, ArmouredCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_BreakthroughTank: { front: [0, 0, 0, 0, 0], side: [1, 0.87, 0.56, 0.15, 0], rear: [1, 1, 0.9, 0.7, 0.64] }, WhatIf_TankHunterCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_FlameTank: { front: [1, 1, 0.81, 0.39, 0.07], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_AmphibiousTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_SnowTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_GliderTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, SelfPropelledGun: { Tiger: { front: [0, 0, 0, 0, 0], side: [0.31, 0.17, 0.02, 0, 0], rear: [0.05, 0, 0, 0, 0] }, Panther: { front: [0, 0, 0, 0, 0], side: [1, 0.92, 0.65, 0.41, 0.16], rear: [1, 1, 0.95, 0.4, 0] }, PanzerIV: { front: [0.36, 0.32, 0.24, 0.04, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "PanzerIV:short": { front: [1, 0.97, 0.73, 0.12, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, StuG3: { front: [0.02, 0, 0, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 0.85, 0.4, 0.3] }, Hummel: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hanomag: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SdKfz222: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, T34: { front: [0.31, 0.1, 0, 0, 0], side: [1, 0.99, 0.63, 0.25, 0.01], rear: [1, 0.78, 0.18, 0, 0] }, "T34-85": { front: [0.02, 0, 0, 0, 0], side: [0.66, 0.55, 0.37, 0.24, 0.01], rear: [1, 0.8, 0.27, 0.01, 0] }, KV1: { front: [0, 0, 0, 0, 0], side: [0.24, 0, 0, 0, 0], rear: [0.32, 0, 0, 0, 0] }, SU85: { front: [0.54, 0.14, 0.01, 0, 0], side: [1, 1, 0.99, 0.53, 0.01], rear: [0.75, 0.13, 0, 0, 0] }, SU76: { front: [1, 1, 0.95, 0.72, 0.53], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Sherman: { front: [0.22, 0.15, 0.02, 0, 0], side: [1, 1, 0.93, 0.73, 0.49], rear: [1, 1, 0.93, 0.74, 0.44] }, M10: { front: [0.56, 0.2, 0.02, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M7: { front: [1, 0.84, 0.54, 0.5, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M8: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Matilda: { front: [0.16, 0, 0, 0, 0], side: [0.52, 0.03, 0, 0, 0], rear: [0.76, 0.65, 0.27, 0, 0] }, Sexton: { front: [1, 0.84, 0.54, 0.5, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 0.85] }, CharB1: { front: [0.47, 0.25, 0.08, 0, 0], side: [1, 0.89, 0.32, 0, 0], rear: [1, 0.9, 0.65, 0.15, 0] }, M13: { front: [1, 1, 1, 1, 0.88], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Semovente: { front: [1, 1, 1, 0.8, 0.41], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, L3: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Type97: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "Type97:47": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "7TP": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, LightTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, MediumTank: { front: [1, 0.86, 0.35, 0.04, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, AssaultGun: { front: [0.98, 0.51, 0.05, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SelfPropelledGun: { front: [1, 0.89, 0.56, 0.5, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, ArmouredCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_BreakthroughTank: { front: [0, 0, 0, 0, 0], side: [0.43, 0.03, 0, 0, 0], rear: [0.84, 0.66, 0.51, 0.09, 0] }, WhatIf_TankHunterCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_FlameTank: { front: [0.71, 0.21, 0.01, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_AmphibiousTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_SnowTank: { front: [1, 1, 1, 0.96, 0.45], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_GliderTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, ArmouredCar: { Tiger: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, Panther: { front: [0, 0, 0, 0, 0], side: [0.3, 0, 0, 0, 0], rear: [0.03, 0, 0, 0, 0] }, PanzerIV: { front: [0, 0, 0, 0, 0], side: [1, 0.88, 0.02, 0, 0], rear: [1, 0.98, 0.65, 0.15, 0] }, "PanzerIV:short": { front: [0, 0, 0, 0, 0], side: [1, 0.88, 0.02, 0, 0], rear: [1, 0.98, 0.65, 0.15, 0] }, StuG3: { front: [0, 0, 0, 0, 0], side: [1, 1, 0.02, 0, 0], rear: [0.3, 0.3, 0.01, 0, 0] }, Hummel: { front: [1, 0.92, 0.5, 0.5, 0.5], side: [1, 1, 1, 0.72, 0.6], rear: [1, 1, 1, 0.62, 0.5] }, Hanomag: { front: [1, 1, 1, 1, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SdKfz222: { front: [1, 1, 1, 0.88, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, T34: { front: [0, 0, 0, 0, 0], side: [0.11, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, "T34-85": { front: [0, 0, 0, 0, 0], side: [0.11, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, KV1: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, SU85: { front: [0, 0, 0, 0, 0], side: [0.17, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, SU76: { front: [0.58, 0.45, 0.07, 0, 0], side: [1, 1, 1, 1, 0.65], rear: [1, 1, 1, 1, 0.58] }, Sherman: { front: [0, 0, 0, 0, 0], side: [0.7, 0.09, 0, 0, 0], rear: [0.67, 0.05, 0, 0, 0] }, M10: { front: [0, 0, 0, 0, 0], side: [1, 1, 0.75, 0, 0], rear: [1, 1, 0.8, 0, 0] }, M7: { front: [0.5, 0.5, 0.5, 0.5, 0.15], side: [1, 1, 1, 1, 0.95], rear: [1, 1, 1, 1, 0.95] }, M8: { front: [1, 0.94, 0.43, 0.13, 0], side: [1, 1, 1, 0.78, 0.65], rear: [1, 1, 1, 0.84, 0.65] }, Matilda: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, Sexton: { front: [0.5, 0.5, 0.5, 0.05, 0], side: [1, 1, 1, 1, 0.95], rear: [1, 0.56, 0.5, 0.5, 0.47] }, CharB1: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, M13: { front: [0.99, 0.65, 0.11, 0, 0], side: [1, 1, 0.95, 0, 0], rear: [1, 1, 0.91, 0, 0] }, Semovente: { front: [0.57, 0.4, 0.04, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 0.98, 0, 0] }, L3: { front: [1, 1, 1, 1, 0.47], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Type97: { front: [1, 1, 0.51, 0, 0], side: [1, 1, 0.59, 0.09, 0], rear: [1, 1, 0.89, 0.11, 0] }, "Type97:47": { front: [1, 1, 0.51, 0, 0], side: [1, 1, 0.59, 0.09, 0], rear: [1, 1, 0.89, 0.11, 0] }, "7TP": { front: [1, 1, 1, 0.86, 0.08], side: [1, 1, 1, 1, 0.73], rear: [1, 1, 1, 1, 0.77] }, LightTank: { front: [1, 1, 0.61, 0, 0], side: [1, 1, 1, 1, 0.29], rear: [1, 1, 1, 1, 0.73] }, MediumTank: { front: [0, 0, 0, 0, 0], side: [1, 0.99, 0.02, 0, 0], rear: [1, 0.98, 0, 0, 0] }, AssaultGun: { front: [0, 0, 0, 0, 0], side: [1, 1, 0.02, 0, 0], rear: [1, 1, 0.27, 0, 0] }, SelfPropelledGun: { front: [0.5, 0.5, 0.5, 0.5, 0.44], side: [1, 1, 0.61, 0.6, 0.6], rear: [1, 1, 0.5, 0.5, 0.5] }, ArmouredCar: { front: [1, 1, 1, 0.87, 0.01], side: [1, 1, 1, 1, 0.94], rear: [1, 1, 1, 1, 0.96] }, WhatIf_BreakthroughTank: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, WhatIf_TankHunterCar: { front: [1, 0.71, 0, 0, 0], side: [1, 1, 1, 1, 0.76], rear: [1, 1, 1, 1, 1] }, WhatIf_FlameTank: { front: [0, 0, 0, 0, 0], side: [1, 0.99, 0.02, 0, 0], rear: [1, 0.98, 0, 0, 0] }, WhatIf_AmphibiousTank: { front: [1, 1, 0.07, 0, 0], side: [1, 1, 1, 1, 0.76], rear: [1, 1, 1, 1, 0.73] }, WhatIf_SnowTank: { front: [0.51, 0.12, 0, 0, 0], side: [1, 1, 1, 0.28, 0], rear: [1, 1, 1, 0.73, 0.2] }, WhatIf_GliderTank: { front: [1, 1, 1, 1, 0.04], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, AntiTankGun: { Tiger: { front: [0.28, 0.05, 0, 0, 0], side: [1, 1, 0.59, 0.24, 0.05], rear: [1, 1, 0.39, 0, 0] }, Panther: { front: [0.15, 0.03, 0, 0, 0], side: [1, 1, 1, 0.99, 0.72], rear: [1, 1, 1, 1, 0.99] }, PanzerIV: { front: [1, 0.99, 0.55, 0.33, 0.28], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "PanzerIV:short": { front: [1, 1, 1, 0.98, 0.88], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, StuG3: { front: [1, 0.89, 0.21, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 0.96] }, Hummel: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hanomag: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SdKfz222: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, T34: { front: [0.75, 0.55, 0.36, 0.2, 0], side: [1, 1, 1, 1, 0.71], rear: [1, 1, 1, 0.91, 0.26] }, "T34-85": { front: [0.74, 0.4, 0.06, 0, 0], side: [1, 1, 0.76, 0.61, 0.38], rear: [1, 1, 1, 0.91, 0.36] }, KV1: { front: [0.95, 0.59, 0.03, 0, 0], side: [1, 1, 0.68, 0.04, 0], rear: [1, 1, 0.74, 0.08, 0] }, SU85: { front: [1, 1, 0.75, 0.22, 0.04], side: [1, 1, 1, 1, 1], rear: [1, 1, 0.94, 0.27, 0] }, SU76: { front: [1, 1, 1, 1, 0.99], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Sherman: { front: [0.71, 0.4, 0.22, 0.2, 0.03], side: [1, 1, 1, 1, 0.98], rear: [1, 1, 1, 1, 0.98] }, M10: { front: [1, 0.91, 0.66, 0.31, 0.03], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M7: { front: [1, 1, 1, 0.96, 0.58], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M8: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Matilda: { front: [1, 0.97, 0.52, 0.02, 0], side: [1, 1, 0.91, 0.22, 0], rear: [1, 1, 0.92, 0.67, 0.41] }, Sexton: { front: [1, 1, 1, 0.96, 0.58], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, CharB1: { front: [0.96, 0.8, 0.61, 0.31, 0.13], side: [1, 1, 1, 0.97, 0.5], rear: [1, 1, 1, 0.97, 0.77] }, M13: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Semovente: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, L3: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Type97: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "Type97:47": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "7TP": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, LightTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, MediumTank: { front: [1, 1, 1, 0.99, 0.47], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, AssaultGun: { front: [1, 1, 1, 0.78, 0.1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SelfPropelledGun: { front: [1, 1, 1, 0.99, 0.62], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, ArmouredCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_BreakthroughTank: { front: [0.04, 0, 0, 0, 0], side: [1, 1, 0.76, 0.2, 0], rear: [1, 1, 0.98, 0.72, 0.61] }, WhatIf_TankHunterCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_FlameTank: { front: [1, 1, 0.93, 0.41, 0.04], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_AmphibiousTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_SnowTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_GliderTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, FlakTruck: { Tiger: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, Panther: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, PanzerIV: { front: [0, 0, 0, 0, 0], side: [0.02, 0, 0, 0, 0], rear: [0.65, 0.02, 0, 0, 0] }, "PanzerIV:short": { front: [0, 0, 0, 0, 0], side: [0.02, 0, 0, 0, 0], rear: [0.65, 0.02, 0, 0, 0] }, StuG3: { front: [0, 0, 0, 0, 0], side: [0.01, 0, 0, 0, 0], rear: [0.01, 0, 0, 0, 0] }, Hummel: { front: [0.5, 0.5, 0, 0, 0], side: [1, 0.63, 0.41, 0, 0], rear: [1, 0.52, 0.4, 0, 0] }, Hanomag: { front: [1, 0.74, 0, 0, 0], side: [1, 1, 0.94, 0, 0], rear: [1, 1, 1, 0, 0] }, SdKfz222: { front: [1, 0.42, 0, 0, 0], side: [1, 1, 0.94, 0, 0], rear: [1, 1, 0.94, 0, 0] }, T34: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, "T34-85": { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, KV1: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, SU85: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, SU76: { front: [0.03, 0, 0, 0, 0], side: [1, 1, 0.33, 0, 0], rear: [1, 1, 0.34, 0, 0] }, Sherman: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, M10: { front: [0, 0, 0, 0, 0], side: [0.64, 0, 0, 0, 0], rear: [0.72, 0, 0, 0, 0] }, M7: { front: [0.5, 0.5, 0, 0, 0], side: [1, 1, 0, 0, 0], rear: [1, 1, 0, 0, 0] }, M8: { front: [0.36, 0.03, 0, 0, 0], side: [1, 0.68, 0.48, 0, 0], rear: [1, 0.74, 0.65, 0, 0] }, Matilda: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, Sexton: { front: [0.5, 0, 0, 0, 0], side: [1, 1, 0, 0, 0], rear: [0.5, 0.5, 0, 0, 0] }, CharB1: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, M13: { front: [0.11, 0, 0, 0, 0], side: [0.93, 0, 0, 0, 0], rear: [0.84, 0, 0, 0, 0] }, Semovente: { front: [0.02, 0, 0, 0, 0], side: [1, 0, 0, 0, 0], rear: [0.97, 0, 0, 0, 0] }, L3: { front: [1, 1, 0, 0, 0], side: [1, 1, 1, 0, 0], rear: [1, 1, 1, 0, 0] }, Type97: { front: [0.45, 0, 0, 0, 0], side: [0.54, 0.02, 0, 0, 0], rear: [0.88, 0, 0, 0, 0] }, "Type97:47": { front: [0.45, 0, 0, 0, 0], side: [0.54, 0.02, 0, 0, 0], rear: [0.88, 0, 0, 0, 0] }, "7TP": { front: [1, 0.61, 0, 0, 0], side: [1, 1, 0, 0, 0], rear: [1, 1, 0.6, 0, 0] }, LightTank: { front: [0.57, 0, 0, 0, 0], side: [1, 1, 0, 0, 0], rear: [1, 1, 0, 0, 0] }, MediumTank: { front: [0, 0, 0, 0, 0], side: [0.02, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, AssaultGun: { front: [0, 0, 0, 0, 0], side: [0.02, 0, 0, 0, 0], rear: [0.27, 0, 0, 0, 0] }, SelfPropelledGun: { front: [0.5, 0.5, 0, 0, 0], side: [0.61, 0.6, 0.48, 0, 0], rear: [0.5, 0.5, 0.43, 0, 0] }, ArmouredCar: { front: [1, 0.64, 0, 0, 0], side: [1, 1, 0.52, 0, 0], rear: [1, 1, 0.65, 0, 0] }, WhatIf_BreakthroughTank: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, WhatIf_TankHunterCar: { front: [0, 0, 0, 0, 0], side: [1, 1, 0, 0, 0], rear: [1, 1, 0.57, 0, 0] }, WhatIf_FlameTank: { front: [0, 0, 0, 0, 0], side: [0.02, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, WhatIf_AmphibiousTank: { front: [0, 0, 0, 0, 0], side: [1, 1, 0.05, 0, 0], rear: [1, 1, 0.56, 0, 0] }, WhatIf_SnowTank: { front: [0, 0, 0, 0, 0], side: [1, 0.06, 0, 0, 0], rear: [1, 0.66, 0, 0, 0] }, WhatIf_GliderTank: { front: [1, 0.9, 0, 0, 0], side: [1, 1, 0.61, 0, 0], rear: [1, 1, 0.66, 0, 0] } }, WhatIf_BreakthroughTank: { Tiger: { front: [0.91, 0.65, 0.17, 0, 0], side: [1, 1, 1, 0.9, 0.38], rear: [1, 1, 1, 0.82, 0.14] }, Panther: { front: [0.46, 0.34, 0.1, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, PanzerIV: { front: [1, 1, 1, 0.83, 0.41], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "PanzerIV:short": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, StuG3: { front: [1, 1, 1, 0.62, 0.06], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hummel: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hanomag: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SdKfz222: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, T34: { front: [1, 1, 0.83, 0.52, 0.35], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "T34-85": { front: [1, 1, 0.76, 0.25, 0.05], side: [1, 1, 1, 0.93, 0.69], rear: [1, 1, 1, 1, 1] }, KV1: { front: [1, 1, 0.89, 0.3, 0], side: [1, 1, 1, 0.98, 0.39], rear: [1, 1, 1, 0.98, 0.47] }, SU85: { front: [1, 1, 1, 1, 0.67], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 0.87] }, SU76: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Sherman: { front: [1, 1, 0.73, 0.28, 0.22], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M10: { front: [1, 1, 1, 0.85, 0.63], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M7: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M8: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Matilda: { front: [1, 1, 1, 0.87, 0.27], side: [1, 1, 1, 1, 0.68], rear: [1, 1, 1, 1, 0.81] }, Sexton: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, CharB1: { front: [1, 1, 0.97, 0.74, 0.53], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M13: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Semovente: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, L3: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Type97: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "Type97:47": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "7TP": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, LightTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, MediumTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, AssaultGun: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SelfPropelledGun: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, ArmouredCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_BreakthroughTank: { front: [0.69, 0.25, 0.01, 0, 0], side: [1, 1, 1, 0.92, 0.56], rear: [1, 1, 1, 1, 0.9] }, WhatIf_TankHunterCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_FlameTank: { front: [1, 1, 1, 1, 0.81], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_AmphibiousTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_SnowTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_GliderTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, WhatIf_TankHunterCar: { Tiger: { front: [0.91, 0.65, 0.17, 0, 0], side: [1, 1, 1, 0.9, 0.38], rear: [1, 1, 1, 0.82, 0.14] }, Panther: { front: [0.46, 0.34, 0.1, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, PanzerIV: { front: [1, 1, 1, 0.83, 0.41], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "PanzerIV:short": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, StuG3: { front: [1, 1, 1, 0.62, 0.06], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hummel: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Hanomag: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SdKfz222: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, T34: { front: [1, 1, 0.83, 0.52, 0.35], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "T34-85": { front: [1, 1, 0.76, 0.25, 0.05], side: [1, 1, 1, 0.93, 0.69], rear: [1, 1, 1, 1, 1] }, KV1: { front: [1, 1, 0.89, 0.3, 0], side: [1, 1, 1, 0.98, 0.39], rear: [1, 1, 1, 0.98, 0.47] }, SU85: { front: [1, 1, 1, 1, 0.67], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 0.87] }, SU76: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Sherman: { front: [1, 1, 0.73, 0.28, 0.22], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M10: { front: [1, 1, 1, 0.85, 0.63], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M7: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M8: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Matilda: { front: [1, 1, 1, 0.87, 0.27], side: [1, 1, 1, 1, 0.68], rear: [1, 1, 1, 1, 0.81] }, Sexton: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, CharB1: { front: [1, 1, 0.97, 0.74, 0.53], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M13: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Semovente: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, L3: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Type97: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "Type97:47": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, "7TP": { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, LightTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, MediumTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, AssaultGun: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SelfPropelledGun: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, ArmouredCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_BreakthroughTank: { front: [0.69, 0.25, 0.01, 0, 0], side: [1, 1, 1, 0.92, 0.56], rear: [1, 1, 1, 1, 0.9] }, WhatIf_TankHunterCar: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_FlameTank: { front: [1, 1, 1, 1, 0.81], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_AmphibiousTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_SnowTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_GliderTank: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, WhatIf_AmphibiousTank: { Tiger: { front: [0, 0, 0, 0, 0], side: [0.01, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, Panther: { front: [0, 0, 0, 0, 0], side: [0.53, 0.3, 0, 0, 0], rear: [0.79, 0.03, 0, 0, 0] }, PanzerIV: { front: [0.2, 0, 0, 0, 0], side: [1, 1, 0.81, 0, 0], rear: [1, 1, 0.94, 0.65, 0.02] }, "PanzerIV:short": { front: [0.6, 0, 0, 0, 0], side: [1, 1, 0.81, 0, 0], rear: [1, 1, 0.94, 0.65, 0.02] }, StuG3: { front: [0, 0, 0, 0, 0], side: [1, 1, 0.94, 0, 0], rear: [0.76, 0.3, 0.29, 0, 0] }, Hummel: { front: [1, 1, 0.85, 0.5, 0.5], side: [1, 1, 1, 1, 0.63], rear: [1, 1, 1, 1, 0.52] }, Hanomag: { front: [1, 1, 1, 1, 0.84], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SdKfz222: { front: [1, 1, 1, 1, 0.58], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, T34: { front: [0, 0, 0, 0, 0], side: [0.43, 0.11, 0, 0, 0], rear: [0.05, 0, 0, 0, 0] }, "T34-85": { front: [0, 0, 0, 0, 0], side: [0.35, 0.11, 0, 0, 0], rear: [0.16, 0, 0, 0, 0] }, KV1: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, SU85: { front: [0.01, 0, 0, 0, 0], side: [0.92, 0.17, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, SU76: { front: [0.76, 0.58, 0.45, 0.02, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Sherman: { front: [0, 0, 0, 0, 0], side: [0.89, 0.7, 0.03, 0, 0], rear: [0.9, 0.67, 0.01, 0, 0] }, M10: { front: [0, 0, 0, 0, 0], side: [1, 1, 1, 0.53, 0], rear: [1, 1, 1, 0.58, 0] }, M7: { front: [0.5, 0.5, 0.5, 0.5, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M8: { front: [1, 1, 0.87, 0.38, 0.04], side: [1, 1, 1, 1, 0.69], rear: [1, 1, 1, 1, 0.74] }, Matilda: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0.2, 0, 0, 0, 0] }, Sexton: { front: [0.5, 0.5, 0.5, 0.5, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 0.52, 0.5, 0.5] }, CharB1: { front: [0.06, 0, 0, 0, 0], side: [0.23, 0, 0, 0, 0], rear: [0.58, 0, 0, 0, 0] }, M13: { front: [1, 0.99, 0.59, 0.05, 0], side: [1, 1, 1, 0.85, 0], rear: [1, 1, 1, 0.71, 0] }, Semovente: { front: [1, 0.57, 0.4, 0, 0], side: [1, 1, 1, 0.95, 0], rear: [1, 1, 1, 0.85, 0] }, L3: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Type97: { front: [1, 1, 1, 0.3, 0], side: [1, 1, 1, 0.46, 0.02], rear: [1, 1, 1, 0.82, 0.01] }, "Type97:47": { front: [1, 1, 1, 0.3, 0], side: [1, 1, 1, 0.46, 0.02], rear: [1, 1, 1, 0.82, 0.01] }, "7TP": { front: [1, 1, 1, 1, 0.65], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, LightTank: { front: [1, 1, 1, 0.38, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, MediumTank: { front: [0.21, 0, 0, 0, 0], side: [1, 1, 0.93, 0, 0], rear: [1, 1, 0.9, 0, 0] }, AssaultGun: { front: [0, 0, 0, 0, 0], side: [1, 1, 0.95, 0, 0], rear: [1, 1, 0.95, 0.19, 0] }, SelfPropelledGun: { front: [0.5, 0.5, 0.5, 0.5, 0.5], side: [1, 1, 0.99, 0.6, 0.6], rear: [1, 1, 0.96, 0.5, 0.5] }, ArmouredCar: { front: [1, 1, 1, 1, 0.69], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_BreakthroughTank: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0.43, 0, 0, 0, 0] }, WhatIf_TankHunterCar: { front: [1, 1, 0.54, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_FlameTank: { front: [0, 0, 0, 0, 0], side: [1, 1, 0.93, 0, 0], rear: [1, 1, 0.9, 0, 0] }, WhatIf_AmphibiousTank: { front: [1, 1, 0.96, 0.01, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_SnowTank: { front: [1, 0.51, 0.07, 0, 0], side: [1, 1, 1, 1, 0.06], rear: [1, 1, 1, 1, 0.66] }, WhatIf_GliderTank: { front: [1, 1, 1, 1, 0.95], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, WhatIf_SnowTank: { Tiger: { front: [0, 0, 0, 0, 0], side: [0.01, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, Panther: { front: [0, 0, 0, 0, 0], side: [0.53, 0.3, 0, 0, 0], rear: [0.79, 0.03, 0, 0, 0] }, PanzerIV: { front: [0.2, 0, 0, 0, 0], side: [1, 1, 0.81, 0, 0], rear: [1, 1, 0.94, 0.65, 0.02] }, "PanzerIV:short": { front: [0.6, 0, 0, 0, 0], side: [1, 1, 0.81, 0, 0], rear: [1, 1, 0.94, 0.65, 0.02] }, StuG3: { front: [0, 0, 0, 0, 0], side: [1, 1, 0.94, 0, 0], rear: [0.76, 0.3, 0.29, 0, 0] }, Hummel: { front: [1, 1, 0.85, 0.5, 0.5], side: [1, 1, 1, 1, 0.63], rear: [1, 1, 1, 1, 0.52] }, Hanomag: { front: [1, 1, 1, 1, 0.84], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SdKfz222: { front: [1, 1, 1, 1, 0.58], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, T34: { front: [0, 0, 0, 0, 0], side: [0.43, 0.11, 0, 0, 0], rear: [0.05, 0, 0, 0, 0] }, "T34-85": { front: [0, 0, 0, 0, 0], side: [0.35, 0.11, 0, 0, 0], rear: [0.16, 0, 0, 0, 0] }, KV1: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, SU85: { front: [0.01, 0, 0, 0, 0], side: [0.92, 0.17, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, SU76: { front: [0.76, 0.58, 0.45, 0.02, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Sherman: { front: [0, 0, 0, 0, 0], side: [0.89, 0.7, 0.03, 0, 0], rear: [0.9, 0.67, 0.01, 0, 0] }, M10: { front: [0, 0, 0, 0, 0], side: [1, 1, 1, 0.53, 0], rear: [1, 1, 1, 0.58, 0] }, M7: { front: [0.5, 0.5, 0.5, 0.5, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M8: { front: [1, 1, 0.87, 0.38, 0.04], side: [1, 1, 1, 1, 0.69], rear: [1, 1, 1, 1, 0.74] }, Matilda: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0.2, 0, 0, 0, 0] }, Sexton: { front: [0.5, 0.5, 0.5, 0.5, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 0.52, 0.5, 0.5] }, CharB1: { front: [0.06, 0, 0, 0, 0], side: [0.23, 0, 0, 0, 0], rear: [0.58, 0, 0, 0, 0] }, M13: { front: [1, 0.99, 0.59, 0.05, 0], side: [1, 1, 1, 0.85, 0], rear: [1, 1, 1, 0.71, 0] }, Semovente: { front: [1, 0.57, 0.4, 0, 0], side: [1, 1, 1, 0.95, 0], rear: [1, 1, 1, 0.85, 0] }, L3: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Type97: { front: [1, 1, 1, 0.3, 0], side: [1, 1, 1, 0.46, 0.02], rear: [1, 1, 1, 0.82, 0.01] }, "Type97:47": { front: [1, 1, 1, 0.3, 0], side: [1, 1, 1, 0.46, 0.02], rear: [1, 1, 1, 0.82, 0.01] }, "7TP": { front: [1, 1, 1, 1, 0.65], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, LightTank: { front: [1, 1, 1, 0.38, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, MediumTank: { front: [0.21, 0, 0, 0, 0], side: [1, 1, 0.93, 0, 0], rear: [1, 1, 0.9, 0, 0] }, AssaultGun: { front: [0, 0, 0, 0, 0], side: [1, 1, 0.95, 0, 0], rear: [1, 1, 0.95, 0.19, 0] }, SelfPropelledGun: { front: [0.5, 0.5, 0.5, 0.5, 0.5], side: [1, 1, 0.99, 0.6, 0.6], rear: [1, 1, 0.96, 0.5, 0.5] }, ArmouredCar: { front: [1, 1, 1, 1, 0.69], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_BreakthroughTank: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0.43, 0, 0, 0, 0] }, WhatIf_TankHunterCar: { front: [1, 1, 0.54, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_FlameTank: { front: [0, 0, 0, 0, 0], side: [1, 1, 0.93, 0, 0], rear: [1, 1, 0.9, 0, 0] }, WhatIf_AmphibiousTank: { front: [1, 1, 0.96, 0.01, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_SnowTank: { front: [1, 0.51, 0.07, 0, 0], side: [1, 1, 1, 1, 0.06], rear: [1, 1, 1, 1, 0.66] }, WhatIf_GliderTank: { front: [1, 1, 1, 1, 0.95], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } }, WhatIf_GliderTank: { Tiger: { front: [0, 0, 0, 0, 0], side: [0.01, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, Panther: { front: [0, 0, 0, 0, 0], side: [0.53, 0.3, 0, 0, 0], rear: [0.79, 0.03, 0, 0, 0] }, PanzerIV: { front: [0.2, 0, 0, 0, 0], side: [1, 1, 0.81, 0, 0], rear: [1, 1, 0.94, 0.65, 0.02] }, "PanzerIV:short": { front: [0.6, 0, 0, 0, 0], side: [1, 1, 0.81, 0, 0], rear: [1, 1, 0.94, 0.65, 0.02] }, StuG3: { front: [0, 0, 0, 0, 0], side: [1, 1, 0.94, 0, 0], rear: [0.76, 0.3, 0.29, 0, 0] }, Hummel: { front: [1, 1, 0.85, 0.5, 0.5], side: [1, 1, 1, 1, 0.63], rear: [1, 1, 1, 1, 0.52] }, Hanomag: { front: [1, 1, 1, 1, 0.84], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, SdKfz222: { front: [1, 1, 1, 1, 0.58], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, T34: { front: [0, 0, 0, 0, 0], side: [0.43, 0.11, 0, 0, 0], rear: [0.05, 0, 0, 0, 0] }, "T34-85": { front: [0, 0, 0, 0, 0], side: [0.35, 0.11, 0, 0, 0], rear: [0.16, 0, 0, 0, 0] }, KV1: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, SU85: { front: [0.01, 0, 0, 0, 0], side: [0.92, 0.17, 0, 0, 0], rear: [0, 0, 0, 0, 0] }, SU76: { front: [0.76, 0.58, 0.45, 0.02, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Sherman: { front: [0, 0, 0, 0, 0], side: [0.89, 0.7, 0.03, 0, 0], rear: [0.9, 0.67, 0.01, 0, 0] }, M10: { front: [0, 0, 0, 0, 0], side: [1, 1, 1, 0.53, 0], rear: [1, 1, 1, 0.58, 0] }, M7: { front: [0.5, 0.5, 0.5, 0.5, 0.5], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, M8: { front: [1, 1, 0.87, 0.38, 0.04], side: [1, 1, 1, 1, 0.69], rear: [1, 1, 1, 1, 0.74] }, Matilda: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0.2, 0, 0, 0, 0] }, Sexton: { front: [0.5, 0.5, 0.5, 0.5, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 0.52, 0.5, 0.5] }, CharB1: { front: [0.06, 0, 0, 0, 0], side: [0.23, 0, 0, 0, 0], rear: [0.58, 0, 0, 0, 0] }, M13: { front: [1, 0.99, 0.59, 0.05, 0], side: [1, 1, 1, 0.85, 0], rear: [1, 1, 1, 0.71, 0] }, Semovente: { front: [1, 0.57, 0.4, 0, 0], side: [1, 1, 1, 0.95, 0], rear: [1, 1, 1, 0.85, 0] }, L3: { front: [1, 1, 1, 1, 1], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, Type97: { front: [1, 1, 1, 0.3, 0], side: [1, 1, 1, 0.46, 0.02], rear: [1, 1, 1, 0.82, 0.01] }, "Type97:47": { front: [1, 1, 1, 0.3, 0], side: [1, 1, 1, 0.46, 0.02], rear: [1, 1, 1, 0.82, 0.01] }, "7TP": { front: [1, 1, 1, 1, 0.65], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, LightTank: { front: [1, 1, 1, 0.38, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, MediumTank: { front: [0.21, 0, 0, 0, 0], side: [1, 1, 0.93, 0, 0], rear: [1, 1, 0.9, 0, 0] }, AssaultGun: { front: [0, 0, 0, 0, 0], side: [1, 1, 0.95, 0, 0], rear: [1, 1, 0.95, 0.19, 0] }, SelfPropelledGun: { front: [0.5, 0.5, 0.5, 0.5, 0.5], side: [1, 1, 0.99, 0.6, 0.6], rear: [1, 1, 0.96, 0.5, 0.5] }, ArmouredCar: { front: [1, 1, 1, 1, 0.69], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_BreakthroughTank: { front: [0, 0, 0, 0, 0], side: [0, 0, 0, 0, 0], rear: [0.43, 0, 0, 0, 0] }, WhatIf_TankHunterCar: { front: [1, 1, 0.54, 0, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_FlameTank: { front: [0, 0, 0, 0, 0], side: [1, 1, 0.93, 0, 0], rear: [1, 1, 0.9, 0, 0] }, WhatIf_AmphibiousTank: { front: [1, 1, 0.96, 0.01, 0], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] }, WhatIf_SnowTank: { front: [1, 0.51, 0.07, 0, 0], side: [1, 1, 1, 1, 0.06], rear: [1, 1, 1, 1, 0.66] }, WhatIf_GliderTank: { front: [1, 1, 1, 1, 0.95], side: [1, 1, 1, 1, 1], rear: [1, 1, 1, 1, 1] } } } };

  // ../../src/battle/land/armour.ts
  var ARMOUR = armour_default;
  var faceOfAspect = (a) => a === "flank" ? "side" : a;
  var DRAWN_TANKS = ["PanzerIV:short", "PanzerIV", "Panther", "Matilda", "Sherman", "LightTank", "T34", "T34-85", "CharB1", "M13", "Type97", "7TP", "MediumTank"];
  var PAR = (() => {
    const out = { front: 0, side: 0, rear: 0 };
    for (const f of ["front", "side", "rear"]) {
      let sum = 0, n = 0;
      for (const s of DRAWN_TANKS) for (const t of DRAWN_TANKS) {
        const c = ARMOUR.duels[s]?.[t];
        if (c) {
          sum += c[f][1] * ARMOUR.models[s].kills;
          n++;
        }
      }
      out[f] = n ? sum / n : 1;
    }
    return out;
  })();
  function vehicleOf(kind, nat, turn3) {
    const d = turnToDate(turn3);
    let m = modelOf(kind, nat, d.year);
    if (m === "PanzerIV" && d.year * 12 + d.month + 1 < 1942 * 12 + 3) m = "PanzerIV:short";
    if (m === "Type97" && d.year * 12 + d.month + 1 >= 1942 * 12 + 4 && ARMOUR.models["Type97:47"]) m = "Type97:47";
    return m && ARMOUR.models[m] && (kind !== "arm" || ARMOUR.duels[m]) ? m : null;
  }
  function through(shooter, target, face, range, turn3) {
    const row = ARMOUR.duels[shooter]?.[target];
    if (!row) return null;
    const heat = ARMOUR.models[shooter]?.heat;
    const d = turnToDate(turn3);
    const before = heat && row.before && d.year * 100 + d.month + 1 < Number(heat.slice(0, 4)) * 100 + Number(heat.slice(5, 7));
    const c = (before ? row.before : row)[face], R = ARMOUR.ranges;
    if (range <= R[0]) return c[0];
    for (let i = 1; i < R.length; i++) if (range <= R[i]) return c[i - 1] + (c[i] - c[i - 1]) * (range - R[i - 1]) / (R[i] - R[i - 1]);
    return c[R.length - 1];
  }
  function armourWeight(shooter, target, face, range, turn3, lo, hi) {
    const c = through(shooter, target, face, range, turn3);
    if (c === null) return null;
    return Math.min(hi, Math.max(lo, c * ARMOUR.models[shooter].kills / PAR[face]));
  }
  function fateOf(target, face, r13, r22) {
    const m = target ? ARMOUR.models[target] : void 0;
    const fire = m?.fire?.[face] ?? 0.5;
    if (r13 >= fire) return "out";
    return r22 < (m?.explodes ?? 0.2) ? "blow" : "burn";
  }

  // ../../src/battle/land/units.ts
  var ORD = (n) => {
    const s = ["th", "st", "nd", "rd"], v = n % 100;
    return n + (s[(v - 20) % 10] || s[v] || s[0]);
  };
  var KIND_WORD = { inf: "Infantry", mtn: "Mountain", cav: "Cavalry", eng: "Engineers", mot: "Motorized", arm: "Armoured", art: "Artillery", fort: "Fortress" };
  var TEAM_WORD = { mg: "Machine-Gun Battalion", mor: "Mortar Battalion" };
  var teamOf = (f) => TEAMS.includes(f.kind) ? f.kind : void 0;
  function partsFor(forces, least = LAND.blocks.least) {
    const parts = forces.map((f) => Array.from({ length: Math.max(1, Math.round(f.divs)) }, (_, i) => ({ divs: [i] })));
    const total = () => parts.reduce((s, p, i) => s + (teamOf(forces[i]) ? 0 : p.length), 0);
    while (total() < least) {
      let best = -1, size = 0;
      parts.forEach((p2, i) => {
        if (teamOf(forces[i]) || !p2.some((x) => x.regiment === void 0 && x.divs.length === 1)) return;
        const each = forces[i].share / p2.length;
        if (each > size) {
          size = each;
          best = i;
        }
      });
      if (best < 0) break;
      const p = parts[best], j = p.findIndex((x) => x.regiment === void 0 && x.divs.length === 1);
      p.splice(j, 1, ...Array.from({ length: LAND.blocks.regiments }, (_, r) => ({ divs: p[j].divs, regiment: r })));
    }
    return parts;
  }
  var BRIGADES = ["UK", "CAN", "AUS", "NZL", "NZ", "SAF", "IND"];
  function kindWord(nat, k, ym = 0) {
    if (k === "arm") return nat === "USA" ? "Armored" : nat === "GER" ? "Panzer" : nat === "SOV" || nat === "JAP" ? "Tank" : "Armoured";
    if (k === "mot") {
      if (nat === "USA" || nat === "SOV") return "Motorized";
      if (BRIGADES.includes(nat)) return "Motor";
      if (nat === "GER") return ym >= 1943 * 12 + 6 ? "Panzergrenadier" : "Motorized";
      return "Motorised";
    }
    if (k === "inf" && nat === "SOV") return "Rifle";
    if (k === "mtn" && nat === "SOV") return "Mountain Rifle";
    return KIND_WORD[k];
  }
  function regimentWord(nat, kind, n, i, ym) {
    const o = ORD(n);
    if (kind === "arm") {
      if (nat === "USA") return `Combat Command ${"ABR"[i] ?? "R"}`;
      if (nat === "GER") return `Panzer-Regiment ${n}`;
      if (nat === "SOV") return ym >= 1942 * 12 + 1 ? `${o} Tank Brigade` : `${o} Tank Regiment`;
      if (BRIGADES.includes(nat)) return `${o} Armoured Brigade`;
      return `${o} Tank Regiment`;
    }
    const arm = kind === "mtn" ? "Mountain" : kind === "cav" ? "Cavalry" : kind === "mot" ? kindWord(nat, "mot", ym) : kind === "eng" ? "Engineer" : "Infantry";
    if (BRIGADES.includes(nat)) return `${o} ${arm} Brigade`;
    if (nat === "GER") {
      const late = ym >= 1942 * 12 + 10;
      const word = kind === "mtn" ? "Gebirgsjäger" : kind === "cav" ? "Reiter" : kind === "mot" ? late ? "Panzergrenadier" : "Infanterie" : kind === "eng" ? "Pionier" : late ? "Grenadier" : "Infanterie";
      return `${word}-Regiment ${n}${kind === "mot" && !late ? " (mot.)" : ""}`;
    }
    if (nat === "SOV") return `${o} ${kind === "mtn" ? "Mountain Rifle" : kind === "cav" ? "Cavalry" : kind === "mot" ? "Motor Rifle" : kind === "eng" ? "Engineer" : "Rifle"} Regiment`;
    if (nat === "ITA" && i === 2 && (kind === "inf" || kind === "mtn")) return `${o} Blackshirt Legion`;
    return `${o} ${arm} Regiment`;
  }
  function armyNames(t, units, turn3) {
    const d = turnToDate(turn3), ym = d.year * 12 + d.month + 1;
    const cut2 = partsFor(t.forces);
    let j = 0;
    t.forces.forEach((f, i) => {
      const team = teamOf(f);
      const k = team ? "art" : Object.keys(LAND.speed).includes(f.kind) ? f.kind : "inf";
      for (const p of cut2[i]) {
        const u = units[j++];
        if (!u || p.regiment === void 0 || team) continue;
        const div = f.divisions?.[p.divs[0]], no = (f.first ?? 1) + p.divs[0];
        const full = div?.name ?? `${ORD(no)} ${kindWord(f.nat, k, ym)} Division`, short = div?.short ?? `${ORD(no)} ${kindWord(f.nat, k, ym)}`;
        const lead = /^(\d+)/.exec(full)?.[1];
        const n = lead ? (Number(lead) - 1) * LAND.blocks.regiments + p.regiment + 1 : p.regiment + 1;
        u.short = regimentWord(u.nat, k, n, p.regiment, ym);
        u.parent = full;
        u.name = `${u.short}, ${short}`;
      }
    });
  }
  function makeBlocks(side, t, power, firstId, least = LAND.blocks.least, turn3) {
    const out = [];
    const ym = turn3 === void 0 ? 0 : ((d) => d.year * 12 + d.month + 1)(turnToDate(turn3));
    const cut2 = partsFor(t.forces, least);
    t.forces.forEach((f, i) => {
      const team = teamOf(f);
      const k = team ? "art" : Object.keys(LAND.speed).includes(f.kind) ? f.kind : "inf";
      const n = Math.max(1, Math.round(f.divs));
      const divs = Array.from({ length: n }, (_, d) => {
        const own = f.divisions?.[d];
        const no = (f.first ?? 1) + d;
        return {
          name: own?.name ?? (team ? `${ORD(no)} ${TEAM_WORD[team]}` : `${ORD(no)} ${kindWord(f.nat, k, ym)} Division`),
          short: own?.short ?? (team ? `${ORD(no)} ${TEAM_WORD[team].split(" ")[0]}` : `${ORD(no)} ${kindWord(f.nat, k, ym)}`),
          str: own?.str ?? f.str,
          xp: own?.xp ?? f.xp,
          men: own?.men ?? f.men / n
        };
      });
      const weightOf2 = (p) => p.divs.reduce((s, d) => s + Math.max(0.01, divs[d].str), 0) / (p.regiment !== void 0 ? LAND.blocks.regiments : 1);
      const all = cut2[i].reduce((s, p) => s + weightOf2(p), 0);
      for (const p of cut2[i]) {
        const ds = p.divs.map((d2) => divs[d2]);
        const name = p.regiment !== void 0 ? `${ORD(p.regiment + 1)} Regiment, ${ds[0].short}` : ds.length === 1 ? ds[0].name : ds.map((d2) => d2.short).join(" and ");
        const short = p.regiment !== void 0 ? `${ORD(p.regiment + 1)} Regiment` : ds.length > 1 ? name : ds[0].short;
        const parent = p.regiment !== void 0 ? ds[0].short : f.name;
        const [w, d] = team ? LAND.teams[team].size : LAND.size[k];
        const per = team ? LAND.teams[team].figures : LAND.figures[k];
        const full = p.regiment !== void 0 ? Math.max(1, Math.round(per / LAND.blocks.regiments)) : per;
        const str0 = Math.max(0.05, Math.min(1, ds.reduce((s, x) => s + x.str, 0) / ds.length));
        const mor0 = Math.min(1, 0.45 + 0.35 * f.org + 0.2 * (f.mor ?? 0.6));
        const share = weightOf2(p) / all;
        out.push({
          id: firstId + out.length,
          side,
          corps: f.corps,
          nat: f.nat,
          kind: k,
          ...team ? { team } : {},
          name,
          short,
          parent,
          x: 0,
          y: 0,
          face: side === "att" ? 0 : Math.PI,
          str: str0,
          str0,
          mor: mor0,
          mor0,
          xp: ds.reduce((s, x) => s + x.xp, 0) / ds.length,
          state: "steady",
          men0: Math.round(ds.reduce((s, x) => s + x.men, 0) / (p.regiment !== void 0 ? LAND.blocks.regiments : 1)),
          figures0: Math.max(1, Math.ceil(full * str0)),
          figures: Math.max(1, Math.ceil(full * str0)),
          tray: full,
          w,
          d,
          act: "idle",
          speed: 0,
          power0: power * f.share * share,
          worth: weightOf2(p),
          order: null,
          path: [],
          dug: !!f.dug,
          home: [0, 0],
          still: 99,
          clear: 99,
          pinned: false,
          flankedAt: -99,
          keepFace: null,
          met: false,
          ...f.cross === false ? { dry: true } : {},
          ...f.wear !== void 0 ? { wear: f.wear } : {},
          ...f.fresh ? { fresh: f.fresh } : {},
          ...f.kit ? { kit: f.kit } : {}
        });
      }
    });
    return out;
  }
  function joinBlocks(t, power, firstId) {
    const out = [];
    for (const side of ["att", "def"]) {
      const js = (t[side].joiners ?? []).filter((f) => Number.isFinite(f.joined) && f.joined > 0 && f.share > 0);
      if (!js.length) continue;
      for (const u of makeBlocks(side, { ...t[side], forces: js }, power[side], firstId + out.length, 0, t.turn)) {
        const j = Math.min(1, js.find((f) => f.corps === u.corps).joined);
        u.joined = j;
        u.priced = u.power0;
        u.power0 = u.priced / j;
        u.dug = false;
        out.push(u);
      }
    }
    return out;
  }
  var FRONT = ["inf", "mtn", "eng", "fort", "cav"];
  var MOBILE = ["arm", "mot"];
  var GAP = 40;
  function rowsOf(mine) {
    const lines = [mine.filter((u) => FRONT.includes(u.kind)), mine.filter((u) => MOBILE.includes(u.kind)), mine.filter((u) => u.kind === "art")];
    if (!lines[0].length && lines[1].length) lines[0] = lines[1].splice(0, Math.ceil(lines[1].length / 2));
    return lines;
  }
  var rowWidth = (row) => row.reduce((s, u) => s + u.w + GAP, -GAP);
  function bendAt(units, z, dry) {
    const att = units.filter((u) => u.side === "att");
    const need2 = (us) => Math.max(0, ...rowsOf(us).map(rowWidth));
    const a = need2(att.filter((u) => u.dry)), b = need2(att.filter((u) => !u.dry)), room = z[2] - z[0] - 2 * BEND.clear;
    const span2 = a + b > 0 ? room >= a + b ? a + (room - a - b) * a / (a + b) : room * a / (a + b) : room / 2;
    return dry === "west" ? z[0] + span2 + BEND.clear : z[2] - span2 - BEND.clear;
  }
  function deploy(units, side, f) {
    const mine = units.filter((u) => u.side === side);
    const z = f.zones[side];
    if (side === "att" && f.bend) {
      const B = f.bend, west = B.dry === "west", inner = (s) => B.x + s * BEND.clear;
      const lanes = [
        [mine.filter((u) => u.dry), west ? 10 : inner(1), west ? inner(-1) : f.w - 10],
        [mine.filter((u) => !u.dry), west ? inner(1) : 10, west ? f.w - 10 : inner(-1)]
      ];
      for (const [us, lo, hi] of lanes) if (us.length) drawUp(us, side, f, (Math.max(lo, z[0]) + Math.min(hi, z[2])) / 2, lo, hi);
    } else if (f.stream && (f.stream.lie === "up" || f.stream.lie === "shore" && f.stream.side !== "back")) streamLanes(mine, side, f);
    else if (side === "def" && f.stream?.lie === "across" && f.stream.join && joinLane(mine, f)) {
    } else drawUp(mine, side, f, side === "def" && f.town ? Math.min(z[2] - 300, Math.max(z[0] + 300, f.town.x)) : (z[0] + z[2]) / 2);
    for (const u of mine) {
      u.x = Math.min(f.w - u.w / 2 - 10, Math.max(u.w / 2 + 10, u.x));
      u.y = Math.min(f.h - 40, Math.max(40, u.y));
      if (f.stream) {
        for (let k = 0; k < 40 && wetUnder(f, u); k++) u.y += side === "att" ? 10 : -10;
        const y0 = u.y;
        for (let k = 0; k < 8 && wetUnder(f, u, STREAM.off); k++) u.y += side === "att" ? 10 : -10;
        const onTop = mine.some((o) => o !== u && Math.abs(o.x - u.x) < (o.w + u.w) / 2 && Math.abs(o.y - u.y) < (o.d + u.d) / 2);
        if (wetUnder(f, u, STREAM.off) || (side === "att" ? u.y > z[3] : u.y < z[1]) || onTop) u.y = y0;
      }
      u.home = [u.x, u.y];
      if (u.dug) digWorks(f, u.x, u.y, u.w, u.d);
    }
  }
  function wetUnder(f, u, off = 0) {
    const hw = u.w / 2 + off, hd = u.d / 2 + off, nx = Math.ceil(2 * hw / (f.cell / 2)), ny = Math.ceil(2 * hd / (f.cell / 2));
    for (let i = 0; i <= nx; i++) for (let j = 0; j <= ny; j++) {
      const c = Math.floor((u.x - hw + 2 * hw * i / nx) / f.cell), r = Math.floor((u.y - hd + 2 * hd * j / ny) / f.cell);
      if (c < 0 || r < 0 || c >= f.cols || r >= f.rows) continue;
      const k = GROUNDS[f.ground[r * f.cols + c]];
      if (k === "water" || k === "bridge" || k === "blown") return true;
    }
    return false;
  }
  function joinLane(mine, f) {
    const J = f.stream.join, z = f.zones.def, y0 = z[1] - 120;
    const xs = J.course.filter(([x, y]) => y >= y0 && x >= 0 && x <= f.w).map(([x]) => x);
    if (!xs.length) return false;
    const clear = J.half * (1 + STREAM.wobble) + f.cell + STREAM.off, west = J.course[0][0] < f.w / 2;
    const [a, b] = west ? [Math.max(...xs) + clear, f.w - 10] : [10, Math.min(...xs) - clear];
    const centre = f.town ? Math.min(z[2] - 300, Math.max(z[0] + 300, f.town.x)) : (z[0] + z[2]) / 2;
    drawUp(mine, "def", f, Math.min(b, Math.max(a, centre)), a, b);
    return true;
  }
  function streamLanes(mine, side, f) {
    const S = f.stream, z = f.zones[side], clear = S.half * (1 + STREAM.wobble) + f.cell + STREAM.off;
    const y0 = side === "att" ? 0 : z[1] - 120, y1 = side === "att" ? z[3] + 120 : f.h;
    const xs = S.course.filter(([, y]) => y >= y0 && y <= y1).map(([x]) => x);
    const lo = Math.min(...xs), hi = Math.max(...xs);
    const lane = (a, b) => [a, b, Math.max(0, Math.min(b, z[2]) - Math.max(a, z[0]))];
    const lanes = S.lie === "shore" ? [lo < f.w / 2 ? lane(hi + STREAM.clear, f.w - 10) : lane(10, lo - STREAM.clear)] : [lane(10, lo - clear), lane(hi + clear, f.w - 10)];
    const open = lanes.filter((l) => l[2] >= 200);
    const groups = open.map(() => []), fill = open.map(() => 0);
    const home = f.town ? open.findIndex(([a, b]) => f.town.x >= a && f.town.x <= b) : -1;
    if (home >= 0 && rowWidth(rowsOf(mine)[0]) <= open[home][2]) groups[home].push(...mine);
    else for (const u of mine) {
      let k = 0;
      for (let j = 1; j < open.length; j++) if ((fill[j] + u.w) / open[j][2] < (fill[k] + u.w) / open[k][2]) k = j;
      groups[k].push(u);
      fill[k] += u.w;
    }
    const centre = side === "def" && f.town ? Math.min(z[2] - 300, Math.max(z[0] + 300, f.town.x)) : (z[0] + z[2]) / 2;
    open.forEach(([a, b], j) => {
      if (groups[j].length) drawUp(groups[j], side, f, Math.min(b, Math.max(a, centre)), a, b);
    });
  }
  function drawUp(mine, side, f, cx, lo, hi) {
    const z = f.zones[side];
    const dir = side === "att" ? 1 : -1;
    let front = side === "att" ? z[3] - 40 : z[1] + 40;
    const lines = rowsOf(mine);
    if (side === "def" && f.streetEdge) {
      const half = rowWidth(lines[0]) / 2;
      const ys2 = f.streetEdge.filter((y, c) => y >= 0 && Math.abs(c * f.cell + f.cell / 2 - cx) <= half).sort((a, b) => a - b);
      if (ys2.length) front = Math.max(front, ys2[Math.floor(ys2.length / 2)] + LAND.city.inside);
      if (f.stream?.front !== void 0) front = Math.min(front, f.stream.front);
    }
    const x0 = lo ?? 10, x1 = hi ?? f.w - 10;
    const wingsAt = (row) => {
      const span2 = lines[0].length * (150 + GAP) / 2;
      const at = row.map((u, i) => {
        const left = i % 2 === 0, k = Math.floor(i / 2);
        return cx + (left ? -1 : 1) * (span2 + 60 + k * (u.w + GAP));
      });
      return row.every((u, i) => at[i] - u.w / 2 >= x0 && at[i] + u.w / 2 <= x1) ? at : null;
    };
    const place = (row, y, wings) => {
      const at = wings ? wingsAt(row) : null;
      if (at) {
        row.forEach((u, i) => {
          u.x = at[i];
          u.y = y;
        });
        return true;
      }
      const width = rowWidth(row);
      let x = cx - width / 2;
      for (const u of row) {
        u.x = x + u.w / 2;
        u.y = y;
        x += u.w + GAP;
      }
      if (lo !== void 0 && hi !== void 0 && row.length > 0) {
        const a = row[0].x - row[0].w / 2, b = row[row.length - 1].x + row[row.length - 1].w / 2;
        const shift = a < x0 ? x0 - a : b > x1 ? x1 - b : 0;
        for (const u of row) u.x += shift;
      }
      return false;
    };
    const held = [];
    if (side === "def" && lines[0].length >= 4) {
      for (let n2 = Math.max(1, Math.floor(lines[0].length / 4)); n2 > 0; n2--) {
        const count = (c) => lines[0].filter((u) => u.corps === c).length;
        let at = -1;
        for (let i = lines[0].length - 1; i >= 0; i--) if (at < 0 || count(lines[0][i].corps) > count(lines[0][at].corps)) at = i;
        held.unshift(...lines[0].splice(at, 1));
      }
    }
    if (lo === void 0 || hi === void 0) {
      place(lines[0], front, false);
      const wide2 = place(lines[1], front - dir * 110, true);
      if (held.length) {
        place(held, front - dir * (wide2 || !lines[1].length ? 110 : 170), false);
        for (const u of held) u.reserve = "held";
      }
      place(lines[2], front - dir * 230, false);
      return;
    }
    const rows = [];
    const cut2 = (line, at) => line.forEach((u, i) => {
      const r = rows[rows.length - 1];
      if (i > 0 && rowWidth([...r.us, u]) <= x1 - x0) r.us.push(u);
      else rows.push({ us: [u], at, first: i === 0, wings: false });
    });
    cut2(lines[0], 0);
    const wide = rows.length <= 1 && lines[1].length > 0 && wingsAt(lines[1]) !== null;
    if (wide) rows.push({ us: lines[1], at: 110, first: true, wings: true });
    else cut2(lines[1], 110);
    cut2(held, wide || !lines[1].length ? 110 : 170);
    for (const u of held) u.reserve = "held";
    cut2(lines[2], 230);
    const deep = (us) => Math.max(...us.map((u) => u.d));
    const steps = (gap) => {
      const ys2 = [];
      rows.forEach((r, i) => {
        const behind = i ? ys2[i - 1] - dir * (deep(rows[i - 1].us) / 2 + gap + deep(r.us) / 2) : front;
        ys2.push(!r.first ? behind : dir > 0 ? Math.min(front - r.at, behind) : Math.max(front + r.at, behind));
      });
      return ys2;
    };
    const back = dir > 0 ? z[1] : z[3], n = rows.length - 1;
    const past = (ys2) => n >= 0 && dir * (ys2[n] - back) < deep(rows[n].us) / 2;
    let ys = steps(GAP);
    for (let gap = GAP - 5; gap >= 10 && past(ys); gap -= 5) ys = steps(gap);
    rows.forEach((r, i) => place(r.us, ys[i], r.wings));
  }

  // ../../data/battle/drill.json
  var drill_default = {
    _doc: [
      "The squad brain (src/battle/land/drill.ts; Brian, 20 Nov 20:43Z: 'super realistic and meaningful and everyone has high AI iq'): what each man, tank and gun inside a block does, second by second. Looks only (ruling 217: the card decides, the field acts it out): the fight never reads any of it, so no number here can move a result.",
      "style: the drill's numbers by the computer general's style (styles.ts), and 'plain' for a side the player commands or a battle with no styles. rush and halt: seconds a team runs forward and lies firing in fire and movement (one team rushes while the other covers it); wave: both teams go together (a mass); assault: the share of a block's riflemen who work forward onto the enemy while the rest keep its head down; spacing: metres between men in a line; tankRush, tankHalt: seconds a pair of tanks drives on while the other pair halts and covers it; scoot: the chance a tank that fired from the open backs off to cover; salvo: 'battery' (the guns fire together), 'rapid' (each as fast as it can), 'ripple' (one after another down the line); gunEvery: seconds between a gun's shots.",
      "xp: how experience changes the men: under raw they bunch (spacing times bunch), look only a short way for cover (seek), and stay down longer when hit (pinned); from veteran they spread wide and look further.",
      "cover: how much shelter a man finds on each ground, 0 none to 1 the best (works and bunkers); 'edge' is added where the ground in front of him toward the enemy is open (he lines the wood's or the houses' edge, never the middle), 'crest' where a fold of ground 1 to 4 m higher lies 15 m in front of him, 'wreck' and 'crater' for a man close behind a burnt-out tank or in a shell hole.",
      "ranges (metres between blocks' edges): column, a block on the march further than this from any enemy goes in files on its road; bound, nearer than this, or under fire, it goes by fire and movement; debus, lorried infantry get down; fo, guns firing further than this have a spotter up with the foot nearest their target; ambush, a defence's anti-tank team goes to its flank once enemy tanks come within this; grenade, men this close to the enemy's men throw.",
      "pin: a block losing this share of its strength a second (smoothed), or shelled within 'shelled' seconds, goes to ground: men in the open crawl to cover, about a third keep their heads down."
    ],
    style: {
      plain: { rush: 3.5, halt: 4, wave: false, assault: 0.45, spacing: 12, tankRush: 4, tankHalt: 5, scoot: 0.3, salvo: "battery", gunEvery: 7 },
      methodical: { rush: 3, halt: 4.5, wave: false, assault: 0.4, spacing: 12, tankRush: 4, tankHalt: 6, scoot: 0.35, salvo: "battery", gunEvery: 8 },
      bold: { rush: 4.5, halt: 2.5, wave: false, assault: 0.6, spacing: 12, tankRush: 5, tankHalt: 3, scoot: 0.15, salvo: "rapid", gunEvery: 5 },
      mass: { rush: 6, halt: 1.5, wave: true, assault: 0.75, spacing: 7, tankRush: 6, tankHalt: 2, scoot: 0, salvo: "ripple", gunEvery: 6 }
    },
    xp: { raw: 0.15, veteran: 0.5, bunch: 0.7, spread: 1.15, seek: [6, 10, 14], pinned: [1.5, 1, 0.7] },
    cover: {
      ground: { open: 0, field: 0.15, woods: 0.6, rough: 0.35, marsh: 0.05, water: -5, road: 0, bridge: 0, buildings: 0.85, rock: 0.55, bunker: 1, works: 1, blown: -5, suburb: 0.65, industry: 0.8, park: 0.2 },
      edge: 0.15,
      crest: 0.35,
      wreck: 0.7,
      crater: 0.5
    },
    ranges: { column: 900, bound: 450, debus: 700, fo: 500, ambush: 500, grenade: 35, hullDown: [1, 3.2] },
    pin: { harm: 4e-3, shelled: 3 },
    rounds: {
      _about: "Every round fired (Brian, 21 November 04:04Z: 'make the right odds if they actually hit so it not always an automatic hit ... when they do hit ... it shows up on the receiving object'). Looks only (ruling 217), the drill's own dice: a man falls and a tank is knocked out only where the rules take the model, and that round is a hit. odds: [best chance, metres at which it halves] for an aimed round from a halted shooter at a halted target in the open (tank: a 75 mm class gun at a tank; at: a launcher or anti-tank rifle; gun: a field or anti-tank gun over open sights; mg and rifle: at a lorry or a gun's shield, never at a man, as a man hit is a man the rules take). Times the shooter moving, the target moving, the target's cover (crest: hull-down) and the crew's experience [at none, more at full]. glance: the share of hits on a tank that bounce, by the face struck (the rest go in and do harm short of a kill). turret: the share of hits on a tank's turret. spread: a miss's error in metres [along the line of fire, more per metre of range, across, more per metre]. indirect: a gun firing over a crest lands round its aim [metres, more per metre of range], a mortar always so (mortar: its own spread), a hit where it lands within onVehicle of a vehicle. mortar (release 2: a support team's tube) is always indirect, so its odds and spread are never read. speed: metres a second a round flies. smallArms: seconds between a rifleman's shots and a machine gun's bursts [least, most]. keep: seconds the drill keeps a round in its list; marks: the most scars one vehicle keeps.",
      odds: { tank: [0.85, 900], at: [0.7, 90], gun: [0.6, 700], mg: [0.5, 300], rifle: [0.3, 200], grenade: [0.6, 15], mortar: [0.3, 300] },
      shooterMoving: 0.3,
      targetMoving: 0.7,
      cover: { none: 1, field: 0.9, rough: 0.8, woods: 0.6, building: 0.5, works: 0.5, rock: 0.6, wreck: 0.6, crater: 0.7, crest: 0.45 },
      xp: [0.8, 0.4],
      glance: { front: 0.75, left: 0.45, right: 0.45, rear: 0.25, top: 0.5 },
      turret: 0.35,
      spread: { tank: [4, 0.012, 1, 3e-3], at: [2, 0.03, 0.6, 0.01], gun: [5, 0.012, 1.2, 3e-3], mg: [1.5, 4e-3, 0.8, 3e-3], rifle: [1.5, 6e-3, 0.8, 4e-3], grenade: [1.5, 0.05, 1, 0.05], mortar: [6, 0.02, 2, 0.01] },
      indirect: { sigma: [8, 0.02], mortar: [8, 0.025], onVehicle: 2.5 },
      speed: { tank: 800, at: 150, gun: 450, mg: 750, rifle: 750, grenade: 12, mortar: 200 },
      smallArms: { rifle: [3, 6], mg: [1.2, 2.4] },
      keep: 2,
      marks: 12
    },
    teams: {
      _about: "Support teams (release 2; Brian, 21 November 2026, 03:49Z: 'units you can place at the begining of a battle on your side and they can then attack from the back'): a machine-gun team's three guns on their tripods (role hmg) or a mortar team's three tubes (role mortar), each with its crew, carried by hand. hmg.burst: seconds between a tripod gun's bursts [least, most] (longer and closer than a section's light gun). mortar.every: a tube's shots as a share of the style's gunEvery (a mortar fires faster than a field gun). file: metres between the crews carrying their weapons in file on the march; pace: metres a battle second a crew runs its weapon to a new place in the line; sep: metres kept between two weapons set up.",
      hmg: { burst: [0.8, 1.6] },
      mortar: { every: 0.5 },
      file: 7,
      pace: 3.5,
      sep: 8
    },
    sections: {
      _about: "Each model drawn as its section (release 2; the 3D ground battle scene for Brian, 21 November 2026, 18:29Z: 'the ground battle needs a lot more soldiers in it'). Looks only (ruling 217): the model stays the rules' one, its key man; the rest of his section stand round him and fall one by one as the block loses strength, the last of them when the rules lift the model. rifle: a rifle section by army (the default for the rest): men in all, its leader the key man, one automatic weapon (US the BAR, Germany the MG 42, Britain the Bren), its number twos and ammunition bearers (loaders), and its scouts out ahead on the move. officer: the headquarters (the officer and his runners); mg: a machine-gun section (the gun, two loaders, its leader); at: an anti-tank team (the launcher and its loader); sapper: an assault pioneer section; holder: a horse holder and the two with him; gun: a field gun's crew (beside the gun); hmg and mortar: a support team's ammunition bearers (beside the three the 3D game draws with each weapon). Each number counts the key man: a gun's counts the gun itself (8: the gun and seven crew), hmg's and mortar's the three drawn with the weapon (7: four bearers; 8: five). space: metres between the men of a section in its line; rows: metres between a block's front and second row of sections; files: men to a file on the march (more files for more men); ahead: metres a section's scouts go before it.",
      rifle: {
        USA: { men: 12, loaders: 2, scouts: 2 },
        GER: { men: 10, loaders: 2, scouts: 0 },
        UK: { men: 10, loaders: 1, scouts: 0 },
        SOV: { men: 10, loaders: 1, scouts: 0 },
        JAP: { men: 13, loaders: 2, scouts: 0 },
        ITA: { men: 10, loaders: 1, scouts: 0 },
        FRA: { men: 12, loaders: 2, scouts: 0 },
        default: { men: 10, loaders: 1, scouts: 0 }
      },
      officer: 4,
      mg: 4,
      at: 2,
      sapper: 8,
      holder: 3,
      gun: 8,
      hmg: 7,
      mortar: 8,
      space: 3.5,
      rows: 18,
      files: 24,
      ahead: 14,
      platoon: {
        _about: "Drawn as platoons (Brian, 22 November 2026: 'I want a lot of troops in the ground battles'): each rifle model drawn as its rifle platoon, by army. squads: its squads, the key man's own leading and the rest one behind another, step metres apart, each on its own leader and the same as the key man's section; hq: its headquarters, its leader counted, inside the leading squad's wedge. The US 3 squads of 12 and 5 (41 men), Germany 3 of 10 and a Zugtrupp of 7 (37), Britain 3 of 10 and 7 (37), the Soviets 4 of 10 and 3, Japan 3 of 13 and 4, Italy 3 of 10 and 4, France 3 of 12 and 4; the rest 3 and 4. rows: metres between a block's front and second row of platoons. Gun crews, support teams, anti-tank teams, sappers, an officer's party and tanks stay as they are.",
        USA: { squads: 3, hq: 5 },
        GER: { squads: 3, hq: 7 },
        UK: { squads: 3, hq: 7 },
        SOV: { squads: 4, hq: 3 },
        JAP: { squads: 3, hq: 4 },
        ITA: { squads: 3, hq: 4 },
        FRA: { squads: 3, hq: 4 },
        default: { squads: 3, hq: 4 },
        step: 9,
        rows: 27
      }
    },
    tanks: {
      _about: "A tank halted in a fight fights from alternate posts (every army but a mass, whose scoot is 0): 'shots' [least, most] from one post, then she backs 'back' metres out of sight, crosses behind the line and comes up at her next post, 'post' metres to one side of her own place and then the other (her own place is where the rules lay her wreck, so she never goes far from it).",
      post: 16,
      back: 12,
      shots: [2, 4]
    },
    book: {
      _about: [
        "The drill book (release 2; Brian, 22 November 2026, 00:23Z: 'i want the troop AI to be smart and move like a platoon with covering fire and historical tactics ... with combines arms'): how each army's platoons fight, in src/battle/land/tactics.ts; the principles, the schools and the historian's and the general's reviews in /mnt/project-files/battles/drill-book/. Looks only (ruling 217). On when the men are drawn as platoons. Times are battle seconds (the 3D plays the fight at half the battle's clock, so a 2 s rush shows as 4 s).",
        "schools: each army's own manual and kit. bound: who rushes with whom in the three turns of fire and movement (tactics.ts Bound: 'body' the platoon's squads a turn each, each squad as one body round its gun, the gun at its head, as a German squad; 'squad' the squads a turn each, the automatic rifle walking with its squad; 'both' the sections a turn each, each gun group a turn after its riflemen; 'wave' all together); raw and veteran: the bound at that experience where it differs. crawl: the share of rushes made at a crouched creep (infiltration), the pace allowing. final: how the assault goes in at the last ('grenade' grenades from cover then in, 'marching' walking forward firing, 'charge' the rush in with a shout from 30 to 50 m). gunStays: the assault leaves its squads' guns firing with the base of fire. counter: battle seconds after the enemy reaches a defence's line before its rear platoons counter-attack (negative: never). hold: metres to which a defence holds its fire, where its drill sets it (a Japanese defence: to 25 to 50 m).",
        "nations: each nation's school (the historian's map, 22 November: ROM and SVK French-model armies; ESP, ARG, CHL and TUR German-trained; NOR, NED, MEX and THA by the principles); any nation not named fights by '_' (the principles as written), so every playable nation fights by the book.",
        "rush: seconds a man is up in a rush [raw, regular, veteran] (the general: up 3 to 5 s, 10 to 20 m); gap: seconds between one turn's rush and the next's; stagger: the most seconds a pair rises after the first of its turn (by twos and threes; the leaders first). aside: after a rush, the share of men who crawl [least, most] metres aside before they fire (the enemy is aiming where they dropped).",
        "suppress: a round landing near a group of men (a squad, a section, a crew) adds to its suppression [metres it reaches, how much it adds] by weapon; it halves every 'half' seconds [raw, regular, veteran]; above 'down' the group's men keep their heads down and its rushes wait, but for its gunner and a share 'keep' who go on firing; above 'heavy' those in the open crawl for cover. rapid: the time between shots of men covering their mates' rushes, as a share of their usual (rapid fire).",
        "lift: metres from friendly foot within which the guns' and the mortars' rounds lift off a target into its depth ('deep' metres beyond its far side), and the support teams' machine guns shift to its far half. mission: a battery's or a mortar team's shoot at a new target, its spotter's signaller calling it on his radio: the call [least, most] seconds, one ranging round over the target ('range' metres off), a call, a second short of it, a call, then fire for effect, 'salvos' [least, most] every 'every' [least, most] seconds, then quiet [least, most] seconds and again (a call of 'again' seconds, no ranging); a mortar team's calls and quiet as a share 'mortar'. Fire over open sights is no mission.",
        "tank: battle seconds between a tank's own aimed rounds in a fight [least, most] (the general: a round every 15 to 30 s at a weapon). tankMg (Brian, 22 November 04:10Z: 'make sure the machine gun from the gunner inside the tank also shoots some times at infantry', and 'the top machine gun cant be used unless the tank commander mans the gun'): of the men a tank block takes off the enemy's foot, the share that fall to her machine guns rather than her main gun's shells, in the open and in cover (woods, streets, works: there high explosive does more of it); each machine-gun round names its gun (g): the commander's 'roof' gun (only in the armies named, only with him up in the hatch, 'roofShare' of his tank's bursts), the hull gunner's when the target is within 'hullArc' degrees of her bow, else the 'coax' beside the main gun. A commander up in his hatch is exposed: a small-arms round that strikes his tank strikes him 'commander' of the time, a shell bursting on her 'splinter' of the time (the round carries cm: 1); she fights buttoned up from then, and her roof gun is silent. hold: metres to which a defence holds its fire: its machine guns, its riflemen [raw, regular, veteran] (the general: machine guns 300 to 400 m, rifles 200, veterans 100, raw troops open at 600 to 800 and give themselves away). counter: a defence counter-attacks once the enemy is within 'at' metres of its line, its rear platoons going 'ahead' metres forward of their places; xp: its pause times this [raw, regular, veteran]. flank: metres [least, most] the assault's men swing out toward the flank on a rush ('rush'), and the most they swing out from their places in the line ('out': the general, inside a platoon 40 to 60 m). smoke: as an assault goes out its platoons lay the school's 'smoke' rounds (a British platoon's 2-inch mortar 3; none for the Soviets) side by side 'spread' metres apart on the enemy's front corner on its flank, again every 'every' seconds while it is out; no harm (the 3D draws a screen drifting with the battle's wind); off ('on' false) until the 3D scene draws smoke rounds. With smoke on call (the battle's smoke switch, release 2, ruling 242 S6) the platoons lay none of these: each screen the rules lay (LandBattle.screens) is drawn as rounds from the block's base of fire, one each 'per' metres of its line, at most 'most' (the design director's looks-only knobs), each hanging ('hang') till the screen's end."
      ],
      schools: {
        GER: { bound: "body", crawl: 0.1, final: "grenade", gunStays: false, counter: 10, smoke: 2 },
        UK: { bound: "both", crawl: 0.15, final: "grenade", gunStays: true, counter: 30, smoke: 3 },
        USA: { bound: "squad", crawl: 0.05, final: "marching", gunStays: false, counter: 30, smoke: 2 },
        SOV: { bound: "squad", raw: "wave", crawl: 0.05, final: "charge", gunStays: false, counter: 25, smoke: 0 },
        JAP: { bound: "body", crawl: 0.6, final: "grenade", gunStays: false, counter: 10, hold: 40, smoke: 2 },
        ITA: { bound: "body", crawl: 0.1, final: "grenade", gunStays: false, counter: 30, smoke: 1 },
        FRA: { bound: "both", crawl: 0.1, final: "grenade", gunStays: true, counter: 45, smoke: 2 },
        _: { bound: "both", crawl: 0.1, final: "grenade", gunStays: true, counter: 30, smoke: 2 }
      },
      nations: {
        GER: "GER",
        HUN: "GER",
        BUL: "GER",
        FIN: "GER",
        CRO: "GER",
        ESP: "GER",
        ARG: "GER",
        CHL: "GER",
        TUR: "GER",
        CHN: "GER",
        UK: "UK",
        CAN: "UK",
        AUS: "UK",
        NZL: "UK",
        SAF: "UK",
        IND: "UK",
        IRQ: "UK",
        IRL: "UK",
        POR: "UK",
        USA: "USA",
        BRA: "USA",
        PHI: "USA",
        SOV: "SOV",
        MGL: "SOV",
        JAP: "JAP",
        MAN: "JAP",
        ITA: "ITA",
        FRA: "FRA",
        VIC: "FRA",
        POL: "FRA",
        BEL: "FRA",
        YUG: "FRA",
        GRE: "FRA",
        ROM: "FRA",
        SVK: "FRA",
        CZE: "FRA",
        IRN: "FRA"
      },
      rush: [2.6, 2, 1.6],
      gap: 0.2,
      stagger: 0.8,
      aside: [0.5, 1, 3],
      suppress: { rifle: [4, 0.15], mg: [5, 0.25], grenade: [8, 0.6], at: [6, 0.4], tank: [15, 1.2], gun: [25, 2], mortar: [18, 1.6], half: [5, 3, 2], down: 0.6, heavy: 1.6, keep: 0.2 },
      rapid: 0.55,
      lift: { gun: 200, mortar: 100, mg: 30, deep: 40 },
      mission: { call: [6, 10], range: 60, salvos: [3, 4], every: [3, 4], quiet: [10, 16], again: 4, mortar: 0.5 },
      tank: { every: [8, 14] },
      tankMg: { kill: { open: 0.5, cover: 0.25 }, roof: ["USA", "GER"], roofShare: 0.33, hullArc: 25, commander: 0.02, splinter: 0.25 },
      hold: { mg: 350, rifle: [700, 200, 100] },
      counter: { at: 45, ahead: 12, xp: [2, 1, 0.6] },
      flank: { rush: [14, 26], out: 45 },
      smoke: { on: true, every: 60, spread: 10, per: 15, most: 12 }
    },
    blast: {
      _about: "Shell blasts (release 2; Brian, 23 November 05:04Z: 'explosions on the ground should be able to take out multiple people if they are close to it'). Looks only, the rules' counts untouched (Drill.thin): with LandBattle.blast on, the men a block loses to a gun's or a tank's high explosive wait up to 'hold' battle seconds for one burst, or until as many are due as one shell of the class takes ('most', a realistic few: men of a section stand 5 to 10 m apart); a 'shell' event that took a key man within 'anchor' seconds is that burst. The men nearest it fall, within shells.ts REACH of it; a man lying down counts his distance over 'lying', kneeling over 'kneeling' (a burst finds him less: fragments fly up and out).",
      hold: 6,
      anchor: 1,
      most: { light: 2, mortar: 3, field: 4, howitzer: 5, heavy: 7, bomb: 10 },
      lying: 0.45,
      kneeling: 0.75
    },
    escort: {
      _about: "Infantry with tanks (release 2; Brian, 23 November 05:05Z: 'have them advance behind the tank using it as cover like they would in real life ... make sure their players cant be run over by their own vechiles and they get out of the way. vechiles can run over enemy soldiers though which would kill them'). Looks only (LandBattle.escort, Drill.escort and clearOfVehicles): a foot block deploying or bounding forward with a friendly tank within 'near' metres sends up to 'men' riflemen within 'reach' of her to file in behind her, 'gap' metres back, 'file' apart, 'side' either side of her tracks, at her pace plus a little ('pace' its least and most, m/s); a tank or lorry is moving over 'moving' m/s. 'clear': a man of her side within 'ahead' metres of her nose (or 'behind' of her middle) and 'half' of her middle line steps out to 'out' metres of it at a run, not again for 'dodge' seconds; an enemy only once within 'enemyAhead', and never while lying pinned; a man the rules take from his block while within 'crush' ahead and 'crushHalf' aside of an enemy tank's middle on the move is run over. 'within': blocks this far beyond a vehicle are not looked at.",
      near: 60,
      men: 6,
      reach: 45,
      gap: 7,
      file: 2.6,
      side: 1.4,
      moving: 0.4,
      pace: [1.2, 4.5],
      clear: { ahead: 10, behind: 3, half: 2.3, out: 3.6, enemyAhead: 2.5, crush: 4, crushHalf: 1.4, dodge: 1.5, within: 60 }
    },
    mech: {
      _about: "Armoured infantry (release 2; Brian, 23 November 07:01Z, yes to 'half-tracks carrying armoured infantry'; LandBattle.mech, off unless turned on; looks only). An armoured block draws the riflemen of its division riding behind its tanks, a section to each carrier, who get down near the enemy as lorried infantry do. 'rows': the historian's (23 November 07:05Z, historian/halftracks-td-spg-movers-1123.md, M unless said), each army's carriers to a block of four tanks, [from, until, models] by the battle's year (a fraction for the month), drawn in that order and as many as the block's tray over four of them (one at least); an army with no row draws none. Britain's Universal Carrier is not modelled: its row draws the lorries beside it (the known list). The Soviet tank riders and the minor armies wait for their own rows. Units decide (the design director, 07:19Z): these rows are only the history start's mix; where the campaign says what a corps carries, 'kit' draws it. 'commonwealth': drawn as Britain's. 'armoured': carriers whose riders a bullet cannot reach while they are aboard (the design director, 07:06Z(2): struck through the vehicle's armour; a man aboard is not the next to fall to small arms). 'share': the riflemen's share of an armoured block's men, so a scaled battle draws as many as its men at that scale (an armoured division about 0.3, M). 'back': metres behind the block's tray their middle keeps. 'base': their k from this up (the 3D game tells a man by block, k and j).",
      rows: {
        USA: [[1942, 9999, ["M3Halftrack", "M3Halftrack", "M3Halftrack"]]],
        GER: [[1940, 9999, ["Hanomag", "Truck", "Hanomag", "Truck"]]],
        UK: [[1940, 1943, ["Truck", "Truck"]], [1943, 9999, ["M3Halftrack", "Truck", "Truck"]]],
        ITA: [[1940, 9999, ["Truck", "Truck"]]],
        JAP: [[1937, 9999, ["Truck", "Truck"]]]
      },
      commonwealth: ["CAN", "AUS", "NZL", "NZ", "SAF", "IND"],
      kit: {
        _about: "Units decide (the design director, 23 November 07:19Z): where the campaign says what a corps carries (ForceKit.halftracks, its share of half-tracks), the rows above set only the history start's mix. 'per': carriers to four tanks; 'models': an army's own half-track; an army with none of its own draws the stand-in in its own coat (the Axis' the Sd.Kfz. 251, the rest the M3), said on the known list.",
        per: 3,
        models: { GER: "Hanomag", USA: "M3Halftrack", UK: "M3Halftrack" },
        axis: ["GER", "ITA", "JAP", "HUN", "ROM", "BUL", "FIN"],
        axisStand: "Hanomag",
        stand: "M3Halftrack"
      },
      armoured: ["Hanomag", "M3Halftrack"],
      share: 0.3,
      back: 24,
      base: 500
    },
    spg: {
      _about: "Self-propelled guns (release 2; Brian, 23 November 07:01Z, yes to 'a tank destroyer and a self-propelled gun for each major army'; LandBattle.spg, off unless turned on; looks only; a campaign battle's only, set pieces stay as the historian drew them). Some of a battery's guns are drawn as its army's self-propelled howitzer: no tractor, no crew on foot, firing when the battery fires and moving with it. 'rows': the historian's (23 November 07:05Z, historian/halftracks-td-spg-movers-1123.md, H): [from, until, models, per, with] by the battle's year (a fraction for the month): its side's blocks of that army of a kind in 'with' each bring 'per' of them (the US an M7 battery behind each armoured block; Britain's Priests, then Sextons, behind theirs; Germany's Hummel battery of six in a panzer division; the Soviet SU-76 regiment of each rifle division), taken from that army's batteries in the order of their blocks, a battery's first guns first (they stand longest), the models in turn. An army with no row draws none (Italy's Semovente is drawn as a tank destroyer; Japan had none worth drawing). Units decide (the design director, 23 November 07:19Z): these rows are only the history start's mix; where the campaign says what a corps has (ForceKit.spg, its share of its guns self-propelled), 'kit' draws it, its army's own model or the stand-in in its coat.",
      rows: {
        USA: [[1943, 9999, ["M7"], 3, ["arm"]]],
        UK: [[1942.75, 1943.75, ["M7"], 1.5, ["arm"]], [1943.75, 1944.67, ["Sexton", "M7"], 1.5, ["arm"]], [1944.67, 9999, ["Sexton"], 1.5, ["arm"]]],
        GER: [[1943.5, 9999, ["Hummel"], 0.5, ["arm"]]],
        SOV: [[1943, 9999, ["SU76"], 0.5, ["inf"]]]
      },
      kit: { models: { USA: "M7", UK: "Sexton", GER: "Hummel", SOV: "SU76" }, axisStand: "Hummel", stand: "M7" }
    },
    td: {
      _about: "Tank destroyers (release 2, the same yes; LandBattle.td, off unless turned on; looks only; a campaign battle's only). The design director, 23 November 07:28Z, (a): a tank destroyer may be one model in an armoured block's mix, as the StuG is; the block keeps its own figures and fights as ever; a whole block of them waits for Units decide to give corps tank-destroyer battalions with figures of their own. ForceKit.td reads first (its share of the block's tanks), its army's own model ('kit') or the stand-in in its coat; else the historian's rows (23 November 07:05Z, H), [from, until, models, share] by the battle's year: the Soviet SU-85 with the tank blocks from August 1943, Italy's Semovente with the armoured blocks to its armistice. The tanks drawn so are spread through the block (a share of a quarter: its fourth tank of each four). Germany's StuG is in its stand-in mix already; the American and British M10 went with the infantry, so they come by the kit.",
      rows: {
        SOV: [[1943.58, 1945, ["SU85"], 0.25]],
        ITA: [[1941, 1943.75, ["Semovente"], 0.25]]
      },
      kit: { models: { USA: "M10", UK: "M10", GER: "StuG3", SOV: "SU85", ITA: "Semovente" }, axisStand: "StuG3", stand: "M10" }
    },
    cars: {
      _about: "Armoured cars (release 2; Brian, 23 November 07:01Z, yes to armoured cars, jeeps, motorcycles and AA; LandBattle.cars, off unless turned on; looks only; a campaign battle's only; the design director, 07:47Z, accepted as proposed). A block's cars are a view of it: out 'ahead' metres before its front, 'side' metres beyond each flank, while it has not met the enemy ('contact' metres) and is not fighting; then back 'back' metres behind its tray. They see nothing the block does not (no ticket words say they scout), fire never, and are lost as the block's strength goes. 'rows': the historian's (23 November 07:47Z, historian/armoured-cars-aa-1123.md, H for where, M for the counts): [from, until, model, {kind: per block}] by the battle's year; 'per' below one is 1 in so many of the side's blocks of that army and kind, in the order of their blocks (a half: every other one). Every row of the month counts. Units decide: ForceKit.cars (its corps' cars per block) reads first, its army's own model or the stand-in; the rows are only the history start's.",
      rows: {
        GER: [[1939, 1943, "SdKfz222", { arm: 0.5, mot: 1 }], [1943, 9999, "SdKfz222", { arm: 0.5, mot: 0.5 }]],
        USA: [[1943.75, 9999, "M8", { arm: 1, inf: 0.3334 }]],
        UK: [[1940, 1941, "ArmouredCar", { arm: 1, mot: 0.5 }], [1941, 9999, "ArmouredCar", { arm: 1, mot: 0.5, inf: 0.3334 }]],
        FRA: [[1939, 1940.75, "ArmouredCar", { arm: 1, mot: 1 }]],
        SOV: [[1939, 1942, "ArmouredCar", { arm: 0.5, inf: 0.25 }], [1942, 9999, "ArmouredCar", { arm: 0.5 }]],
        ITA: [[1941, 9999, "ArmouredCar", { arm: 0.5, mot: 0.5 }]]
      },
      kit: { models: { GER: "SdKfz222", USA: "M8" }, axisStand: "SdKfz222", stand: "ArmouredCar" },
      ahead: 160,
      side: 18,
      back: 30,
      contact: 700
    },
    aa: {
      _about: "AA vehicles (release 2, the same yes; LandBattle.aa, off unless turned on; looks only; a campaign battle's only). A block's AA vehicles are a view of it, kept 'back' metres behind its tray (a battery's behind its tractors, 'battery' more), moving with it; lost as its strength goes. They hit nothing the rules do not take, and a strike's losses stay the rules'; until the 3D game aims them upward they ride and park. 'rows': the historian's (23 November 07:47Z, historian/armoured-cars-aa-1123.md), read as the cars' are ('art': a battery, not a support team). Units decide: ForceKit.aa (its corps' AA vehicles per block) reads first.",
      rows: {
        USA: [[1943, 9999, "M16", { arm: 1, inf: 0.3334 }]],
        GER: [[1939, 9999, "FlakHalftrack", { arm: 1, mot: 0.5 }], [1939, 9999, "FlakTruck", { inf: 0.3334 }]],
        UK: [[1941, 9999, "FlakTruck", { art: 1, arm: 0.5 }]],
        SOV: [[1939, 9999, "FlakTruck", { art: 1 }], [1944, 9999, "M16", { arm: 0.3334 }]],
        ITA: [[1939, 9999, "FlakTruck", { art: 1, arm: 0.5 }]],
        JAP: [[1937, 9999, "FlakTruck", { art: 0.5 }]]
      },
      kit: { models: { USA: "M16", GER: "FlakHalftrack" }, axisStand: "FlakHalftrack", stand: "FlakTruck" },
      back: 40,
      battery: 40
    },
    light: {
      _about: "Jeeps, field cars and motorcycles (release 2; Brian, 23 November 07:01Z, yes to armoured cars, jeeps, motorcycles and AA; LandBattle.light, off unless turned on; looks only; a campaign battle's only; as the armoured cars, the design director's 07:47Z ruling). A block's light vehicles are a view of it: those 'ahead' ride out on its flanks ('ahead' metres before its front, 'side' beyond each flank) while it has not met the enemy (the armoured cars' 'contact'), then keep behind it; those behind park at its rear corners ('back'), and a motorcycle there rides despatch, back 'run' metres and up again each 'every' seconds. They see nothing the block does not, fire never, and are lost as the block's strength goes; never more than 'most' to a block, cars first. 'cars' and 'bikes' rows: the historian's (23 November 08:18Z, historian/light-cars-motorcycles-1123.md; H for where, M for most counts): by army ('_' every army not named), [from, until, models (taken in turn), {kind: per block}, {kind: 'ahead'} (else behind)]; 'per' below one is 1 in so many of the side's blocks of that army and kind, in the order of their blocks. 'kit': each army's own model where the campaign says what the corps has (ForceKit.jeeps, ForceKit.bikes); 'stand' the rest's, in their own coat. 'sw': the swaps a model is drawn with by army ('sidecar' on for the Poles, French, Hungarians and Romanians); a car riding ahead keeps her machine gun, one behind is a staff car without it.",
      cars: {
        USA: [[1939, 1942, ["FieldCar"], { arm: 1, mot: 1, inf: 1, art: 1 }, { arm: "ahead" }], [1942, 9999, ["Jeep"], { arm: 1, mot: 1, inf: 1, art: 1 }, { arm: "ahead" }]],
        UK: [[1939, 1942, ["FieldCar"], { arm: 1, mot: 1, inf: 0.5, art: 0.5 }, {}], [1942, 9999, ["Jeep"], { arm: 1, mot: 1, inf: 0.5, art: 0.5 }, {}]],
        GER: [[1939, 1940.5, ["FieldCar"], { arm: 1, mot: 1, inf: 0.5, art: 0.5 }, {}], [1940.5, 9999, ["Kubelwagen"], { arm: 1, mot: 1, inf: 0.5, art: 0.5 }, {}]],
        SOV: [[1941, 1943, ["GAZ67"], { arm: 0.5, mot: 0.5, inf: 0.25, art: 0.25 }, {}], [1943, 9999, ["GAZ67", "Jeep"], { arm: 0.5, mot: 0.5, inf: 0.25, art: 0.25 }, {}]],
        ITA: [[1939, 9999, ["Fiat508"], { arm: 0.5, mot: 0.5, inf: 0.3334, art: 0.3334 }, {}]],
        JAP: [[1936, 9999, ["Kurogane"], { arm: 0.5, mot: 0.5, inf: 0.25, art: 0.25 }, {}]],
        _: [[1939, 9999, ["FieldCar"], { arm: 0.5, mot: 0.5, inf: 0.25, art: 0.25 }, {}]]
      },
      bikes: {
        GER: [[1939, 1943, ["R75"], { arm: 1, mot: 1, inf: 0.5, art: 0.3334 }, { arm: "ahead", mot: "ahead" }], [1943, 9999, ["R75"], { arm: 0.5, mot: 0.5, inf: 0.5, art: 0.3334 }, {}]],
        SOV: [[1939, 1941, ["Motorcycle"], { arm: 0.5, mot: 0.5, inf: 0.25 }, { arm: "ahead", mot: "ahead", inf: "ahead" }], [1941, 9999, ["M72"], { arm: 0.5, mot: 0.5, inf: 0.25 }, { arm: "ahead", mot: "ahead", inf: "ahead" }]],
        USA: [[1941, 9999, ["WLA"], { arm: 0.3334, mot: 0.3334, inf: 0.3334, art: 0.25 }, {}]],
        UK: [[1939, 9999, ["M20"], { arm: 0.5, mot: 0.5, inf: 0.5, art: 0.3334 }, {}]],
        ITA: [[1939, 9999, ["Alce"], { arm: 1, mot: 1, inf: 0.3334, art: 0.25 }, { arm: "ahead", mot: "ahead" }]],
        JAP: [[1937, 9999, ["Rikuo"], { arm: 0.5, mot: 0.5, inf: 0.3334, art: 0.25 }, { arm: "ahead", mot: "ahead" }]],
        _: [[1939, 9999, ["Motorcycle"], { arm: 0.5, mot: 0.5, inf: 0.25, art: 0.25 }, { arm: "ahead", mot: "ahead" }]]
      },
      kit: {
        cars: { models: { USA: "Jeep", UK: "Jeep", GER: "Kubelwagen", SOV: "GAZ67", ITA: "Fiat508", JAP: "Kurogane" }, axisStand: "FieldCar", stand: "FieldCar" },
        bikes: { models: { GER: "R75", SOV: "M72", USA: "WLA", UK: "M20", ITA: "Alce", JAP: "Rikuo" }, axisStand: "Motorcycle", stand: "Motorcycle" }
      },
      sw: { Motorcycle: { sidecar: { POL: "on", FRA: "on", HUN: "on", ROM: "on", _: "off" } } },
      most: 2,
      ahead: 110,
      side: 45,
      back: 25,
      run: 220,
      every: 60
    }
  };

  // ../../src/battle/land/tactics.ts
  var BOOK = drill_default.book;
  function schoolOf(nat) {
    return BOOK.nations[nat] ?? "_";
  }
  function schoolNums(nat) {
    return BOOK.schools[schoolOf(nat)] ?? BOOK.schools._;
  }
  function boundOf(nat, band, wave) {
    if (wave) return "wave";
    const S = schoolNums(nat);
    return (band === 0 ? S.raw : band === 2 ? S.veteran : void 0) ?? S.bound;
  }
  var turns = (bound) => bound === "wave" ? 1 : 3;
  function turnOf(bound, gun, sq, hq) {
    const s = hq ? 0 : sq;
    switch (bound) {
      case "wave":
        return 0;
      case "body":
      case "squad":
        return s % 3;
      case "both":
        return (s + (gun ? 1 : 0)) % 3;
      default:
        return 0;
    }
  }
  var Suppression = class {
    constructor() {
      __publicField(this, "felt", /* @__PURE__ */ new Map());
    }
    /** A group's suppression now. */
    level(block, g, t) {
      const f = this.felt.get(block);
      if (!f) return 0;
      const v = f.lv.get(g);
      return v ? v * 0.5 ** ((t - f.at) / BOOK.suppress.half[f.band]) : 0;
    }
    /**
     * Brings a block's groups up to date at time t: decays every level, then adds each round fired at the block since
     * the last look (numbered n, landed by t) to every group whose middle lies within the round's reach of where it fell.
     */
    feel(block, t, band, rounds, groups) {
      let f = this.felt.get(block);
      if (!f) this.felt.set(block, f = { lv: /* @__PURE__ */ new Map(), at: t, seen: 0, air: [], band });
      const S = BOOK.suppress, k = 0.5 ** ((t - f.at) / S.half[band]);
      f.band = band;
      for (const [g, v] of f.lv) {
        const w = v * k;
        if (w < 0.01) f.lv.delete(g);
        else f.lv.set(g, w);
      }
      f.at = t;
      for (const r of rounds) if (r.n > f.seen) {
        f.seen = r.n;
        if (r.on === block) f.air.push({ l: r.l, w: r.w, x: r.x, y: r.y });
      }
      if (!f.air.length) return;
      const left = [];
      for (const r of f.air) {
        if (r.l > t) {
          left.push(r);
          continue;
        }
        const by2 = S[r.w];
        if (!by2) continue;
        const [reach, add] = by2;
        for (const [g, [x, y]] of groups) {
          const d = Math.hypot(x - r.x, y - r.y);
          if (d < reach) f.lv.set(g, (f.lv.get(g) ?? 0) + add * (1 - 0.5 * d / reach));
        }
      }
      f.air = left;
    }
  };
  var span = (a, r) => a[0] + (a[1] - a[0]) * r;
  function missionStep(ms, on, t, k, r) {
    const M = BOOK.mission;
    if (!ms || ms.on !== on) return [{ on, stage: 0, next: t + k * span(M.call, r(1)), left: 0, call: t }, 0];
    if (t < ms.next) return [ms, 0];
    switch (ms.stage) {
      case 0:
        return [{ ...ms, stage: 1, next: t + k * span(M.call, r(2)), call: t }, "over"];
      case 1:
        return [{ ...ms, stage: 2, next: t + k * span(M.call, r(3)), call: t }, "short"];
      case 2:
        return [{ ...ms, stage: 3, left: Math.round(span(M.salvos, r(4))) - 1, next: t + span(M.every, r(5)) }, "salvo"];
      case 3:
        if (ms.left > 0) return [{ ...ms, left: ms.left - 1, next: t + span(M.every, r(6)) }, "salvo"];
        return [{ ...ms, stage: 4, next: t + k * span(M.quiet, r(7)) }, 0];
      default:
        return [{ ...ms, stage: 2, next: t + k * M.again, call: t }, 0];
    }
  }

  // ../../src/battle/land/drill.ts
  var DR = drill_default;
  var mix = (n) => {
    let h = n >>> 0;
    h ^= h >>> 16;
    h = Math.imul(h, 2246822507);
    h ^= h >>> 13;
    h = Math.imul(h, 3266489909);
    h ^= h >>> 16;
    return (h >>> 0) / 4294967296;
  };
  var roll = (...ns) => {
    let h = 2654435769;
    for (const n of ns) h = Math.imul(h ^ Math.floor(n) >>> 0, 16777619) + 2135587861;
    return mix(h);
  };
  var clamp2 = (v, a, b) => v < a ? a : v > b ? b : v;
  var live = (u) => u.state !== "gone" && u.state !== "withdrawn";
  var fighting = (u) => live(u) && u.state !== "routing";
  var FOOT_KINDS = ["inf", "mtn", "eng", "fort", "cav"];
  var lookOf = (ml, sw) => ml + Object.keys(sw ?? {}).sort().map((k) => `|${k}=${sw[k]}`).join("");
  var isFoot2 = (u) => MOTION[u.kind] === "foot" || u.kind === "mot";
  var CLOSE2 = ["woods", "buildings", "industry", "suburb"];
  var BUILT = ["buildings", "industry", "suburb"];
  var MOVING = /* @__PURE__ */ new Set(["walk", "run", "advance", "creep", "crawl", "flee", "drive", "reverse", "towed", "stretcher_front", "stretcher_rear"]);
  var CARRIED = /* @__PURE__ */ new Set(["ride", "mounted", "horses"]);
  var LYING = /* @__PURE__ */ new Set(["fire_prone", "hide", "crawl", "observe", "cower", "lmg_burst"]);
  var SMALL_ARMS = /* @__PURE__ */ new Set(["fire_stand", "fire_kneel", "fire_prone", "lmg_burst"]);
  var VEHICLE = /* @__PURE__ */ new Set(["tank", "gun", "lorry"]);
  var CREWED = /* @__PURE__ */ new Set(["hmg", "mortar"]);
  var gauss = (a, b) => Math.sqrt(-2 * Math.log(Math.max(1e-9, a))) * Math.cos(2 * Math.PI * b);
  var CATCH_UP = 7.5;
  var TANK_TOP = 12;
  var TANK_GAIN = 3;
  var TANK_SHED = 6;
  var tankTop = (sq) => Math.max(TANK_TOP, Math.min(sq.speed, sq.u.speed ?? 0));
  var MOUNT = { down: 4.5, downAfter: 1, up: 5.5, upAfter: 6, stayAfter: 3, climb: 0.05, saddle: 1, span: 3, hop: 0.4, hopMost: 8, tail: 4.2, clear: 2.5 };
  var RIDERS_RING = 120;
  var COVER_OF = { works: "works", bunker: "works", buildings: "building", industry: "building", suburb: "building", woods: "woods", rock: "rock", rough: "rough", field: "field" };
  var fwd = (f) => [Math.sin(f), Math.cos(f)];
  var right = (f) => [Math.cos(f), -Math.sin(f)];
  var toWorld = (u, f, a, l) => {
    const [fx, fy] = fwd(f), [rx, ry] = right(f);
    return [u.x + a * rx + l * fx, u.y + a * ry + l * fy];
  };
  var bearing2 = (x0, y0, x1, y1) => Math.atan2(x1 - x0, y1 - y0);
  var wrap = (a) => {
    const r = (a + Math.PI) % (2 * Math.PI);
    return (r <= 0 ? r + 2 * Math.PI : r) - Math.PI;
  };
  var turnTo = (f, want, max) => {
    const d = wrap(want - f);
    return wrap(Math.abs(d) <= max ? want : f + Math.sign(d) * max);
  };
  var TURN = { foot: 8, ride: 4, tank: 0.8, wheels: 1.5 };
  var groundOf = (f, x, y) => {
    const c = clamp2(Math.floor(x / f.cell), 0, f.cols - 1), r = clamp2(Math.floor(y / f.cell), 0, f.rows - 1);
    return GROUNDS[f.ground[r * f.cols + c]];
  };
  var heightOf = (f, x, y) => {
    const fx = clamp2(x / f.cell - 0.5, 0, f.cols - 1), fy = clamp2(y / f.cell - 0.5, 0, f.rows - 1);
    const c0 = Math.floor(fx), r0 = Math.floor(fy), c1 = Math.min(f.cols - 1, c0 + 1), r13 = Math.min(f.rows - 1, r0 + 1), ax = fx - c0, ay = fy - r0;
    const H = f.height, C = f.cols;
    return (H[r0 * C + c0] * (1 - ax) + H[r0 * C + c1] * ax) * (1 - ay) + (H[r13 * C + c0] * (1 - ax) + H[r13 * C + c1] * ax) * ay;
  };
  var storeysOf = (f, x, y) => {
    if (!f.storeys) return 0;
    const c = clamp2(Math.floor(x / f.cell), 0, f.cols - 1), r = clamp2(Math.floor(y / f.cell), 0, f.rows - 1);
    return f.storeys[r * f.cols + c] ?? 0;
  };
  var LEADS = { loader: ["mg", "at", "hmg", "mortar"], crew: ["gun"], rifle: ["officer"], holder: ["holder"] };
  var SPACE = { men: 1.6, crew: 1.1, cell: 2 };
  var GUN_CREW = [[-1.4, -1.6], [1.4, -1.9], [-0.9, -3.4], [1, -3.8], [-2.4, -4.6], [2.2, -5.3], [0, -7.5], [-3, -8]];
  var Drill = class {
    constructor(b, opts = {}) {
      __publicField(this, "b");
      __publicField(this, "squads", /* @__PURE__ */ new Map());
      /** The rounds of the last few seconds (DR.rounds.keep), oldest first, each numbered (n) in the order fired. */
      __publicField(this, "shots", []);
      __publicField(this, "fired", 0);
      /**
       * Models the rules took this step ('block:model') whose round is already made: a shell's (Drill.shelled), or none of
       * the field's (the air's: Drill.fromAir). And the block whose small arms took each other one (Drill.struckBy).
       */
      __publicField(this, "shellKills", /* @__PURE__ */ new Set());
      /** Infantry with tanks (release 2): each tank's followers (her block's id x 1000 + her k), and where each tank is. */
      __publicField(this, "escorts", /* @__PURE__ */ new Map());
      __publicField(this, "byOf", /* @__PURE__ */ new Map());
      /** Where each wreck lies as drawn (the rules' wreck id to where her tank fell): she burns where she stood. */
      __publicField(this, "wreckAt", /* @__PURE__ */ new Map());
      __publicField(this, "seed");
      __publicField(this, "settled", false);
      /**
       * Platoon cam (release 2; Brian, 22 November: "Do 2 and 3"): the squad leading each side's assault, as [block, group
       * (grp)], chosen every half second (Drill.pickLeads), and the battle time the next choice is made.
       */
      __publicField(this, "leads", /* @__PURE__ */ new Map());
      __publicField(this, "leadAt", 0);
      /** The battle time each squad ('block:group') was last seen on the move, for the platoon cam. */
      __publicField(this, "rushed", /* @__PURE__ */ new Map());
      /**
       * Each model drawn as its section (release 2; the 3D ground battle scene for Brian, 21 November 2026, 18:29Z: "the
       * ground battle needs a lot more soldiers in it"): the model's man is its key man, and the rest of his section stand
       * round him (DR.sections). Off, a model is one man, as before.
       */
      __publicField(this, "sections");
      /** Drawn as platoons (release 2; Brian, 22 November: "I want a lot of troops in the ground battles"): each rifle model is
       * its platoon, its key man's squad leading, the platoon's other squads and its headquarters behind (sections too). */
      __publicField(this, "platoons");
      /**
       * The drill book (tactics.ts; Brian, 22 November 00:23Z: "move like a platoon with covering fire and historical
       * tactics"): each army's platoons fight by their school, under suppression, with lift and shift and tanks and infantry
       * together. On with platoons unless asked off.
       */
      __publicField(this, "byBook");
      /**
       * The battle's scale (the true-scale plan, GR1 and GR3; Brian's standing rule of 22 November 04:14Z: the numbers on
       * screen reflect the units each side brings): one drawn man stands for this many. No block draws more men than its
       * men0 over it: drawn as platoons where they fit, else as sections, else its key men only. Null: the battle's setting
       * for every block, as before.
       */
      __publicField(this, "scale");
      __publicField(this, "sup", new Suppression());
      /** Each side's tanks standing this step, and each block's most dangerous men (its guns), worked out once a step. */
      __publicField(this, "tankMemo", { t: -1, at: {} });
      __publicField(this, "priMemo", /* @__PURE__ */ new Map());
      this.b = b;
      this.seed = b.ticket.seed;
      this.sections = !!opts.sections || !!opts.platoons;
      this.platoons = !!opts.platoons;
      this.byBook = opts.book ?? this.platoons;
      this.scale = opts.scale !== void 0 && Number.isFinite(opts.scale) && opts.scale > 0 ? opts.scale : null;
      for (const u of b.units) {
        this.squads.set(u.id, this.make(u));
        this.addMech(u);
        this.addEscorts(u);
      }
    }
    /** A block placed in the setup step (release 2) before the clock starts: its men drawn up afresh where it now stands. */
    relay(u) {
      if (this.b.t > 0 || !this.squads.has(u.id)) return;
      this.squads.set(u.id, this.make(u));
      this.addMech(u);
      this.addEscorts(u);
    }
    /** A block come onto the field after the drill was made (a corps from next door, release 2): its men drawn where it walks on. */
    join(u) {
      if (!this.squads.has(u.id)) {
        this.squads.set(u.id, this.make(u));
        this.addMech(u);
        this.addEscorts(u);
      }
    }
    /**
     * Armoured infantry (release 2; Brian, 23 November 07:01Z, yes to "half-tracks carrying armoured infantry"; the
     * historian's rows and the design director's rulings, 07:05Z and 07:06Z; LandBattle.mech, off unless turned on; looks
     * only). An armoured block's riflemen, drawn as lorried foot of their own: a section to each carrier of its army's row
     * (mechOf), riding DR.mech.back metres behind its tray and getting down near the enemy as lorried foot do. They are a
     * view of the block (kind 'mot'): its place (behind its tray, on its facing), its order, state, strength and harm are
     * the block's, read live, so the rules' counts and places are untouched. Their key men are numbered k from DR.mech.base
     * and fall in step with its tanks (its figures over its figures0); the rest of their sections fall as its strength goes
     * (thin). They are kept under the block's id less one, negative, so no lookup by a block's id finds them, and the 3D
     * game draws them with the block's own men (view).
     */
    addMech(u) {
      const key = -(u.id + 1);
      this.squads.delete(key);
      if (!this.b.mech || u.kind !== "arm") return;
      const carriers = this.mechOf(u);
      if (!carriers.length) return;
      const M = DR.mech, c = carriers.length, base = M.base, back = () => u.d / 2 + M.back;
      const v = Object.create(u);
      Object.defineProperties(v, {
        kind: { value: "mot", enumerable: true },
        x: { get: () => u.x - Math.sin(u.face) * back(), enumerable: true },
        y: { get: () => u.y - Math.cos(u.face) * back(), enumerable: true },
        figures0: { value: base + c, enumerable: true },
        figures: { get: () => u.figures <= 0 ? 0 : base + Math.min(c, Math.ceil(c * u.figures / Math.max(1, u.figures0))), enumerable: true },
        tray: { value: c, enumerable: true },
        men0: { value: Math.round(u.men0 * M.share), enumerable: true }
      });
      this.squads.set(key, this.make(v, { base, n: c, carriers, of: u.id }));
    }
    /**
     * An armoured block's carriers, its tray over four of them (one at least). What its corps carries where the campaign
     * says ("Units decide", ForceKit.halftracks; the design director, 23 November 07:19Z): DR.mech.kit.per to four tanks,
     * its share of them half-tracks (first), each its army's own model or the stand-in in its coat, the rest lorries. Else
     * the history start's mix: its army's row in the battle's month (DR.mech.rows).
     */
    mechOf(u) {
      const M = DR.mech, nat = M.commonwealth.includes(u.nat) ? "UK" : u.nat;
      const share = u.kit?.halftracks;
      if (share !== void 0) {
        const n2 = Math.max(1, Math.round(M.kit.per * u.tray / 4)), h = Math.round(n2 * clamp2(share, 0, 1));
        const own = M.kit.models[nat] ?? (M.kit.axis.includes(nat) ? M.kit.axisStand : M.kit.stand);
        return Array.from({ length: n2 }, (_, i) => i < h ? own : "Truck");
      }
      const d = turnToDate(this.b.ticket.turn), when = d.year + d.month / 12;
      const row = (M.rows[nat] ?? []).find(([from, to]) => when >= from && when < to);
      if (!row) return [];
      const list = row[2], n = Math.max(1, Math.round(list.length * u.tray / 4));
      return Array.from({ length: n }, (_, i) => list[i % list.length]);
    }
    /**
     * The models a block's vehicles are drawn with besides its own (release 2, looks only): its armoured infantry's
     * carriers, its self-propelled guns, its tank destroyers, its armoured cars, AA vehicles, jeeps and motorcycles (with
     * the swaps the rules pick for them: lookOf), as the switches stand. For the 3D game to read them before the
     * battle opens (the header's "mls"); worked out from the block alone, so a block still coming up from next door has its
     * own too.
     */
    modelsFor(u) {
      const n = Math.max(0, u.figures0), out = /* @__PURE__ */ new Set();
      if (this.b.mech && u.kind === "arm") for (const m of this.mechOf(u)) out.add(m);
      for (const m of this.spgOf(u, n)) if (m) out.add(m);
      for (const m of this.tdOf(u, n)) if (m) out.add(m);
      for (const m of [...this.escortOf(u, "cars"), ...this.escortOf(u, "aa"), ...this.lightOf(u).map((x) => lookOf(x.ml, x.sw))]) out.add(m);
      return [...out];
    }
    /** The army whose rows a block's nation reads (the Commonwealth's as Britain's), and the battle's year with its month. */
    armyOf(nat) {
      return DR.mech.commonwealth.includes(nat) ? "UK" : nat;
    }
    when() {
      const d = turnToDate(this.b.ticket.turn);
      return d.year + d.month / 12;
    }
    /** A kit's own model for an army, or the stand-in in its coat (the Axis' or the rest's). */
    kitModel(K, army2) {
      return K.models[army2] ?? (DR.mech.kit.axis.includes(army2) ? K.axisStand : K.stand);
    }
    /**
     * Self-propelled guns (release 2, LandBattle.spg; looks only): each of a battery's n guns, its model drawn
     * self-propelled or '' towed. Where the campaign says what the corps has (ForceKit.spg, "Units decide"), that share
     * of its guns, its army's own model; else the history start's (DR.spg.rows): its side's blocks of that army of the
     * row's kinds bring 'per' each, taken from that army's batteries in the order of their blocks. A battery's first guns
     * first (the rules lift the last first, so they stand longest).
     */
    spgOf(u, n) {
      const none2 = Array.from({ length: n }, () => "");
      if (!this.b.spg || u.kind !== "art" || u.team || n <= 0) return none2;
      const army2 = this.armyOf(u.nat), share = u.kit?.spg;
      if (share !== void 0) {
        const s2 = Math.round(n * clamp2(share, 0, 1)), own = this.kitModel(DR.spg.kit, army2);
        return none2.map((_, k) => k < s2 ? own : "");
      }
      const when = this.when();
      const row = (DR.spg.rows[army2] ?? []).find(([from, to]) => when >= from && when < to);
      if (!row) return none2;
      const [, , list, per, kinds] = row;
      const mine = this.b.allBlocks().filter((x) => x.side === u.side && this.armyOf(x.nat) === army2);
      let left = Math.round(mine.filter((x) => kinds.includes(x.kind)).length * per);
      let before = 0;
      for (const x of mine) {
        if (x.id === u.id) break;
        if (x.kind === "art" && !x.team && x.kit?.spg === void 0) before += Math.max(0, x.figures0);
      }
      left = Math.max(0, left - before);
      const s = Math.min(n, left);
      return none2.map((_, k) => k < s ? list[(before + k) % list.length] : "");
    }
    /**
     * Tank destroyers (release 2, LandBattle.td; looks only; the design director, 23 November 07:28Z, (a)): each of an
     * armoured block's n tanks, its model drawn as a tank destroyer or '' its block's own. The corps' share where the
     * campaign says it (ForceKit.td), its army's own model; else its army's row in the battle's month (DR.td.rows). Spread
     * through the block: the k-th is one where the share's count steps up (a quarter: the fourth of each four).
     */
    tdOf(u, n) {
      const none2 = Array.from({ length: n }, () => "");
      if (!this.b.td || u.kind !== "arm" || n <= 0) return none2;
      const army2 = this.armyOf(u.nat), kit = u.kit?.td;
      let share, list;
      if (kit !== void 0) {
        share = clamp2(kit, 0, 1);
        list = [this.kitModel(DR.td.kit, army2)];
      } else {
        const when = this.when();
        const row = (DR.td.rows[army2] ?? []).find(([from, to]) => when >= from && when < to);
        if (!row) return none2;
        share = row[3];
        list = row[2];
      }
      let c = 0;
      return none2.map((_, k) => Math.floor((k + 1) * share + 1e-9) > Math.floor(k * share + 1e-9) ? list[c++ % list.length] : "");
    }
    /**
     * Armoured cars and AA vehicles (release 2, LandBattle.cars and aa; looks only; the historian's rows, 23 November
     * 07:47Z; the design director, 07:47Z): the models of a block's cars or AA vehicles, none where it brings none. Where
     * the campaign says what the corps has (ForceKit.cars, ForceKit.aa: so many to each of its blocks, 1 in so many below
     * one), that many, its army's own model or the stand-in in its coat; else every row of its army in the battle's month
     * that names its kind (a battery is 'art'; a support team none). Below one a block, 1 in so many in the order of the
     * blocks of its kind (its corps' by the kit, its army's on its side by the rows): every other one at a half.
     */
    escortOf(u, what) {
      if (!(what === "cars" ? this.b.cars : this.b.aa) || u.team || u.kind === "fort") return [];
      const E = DR[what], army2 = this.armyOf(u.nat);
      const many = (per, i2) => Math.max(0, Math.floor((i2 + 1) * per + 1e-9) - Math.floor(i2 * per + 1e-9));
      const before = (same3) => this.b.allBlocks().filter((x) => x.side === u.side && x.kind === u.kind && !x.team && x.id < u.id && same3(x)).length;
      const kit = u.kit?.[what];
      if (kit !== void 0) return Array.from({ length: many(kit, before((x) => x.corps === u.corps)) }, () => this.kitModel(E.kit, army2));
      const when = this.when(), i = before((x) => this.armyOf(x.nat) === army2), out = [];
      for (const [from, to, model, per] of E.rows[army2] ?? []) {
        if (when < from || when >= to || per[u.kind] === void 0) continue;
        for (let c = many(per[u.kind], i); c > 0; c--) out.push(model);
      }
      return out;
    }
    /**
     * Jeeps, field cars and motorcycles (release 2, LandBattle.light; looks only; the historian's rows, 23 November
     * 08:18Z, historian/light-cars-motorcycles-1123.md): a block's light vehicles, cars first, never more than
     * DR.light.most. Where the campaign says what the corps has (ForceKit.jeeps, ForceKit.bikes: so many to each of its
     * blocks, 1 in so many below one), its army's own models; else its army's rows in the battle's month (every other
     * army's, '_', where its army has none) that name its kind, 1 in so many as the armoured cars, a row's models taken in
     * turn. Each with her work: out ahead where the month's rows say so for the block's kind, else parked behind it (a
     * motorcycle there riding despatch); and her swaps: a car behind is a staff car without her gun, a motorcycle's
     * sidecar by nation (DR.light.sw).
     */
    lightOf(u) {
      if (!this.b.light || u.team || u.kind === "fort") return [];
      const L2 = DR.light, army2 = this.armyOf(u.nat), when = this.when();
      const many = (per, i) => Math.max(0, Math.floor((i + 1) * per + 1e-9) - Math.floor(i * per + 1e-9));
      const before = (same3) => this.b.allBlocks().filter((x) => x.side === u.side && x.kind === u.kind && !x.team && x.id < u.id && same3(x)).length;
      const out = [];
      for (const what of ["cars", "bikes"]) {
        const rows = (L2[what][army2] ?? L2[what]._ ?? []).filter(([from, to]) => when >= from && when < to);
        const ahead2 = rows.some((r) => r[4][u.kind] === "ahead");
        const add = (ml) => {
          const sw = {};
          for (const [swap, by2] of Object.entries(L2.sw[ml] ?? {})) {
            const c = by2[u.nat] ?? by2._;
            if (c) sw[swap] = c;
          }
          if (what === "cars" && !ahead2) sw.mg = "off";
          out.push({ ml, duty: ahead2 ? "ahead" : what === "bikes" ? "despatch" : "behind", ...Object.keys(sw).length ? { sw } : {} });
        };
        const kit = u.kit?.[what === "cars" ? "jeeps" : "bikes"];
        if (kit !== void 0) {
          for (let c = many(kit, before((x) => x.corps === u.corps)); c > 0; c--) add(this.kitModel(L2.kit[what], army2));
          continue;
        }
        const i = before((x) => this.armyOf(x.nat) === army2);
        for (const [, , models, per] of rows) {
          if (per[u.kind] === void 0) continue;
          const n0 = Object.entries(per).reduce((n, [K, p]) => n + Math.floor(this.b.allBlocks().filter((x) => x.side === u.side && x.kind === K && !x.team && x.id < u.id && this.armyOf(x.nat) === army2).length * p + 1e-9), 0);
          for (let c = many(per[u.kind], i), j = 0; j < c; j++) add(models[(n0 + j) % models.length]);
        }
      }
      return out.slice(0, L2.most);
    }
    /**
     * A block's armoured cars, its AA vehicles and its jeeps and motorcycles (escortOf, lightOf), each a squad of vehicles only, drawn as tanks of their own
     * models (ml) with the block's men (view). Like armoured infantry they are a view of the block: its place, facing,
     * order, state and strength are its own, read live, and its box is grown to hold where they keep (outriders), so the
     * rules' counts and places are untouched. Kept under the block's id less one, less a million for the cars, two for
     * the AA and three for the light vehicles, numbered k from DR.mech.base and 100, 200 or 300 on; they fall as the block's figures go, in that share (the last
     * at half its figures), each to a round from the enemy owed most for its harm.
     */
    addEscorts(u) {
      for (const what of ["cars", "aa", "light"]) {
        const key = -(u.id + 1) - (what === "cars" ? 1e6 : what === "aa" ? 2e6 : 3e6);
        this.squads.delete(key);
        const light = what === "light" ? this.lightOf(u) : [];
        const models = what === "light" ? light.map((x) => x.ml) : this.escortOf(u, what);
        if (!models.length) continue;
        const c = models.length, base = DR.mech.base + (what === "cars" ? 100 : what === "aa" ? 200 : 300), L2 = DR.light;
        const reach = what === "cars" ? DR.cars.ahead + DR.cars.side + 40 : what === "aa" ? DR.aa.back + DR.aa.battery + 40 : Math.max(L2.ahead + L2.side, L2.back + L2.run) + 40;
        const v = Object.create(u);
        Object.defineProperties(v, {
          // (no foot's drill nor a tank block's reads them: a battery's kind, with no guns)
          kind: { value: "art", enumerable: true },
          team: { value: void 0, enumerable: true },
          figures0: { value: base + c, enumerable: true },
          figures: { get: () => u.figures <= 0 ? 0 : base + Math.min(c, Math.round(c * u.figures / Math.max(1, u.figures0))), enumerable: true },
          tray: { value: c, enumerable: true },
          w: { get: () => u.w + 2 * reach, enumerable: true },
          d: { get: () => u.d + 2 * reach, enumerable: true }
        });
        const men = models.map((ml, i) => ({
          k: base + i,
          n: (base + i) * 100,
          role: "tank",
          team: 0,
          x: u.x,
          y: u.y,
          face: u.face,
          pose: "halt",
          v: 0,
          spot: [u.x, u.y],
          cover: "none",
          until: 0,
          slotAt: [u.x, u.y],
          jx: 0,
          jy: 0,
          ml,
          ...light[i] ? { duty: light[i].duty, ...light[i].sw ? { sw: light[i].sw } : {} } : {}
        }));
        const sq = {
          u: v,
          tier: 0,
          men,
          veh: [],
          fo: null,
          phase: "rest",
          since: 0,
          nextThink: roll(this.seed, u.id, 5, base) * 0.5,
          lastStr: u.str,
          harm: 0,
          shelledAt: -99,
          lastX: u.x,
          lastY: u.y,
          heading: u.face,
          speed: 0,
          track: [],
          wx: 0,
          wy: 0,
          net: 0,
          down: true,
          fast: 0,
          slow: 0,
          close: false,
          ran: false,
          salvoAt: 0,
          ripple: 0,
          flank: 0,
          officerAt: 0,
          leap: true,
          m0: c,
          lost: 0,
          of: u.id,
          esc: what
        };
        const ahead2 = what !== "aa" && this.screening(sq, u);
        men.forEach((m, i) => {
          const s = this.escortSpot(sq, i, u, ahead2);
          m.x = s[0];
          m.y = s[1];
          m.spot = s;
          m.slotAt = s;
        });
        this.squads.set(key, sq);
      }
    }
    /** Whether a block's armoured cars are out ahead as its screen: it has not met the enemy (DR.cars.contact), nor fights, holds or goes back. */
    screening(sq, own) {
      const ph = this.squads.get(own.id)?.phase ?? "rest", e = this.threat(own);
      return own.engaged === void 0 && fighting(own) && (!e || this.b.gap(own, e) > DR.cars.contact) && !["fight", "pinned", "hold", "fallback", "flee", "battery", "gone"].includes(ph) && sq.harm <= 0;
    }
    /** Where a block's i-th car or AA vehicle keeps: cars on its flanks, out ahead or back behind; AA in a row behind it (a battery's behind its tractors). */
    escortSpot(sq, i, own, ahead2) {
      if (sq.esc === "light") {
        const L2 = DR.light, m = sq.men[i], s = i % 2 ? 1 : -1;
        if (m.duty === "ahead" && ahead2) return toWorld(own, own.face, s * (own.w / 2 + L2.side), own.d / 2 + L2.ahead);
        const p = m.duty === "despatch" ? (this.b.t + roll(this.seed, own.id, m.k, 7) * L2.every) % L2.every / L2.every : 0;
        return toWorld(own, own.face, s * (own.w / 2 + 8), -own.d / 2 - L2.back - (p >= 0.35 && p < 0.7 ? L2.run : 0));
      }
      if (sq.esc === "cars") {
        const C = DR.cars, s = i % 2 ? 1 : -1, row = Math.floor(i / 2);
        return toWorld(own, own.face, s * (own.w / 2 + C.side + row * 20), ahead2 ? own.d / 2 + C.ahead + row * 25 : -own.d / 2 - C.back - row * 20);
      }
      const A = DR.aa, n = sq.men.length;
      return toWorld(own, own.face, (i - (n - 1) / 2) * 22, -own.d / 2 - A.back - (own.kind === "art" ? A.battery : 0));
    }
    /**
     * A block's armoured cars or AA vehicles, each half second: each drives to her place (escortSpot) at the block's pace
     * and a little more, and halts there. A car's turret is on the enemy once he is within sight of the block; buttoned
     * up close to him. None fires (looks only: the design director, 23 November 07:47Z).
     */
    outriders(sq) {
      const own = Object.getPrototypeOf(sq.u), t = this.b.t;
      sq.phase = this.squads.get(own.id)?.phase ?? "rest";
      if (sq.phase === "gone") return;
      const e = this.threat(own), near = e ? this.b.gap(own, e) : Infinity;
      const ahead2 = sq.esc !== "aa" && this.screening(sq, own);
      sq.men.forEach((m, i) => {
        if (!this.stands(sq, m)) return;
        const s = this.escortSpot(sq, i, own, ahead2), d = Math.hypot(s[0] - m.x, s[1] - m.y);
        m.aim = sq.esc === "cars" && e && near < 1200 ? [e.x, e.y] : void 0;
        m.buttoned = sq.esc === "cars" && near < 400 || void 0;
        if (d > 5) {
          m.spot = s;
          m.slotAt = s;
          m.pose = "drive";
          m.v = clamp2(sq.speed * 1.2 + d * 0.25, 3, 12);
          m.until = t + 1;
        } else if (m.pose === "drive" || m.pose === "reverse") {
          m.spot = [m.x, m.y];
          m.slotAt = s;
          m.v = 0;
          m.pose = "halt";
        }
      });
    }
    // ---- Who is in a block ------------------------------------------------------------------------------------------
    make(u, mech) {
      const men = [], veh = [];
      const n = mech ? mech.n : Math.max(0, u.figures0);
      const roles = rolesFor(u.kind, u.tray, n, u.team);
      const fill = this.fillFor(u, roles.slice(0, n).map((r) => r[0])), tier = mech ? Math.min(1, fill.tier) : fill.tier;
      const B = mech?.base ?? 0;
      const man = (k, nn, role, team) => {
        nn += B * 100;
        const jx = (roll(this.seed, u.id, nn, 1) - 0.5) * 6, jy = (roll(this.seed, u.id, nn, 2) - 0.5) * 5;
        return { k: k + B, n: nn, role, team, x: u.x, y: u.y, face: u.face, pose: "idle", v: 0, spot: [u.x, u.y], cover: "none", until: 0, slotAt: [u.x, u.y], jx, jy };
      };
      for (let k = 0; k < n; k++) men.push(man(k, k, roles[k][0], roles[k][1]));
      const sp = mech ? [] : this.spgOf(u, n), td = mech ? [] : this.tdOf(u, n);
      let selfPropelled = 0;
      for (let k = 0; k < n; k++) {
        const ml = sp[k] || td[k];
        if (!ml) continue;
        men[k].ml = ml;
        if (sp[k]) {
          fill.sec[k] = 0;
          fill.plat[k] = 0;
          selfPropelled++;
        }
      }
      let lor = 0;
      if (tier >= 1) for (let k = 0; k < n; k++) {
        const key = men[k], mot = u.kind === "mot" && tier === 2;
        if (mot) {
          key.lor = lor;
          key.seat = 0;
        }
        const sec = sectionOf(u.nat, key.role);
        sec.slice(0, fill.sec[k]).forEach(([role, pr, scout], i) => {
          const m = man(k, 1e3 + k * 32 + i + 1, role, key.team);
          m.j = i + 1;
          m.sec = k + B;
          if (pr) m.pr = true;
          if (scout) m.scout = true;
          if (mot) {
            m.lor = lor;
            m.seat = i + 1;
          }
          men.push(m);
        });
        if (fill.sec[k] > 0) key.hasSec = true;
        lor++;
        if (tier < 2 || key.role !== "rifle" || fill.plat[k] <= 0) continue;
        key.plat = true;
        const P = platoonOf(u.nat), most = fill.plat[k];
        let j = sec.length, x = 0;
        const add = (sub, role, seat, opts = {}) => {
          if (x >= most) return false;
          const m = man(k, 2e4 + k * 128 + x++, role, key.team);
          m.j = ++j;
          m.sec = k + B;
          m.sub = sub;
          if (opts.pr) m.pr = true;
          if (opts.lead) m.lead = true;
          if (mot) {
            m.lor = lor;
            m.seat = seat;
          }
          men.push(m);
          return true;
        };
        for (let s = 1; s < P.squads && x < most; s++, lor++) {
          add(s, "rifle", 0, { lead: true });
          sec.forEach(([role, pr], i) => add(s, role, i + 1, { pr }));
        }
        if (x < most) {
          add(P.squads, "rifle", 0, { lead: true });
          for (let i = 1; i < P.hq; i++) add(P.squads, "rifle", i);
          lor++;
        }
      }
      const towed = n - selfPropelled;
      const lorries = mech ? n : u.kind === "mot" ? tier >= 1 ? lor : n >= 4 ? 2 : 1 : u.kind === "art" && !u.team ? tier >= 1 ? towed : towed > 0 ? 1 : 0 : 0;
      for (let i = 0; i < lorries; i++) veh.push({ k: 100 + i, n: 100 + i + B * 100, role: "lorry", team: 0, x: u.x, y: u.y, face: u.face, pose: "parked", v: 0, spot: [u.x, u.y], cover: "none", until: 0, slotAt: [u.x, u.y], jx: 0, jy: 0, ...mech ? { ml: mech.carriers[i % mech.carriers.length] } : {} });
      const sq = {
        u,
        tier,
        men,
        veh,
        fo: null,
        phase: "rest",
        since: 0,
        nextThink: roll(this.seed, u.id, 3) * 0.5,
        lastStr: u.str,
        harm: 0,
        shelledAt: -99,
        lastX: u.x,
        lastY: u.y,
        heading: u.face,
        speed: 0,
        track: [],
        wx: 0,
        wy: 0,
        net: 0,
        down: u.kind !== "mot" && u.kind !== "cav",
        fast: 0,
        slow: 0,
        close: false,
        ran: false,
        salvoAt: 0,
        ripple: 0,
        flank: 0,
        officerAt: 0,
        leap: true,
        m0: men.length,
        lost: 0,
        ...mech ? { of: mech.of } : {}
      };
      this.layStart(sq);
      return sq;
    }
    /**
     * How many of each key man's section (sec) and platoon (plat: its other squads and headquarters, in order) a block
     * draws (GR3; the production lead, 22 November 07:23Z). Unscaled, all the battle's setting draws. Under the scale its
     * key men always (the rules' models), then their sections in squad order, then the rifle models' platoons squad by
     * squad, as many men as fit its men0 over N, counted as the 3D game shows them (menOnScreen): the last group drawn
     * short, so every block draws within a man of its cap where its whole draw would be more. tier: the most any key draws.
     */
    fillFor(u, keys) {
      const top = this.platoons ? 2 : this.sections ? 1 : 0;
      const secs = keys.map((r) => top >= 1 ? sectionOf(u.nat, r).length : 0);
      const P = platoonOf(u.nat);
      const plats = keys.map((r, i) => top === 2 && r === "rifle" ? (P.squads - 1) * (1 + secs[i]) + P.hq : 0);
      if (this.scale === null || top === 0) return { tier: top, sec: secs, plat: plats };
      let room = Math.floor(u.men0 / this.scale) - keys.reduce((c, r) => c + menOnScreen(r), 0);
      const take = (most) => {
        const n = Math.max(0, Math.min(room, most));
        room -= n;
        return n;
      };
      const sec = secs.map(take), plat = plats.map(take);
      return { tier: plat.some((x) => x > 0) ? 2 : sec.some((x) => x > 0) ? 1 : 0, sec, plat };
    }
    layStart(sq) {
      const u = sq.u;
      sq.phase = this.phaseOf(sq);
      const tpl = u.kind === "arm" || u.kind === "art" ? "line" : sq.phase === "rest" ? "rest" : "line";
      for (const m of sq.men) {
        const [a, l] = this.slot(sq, m, tpl);
        const s = toWorld(u, u.face, a, l);
        const best = this.seek(sq, m, s, this.threatDir(sq), sq.men);
        m.x = best.x;
        m.y = best.y;
        m.spot = [best.x, best.y];
        m.cover = best.kind;
        m.slotAt = s;
        m.pose = u.kind === "arm" ? "halt" : u.kind === "art" ? "deployed" : u.kind === "mot" || u.kind === "cav" ? u.kind === "mot" ? "mounted" : "ride" : "wait";
      }
      sq.veh.forEach((v, i) => {
        const s = toWorld(u, u.face, (i - (sq.veh.length - 1) / 2) * 14, -u.d / 2 - 30);
        v.x = s[0];
        v.y = s[1];
        v.spot = s;
        v.slotAt = s;
        v.pose = "parked";
      });
      if (u.kind === "mot") {
        if (sq.phase === "hold" || sq.phase === "startline" || sq.phase === "fight" || sq.phase === "pinned" || !sq.veh.length) {
          sq.down = true;
          for (const m of sq.men) m.pose = "wait";
        } else for (const m of sq.men) {
          const p = this.seat(sq, m);
          m.x = p[0];
          m.y = p[1];
          m.spot = p;
          m.cover = "none";
        }
      }
      if (sq.tier >= 1) {
        const room = /* @__PURE__ */ new Map(), order = [...sq.men].sort((a, c) => (a.j === void 0 ? 0 : 1) - (c.j === void 0 ? 0 : 1) || (a.sec ?? a.k) - (c.sec ?? c.k) || (a.j ?? 0) - (c.j ?? 0));
        for (const m of order) {
          if (VEHICLE.has(m.role) || CARRIED.has(m.pose)) continue;
          const p = this.leash(sq, this.clear(room, sq, m, [m.x, m.y]), -4);
          m.x = p[0];
          m.y = p[1];
          m.spot = p;
          this.bookRoom(room, m, p);
        }
      }
    }
    // ---- Each step ----------------------------------------------------------------------------------------------------
    step() {
      const b = this.b, t = b.t;
      if (b.over) {
        if (!this.settled) {
          for (const sq of this.squads.values()) this.falls(sq);
          this.settle();
        }
        return;
      }
      let old = 0;
      while (old < this.shots.length && this.shots[old].t < t - DR.rounds.keep) old++;
      if (old) this.shots.splice(0, old);
      for (const sq of this.squads.values()) {
        const u = sq.u;
        const lost = Math.max(0, sq.lastStr - u.str) / Math.max(1e-6, u.str0);
        sq.harm = sq.harm * 0.95 + lost / DT * 0.05;
        sq.lastStr = u.str;
        const moved = Math.hypot(u.x - sq.lastX, u.y - sq.lastY);
        sq.speed = sq.speed * 0.8 + moved / DT * 0.2;
        if (moved > 0.05) sq.heading = bearing2(sq.lastX, sq.lastY, u.x, u.y);
        sq.lastX = u.x;
        sq.lastY = u.y;
        sq.track.push(u.x, u.y);
        if (sq.track.length > 2 * (Math.round(MOUNT.span / DT) + 1)) sq.track.splice(0, 2);
        const span2 = (sq.track.length / 2 - 1) * DT;
        sq.wx = span2 > 0 ? (u.x - sq.track[0]) / span2 : 0;
        sq.wy = span2 > 0 ? (u.y - sq.track[1]) / span2 : 0;
        sq.net = Math.hypot(sq.wx, sq.wy);
        sq.slow = sq.speed < MOUNT.down || sq.net < MOUNT.down ? sq.slow + DT : 0;
        sq.fast = sq.net > MOUNT.up && sq.speed > MOUNT.up ? sq.fast + DT : sq.slow >= 1 ? 0 : Math.max(0, sq.fast - 2 * DT);
        this.falls(sq);
        if (b.bearers && !sq.esc) this.litters(sq);
        if (t >= sq.nextThink && sq.esc) {
          sq.nextThink = t + 0.5;
          this.outriders(sq);
        } else if (t >= sq.nextThink) {
          sq.nextThink = t + 0.5;
          if (this.byBook) this.feel(sq);
          this.think(sq);
          this.pairs(sq, this.space(sq));
          if (b.escort) this.escort(sq);
          if (sq.screenQ) this.screenRound(sq);
        }
        this.walk(sq);
        this.fire(sq);
      }
      this.shellKills.clear();
      this.byOf.clear();
      if (b.escort) this.clearOfVehicles();
      if (t >= this.leadAt) {
        this.leadAt = t + 0.5;
        this.pickLeads();
      }
    }
    /**
     * The squad leading each side's assault, for the platoon cam (release 2): of the side's foot squads still standing,
     * those in the assault (on the move now, or within LEAD_RUSH seconds: a squad rushes, drops and fires, and rushes
     * again on its turn), the one nearest the enemy; where none is, the foot squad nearest the enemy. The camera is not
     * thrown from squad to squad: the one leading keeps the lead while it stands and is still in the assault (or none
     * is), unless another is LEAD_BY metres nearer the enemy. Looks only: nothing in the battle reads it.
     */
    pickLeads() {
      const LEAD_BY = 25, LEAD_RUSH = 15, t = this.b.t;
      const live_ = [...this.squads.values()].filter((sq) => live(sq.u));
      for (const side of ["att", "def"]) {
        const foes = live_.filter((sq) => sq.u.side !== side && !sq.u.hid);
        const near = (x, y) => foes.reduce((d, f) => Math.min(d, Math.hypot(f.u.x - x, f.u.y - y)), Infinity);
        const cands = [];
        for (const sq of live_) {
          if (sq.u.side !== side || sq.u.kind === "arm" || sq.u.kind === "art" || sq.u.kind === "fort") continue;
          const groups = /* @__PURE__ */ new Map();
          for (const m of sq.men) {
            if (!this.stands(sq, m) || m.role === "holder") continue;
            const key = grp(m), e = groups.get(key) ?? { x: 0, y: 0, n: 0, moving: false };
            e.x += m.x;
            e.y += m.y;
            e.n++;
            if (this.byBook ? m.el === 2 : MOVING.has(m.pose)) e.moving = true;
            groups.set(key, e);
          }
          for (const [g, e] of groups) {
            const key = `${sq.u.id}:${g}`;
            if (e.moving) this.rushed.set(key, t);
            const at = this.rushed.get(key);
            cands.push({ id: sq.u.id, g, d: foes.length ? near(e.x / e.n, e.y / e.n) : 0, on: at !== void 0 && t - at <= LEAD_RUSH });
          }
        }
        if (!cands.length) {
          this.leads.delete(side);
          continue;
        }
        const pool = cands.some((c) => c.on) ? cands.filter((c) => c.on) : cands;
        const best = pool.reduce((a, c) => c.d < a.d ? c : a);
        const was = this.leads.get(side), cur = was && pool.find((c) => c.id === was[0] && c.g === was[1]);
        this.leads.set(side, cur && best.d > cur.d - LEAD_BY ? was : [best.id, best.g]);
      }
    }
    /**
     * The battle is over: every man stops where he is. The winners stand easy, the beaten keep their heads down, and those
     * who ran crouch where they got to; tanks halt, guns stand, lorries park.
     */
    settle() {
      this.settled = true;
      const won = this.b.over?.winner;
      for (const sq of this.squads.values()) {
        const u = sq.u;
        for (const c of sq.carries ?? []) if (c.stage === "carry") c.man.pose = "wounded";
        for (const m of [...sq.men, ...sq.veh, ...sq.fo ?? []]) {
          if (m.fell !== void 0) continue;
          m.v = 0;
          m.went = 0;
          m.spot = [m.x, m.y];
          m.aim = void 0;
          m.hidden = void 0;
          m.pose = m.role === "tank" ? "halt" : m.role === "gun" || CREWED.has(m.role) ? "deployed" : m.role === "lorry" ? "parked" : m.role === "holder" ? m.pose === "ride" ? "ride" : "horses" : m.pose === "mounted" || m.pose === "ride" ? m.pose : sq.phase === "flee" ? "cower" : u.side === won ? m.k % 3 ? "idle" : "wait" : "wait";
        }
      }
    }
    /** A shell burst at a point (sim.ts shell): men of every block near it duck. */
    shelled(x, y, by2, on, k) {
      this.duck(x, y);
      if (on === void 0 || k === void 0) return;
      this.shellKills.add(on + ":" + k);
      const vq = this.squads.get(on), v = vq?.men.find((x2) => x2.k === k);
      if (vq && v) this.killRound(by2 !== void 0 ? this.b.unit(by2) : void 0, vq, v, [x, y]);
      if (vq && this.b.blast) vq.anchor = { x, y, t: this.b.t };
    }
    /** A model the rules took with no striker on the field (the air's bombs and guns): no round of the field's for him. */
    fromAir(on, k) {
      this.shellKills.add(on + ":" + k);
    }
    /** A model the rules took to a block's small arms (no shell): his round comes from that block (falls). */
    struckBy(by2, on, k) {
      this.byOf.set(on + ":" + k, by2);
    }
    /** Men of every block near a burst duck. */
    duck(x, y) {
      for (const sq of this.squads.values()) if (Math.hypot(sq.u.x - x, sq.u.y - y) < Math.max(sq.u.w, sq.u.d) / 2 + 40) sq.shelledAt = this.b.t;
    }
    /** Where model k of block id stands now (alive, or the moment he falls): the shell that cost him lands there. */
    manAt(id, k) {
      const sq = this.squads.get(id);
      const m = sq?.men.find((x) => x.k === k);
      return m ? [m.x, m.y] : null;
    }
    /** Where a wreck lies as drawn: where her tank fell, or the rules' own place for one that fell before the men were drawn. */
    wreckSpot(z) {
      return this.wreckAt.get(z.id) ?? [z.x, z.y];
    }
    /**
     * Men the rules have lifted off the tray fall where they stand, a tank too: she burns where she stood (at one of her
     * posts, up to about 20 m from the place the rules lay her wreck, their own mark for its shelter; wreckSpot).
     */
    falls(sq) {
      const u = sq.u, t = this.b.t;
      for (const m of sq.men) {
        if (m.j !== void 0 || m.fell !== void 0 || m.k < u.figures) continue;
        if (u.state === "withdrawn" || u.state === "gone" && u.figures > 0) continue;
        if (u.state === "gone" && (u.y < 0 || u.y > this.b.field.h)) continue;
        const key = u.id + ":" + m.k, by2 = this.byOf.get(key), shell = this.shellKills.delete(key);
        this.byOf.delete(key);
        m.fell = t;
        m.v = 0;
        m.aim = void 0;
        m.hidden = void 0;
        sq.memo = void 0;
        if (m.role === "tank" && !sq.esc) {
          const nth = sq.men.filter((x) => x.role === "tank" && x.fell !== void 0 && x !== m).length;
          const w = this.b.wrecks.filter((z) => z.unit === u.id)[nth];
          if (w) this.wreckAt.set(w.id, [m.x, m.y]);
        }
        if (!shell && by2 !== void 0) this.killRound(this.b.unit(by2), sq, m);
        else if (!shell && sq.esc) {
          const st = this.strikerOf(u);
          if (st) this.killRound(st, sq, m, st.kind === "arm" || st.kind === "art" && st.team !== "mg" ? [m.x, m.y] : void 0);
        }
        m.pose = "down";
        m.spot = [m.x, m.y];
        if (this.b.bearers && !sq.esc) this.hurt(sq, m);
        this.aid(sq, m);
        if (sq.tier >= 1) this.regroup(sq, m);
      }
      if (sq.tier >= 1) this.thin(sq);
    }
    /** A comrade goes to a man who fell, where one is near and the block is not running (nor riding, nor mounting again). */
    aid(sq, m) {
      const u = sq.u, t = this.b.t;
      if (!fighting(u) || m.role === "tank" || m.role === "gun" || !sq.down || sq.boarding !== void 0) return;
      const helper = sq.men.filter((h) => this.stands(sq, h) && h.role === "rifle" && !h.pr && h.aid === void 0 && Math.hypot(h.x - m.x, h.y - m.y) < 30).sort((a, c) => Math.hypot(a.x - m.x, a.y - m.y) - Math.hypot(c.x - m.x, c.y - m.y))[0];
      if (helper && roll(this.seed, u.id, m.n, t * 10) < 0.6) {
        helper.aid = m.k;
        helper.spot = [m.x + 1.2, m.y - 0.8];
        helper.until = t + 6;
        helper.aim = void 0;
        helper.pose = sq.harm > DR.pin.harm ? "crawl" : "creep";
        helper.v = helper.pose === "crawl" ? 1.2 : 2.5;
      }
    }
    /**
     * A key man fell (his model lifted): the rest of his section still on their feet join the nearest section left, as the
     * remnant of a shattered section does: a gun's crew another gun, a support team's bearers another weapon of its kind,
     * the rest the nearest section on foot. With none to join, a loader takes up a rifle and fights as one.
     */
    regroup(sq, key) {
      const left = sq.men.filter((m) => m.j !== void 0 && m.fell === void 0 && (m.sec ?? m.k) === key.k);
      if (!left.length) return;
      const heavy = key.role === "gun" || CREWED.has(key.role);
      const keys = sq.men.filter((x) => x.j === void 0 && x !== key && this.stands(sq, x) && (heavy ? x.role === key.role : !VEHICLE.has(x.role) && !CREWED.has(x.role)));
      const to = this.nearest(keys, key)?.[0];
      for (const m of left) {
        if (to) {
          m.sec = to.k;
          continue;
        }
        if (m.pr && m.role !== "crew") {
          m.pr = void 0;
          if (m.role === "loader") m.role = "rifle";
        }
      }
      sq.memo = void 0;
    }
    /**
     * The rest of the sections fall one by one as the block loses strength: its men on their feet are kept to its men in
     * all times the share of its strength left, so a block at half strength shows half its men (the key men fall only
     * with their models). Each goes to the enemy block owed most for the harm (the one the rules give the next model to,
     * sim.ts suffer), the man nearest it first (a man in good cover less often), with the round that took him: a shell's
     * takes the men round its burst together, one round each at the one burst (the same t, x and y); small arms one by one.
     * A block that ran off the field, or left it in order, takes its men with it.
     */
    // ---- Infantry with tanks (release 2) -----------------------------------------------------------------------------
    //
    // Brian, 23 November 05:05Z: "if the troops are with tanks, have them advance behind the tank using it as cover like
    // they would in real life for the units near the tank and make sure their players cant be run over by their own
    // vechiles and they get out of the way. vechiles can run over enemy soldiers though which would kill them".
    // LandBattle.escort, looks only (the drill's, DRILL.escort): the rules' counts and places are untouched.
    /** A tank's key (her block's id x 1000 + her k), and the tanks of a side standing, each with her key. */
    tanksStanding(side, x, y, within) {
      const out = [];
      for (const tq of this.squads.values()) {
        if (tq.u.side !== side || tq.u.kind !== "arm" || !live(tq.u)) continue;
        if (Math.hypot(tq.u.x - x, tq.u.y - y) > within + Math.max(tq.u.w, tq.u.d) / 2) continue;
        for (const v of tq.men) if (v.role === "tank" && this.stands(tq, v)) out.push([v, tq.u.id * 1e3 + v.k]);
      }
      return out;
    }
    /**
     * A foot block on the advance with friendly tanks near: up to DRILL.escort.men of its riflemen nearest each tank file
     * in behind her, a few metres back and a stride to either side of her tracks, at her pace (she is their cover); when
     * she halts, they kneel behind her. Not while riding, mounting or carrying; the rest of the block keeps its drill.
     */
    escort(sq) {
      const u = sq.u, E = DR.escort, t = this.b.t;
      const on = isFoot2(u) && sq.tier >= 1 && (sq.phase === "deploy" || sq.phase === "bound") && sq.boarding === void 0 && (u.kind !== "mot" || !!sq.down) && fighting(u);
      const tanks = on ? this.tanksStanding(u.side, u.x, u.y, E.near + Math.max(u.w, u.d) / 2) : [];
      const keys = new Set(tanks.map(([, k]) => k));
      for (const m of sq.men) if (m.esc !== void 0 && (!keys.has(m.esc) || !this.stands(sq, m))) m.esc = void 0;
      for (const [k, list] of this.escorts) this.escorts.set(k, list.filter((m) => m.esc === k && m.fell === void 0));
      if (!tanks.length) return;
      const free = (m) => this.stands(sq, m) && m.esc === void 0 && !m.pr && !m.aid && !m.scout && m.role === "rifle" && !CARRIED.has(m.pose);
      for (const [v, k] of tanks) {
        const list = this.escorts.get(k) ?? [];
        if (list.length < E.men) {
          const [bx, by2] = fwd(v.face), back = [v.x - bx * E.gap, v.y - by2 * E.gap];
          const cand = sq.men.filter((m) => free(m) && Math.hypot(m.x - back[0], m.y - back[1]) < E.reach).sort((a, c) => Math.hypot(a.x - back[0], a.y - back[1]) - Math.hypot(c.x - back[0], c.y - back[1]) || a.n - c.n);
          for (const m of cand.slice(0, E.men - list.length)) {
            m.esc = k;
            list.push(m);
          }
          this.escorts.set(k, list);
        }
        const [fx, fy] = fwd(v.face), [rx, ry] = right(v.face), moving = (v.went ?? 0) > E.moving;
        list.forEach((m, i) => {
          if (m.esc !== k || !sq.men.includes(m)) return;
          const back = E.gap + E.file * Math.floor(i / 2), side = (i % 2 ? 1 : -1) * E.side;
          const p = [v.x - fx * back + rx * side, v.y - fy * back + ry * side];
          m.spot = p;
          m.slotAt = p;
          m.cover = "wreck";
          m.aim = void 0;
          if (moving || Math.hypot(m.x - p[0], m.y - p[1]) > 1.5) {
            m.pose = "advance";
            m.v = clamp2((v.went ?? 0) + 0.8, E.pace[0], E.pace[1]);
            m.until = t + 1;
          }
        });
      }
    }
    /**
     * Every man on his feet gets out of a vehicle's way: a man of her own side in front of a tank or a lorry on the move
     * (within DRILL.escort.clear.ahead of her nose and a little to either side of her tracks) steps out to the side he is
     * nearer, at a run, and his place is moved off her way; an enemy only once she is almost on him (clear.enemyAhead), so
     * she can catch one the rules take then (Drill.thin: a tank crushes him). Fallen and wounded men are not moved.
     */
    clearOfVehicles() {
      const C = DR.escort.clear, t = this.b.t;
      const movers = [];
      for (const vq of this.squads.values()) {
        if (!live(vq.u)) continue;
        for (const v of vq.men) {
          if (v.role !== "tank" && v.role !== "lorry" || v.fell !== void 0 || (v.went ?? 0) <= DR.escort.moving) continue;
          const f = v.pose === "reverse" ? v.face + Math.PI : v.face;
          const [fx, fy] = fwd(f), [rx, ry] = right(f);
          movers.push({ v, side: vq.u.side, fx, fy, rx, ry });
        }
      }
      if (!movers.length) return;
      for (const sq of this.squads.values()) {
        const u = sq.u;
        if (!live(u) || u.kind === "arm" || u.kind === "art") continue;
        const near = movers.filter((o) => Math.hypot(o.v.x - u.x, o.v.y - u.y) < C.within + Math.max(u.w, u.d) / 2);
        if (!near.length) continue;
        for (const m of sq.men) {
          if (!this.stands(sq, m) || VEHICLE.has(m.role) || CARRIED.has(m.pose) || m.pose === "wounded") continue;
          if (m.dodged !== void 0 && t - m.dodged < C.dodge) continue;
          for (const o of near) {
            const dx = m.x - o.v.x, dy = m.y - o.v.y;
            const a = dx * o.fx + dy * o.fy, l = dx * o.rx + dy * o.ry;
            if (o.side !== u.side && LYING.has(m.pose)) continue;
            const ahead2 = o.side === u.side ? C.ahead : C.enemyAhead;
            if (a < -C.behind || a > ahead2 || Math.abs(l) >= C.half) continue;
            const s = l >= 0 ? 1 : -1, go = C.out - Math.abs(l);
            const out = [m.x + o.rx * s * go, m.y + o.ry * s * go];
            const sa = (m.spot[0] - o.v.x) * o.fx + (m.spot[1] - o.v.y) * o.fy, sl = (m.spot[0] - o.v.x) * o.rx + (m.spot[1] - o.v.y) * o.ry;
            m.spot = sa > -C.behind && sa < 30 && Math.abs(sl) < C.out ? [m.spot[0] + o.rx * (s * C.out - sl), m.spot[1] + o.ry * (s * C.out - sl)] : m.spot;
            m.slotAt = m.spot;
            m.pose = "run";
            m.v = Math.max(m.v, 4.5);
            m.until = t + C.dodge;
            m.aim = void 0;
            m.dodged = t;
            m.spot = Math.hypot(m.spot[0] - m.x, m.spot[1] - m.y) < 0.5 ? out : m.spot;
            if (m.esc === void 0) m.spot = out;
            break;
          }
        }
      }
    }
    /**
     * A man the rules take from sq lying in the path of an enemy tank on the move, close under her nose (Drill.thin): her,
     * him and her block. Any enemy tank near the block will do: which enemy took the man is the drill's to choose.
     */
    crushOf(sq, cand) {
      const C = DR.escort.clear, u = sq.u;
      for (const bq of this.squads.values()) {
        if (bq.u.side === u.side || bq.u.kind !== "arm" || !live(bq.u)) continue;
        if (Math.hypot(bq.u.x - u.x, bq.u.y - u.y) > C.within + (Math.max(u.w, u.d) + Math.max(bq.u.w, bq.u.d)) / 2) continue;
        for (const v of bq.men) {
          if (v.role !== "tank" || !this.stands(bq, v) || (v.went ?? 0) <= DR.escort.moving) continue;
          const f = v.pose === "reverse" ? v.face + Math.PI : v.face;
          const [fx, fy] = fwd(f), [rx, ry] = right(f);
          for (const m of cand) {
            if (!this.stands(sq, m)) continue;
            const dx = m.x - v.x, dy = m.y - v.y, a = dx * fx + dy * fy, l = dx * rx + dy * ry;
            if (a > -1 && a < C.crush && Math.abs(l) < C.crushHalf) return [v, m, bq.u];
          }
        }
      }
      return void 0;
    }
    /** How much of him a burst finds (shell blasts): a man lying down DR.blast.lying, kneeling DR.blast.kneeling, else 1. */
    exposed(m) {
      if (LYING.has(m.pose)) return DR.blast.lying;
      if (m.pose === "fire_kneel" || m.pose === "wait" || m.pose === "creep") return DR.blast.kneeling;
      return 1;
    }
    /** Who struck a block: the enemy block owed most for its harm (sim.ts suffer), else the one it faces. */
    strikerOf(u) {
      const b = this.b, owed = u.owed ?? {};
      let by2;
      for (const id of Object.keys(owed).map(Number)) {
        const e = b.unit(id);
        if (e && live(e) && e.side !== u.side && (!by2 || owed[id] > owed[by2.id])) by2 = e;
      }
      return by2 ?? this.threat(u) ?? void 0;
    }
    thin(sq) {
      const u = sq.u, b = this.b;
      if (u.state === "withdrawn" || u.state === "gone" && (u.figures > 0 || u.y < 0 || u.y > b.field.h)) return;
      const want = u.str <= 0 ? 0 : Math.ceil(sq.m0 * u.str / Math.max(1e-9, u.str0) - 1e-9);
      let up = 0;
      for (const m of sq.men) if (this.stands(sq, m) || m.j !== void 0 && m.fell === void 0) up++;
      let n = up - want;
      if (n < 0 && this.b.bearers) {
        this.rise(sq, -n);
        return;
      }
      if (n <= 0) return;
      const cand = sq.men.filter((m) => m.j !== void 0 && m.fell === void 0 && !m.job);
      if (!cand.length) return;
      const by2 = this.strikerOf(u);
      const shell = !!by2 && (by2.kind === "arm" && !this.mgKill(by2, u, sq.lost) || by2.kind === "art" && by2.team !== "mg");
      if (sq.of !== void 0 && !shell) {
        for (let i = cand.length - 1; i >= 0; i--) if (cand[i].pose === "mounted" && DR.mech.armoured.includes(this.lorryOf(sq, cand[i]).ml ?? "")) cand.splice(i, 1);
        if (!cand.length) return;
      }
      const from = by2 ? [by2.x, by2.y] : [u.x, u.y];
      let burst;
      if (b.escort) {
        const crushed = this.crushOf(sq, cand);
        if (crushed) {
          const [v, m, tu] = crushed;
          cand.splice(cand.indexOf(m), 1);
          sq.lost++;
          n--;
          m.fell = b.t;
          m.v = 0;
          m.aim = void 0;
          m.hidden = void 0;
          this.killRound(tu, sq, m, void 0, false, v);
          m.pose = "down";
          m.spot = [m.x, m.y];
          this.aid(sq, m);
          if (n <= 0 || !cand.length) return;
        }
      }
      const blast = shell && b.blast && !!by2;
      const cal = by2?.cal ?? "field";
      if (blast) {
        const a = sq.anchor && b.t - sq.anchor.t <= DR.blast.anchor ? sq.anchor : void 0;
        if (a) burst = [a.x, a.y];
        else if (fighting(u) && !b.over && n < DR.blast.most[cal] && b.t - (sq.held ?? (sq.held = b.t)) < DR.blast.hold) return;
        sq.held = void 0;
      }
      const reach = blast ? REACH[cal] : 12;
      while (n-- > 0 && cand.length) {
        let best = 0, score = Infinity;
        cand.forEach((m2, i) => {
          const p = burst ?? from;
          const sc = Math.hypot(m2.x - p[0], m2.y - p[1]) * (2 - (DR.rounds.cover[m2.cover] ?? 1)) / (blast ? this.exposed(m2) : 1) + (burst ? 6 : 40) * roll(this.seed, u.id, m2.n, sq.lost, 81);
          if (sc < score) {
            score = sc;
            best = i;
          }
        });
        const m = cand.splice(best, 1)[0];
        sq.lost++;
        let mate = false;
        if (shell && (!burst || Math.hypot(m.x - burst[0], m.y - burst[1]) > reach)) {
          burst = [m.x, m.y];
          this.duck(m.x, m.y);
        } else if (blast) mate = true;
        m.fell = b.t;
        m.v = 0;
        m.aim = void 0;
        m.hidden = void 0;
        if (by2) this.killRound(by2, sq, m, shell ? burst : void 0, mate);
        m.pose = "down";
        m.spot = [m.x, m.y];
        if (b.bearers) this.hurt(sq, m);
        this.aid(sq, m);
      }
      sq.memo = void 0;
    }
    // ---- Stretcher bearers (release 2) ------------------------------------------------------------------------------
    //
    // Brian, 22 November 17:33Z; the design director's rulings Q4 and Q5 (18:36Z); LandBattle.bearers and LAND.bearers.
    // The rules count the bearers and the wounded (LandBattle.litter); the drill draws them, looks only. Of the men who
    // fall, the rules' share (1 - killed) lie wounded: a man of a section a light case at the rules' share, lying for his
    // dressing and up again once it is done and his block's strength has them back (thin, rise); the rest lie for the
    // bearers. The bearers are riflemen of the block (br), two to a stretcher, as many stretchers as the rules' bearers
    // carry cases at once (its army's share of its men over LAND.bearers.litter). A pair fetches the litter case fallen
    // earliest, lifts him and carries him head first to its aid post (LandBattle.aidOf) at LAND.bearers.pace, his place the
    // stretcher's middle, the front bearer 1.12 m ahead of him and the rear 1.35 m behind; he stays there (gone), and they
    // walk home. A block running, fled or gone sends no stretcher out; one on its way goes on. Every choice is the drill's
    // own dice: the men drawn and the fight are the same as without them.
    /** A man who falls, with the bearers on: wounded at the rules' share (else killed), a man of a section a light case at theirs. */
    hurt(sq, m) {
      if (VEHICLE.has(m.role) || CREWED.has(m.role) || m.wd !== void 0) return;
      const B = LAND.bearers, u = sq.u;
      if (roll(this.seed, u.id, m.n, 71) >= 1 - killedShare(u.nat)) return;
      m.wd = this.b.t;
      m.pose = "wounded";
      if (m.j !== void 0 && roll(this.seed, u.id, m.n, 72) < B.light.share) {
        m.light = true;
        (sq.dressing ?? (sq.dressing = [])).push(m);
      } else (sq.lying ?? (sq.lying = [])).push(m);
    }
    /** Its light cases whose dressing is done get up, up to n of them, the earliest hurt first, while its block fights. */
    rise(sq, n) {
      const u = sq.u, t = this.b.t, L2 = LAND.bearers.light, foot = LAND.speed.inf;
      if (!fighting(u) || !sq.dressing?.length) return;
      const walk2 = L2.at > 0 ? L2.at / (foot * L2.out) + L2.at / (foot * L2.back) : 0;
      while (n > 0 && sq.dressing.length && sq.dressing[0].wd + walk2 + L2.dress <= t + 1e-9) {
        const m = sq.dressing.shift();
        m.light = void 0;
        m.fell = void 0;
        m.back = t;
        m.pose = "wait";
        m.v = 0;
        m.spot = [m.x, m.y];
        m.slotAt = [m.x, m.y];
        m.until = t + 1;
        if (!this.memo(sq).keyOf.has(m.sec ?? m.k)) {
          const to = this.nearest(this.memo(sq).keys.filter((x) => !VEHICLE.has(x.role) && !CREWED.has(x.role)), m)?.[0];
          if (to) {
            m.sec = to.k;
            m.sub = void 0;
            m.lead = void 0;
          }
        }
        sq.memo = void 0;
        n--;
      }
    }
    /** A bearer named in a carry: his model and his place in it. */
    byId(m) {
      return m.k * 1e3 + (m.j ?? 0);
    }
    /**
     * Each step with the bearers on: its bearers picked (the first time), each pair free of a carry and on its feet sent
     * for the next litter case (its block fighting, its men down), and every carry moved on a step.
     */
    litters(sq) {
      const u = sq.u, b = this.b, t = b.t, B = LAND.bearers, f = b.field;
      if (!sq.brs) {
        const cand = sq.men.filter((m) => m.j !== void 0 && m.role === "rifle" && !m.pr && !m.scout && !m.lead && m.fell === void 0).sort((a, c) => roll(this.seed, u.id, a.n, 73) - roll(this.seed, u.id, c.n, 73) || a.n - c.n);
        const pairs2 = Math.min(Math.floor(cand.length / 2), Math.max(1, Math.round(bearerShare(u.nat) * sq.m0 / B.litter)));
        sq.brs = cand.slice(0, 2 * pairs2);
        for (const m of sq.brs) m.br = true;
      }
      const carries = sq.carries ?? (sq.carries = []);
      if (sq.lying?.length && fighting(u) && sq.down && sq.boarding === void 0) {
        const free = sq.brs.filter((m) => !m.job && this.stands(sq, m) && !CARRIED.has(m.pose));
        for (let i = 0; i + 1 < free.length && sq.lying.length; i += 2) {
          const man = sq.lying.shift(), [ax, ay] = b.aidOf(u);
          const to = [clamp2(ax, 3, f.w - 3), clamp2(ay, 3, f.h - 3)], way = bearing2(man.x, man.y, to[0], to[1]);
          const c = { man, front: free[i], rear: free[i + 1], stage: "fetch", from: [man.x, man.y], to, way, t0: t, t1: t };
          for (const m of [c.front, c.rear]) {
            m.job = c;
            m.aim = void 0;
            m.hidden = void 0;
            m.pose = "walk";
            m.v = B.pace;
          }
          carries.push(c);
          sq.memo = void 0;
        }
      }
      for (const c of carries) this.carryStep(sq, c);
      if (carries.some((c) => c.front.job !== c && c.rear.job !== c)) sq.carries = carries.filter((c) => c.front.job === c || c.rear.job === c);
    }
    /** A carry's step: the bearers walk to the man, lift him, carry him to the aid post and walk home. */
    carryStep(sq, c) {
      var _a;
      const u = sq.u, t = this.b.t, B = LAND.bearers, rate = TURN.foot * DT, [wx, wy] = fwd(c.way);
      const FRONT2 = 1.12, REAR = 1.35;
      const stride = (m, p, v) => {
        const ddx = p[0] - m.x, ddy = p[1] - m.y, d = Math.hypot(ddx, ddy);
        if (d < 0.05) {
          m.went = 0;
          return true;
        }
        const want = Math.atan2(ddx, ddy);
        m.face = turnTo(m.face, want, rate);
        const s = Math.min(d, v * DT) * Math.max(0, Math.cos(wrap(want - m.face)));
        m.x += ddx / d * s;
        m.y += ddy / d * s;
        m.went = s / DT;
        return s >= d - 1e-9;
      };
      const bearers = [c.front, c.rear];
      if (c.stage === "fetch") {
        const at = (m) => m === c.front ? [c.man.x + wx * FRONT2, c.man.y + wy * FRONT2] : [c.man.x - wx * REAR, c.man.y - wy * REAR];
        let there = 0;
        for (const m of bearers) if (stride(m, at(m), B.pace)) there++;
        if (there === 2) {
          c.stage = "lift";
          for (const m of bearers) {
            m.pose = "wait";
            m.v = 0;
            m.went = 0;
          }
        }
        return;
      }
      if (c.stage === "lift") {
        for (const m of [...bearers, c.man]) m.face = turnTo(m.face, c.way, rate);
        if (![...bearers, c.man].every((m) => Math.abs(wrap(m.face - c.way)) < 1e-6)) return;
        c.stage = "carry";
        c.t0 = t;
        c.t1 = Math.round((t + Math.hypot(c.to[0] - c.man.x, c.to[1] - c.man.y) / B.pace) * 1e3) / 1e3;
        c.from = [c.man.x, c.man.y];
        ((_a = c.man).cy ?? (_a.cy = [])).push([c.t0, c.t1, this.byId(c.front), this.byId(c.rear)]);
        c.man.pose = "carried";
        c.front.pose = "stretcher_front";
        c.rear.pose = "stretcher_rear";
        for (const m of bearers) m.v = B.pace;
        return;
      }
      if (c.stage === "carry") {
        const left = Math.hypot(c.to[0] - c.man.x, c.to[1] - c.man.y), s = Math.min(left, B.pace * DT);
        c.man.x += wx * s;
        c.man.y += wy * s;
        c.man.spot = [c.man.x, c.man.y];
        c.front.x = c.man.x + wx * FRONT2;
        c.front.y = c.man.y + wy * FRONT2;
        c.rear.x = c.man.x - wx * REAR;
        c.rear.y = c.man.y - wy * REAR;
        for (const m of bearers) {
          m.face = c.way;
          m.went = s / DT;
        }
        if (s < left - 1e-9) return;
        c.man.gone = t;
        c.man.pose = "wounded";
        c.stage = "home";
        for (const m of bearers) {
          m.pose = "walk";
          m.v = B.pace;
        }
        return;
      }
      for (const m of bearers) {
        if (m.job !== c) continue;
        const p = this.leash(sq, [m.x, m.y], -4);
        if (!live(u) || stride(m, p, B.pace)) {
          m.job = void 0;
          m.pose = "wait";
          m.v = 0;
          m.went = 0;
          m.spot = [m.x, m.y];
          m.slotAt = [m.x, m.y];
          m.until = t + 1;
          sq.memo = void 0;
        }
      }
    }
    // ---- What the block is doing ------------------------------------------------------------------------------------
    phaseOf(sq) {
      const b = this.b, u = sq.u;
      if (!live(u)) return "gone";
      if (u.state === "routing") return "flee";
      if (u.order?.kind === "withdraw" || u.gives || u.fellBack) return "fallback";
      if (sq.ran && u.state === "wavering") return "rally";
      const moving = sq.speed > 0.6 && !!u.order;
      const near = this.nearestGap(u);
      if (u.kind === "art") return moving ? "limbered" : "battery";
      const hot = sq.harm > DR.pin.harm * this.pinScale(u) || b.t - sq.shelledAt < DR.pin.shelled;
      if (u.state === "wavering" && (u.engaged !== void 0 || hot)) return "pinned";
      if (hot && !moving && u.kind !== "arm") return this.byBook && u.engaged !== void 0 && u.state === "steady" ? "fight" : "pinned";
      if (u.engaged !== void 0) return moving ? "bound" : "fight";
      if (moving) {
        if (near < DR.ranges.bound || sq.harm > 5e-4) return "bound";
        return near < DR.ranges.column ? "deploy" : "column";
      }
      const P = b.plan[u.side];
      if (P && P.phase === "guns" && P.role[u.id] === "main" && !u.taken && near < DR.ranges.column) return "startline";
      if (u.reserve === "held" && near > 250) return "rest";
      return near < DR.ranges.column || u.side === "def" ? "hold" : "rest";
    }
    /** How readily its men go to ground: raw troops sooner, veterans later. */
    pinScale(u) {
      return 1 / DR.xp.pinned[this.band(u)];
    }
    band(u) {
      return u.xp < DR.xp.raw ? 0 : u.xp >= DR.xp.veteran ? 2 : 1;
    }
    nums(u) {
      const s = this.b.style[u.side];
      return DR.style[s && this.b.ai[u.side] ? s.id : "plain"];
    }
    /** Metres from this block's edge to the nearest enemy that is still fighting. */
    nearestGap(u) {
      let g = Infinity;
      for (const e of this.b.units) if (e.side !== u.side && fighting(e) && !e.hid) g = Math.min(g, this.b.gap(u, e));
      return g;
    }
    /** What the block faces: who it fights, else its order's target, else the nearest enemy, else nothing. */
    threat(u) {
      const b = this.b;
      if (u.engaged !== void 0) {
        const e = b.unit(u.engaged);
        if (e && live(e)) return e;
      }
      if (u.order?.target !== void 0) {
        const e = b.unit(u.order.target);
        if (e && live(e)) return e;
      }
      let best = null, d = 1500;
      for (const e of b.units) if (e.side !== u.side && fighting(e) && !e.hid) {
        const g = b.gap(u, e);
        if (g < d) {
          d = g;
          best = e;
        }
      }
      return best;
    }
    threatDir(sq) {
      const u = sq.u, e = this.threat(u);
      if (!e) return fwd(u.face);
      const d = Math.hypot(e.x - u.x, e.y - u.y) || 1;
      return [(e.x - u.x) / d, (e.y - u.y) / d];
    }
    // ---- The drill book (tactics.ts) -----------------------------------------------------------------------------------
    /** Its groups' middles now; then the rounds fired at it since its last think are felt (suppression). */
    feel(sq) {
      const u = sq.u, mids = /* @__PURE__ */ new Map(), n = /* @__PURE__ */ new Map();
      if (live(u)) for (const m of this.memo(sq).stand) {
        if (VEHICLE.has(m.role)) continue;
        const g = grp(m), c = mids.get(g);
        if (c) {
          c[0] += m.x;
          c[1] += m.y;
        } else mids.set(g, [m.x, m.y]);
        n.set(g, (n.get(g) ?? 0) + 1);
      }
      for (const [g, c] of mids) {
        const k = n.get(g) ?? 1;
        c[0] /= k;
        c[1] /= k;
      }
      sq.mids = mids;
      this.sup.feel(u.id, this.b.t, this.band(u), this.shots, mids);
    }
    /** A man's group's suppression now (none with the book off). */
    supOf(sq, m) {
      return this.byBook ? this.sup.level(sq.u.id, grp(m), this.b.t) : 0;
    }
    /** Whether his group's heads are down (its suppression over BOOK.suppress.down). */
    downed(sq, m) {
      return this.byBook && this.supOf(sq, m) > BOOK.suppress.down;
    }
    /** How a block bounds: by its army's school at its experience, or all together where its general's style is a wave. */
    boundFor(sq, N) {
      return boundOf(sq.u.nat, this.band(sq.u), N.wave);
    }
    /**
     * A block's fire-and-movement cycle by its bound: how long a man is up in a rush, his turn's slot (his rush and the
     * pause before the next turn's), and the whole cycle (its turns). A wave keeps its style's rush and halt.
     */
    cycle(sq, N, bound) {
      if (bound === "wave") {
        const cyc = N.rush + N.halt;
        return { rush: N.rush, slot: cyc, cyc };
      }
      const rush = BOOK.rush[this.band(sq.u)], slot = rush + BOOK.gap;
      return { rush, slot, cyc: turns(bound) * slot };
    }
    /**
     * His turn in the cycle (tactics.ts turnOf): by his squad in his platoon (its headquarters with the leading squad), or
     * by his section where the men are drawn as sections.
     */
    turnFor(sq, m, bound) {
      const key = this.memo(sq).keyOf.get(m.sec ?? m.k);
      const plat = key ? !!key.plat : sq.tier === 2 && m.role === "rifle";
      const squads = plat ? platoonOf(sq.u.nat).squads : 0, s = plat ? m.sub ?? 0 : m.sec ?? m.k;
      return turnOf(bound, m.role === "mg", s, plat && s >= squads);
    }
    /** A block's most dangerous men to foot coming at it, standing now: its machine guns, anti-tank teams, guns and mortars. */
    priority(eq) {
      const t = this.b.t, c = this.priMemo.get(eq.u.id);
      if (c && c.t === t) return c.men;
      const men = live(eq.u) ? this.memo(eq).stand.filter((x) => x.role === "mg" || x.role === "hmg" || x.role === "at" || x.role === "gun" || x.role === "mortar") : [];
      this.priMemo.set(eq.u.id, { t, men });
      return men;
    }
    /** Where a side's tanks stand this step. */
    tanksOf(side) {
      const t = this.b.t;
      if (this.tankMemo.t !== t) this.tankMemo = { t, at: {} };
      let at = this.tankMemo.at[side];
      if (!at) {
        at = [];
        for (const sq of this.squads.values()) if (sq.u.side === side && sq.u.kind === "arm" && live(sq.u)) {
          for (const m of this.memo(sq).stand) if (m.role === "tank") at.push([m.x, m.y]);
        }
        this.tankMemo.at[side] = at;
      }
      return at;
    }
    /** The place of the man of 'of' nearest p (within a reach). */
    nearestOf(of, p, within = Infinity) {
      let best, d = within;
      for (const o of of) {
        const dd = Math.hypot(o.x - p[0], o.y - p[1]);
        if (dd < d) {
          d = dd;
          best = o;
        }
      }
      return best ? [best.x, best.y] : void 0;
    }
    /**
     * Where a man covering his mates aims (the drill book): three times in five at the enemy's weapons that would cut them
     * down as they rush (its machine guns, anti-tank teams, guns), the nearest of them; near friendly tanks, at its tank
     * hunters first; else at the nearest man of the block he fights. 'away': his fire lifted off the enemy's near edge,
     * where his own assault is closing, onto the man nearest that point.
     */
    aimFor(sq, m, e, away) {
      if (!this.byBook || !e) return this.aimAt(m, e);
      const eq = this.squads.get(e.id), t = this.b.t;
      if (!eq) return this.aimAt(m, e);
      if (away) return this.nearestOf(this.memo(eq).stand, away) ?? this.aimAt(m, e);
      const pri = this.priority(eq);
      if (pri.length && roll(this.seed, sq.u.id, m.n, Math.floor(t / 6), 91) < 0.6) {
        const near = this.tanksOf(sq.u.side).some(([x, y]) => Math.abs(x - m.x) < 120 && Math.abs(y - m.y) < 120);
        const at = near ? pri.filter((x) => x.role === "at") : [];
        const p = this.nearestOf(at.length ? at : pri, [m.x, m.y], 700);
        if (p) return p;
      }
      return this.aimAt(m, e);
    }
    /**
     * Where a tank aims (the drill book): where her own foot is fighting the foot block she fires on, at what pins them (its
     * machine guns, anti-tank teams and guns: the targets the infantry point out to her); else the nearest of it.
     */
    tankAim(sq, m, e) {
      if (!this.byBook || !e || e.kind === "arm") return this.aimAt(m, e);
      const b = this.b, eq = this.squads.get(e.id);
      if (!eq || !b.units.some((f) => f.side === sq.u.side && fighting(f) && FOOT_KINDS.includes(f.kind) && b.gap(f, e) < 300)) return this.aimAt(m, e);
      return this.nearestOf(this.priority(eq), [m.x, m.y], 1e3) ?? this.aimAt(m, e);
    }
    /**
     * Where a gun, a mortar or a support team's machine gun aims at block e: anywhere in it, until friendly foot comes
     * within BOOK.lift of it (the drill book's lift and shift); then the guns and mortars lift into its depth, beyond its far
     * side, and the machine guns shift to its far half, clear of their own men.
     */
    liftAim(sq, g, e, kind) {
      const b = this.b, u = sq.u, t = b.t;
      const r13 = roll(this.seed, u.id, g.n, t), r22 = roll(this.seed, u.id, g.n, t, 1);
      const plain2 = [e.x + (r13 - 0.5) * e.w * 0.6, e.y + (r22 - 0.5) * e.d * 0.6];
      if (!this.byBook) return plain2;
      let near = null, gap = BOOK.lift[kind];
      for (const f of b.units) if (f.side === u.side && fighting(f) && FOOT_KINDS.includes(f.kind)) {
        const d = b.gap(f, e);
        if (d < gap) {
          gap = d;
          near = f;
        }
      }
      if (!near) return plain2;
      const dx = e.x - near.x, dy = e.y - near.y, dd = Math.hypot(dx, dy) || 1, ax = dx / dd, ay = dy / dd;
      const deep = kind === "mg" ? e.d * 0.3 : Math.max(e.w, e.d) / 2 + BOOK.lift.deep, across = (r13 - 0.5) * e.w * 0.6;
      return [clamp2(e.x + ax * deep - ay * across, 1, b.field.w - 1), clamp2(e.y + ay * deep + ax * across, 1, b.field.h - 1)];
    }
    /**
     * The assault (the drill book): the whole squads of the platoons nearest the flank it works onto, their riflemen (and
     * their guns, where the school takes them in), up to nA men.
     */
    flankPlatoons(sq, men, nA, gunStays) {
      const keys = this.memo(sq).keys.filter((k) => k.role === "rifle"), across = /* @__PURE__ */ new Map();
      for (const k of keys) across.set(k.k, this.slot(sq, k, "line")[0] * sq.flank);
      keys.sort((a, c) => (across.get(c.k) ?? 0) - (across.get(a.k) ?? 0) || a.k - c.k);
      const out = /* @__PURE__ */ new Set();
      for (const k of keys) {
        if (out.size >= nA) break;
        for (const m of men) if ((m.sec ?? m.k) === k.k && (m.role === "rifle" || m.role === "sapper" || !gunStays && m.role === "mg")) out.add(m);
      }
      return out;
    }
    /** A defence's counter-attack (the drill book): the riflemen of its platoons in the second row. */
    rearPlatoons(sq, men) {
      const keys = this.memo(sq).keys.filter((k) => k.role === "rifle"), out = /* @__PURE__ */ new Set();
      if (!keys.length) return out;
      const ls = keys.map((k) => this.slot(sq, k, "line")[1]), top = Math.max(...ls);
      const rear = new Set(keys.filter((_, i) => ls[i] < top - 3).map((k) => k.k));
      for (const m of men) if (rear.has(m.sec ?? m.k) && (m.role === "rifle" || m.role === "sapper")) out.add(m);
      return out;
    }
    // ---- Thinking: each block twice a second --------------------------------------------------------------------------
    think(sq) {
      const u = sq.u, t = this.b.t;
      const was = sq.phase;
      let ph = this.phaseOf(sq);
      let got = false;
      const hulls = sq.of !== void 0 && sq.veh.length > 0 && sq.veh.every((v) => DR.mech.armoured.includes(v.ml ?? ""));
      const cue = hulls ? this.b.hullThreat(u) : ph === "fight" || ph === "pinned" || ph === "hold" || ph === "startline" || this.nearestGap(u) < DR.ranges.debus || sq.harm > 0;
      if (!sq.down && sq.slow >= MOUNT.downAfter && cue) {
        if (u.kind === "mot" || u.kind === "cav" && this.nearestGap(u) < DR.ranges.debus * 0.85) {
          got = sq.down = true;
          this.alight(sq);
        }
      }
      if (sq.down && ph !== "gone" && (u.kind === "cav" || u.kind === "mot" && sq.veh.length)) {
        sq.close = sq.fast >= 1 ? true : sq.slow >= 3 ? false : sq.close;
        if (sq.boarding === void 0 && sq.fast >= MOUNT.upAfter) this.mountUp(sq);
        else if (sq.boarding !== void 0 && sq.slow >= MOUNT.stayAfter) {
          got = true;
          this.alight(sq);
        }
      }
      if (ph !== was || got) {
        sq.phase = ph;
        sq.since = t;
      }
      ph = sq.phase;
      sq.ran = u.state === "routing" || sq.ran && u.state === "wavering" && t - sq.since < 8;
      if (ph === "gone") return;
      if (u.kind === "arm") {
        this.tanks(sq);
        return;
      }
      if (u.kind === "art") {
        if (u.team) this.teams(sq);
        else this.guns(sq);
        return;
      }
      if (sq.boarding !== void 0) {
        this.board(sq);
        return;
      }
      if (sq.hop !== void 0) {
        if (!this.hopOut(sq)) return;
        sq.since = t;
      }
      this.lorries(sq);
      if ((u.kind === "mot" || u.kind === "cav") && !sq.down) {
        this.mounted(sq);
        return;
      }
      switch (ph) {
        case "column":
          this.column(sq);
          break;
        case "deploy":
          this.advance(sq, false);
          break;
        case "bound":
          this.advance(sq, true);
          break;
        case "startline":
          this.lie(sq, "wait");
          break;
        case "fight":
          if (this.byBook) this.fightBook(sq);
          else this.fight(sq);
          break;
        case "hold":
          this.hold(sq);
          break;
        case "rest":
          this.rest(sq);
          break;
        case "pinned":
          this.pinned(sq);
          break;
        case "fallback":
          this.fallback(sq);
          break;
        case "flee":
          this.flee(sq);
          break;
        case "rally":
          this.rally(sq);
          break;
        default:
          break;
      }
      this.ambush(sq);
      this.officer(sq);
      for (const m of sq.men) if (m.fell === void 0 && !m.job && MOVING.has(m.pose) && Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) < 0.3) this.arrive(sq, m);
    }
    /**
     * He has reached his place: off his feet into the pose for it (no man runs on the spot). A rifleman with a target
     * fires from it, without one he kneels watching; a routing man crouches where he got to; a tank halts, a gun towed
     * into the line unhooks, a lorry parks.
     */
    arrive(sq, m) {
      m.v = 0;
      if (m.pr) {
        m.pose = m.pose === "flee" ? "cower" : "wait";
        return;
      }
      switch (m.role) {
        case "tank":
          m.pose = "halt";
          return;
        case "lorry":
          m.pose = "parked";
          return;
        case "gun":
          if (m.pose === "towed" && sq.phase === "battery") m.pose = "deployed";
          return;
        // A support team's crew sets its weapon up in the line, or down a moment on the march; a routing one crouches.
        case "hmg":
        case "mortar":
          m.pose = m.pose === "flee" ? "cower" : sq.phase === "battery" ? "deployed" : "wait";
          return;
        default: {
          m.until = Math.max(m.until, this.b.t + 1.5);
          if (m.pose === "flee") {
            m.pose = "cower";
            return;
          }
          if (m.aid !== void 0) {
            m.pose = "wait";
            m.aim = void 0;
            return;
          }
          if (!m.aim && m.role !== "officer" && (sq.phase === "fight" || sq.phase === "pinned")) m.aim = this.aimAt(m, this.threat(sq.u));
          if (m.aim && m.role !== "officer") m.pose = this.firePose(m);
          else m.pose = m.role === "mg" && sq.phase !== "fight" ? "hide" : "wait";
        }
      }
    }
    /** The men still standing, not helping a fallen comrade. */
    up(sq) {
      return this.memo(sq).up.filter((m) => !this.aiding(m) && !(m.role === "holder" && sq.down));
    }
    /**
     * Whether a man is on his feet: a key man while the rules keep his model; the rest of a section while their block is
     * on the field and he has not fallen (falls).
     */
    stands(sq, m) {
      return m.fell === void 0 && !m.job && (m.j === void 0 ? m.k < sq.u.figures : live(sq.u));
    }
    /**
     * The block's men on their feet, worked out again only when that changes: a man falls or joins another section (falls,
     * pairs: they clear it), or the rules lift a model or take the block off the field.
     */
    memo(sq) {
      const u = sq.u, on = live(u);
      if (sq.memo && sq.memo.fig === u.figures && sq.memo.on === on) return sq.memo;
      const up = [], keys = [], keyOf2 = /* @__PURE__ */ new Map(), bySec = /* @__PURE__ */ new Map(), stand = [];
      for (const m of sq.men) {
        if (!this.stands(sq, m)) continue;
        stand.push(m);
        if (m.j === void 0) {
          keys.push(m);
          keyOf2.set(m.k, m);
        }
        if (m.pr) continue;
        up.push(m);
        if (m.j !== void 0) {
          const s = grp(m);
          let l = bySec.get(s);
          if (!l) bySec.set(s, l = []);
          l.push(m);
        }
      }
      const all = sq.tier >= 1 ? [...up].sort((a, c) => (a.sec ?? a.k) - (c.sec ?? c.k) || (a.j ?? 0) - (c.j ?? 0)) : up;
      return sq.memo = { fig: u.figures, on, stand, struck: on ? [...stand, ...sq.veh] : stand, up, keys, keyOf: keyOf2, all, bySec };
    }
    /** Whom a man who keeps by another keeps by in his section now: a loader his weapon, a gun's crew its gun, a runner his officer. */
    leadOf(sq, m) {
      const want = LEADS[m.role];
      if (!want) return void 0;
      const M = this.memo(sq), s = m.sec ?? m.k, key = M.keyOf.get(s);
      if (key && key !== m && !m.sub && want.includes(key.role)) return key;
      return M.bySec.get(grp(m))?.find((x) => x !== m && want.includes(x.role));
    }
    /**
     * Where a platoon's group forms, about its key man's place (across, along; outer: toward the block's flank): its other
     * squads one behind another, a little to either side, PLATOON.step metres apart; its headquarters inside the leading
     * squad's wedge, behind its point. At rest, their knots side by side.
     */
    inPlatoon(sq, sub, outer, tpl) {
      const P = DR.sections.platoon, hq = sub >= platoonOf(sq.u.nat).squads;
      if (tpl === "rest") return hq ? [0, -9] : [(sub % 2 ? outer : -outer) * 11 * Math.ceil(sub / 2), 0];
      return hq ? [0, -4] : [(sub % 2 ? -outer : outer) * 4, -P.step * sub];
    }
    /**
     * His place in his section, about his key man's place in the block (slot): in line, a shallow wedge with the key man
     * at its point, its automatic weapon at the outer end (toward the block's flank), the rest alternately either side of
     * him and a little back, a few metres apart; on the move its scouts out ahead; at rest, a knot round him.
     */
    inSection(sq, m, tpl) {
      const S = DR.sections, M = this.memo(sq), s = m.sec ?? m.k, key = M.keyOf.get(s);
      let [ka, kl] = key ? this.slot(sq, key, tpl) : [m.jx * 3, 0];
      if (m.sub) {
        const [oa, ol] = this.inPlatoon(sq, m.sub, key?.team === 1 ? -1 : 1, tpl);
        ka += oa;
        kl += ol;
      }
      if (m.lead) return [ka + m.jx * 0.2, kl + m.jy * 0.2];
      const mates = (M.bySec.get(grp(m)) ?? [m]).filter((x) => !x.lead), i = Math.max(0, mates.indexOf(m));
      if (tpl === "rest") {
        const n = Math.max(1, mates.length), ang = i / n * Math.PI * 2 + m.jx * 0.1, r = 3 + n * 0.25;
        return [ka + Math.cos(ang) * r, kl + Math.sin(ang) * r * 0.8];
      }
      const ph = sq.phase;
      if (m.scout && (ph === "deploy" || ph === "bound" || ph === "column")) return [ka + (i % 2 ? 5 : -5) + m.jx * 0.3, kl + S.ahead + m.jy * 0.4];
      const outer = key?.team === 1 ? -1 : 1, rest = mates.filter((x) => x.role !== "mg");
      let side, rank;
      if (m.role === "mg") {
        const g = mates.filter((x) => x.role === "mg").indexOf(m);
        side = g % 2 ? -outer : outer;
        rank = Math.ceil(rest.length / 2) + 1;
      } else {
        const r = Math.max(0, rest.indexOf(m));
        side = r % 2 ? outer : -outer;
        rank = Math.floor(r / 2) + 1;
      }
      return [ka + side * rank * S.space + m.jx * 0.25, kl - rank * S.space * 0.35 + m.jy * 0.3];
    }
    /**
     * The men who keep by a weapon or an officer (Man.pr), each step: an automatic weapon's or a launcher's number two
     * beside it and its bearers a little behind, a gun's crew at their places round it, a support team's bearers behind
     * the weapon, the officer's runners at his elbow, a horse holder's two by him. Each goes where it goes, as it goes, and
     * takes the pose that goes with its: down beside the gun as it fires, kneeling by it set up, aboard as it rides. A
     * rifle section's gunner fallen, his number two takes the gun up; with none to keep by, he fights as a rifleman.
     */
    pairs(sq, room) {
      if (sq.tier < 1) return;
      const u = sq.u, t = this.b.t;
      const count = /* @__PURE__ */ new Map(), booked = /* @__PURE__ */ new Set();
      for (const m of sq.men) {
        if (!m.pr || !this.stands(sq, m)) continue;
        if (sq.boarding !== void 0 && m.role !== "holder") continue;
        const L2 = this.leadOf(sq, m);
        if (!L2) {
          const key = this.memo(sq).keyOf.get(m.sec ?? m.k);
          m.pr = void 0;
          if (m.role === "loader" && key && key.role === "rifle") m.role = "mg";
          else if (m.role === "loader") m.role = "rifle";
          sq.memo = void 0;
          continue;
        }
        const i = count.get(L2) ?? 0;
        count.set(L2, i + 1);
        const [oa, ol] = L2.role === "gun" ? GUN_CREW[i % GUN_CREW.length] : L2.role === "officer" ? [[-2.5, -1.2], [2.2, -2.2], [0.6, -4]][i % 3] : L2.role === "holder" ? [i % 2 ? 3 : -3, -1] : CREWED.has(L2.role) ? [i % 2 ? 2 : -2, -4 - 2.5 * Math.floor(i / 2)] : L2.role === "at" ? i ? [-1.5, -3] : [1.3, -0.6] : ((o) => [o[0], o[1] - 3 * Math.floor(i / 3)])([[-1.6, -0.6], [1.5, -3.5], [-2, -5.5]][i % 3]);
        const moving = MOVING.has(L2.pose) && L2.v > 0;
        const lw = Math.hypot(L2.spot[0] - L2.x, L2.spot[1] - L2.y), la = moving && lw > 0 ? Math.min(lw, L2.v * 0.75) / lw : 0;
        const anchor = moving ? { x: L2.x + (L2.spot[0] - L2.x) * la, y: L2.y + (L2.spot[1] - L2.y) * la } : L2;
        const carried = L2.pose === "mounted" || L2.pose === "ride" || L2.pose === "horses";
        let p = this.leash(sq, toWorld(anchor, L2.face, oa, ol), -4);
        if (room && !carried) {
          if (moving && !booked.has(L2)) {
            booked.add(L2);
            this.bookRoom(room, L2, [L2.x, L2.y]);
            this.bookRoom(room, L2, [anchor.x, anchor.y]);
          }
          p = this.leash(sq, this.clear(room, sq, m, p), -4);
          this.bookRoom(room, m, p);
        }
        const d = Math.hypot(p[0] - m.x, p[1] - m.y);
        m.aim = void 0;
        m.hidden = void 0;
        m.cover = L2.cover;
        m.until = t + 0.5;
        m.spot = p;
        if (carried) {
          m.pose = L2.pose;
          m.v = L2.pose === "mounted" ? 0 : Math.min(14, Math.max(L2.v, d));
          continue;
        }
        if (L2.role === "gun" && L2.pose === "towed" && (sq.phase === "limbered" || sq.phase === "flee" || sq.phase === "fallback") && sq.veh.length) {
          const seat = this.seat(sq, m);
          if (m.pose === "mounted" || Math.hypot(seat[0] - m.x, seat[1] - m.y) < 0.6) {
            m.pose = "mounted";
            m.v = 0;
          } else {
            m.spot = seat;
            m.pose = "run";
            m.v = CATCH_UP;
          }
          continue;
        }
        if (moving) {
          m.pose = L2.pose === "towed" ? "walk" : L2.pose;
          m.v = Math.min(CATCH_UP, Math.max(L2.v, d / 1.2));
          continue;
        }
        const still = L2.hidden && (L2.role === "gun" || CREWED.has(L2.role)) || L2.pose === "cower" ? "cower" : L2.role === "gun" ? L2.pose === "fire" ? "fire" : i === 0 && u.engaged === void 0 ? "idle" : "wait" : L2.pose === "lmg_burst" || L2.pose === "fire_prone" || L2.pose === "hide" ? "hide" : L2.pose === "idle" && L2.role === "officer" ? "wait" : L2.pose === "idle" ? "idle" : "wait";
        if (d > 0.45) {
          const far = d > 8;
          m.pose = far ? "run" : still === "hide" || still === "cower" ? "crawl" : "creep";
          m.v = far ? 4.5 : m.pose === "crawl" ? 1.4 : 2.5;
        } else {
          m.pose = still;
          m.v = 0;
        }
      }
    }
    /**
     * Room between the men of a section block (SPACE): the key men's and the rest's places (all but those who keep by a
     * weapon or an officer, whom pairs places after), each kept clear of the places of those before him (key men first,
     * then section by section). A man still is placed where he is; one on his way, where he is going. One whose place
     * moves while he is still gets up and steps across to it (creeping, or crawling from lying), and takes his pose again
     * there. Returns the places booked, for pairs.
     */
    space(sq) {
      if (sq.tier < 1) return void 0;
      const M = this.memo(sq), t = this.b.t, room = /* @__PURE__ */ new Map();
      const order = [...M.up].sort((a, c) => (a.j === void 0 ? 0 : 1) - (c.j === void 0 ? 0 : 1) || (a.sec ?? a.k) - (c.sec ?? c.k) || (a.j ?? 0) - (c.j ?? 0));
      for (const m of order) {
        if (VEHICLE.has(m.role) || CARRIED.has(m.pose) || sq.boarding !== void 0) continue;
        const going = MOVING.has(m.pose) && m.v > 0, at = going ? m.spot : [m.x, m.y];
        const p = this.leash(sq, this.clear(room, sq, m, at), -4);
        if (Math.hypot(p[0] - at[0], p[1] - at[1]) > 0.05) {
          m.spot = p;
          if (!going) {
            const low = m.pose === "fire_prone" || m.pose === "hide" || m.pose === "cower" || m.pose === "lmg_burst" || m.pose === "observe";
            m.pose = low ? "crawl" : "creep";
            m.v = low ? 1.2 : 2;
            m.until = t + 1;
            m.aim = void 0;
          }
        }
        this.bookRoom(room, m, p);
      }
      return room;
    }
    /**
     * The nearest place to p clear of every man booked in the room (SPACE): p itself, or the first free of the points on
     * rings round it (0.8 to 4 m out, nearest first, from a turn of his own); in a crowd beyond that, p.
     */
    clear(room, sq, m, p) {
      const free = (x, y) => {
        const cx = Math.floor(x / SPACE.cell), cy = Math.floor(y / SPACE.cell);
        for (let i = -1; i <= 1; i++) for (let j = -1; j <= 1; j++) for (const q of room.get((cx + i) * 65536 + cy + j) ?? []) {
          const min = m.role === "crew" && q.m.role === "crew" && (m.sec ?? m.k) === (q.m.sec ?? q.m.k) ? SPACE.crew : SPACE.men;
          if (Math.hypot(q.x - x, q.y - y) < min) return false;
        }
        return true;
      };
      if (free(p[0], p[1])) return p;
      const a0 = roll(this.seed, sq.u.id, m.n, 53) * 2 * Math.PI;
      for (let r = 0.8; r <= 4.01; r += 0.8) {
        const n = Math.max(8, Math.round(r * 6));
        for (let i = 0; i < n; i++) {
          const a = a0 + i * 2 * Math.PI / n, x = p[0] + Math.sin(a) * r, y = p[1] + Math.cos(a) * r;
          if (free(x, y)) return [x, y];
        }
      }
      return p;
    }
    /** Books a man's place in the room. */
    bookRoom(room, m, p) {
      const key = Math.floor(p[0] / SPACE.cell) * 65536 + Math.floor(p[1] / SPACE.cell);
      let l = room.get(key);
      if (!l) room.set(key, l = []);
      l.push({ m, x: p[0], y: p[1] });
    }
    aiding(m) {
      if (m.aid === void 0) return false;
      if (this.b.t >= m.until) {
        m.aid = void 0;
        return false;
      }
      return true;
    }
    /**
     * His place in the block's line, metres across (right of the middle) and along (forward of the middle). 'line': the
     * two teams side by side across the front, each about its half's middle at the style's spacing, the machine gun at
     * its team's outer end, the officer behind the middle, the anti-tank team beside him. 'column': two files up the
     * middle of the block. 'rest': three knots, the teams and the headquarters.
     * Drawn as sections (Drill.sections), each key man has the place a model has, the sections spread wider and in two
     * rows so each has room for its men; the rest of a section stand round their key man (inSection); on the march the
     * whole block goes in files (as many as its men need), section after section.
     */
    slot(sq, m, tpl) {
      const u = sq.u, N = this.nums(u), band = this.band(u), M = this.memo(sq), S = DR.sections, sec = sq.tier >= 1;
      if (m.pr) {
        const L2 = this.leadOf(sq, m);
        if (L2) return this.slot(sq, L2, tpl);
      }
      if (sec && tpl === "column" && u.kind !== "arm" && u.kind !== "art") {
        const i2 = Math.max(0, M.all.indexOf(m)), nf = Math.max(2, Math.ceil(M.all.length / S.files));
        return [(i2 % nf - (nf - 1) / 2) * 2.5 + m.jx * 0.15, u.d / 2 - 4 - Math.floor(i2 / nf) * 3];
      }
      if (sec && m.j !== void 0) return this.inSection(sq, m, tpl === "rest" ? "rest" : "line");
      const sp = N.spacing * (band === 0 ? DR.xp.bunch : band === 2 ? DR.xp.spread : 1) * (sec ? 2 : 1);
      const all = M.keys;
      if (tpl === "column") {
        const i2 = all.indexOf(m);
        return [(i2 % 2 ? 1 : -1) * 2.5 + m.jx * 0.2, u.d / 2 - 4 - Math.floor(i2 / 2) * 5];
      }
      if (u.kind === "arm" || u.kind === "art") {
        const i2 = all.indexOf(m), n2 = Math.max(1, all.length), gap = Math.min(u.w / n2, u.kind === "art" ? 32 : 40);
        return [(i2 - (n2 - 1) / 2) * gap + m.jx, (u.kind === "arm" ? i2 % 2 ? -6 : 6 : 0) + m.jy];
      }
      if (tpl === "rest") {
        const c = m.team === 0 ? [0, -u.d / 4] : [(m.team === 1 ? -1 : 1) * u.w / (sec ? 3 : 5), 0];
        const mates2 = all.filter((x) => x.team === m.team), i2 = mates2.indexOf(m), ang = i2 / Math.max(1, mates2.length) * Math.PI * 2 + m.jx;
        const r = sec ? 6 + 3 * mates2.length : 4.5;
        return [c[0] + Math.cos(ang) * r, c[1] + Math.sin(ang) * (sec ? r * 0.8 : 3.5)];
      }
      if (m.team === 0) {
        if (m.role === "officer") return [m.jx, -u.d / 2 + 6];
        return [u.w / 8 + m.jx, -u.d / 2 + 10];
      }
      const side = m.team === 1 ? -1 : 1;
      const mates = all.filter((x) => x.team === m.team).sort((a2, c) => a2.role === "mg" ? -1 : c.role === "mg" ? 1 : a2.k - c.k);
      const i = mates.indexOf(m), n = mates.length;
      const half = u.w / 2 - 6 + (sec ? 25 : 0), span2 = Math.min(half - 4, sp * Math.max(0, n - 1));
      const outer = side * Math.min(half, u.w / 4 + span2 / 2);
      const a = outer - side * (n > 1 ? span2 * i / (n - 1) : 0);
      const l = u.d / 2 - 4 + (i % 2 ? sec ? -(sq.tier === 2 ? S.platoon.rows : S.rows) : -3 : 0) + m.jy * 0.5;
      return [a + m.jx * 0.4, l];
    }
    /**
     * The best place near p for a man to stand: the shelter of the ground and what lies on it, a little for being near his
     * place in the line, never on top of a mate. 'dir' points at the enemy. Within his experience's reach of p.
     */
    seek(sq, m, p, dir, mates, reach) {
      const f = this.b.field, u = sq.u;
      const R = reach ?? DR.xp.seek[this.band(u)] * (u.kind === "arm" || u.kind === "art" ? 2 : 1);
      const going = m.role === "tank" ? "tracks" : m.role === "gun" ? "guns" : m.role === "lorry" ? "wheels" : "foot";
      const pace = LAND.pace[going];
      const sep = m.role === "tank" ? 16 : m.role === "gun" ? 18 : m.role === "lorry" ? 10 : CREWED.has(m.role) ? DR.teams.sep : 3.5;
      const cand = [p];
      const a0 = roll(this.seed, u.id, m.n, 7) * Math.PI * 2;
      const settle = reach === void 0 && going === "foot" && (sq.phase === "fight" || sq.phase === "hold" || sq.phase === "startline" || sq.phase === "pinned" || sq.phase === "rest");
      const rings = settle ? [R * 0.5, R, R * 2, R * 3] : [R * 0.5, R];
      rings.forEach((r, j) => {
        for (let i = 0; i < 8; i++) {
          const a = a0 + i * Math.PI / 4 + (j % 2 ? Math.PI / 8 : 0);
          cand.push([p[0] + Math.sin(a) * r, p[1] + Math.cos(a) * r]);
        }
      });
      if (going === "foot") {
        for (const z of this.b.wrecks) {
          const [zx, zy] = this.wreckSpot(z);
          if (Math.abs(zx - p[0]) < R + 8 && Math.abs(zy - p[1]) < R + 8) cand.push([zx - dir[0] * 4, zy - dir[1] * 4]);
        }
        for (const z of this.b.craters) if (Math.abs(z.x - p[0]) < R && Math.abs(z.y - p[1]) < R) cand.push([z.x, z.y]);
      }
      const span2 = rings[rings.length - 1] + sep + 1;
      const near = mates.filter((o) => o !== m && o.fell === void 0 && !o.pr && Math.abs(o.spot[0] - p[0]) < span2 && Math.abs(o.spot[1] - p[1]) < span2);
      let best = { x: p[0], y: p[1], kind: "none", v: -Infinity };
      for (const [cx0, cy0] of cand) {
        const cx = clamp2(cx0, 3, f.w - 3), cy = clamp2(cy0, 3, f.h - 3);
        const [qx, qy] = this.leash(sq, [cx, cy], m.role === "lorry" ? 116 : -2);
        if (qx !== cx || qy !== cy) continue;
        const g = groundOf(f, cx, cy);
        if (pace[g] <= (going === "foot" ? 0 : 0.12)) continue;
        const c = this.shelter(cx, cy, g, dir, going === "foot", m.role === "tank");
        let s = c.v - (settle ? 8e-3 : 0.015) * Math.hypot(cx - p[0], cy - p[1]);
        for (const o of near) if (Math.hypot(o.spot[0] - cx, o.spot[1] - cy) < sep) s -= 0.3;
        if (s > best.v) best = { x: cx, y: cy, kind: c.kind, v: s };
      }
      if (best.v === -Infinity) best = { x: clamp2(p[0], 3, f.w - 3), y: clamp2(p[1], 3, f.h - 3), kind: "none", v: 0 };
      return best;
    }
    /** How much shelter a point gives a man (or a tank: a crest is her hull-down) facing 'dir'. */
    shelter(x, y, g, dir, foot, tank) {
      const f = this.b.field, C = DR.cover;
      let v = tank ? g === "woods" || BUILT.includes(g) ? 0.4 : 0 : C.ground[g];
      let kind = COVER_OF[g] ?? "none";
      if (v < -1) return { v, kind: "none" };
      if (v >= 0.3) {
        const ahead2 = groundOf(f, x + dir[0] * 15, y + dir[1] * 15);
        if (C.ground[ahead2] < v - 0.2) v += C.edge;
      }
      const rise = heightOf(f, x + dir[0] * 15, y + dir[1] * 15) - heightOf(f, x, y);
      const [lo, hi] = DR.ranges.hullDown;
      if (rise >= (tank ? lo : 0.8) && rise <= (tank ? hi : 4)) {
        const c = tank ? 0.8 : C.crest;
        if (c > v - 0.05) {
          v = Math.max(v, c) + 0.05;
          kind = "crest";
        }
      }
      if (foot) {
        for (const z of this.b.wrecks) {
          const [zx, zy] = this.wreckSpot(z), dx = zx - x, dy = zy - y, d = Math.hypot(dx, dy);
          if (d > 2 && d < 8 && (dx * dir[0] + dy * dir[1]) / d > 0.5 && C.wreck > v) {
            v = C.wreck;
            kind = "wreck";
          }
        }
        if (v < C.crater) {
          for (const z of this.b.craters) if (Math.hypot(z.x - x, z.y - y) <= Math.max(1.5, z.r)) {
            v = C.crater;
            kind = "crater";
            break;
          }
        }
      }
      return { v, kind };
    }
    /**
     * A point pulled inside the block's leash: its box, as the rules hold it, and a margin round it (more for men running).
     * The block is the truth: no man strays from it further than this.
     */
    /**
     * Where a man on foot makes for next, on a field with hedges: the gate on his way, found again when his spot has moved
     * more than 2 m (or he more than 5 m) since, and on through it and beyond. Only a gate inside his block's ring (he
     * never leaves his block for one). He goes through square, a stride either side of the gap, at his own place across it
     * by his number: a section goes through abreast and not in one file on one point.
     */
    via(sq, m) {
      const sp = m.spot;
      if (!m.viaFor || Math.hypot(sp[0] - m.viaFor[0], sp[1] - m.viaFor[1]) > 2 || !m.via?.length && (!m.viaAt || Math.hypot(m.x - m.viaAt[0], m.y - m.viaAt[1]) > 5)) {
        m.viaFor = [sp[0], sp[1]];
        m.viaAt = [m.x, m.y];
        m.via = this.gateFor(sq, m);
      }
      while (m.via?.length && Math.hypot(m.via[0][0] - m.x, m.via[0][1] - m.y) < 0.4) {
        m.via.shift();
        if (!m.via.length) {
          m.viaAt = [m.x, m.y];
          m.via = this.gateFor(sq, m);
          break;
        }
      }
      return m.via?.[0] ?? null;
    }
    gateFor(sq, m) {
      const g = this.b.gateOn(m.x, m.y, m.spot[0], m.spot[1]);
      if (!g) return void 0;
      const [lx, ly] = this.leash(sq, [g.x, g.y]);
      if (Math.hypot(lx - g.x, ly - g.y) > 0.5) return void 0;
      let [hx, hy] = this.b.hedgeDir(g.x, g.y) ?? [0, 0];
      if (!hx && !hy) {
        const wx = g.x - m.x, wy = g.y - m.y, wl = Math.hypot(wx, wy) || 1;
        hx = -wy / wl;
        hy = wx / wl;
      }
      let nx = -hy, ny = hx;
      if ((m.spot[0] - g.x) * nx + (m.spot[1] - g.y) * ny < 0) {
        nx = -nx;
        ny = -ny;
      }
      const half = Math.max(0, (g.width ?? 3) / 2 - 0.5), o = (roll(this.seed, sq.u.id, m.n, 61) * 2 - 1) * half, S = 1.5;
      const cx = g.x + hx * o, cy = g.y + hy * o, at = (m.x - g.x) * nx + (m.y - g.y) * ny;
      const inGap = Math.abs((m.x - cx) * hx + (m.y - cy) * hy) <= half + 0.5;
      const way = [];
      if (!inGap || at < -S + 0.3) way.push([cx - nx * S, cy - ny * S]);
      if (!inGap || at < S - 0.3) way.push([cx + nx * S, cy + ny * S]);
      return way.length ? way : void 0;
    }
    leash(sq, p, extra = 0) {
      const u = sq.u, m = (sq.phase === "flee" ? 70 : 45) + extra;
      if (!sq.ax || sq.ax[0] !== u.face) {
        const [rx0, ry0] = right(u.face), [fx0, fy0] = fwd(u.face);
        sq.ax = [u.face, rx0, ry0, fx0, fy0];
      }
      const [, rx, ry, fx, fy] = sq.ax;
      const dx = p[0] - u.x, dy = p[1] - u.y, a0 = dx * rx + dy * ry, l0 = dx * fx + dy * fy;
      const a = clamp2(a0, -u.w / 2 - m, u.w / 2 + m), l = clamp2(l0, -u.d / 2 - m, u.d / 2 + m);
      return a === a0 && l === l0 ? p : [u.x + a * rx + l * fx, u.y + a * ry + l * fy];
    }
    /**
     * Keeps a man (a tank, a lorry) at a place that moves with his block: he heads a little ahead of it, so he keeps its
     * pace and never stops short; one left behind closes up faster. When the block halts he reaches it and settles.
     */
    follow(sq, m, s, pose, mul = 1) {
      const v = sq.speed, on = v > 0.4, [hx, hy] = fwd(sq.heading);
      const off = Math.hypot(s[0] - m.x, s[1] - m.y), behind = (s[0] - m.x) * hx + (s[1] - m.y) * hy;
      const lead = on ? v * 1.5 + clamp2(-behind, 0, 10) : 0;
      m.spot = [s[0] + hx * lead, s[1] + hy * lead];
      m.slotAt = s;
      m.cover = "none";
      m.v = on ? behind < -8 - v * 1.5 ? 0 : v * (behind > 6 ? 1.4 * mul : behind > 2 ? 1.1 : behind < -2 ? 0.85 : 1) : off > 1 ? 2.5 : 0;
      m.pose = pose === "walk" && m.v > 2.5 ? "run" : pose === "creep" && m.v > 2.8 ? "advance" : pose === "crawl" && m.v > 1.6 ? "creep" : pose;
      m.v = Math.min(m.v, m.role === "tank" ? tankTop(sq) : m.role === "lorry" || m.role === "gun" ? 16 : pose === "ride" ? 14 : CATCH_UP);
      if (m.v === 0) {
        m.spot = [m.x, m.y];
        this.arrive(sq, m);
      }
    }
    /** Sends a man to a place near p (the best shelter there unless 'open'), at a pace, in a pose. */
    go(sq, m, p, pose, pace, until, dir, reach) {
      p = this.leash(sq, p, -4);
      const best = this.seek(sq, m, p, dir ?? this.threatDir(sq), sq.men, reach);
      m.spot = [best.x, best.y];
      m.cover = best.kind;
      m.slotAt = p;
      m.pose = pose;
      m.v = pace;
      m.until = until;
    }
    /** The firing pose for where he lies: standing in fieldworks or at a window, kneeling in woods or behind rocks or a crest, lying in the open. */
    firePose(m) {
      if (m.role === "mg") return "lmg_burst";
      switch (m.cover) {
        case "works":
        case "building":
          return "fire_stand";
        case "woods":
        case "rock":
        case "wreck":
        case "rough":
          return "fire_kneel";
        default:
          return "fire_prone";
      }
    }
    /** The nearest standing man of the block it fights, else a point in that block: where he fires. */
    aimAt(m, e) {
      if (!e) return void 0;
      const sq = this.squads.get(e.id);
      if (!sq) return [e.x, e.y];
      const M = this.memo(sq);
      let best, d = Infinity;
      for (const o of M.keys) {
        const dd = Math.hypot(o.x - m.x, o.y - m.y);
        if (dd < d) {
          d = dd;
          best = o;
        }
      }
      if (best && sq.tier >= 1) for (let g = 0; g < 8; g++) for (const o of M.bySec.get(best.k * 8 + g) ?? []) {
        const dd = Math.hypot(o.x - m.x, o.y - m.y);
        if (dd < d) {
          d = dd;
          best = o;
        }
      }
      return best ? [best.x, best.y] : [e.x, e.y];
    }
    // ---- Foot ---------------------------------------------------------------------------------------------------------
    column(sq) {
      const u = sq.u, t = this.b.t, h = sq.heading;
      for (const m of this.up(sq)) {
        const [a, l] = this.slot(sq, m, "column");
        const s = toWorld(u, h, a, l);
        m.aim = void 0;
        m.hidden = void 0;
        m.until = t + 1;
        this.follow(sq, m, s, u.kind === "cav" && !sq.down ? "ride" : "walk");
      }
    }
    /**
     * On the march toward the enemy, in open order across the block's front. Within striking range or under fire it goes
     * by fire and movement: where the block itself goes slowly, the two teams leapfrog, one rushing to the next cover while
     * the other lies firing; where it goes fast, all advance at the crouch and a rolling few drop to fire a moment, then
     * sprint to catch up. A mass goes in waves, both teams together.
     */
    advance(sq, under) {
      const u = sq.u, t = this.b.t, N = this.nums(u), h = sq.heading, v = Math.max(0.5, sq.speed);
      const e = this.threat(u), dir = this.threatDir(sq);
      const [hx, hy] = fwd(h);
      const close = e ? this.b.gap(u, e) < DR.ranges.bound : false;
      const book = this.byBook, bound = book ? this.boundFor(sq, N) : "wave", S = schoolNums(u.nat), bk = this.cycle(sq, N, bound);
      const vLeap = book ? CATCH_UP * 0.85 * bk.rush / bk.cyc : 4.7 * N.rush / (N.rush + N.halt);
      sq.leap = sq.leap ? v < vLeap * 1.1 : v < vLeap * 0.9;
      const dbl = u.order?.kind === "move" && !!u.order.run;
      for (const m of this.up(sq)) {
        const [a, l] = this.slot(sq, m, "line");
        const s = toWorld(u, u.face, a, l);
        if (!under || dbl) {
          m.aim = void 0;
          m.until = t + 1;
          m.el = void 0;
          this.follow(sq, m, s, dbl ? "run" : close ? "advance" : "walk");
          continue;
        }
        if (m.role === "officer" && sq.leap) {
          this.follow(sq, m, s, "creep");
          m.aim = void 0;
          m.el = void 0;
          continue;
        }
        if (sq.leap) {
          let cyc = N.rush + N.halt, off = N.wave ? 0 : m.team === 2 ? cyc / 2 : 0, rush = N.rush;
          if (book) {
            const first = m.lead || m.j === void 0 && m.role === "rifle" || bound === "body" && m.role === "mg";
            rush = bk.rush;
            cyc = bk.cyc;
            off = -(this.turnFor(sq, m, bound) * bk.slot + (first ? 0 : 0.3 + (BOOK.stagger - 0.3) * roll(this.seed, u.id, grp(m), Math.floor((m.j ?? 0) / 2), 92)));
          } else off += m.jx * 0.08;
          const at = t + off + 100 * cyc, n = Math.floor(at / cyc);
          const rushing = at - n * cyc < rush;
          const gain = (s[0] - m.x) * hx + (s[1] - m.y) * hy, behind = gain > 30;
          const lead = v * (rush + (cyc - rush) / 2);
          if (rushing && m.rushed !== n && gain + lead < 3) m.rushed = n;
          if (book && rushing && m.rushed !== n && !behind && !MOVING.has(m.pose) && this.downed(sq, m)) {
            m.rushed = n;
            m.v = 0;
            m.el = 1;
            m.until = t + 1.5;
            const keep = m.role === "mg" || roll(this.seed, u.id, m.n, n, 94) < BOOK.suppress.keep;
            m.aim = keep && close && m.role !== "officer" ? this.aimFor(sq, m, e) : void 0;
            m.pose = m.aim ? this.firePose(m) : "cower";
            continue;
          }
          if (rushing && m.rushed !== n && m.role !== "officer" || behind && m.pose !== "advance") {
            this.go(sq, m, [s[0] + hx * lead, s[1] + hy * lead], "advance", 3, t + rush, dir, 6);
            m.v = clamp2(Math.hypot(m.spot[0] - m.x, m.spot[1] - m.y) / (rush * 0.85), 2, behind || book ? CATCH_UP : 5.5);
            if (book && m.v <= 2.8 && !behind && roll(this.seed, u.id, m.n, n, 93) < S.crawl) m.pose = "creep";
            m.rushed = n;
            m.aim = void 0;
            m.el = 2;
          } else if (m.pose === "advance" || m.pose === "creep") {
            if (Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) > 1.5 && t < m.until + 1) continue;
            m.v = 0;
            m.spot = [m.x, m.y];
            m.cover = this.shelter(m.x, m.y, groundOf(this.b.field, m.x, m.y), dir, true, false).kind;
            const to = book && m.role !== "officer" && roll(this.seed, u.id, m.n, n, 95) < BOOK.aside[0] ? this.aside(sq, m, n) : void 0;
            if (to) {
              m.spot = to;
              m.pose = "crawl";
              m.v = 1.2;
              m.until = t + 2.5;
              m.aim = void 0;
              m.el = 1;
              continue;
            }
            m.aim = close && m.role !== "officer" ? this.aimFor(sq, m, e) : void 0;
            m.pose = m.aim ? this.firePose(m) : "wait";
            m.until = t + 1.5;
            m.el = m.role === "officer" ? void 0 : 1;
          } else if (book && m.pose === "crawl" && t < m.until && Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) > 0.3) continue;
          else if (t >= m.until && close && m.role !== "officer") {
            m.aim = this.aimFor(sq, m, e);
            m.pose = this.firePose(m);
            m.until = t + 1.5;
            m.el = 1;
          }
          if (book && m.aim && SMALL_ARMS.has(m.pose)) m.rapid = t + 1;
        } else {
          const share = 0.3, cyc = 6;
          const ph = book && bound !== "wave" ? (t - this.turnFor(sq, m, bound) * 2 + 600) % 6 : (t + roll(this.seed, u.id, m.n, 11) * cyc) % cyc;
          const behind = (s[0] - m.x) * hx + (s[1] - m.y) * hy > 12;
          if ((book && bound !== "wave" ? ph < 2 : ph < cyc * share) && m.role !== "officer" && !behind) {
            if (m.pose !== "fire_prone" && m.pose !== "lmg_burst" && m.pose !== "fire_kneel") {
              m.spot = [m.x, m.y];
              m.cover = "none";
            }
            m.v = 0;
            m.pose = close ? m.role === "mg" ? "lmg_burst" : "fire_prone" : "wait";
            m.aim = close ? this.aimFor(sq, m, e) : void 0;
            m.el = 1;
            if (book && m.aim) m.rapid = t + 1;
          } else {
            m.aim = void 0;
            m.el = m.role === "officer" ? void 0 : 2;
            this.follow(sq, m, s, "advance", 1.1);
          }
          m.until = t + 0.5;
        }
      }
    }
    /** Lying where they are, in the best cover near their places, in one pose (the start line: waiting for the guns to lift). */
    lie(sq, pose) {
      const u = sq.u, t = this.b.t, dir = this.threatDir(sq);
      for (const m of this.up(sq)) {
        if (m.role === "officer") continue;
        const [a, l] = this.slot(sq, m, "line");
        const s = toWorld(u, u.face, a, l);
        if (sq.since === t || Math.hypot(s[0] - m.slotAt[0], s[1] - m.slotAt[1]) > 5 || t >= m.until) this.go(sq, m, s, pose, 3, t + 30, dir);
        if (Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) < 1) {
          m.pose = m.role === "mg" ? "hide" : pose;
          m.v = 0;
        } else {
          const far = Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) > 15, r = roll(this.seed, u.id, m.n, 41);
          m.pose = far ? "run" : "creep";
          m.v = far ? 3.8 + 1.4 * r : 2.1 + 0.6 * r;
        }
      }
    }
    /**
     * In a fight. The base of fire (the machine guns and the riflemen not in the assault) lines the block's front in the
     * best cover there, firing, and shifts its place now and then; the assault works forward from cover to cover onto
     * the nearer flank of the block it fights, throws grenades from close range and closes on it as it wavers. A defence
     * on its own ground keeps everyone in the line and fires from its works.
     */
    fight(sq) {
      const u = sq.u, b = this.b, t = b.t, N = this.nums(u), e = this.threat(u), dir = this.threatDir(sq);
      const home = u.side === "def" && u.reserve !== "sent" && Math.hypot(u.x - u.home[0], u.y - u.home[1]) < LAND.hold.within;
      const men = this.up(sq);
      const rifles = men.filter((m) => m.role === "rifle" || m.role === "sapper").sort((a, c) => a.team === c.team ? a.k - c.k : c.team - a.team);
      const nA = home ? 0 : Math.round(rifles.length * N.assault * (e && e.state === "wavering" ? 1.2 : 1));
      const assault = new Set(rifles.slice(0, Math.min(rifles.length, nA)));
      if (e && (sq.flankOf !== e.id || !sq.flank)) {
        sq.flankOf = e.id;
        const [rx, ry] = right(u.face);
        const friends = b.units.filter((f) => f.side === u.side && f !== u && fighting(f) && f.engaged === e.id);
        const lean = friends.reduce((s, f) => s + Math.sign((f.x - u.x) * rx + (f.y - u.y) * ry), 0);
        sq.flank = lean > 0 ? -1 : lean < 0 ? 1 : roll(this.seed, u.id, e.id) < 0.5 ? -1 : 1;
      }
      const gap = e ? b.gap(u, e) : 999, fresh = sq.since === t;
      for (const m of men) {
        if (m.role === "officer") continue;
        const [a, l] = this.slot(sq, m, "line");
        const s = toWorld(u, u.face, a, l);
        if (assault.has(m) && e) {
          if (t >= m.until) {
            const rushNow = m.pose !== "advance" && m.pose !== "crawl" && m.pose !== "creep";
            if (rushNow) {
              const [rx, ry] = right(u.face), [fx, fy] = fwd(u.face);
              const wavering = e.state !== "steady";
              const toE = Math.max(0, gap - (wavering ? 5 : DR.ranges.grenade * 0.7));
              const reach = Math.min(u.d / 2 + Math.min(toE, 38), Math.max(0, (m.slotAt[0] - u.x) * fx + (m.slotAt[1] - u.y) * fy) + 6 + 8 * roll(this.seed, u.id, m.n, t));
              const across = clamp2(a + sq.flank * (6 + 6 * roll(this.seed, u.id, m.n, t, 3)), -u.w / 2 - 20, u.w / 2 + 20);
              const p = [u.x + across * rx + reach * fx, u.y + across * ry + reach * fy];
              const pose = sq.harm > DR.pin.harm * 0.6 ? "crawl" : wavering ? "advance" : roll(this.seed, u.id, m.n, t, 5) < 0.3 ? "creep" : "advance";
              const pace = pose === "crawl" ? 1.2 : pose === "creep" ? 2.5 : 5;
              this.go(sq, m, p, pose, pace, t + N.rush + 1, dir, 8);
              m.aim = void 0;
            } else {
              const aim2 = this.aimAt(m, e), near2 = aim2 ? Math.hypot(aim2[0] - m.x, aim2[1] - m.y) : 999;
              if (near2 < DR.ranges.grenade && roll(this.seed, u.id, m.n, t, 9) < 0.45) {
                m.pose = "throw_grenade";
                m.shot = t;
                m.aim = aim2;
                m.until = t + 1.6;
              } else {
                m.pose = this.firePose(m);
                m.aim = aim2;
                m.until = t + N.halt + 1.5 * roll(this.seed, u.id, m.n, t, 4);
              }
              m.v = 0;
              m.spot = [m.x, m.y];
            }
          } else if ((m.pose === "advance" || m.pose === "creep" || m.pose === "crawl") && Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) < 1) {
            m.pose = this.firePose(m);
            m.v = 0;
            m.aim = this.aimAt(m, e);
            m.until = Math.max(m.until, t + N.halt);
          }
          continue;
        }
        const moved = fresh || Math.hypot(s[0] - m.slotAt[0], s[1] - m.slotAt[1]) > 6;
        if (moved || t >= m.until + (m.role === "mg" ? 20 : 30) * (0.6 + 0.8 * roll(this.seed, u.id, m.n, Math.floor(t / 20)))) {
          this.go(sq, m, s, sq.harm > DR.pin.harm * 0.6 ? "crawl" : "creep", sq.harm > DR.pin.harm * 0.6 ? 1.2 : 2.5, t + 1, dir);
          m.aim = void 0;
          continue;
        }
        if (Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) > 1) continue;
        m.v = 0;
        if (t < m.until && m.pose !== "creep" && m.pose !== "crawl") continue;
        const aim = this.aimAt(m, e), near = aim ? Math.hypot(aim[0] - m.x, aim[1] - m.y) : 999;
        const r = roll(this.seed, u.id, m.n, t, 6);
        if (sq.harm > DR.pin.harm * 0.5 && m.role !== "mg" && r < 0.25) {
          m.pose = "cower";
          m.aim = void 0;
          m.until = t + 1.5 + r * 4;
        } else if (near < DR.ranges.grenade && m.role === "rifle" && r < 0.4) {
          m.pose = "throw_grenade";
          m.shot = t;
          m.aim = aim;
          m.until = t + 1.6;
        } else if (r > 0.88 && m.role !== "mg") {
          m.pose = "wait";
          m.aim = void 0;
          m.until = t + 1.2;
        } else {
          m.pose = this.firePose(m);
          m.aim = aim;
          m.until = t + 3 + 3 * r;
        }
        if (home && m.cover === "none" && u.dug) m.until = Math.min(m.until, t + 1);
      }
    }
    /**
     * In a fight, by the drill book (tactics.ts). The attack: the base of fire (the platoons not in the assault, and the
     * squads' guns where its army leaves them) lies in the best cover along the block's front firing, rapid while the
     * assault is out, at the enemy's weapons first, and shifts to his far side once its assault is close; the assault
     * (whole squads of the platoons on the flank it works onto) goes forward by its turns, one squad down firing while
     * another rushes, swinging out onto the enemy's flank, grenades from cover once in throwing range, and goes in at the
     * last by its army's drill as he wavers: grenades and in, walking forward firing, or the rush with a shout. Each
     * group's heads go down under the fire landing near it, the gunner and a few firing on. The defence on its own
     * ground holds its fire till the enemy is in its weapons' reach, then all fire at once; with the enemy at its line,
     * its rear platoons counter-attack to their old line.
     */
    fightBook(sq) {
      const u = sq.u, b = this.b, t = b.t, N = this.nums(u), e = this.threat(u), dir = this.threatDir(sq), band = this.band(u);
      const S = schoolNums(u.nat), bound = this.boundFor(sq, N), bk = this.cycle(sq, N, bound);
      const home = u.side === "def" && u.reserve !== "sent" && Math.hypot(u.x - u.home[0], u.y - u.home[1]) < LAND.hold.within;
      const men = this.up(sq), gap = e ? b.gap(u, e) : 999, fresh = sq.since === t, wavering = !!e && e.state !== "steady";
      const [rx, ry] = right(u.face), [fx, fy] = fwd(u.face), G2 = DR.ranges.grenade;
      if (e && (sq.flankOf !== e.id || !sq.flank)) {
        sq.flankOf = e.id;
        const friends = b.units.filter((f) => f.side === u.side && f !== u && fighting(f) && f.engaged === e.id);
        const lean = friends.reduce((n, f) => n + Math.sign((f.x - u.x) * rx + (f.y - u.y) * ry), 0);
        sq.flank = lean > 0 ? -1 : lean < 0 ? 1 : roll(this.seed, u.id, e.id) < 0.5 ? -1 : 1;
      }
      let assault = /* @__PURE__ */ new Set();
      if (!home && e) {
        const rifles = men.filter((m) => m.role === "rifle" || m.role === "sapper").length;
        assault = this.flankPlatoons(sq, men, Math.round(rifles * N.assault * (wavering ? 1.2 : 1)), S.gunStays);
      } else if (home) {
        if (e && gap < BOOK.counter.at) sq.closeAt ?? (sq.closeAt = t);
        else if (!e || gap > BOOK.counter.at * 2) sq.closeAt = void 0;
        if (S.counter >= 0 && sq.closeAt !== void 0 && t - sq.closeAt >= S.counter * BOOK.counter.xp[band] && u.state === "steady") assault = this.rearPlatoons(sq, men);
      }
      let near = Infinity;
      if (e && !home) for (const m of assault) near = Math.min(near, this.toBox(m, e));
      const away = e && near < BOOK.lift.mg + 15 ? [e.x + fx * e.d / 2, e.y + fy * e.d / 2] : void 0;
      const out = assault.size > 0;
      if (BOOK.smoke.on && !b.smoke && !home && e && out && S.smoke > 0 && (sq.smokeAt === void 0 || t - sq.smokeAt >= BOOK.smoke.every)) {
        sq.smokeAt = t;
        sq.smokeLeft = S.smoke;
      }
      if (sq.smokeLeft && e && !home && !b.smoke) {
        sq.smokeLeft--;
        this.smoke(sq, e, men.filter((m) => !assault.has(m)), S.smoke - sq.smokeLeft - 1, S.smoke);
      }
      for (const m of men) {
        if (m.role === "officer") continue;
        const [a, l] = this.slot(sq, m, "line");
        const s = toWorld(u, u.face, a, l);
        const lv = this.supOf(sq, m), down = lv > BOOK.suppress.down;
        if (assault.has(m) && e && !home) {
          this.assaultMan(sq, m, e, bound, bk, S.final, S.crawl, lv, wavering, dir);
          continue;
        }
        if (assault.has(m) && home) {
          const p = toWorld(u, u.face, a, l + BOOK.counter.ahead);
          if (Math.hypot(p[0] - m.slotAt[0], p[1] - m.slotAt[1]) > 3) {
            this.go(sq, m, p, "advance", 4.5, t + 3, dir);
            m.aim = void 0;
            m.el = 2;
            continue;
          }
          if (Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) > 1) continue;
          m.v = 0;
          m.el = 2;
          if (t >= m.until) {
            m.aim = this.aimFor(sq, m, e);
            m.pose = m.aim ? this.firePose(m) : "wait";
            m.until = t + 2 + 2 * roll(this.seed, u.id, m.n, t, 6);
          }
          continue;
        }
        const moved = fresh || Math.hypot(s[0] - m.slotAt[0], s[1] - m.slotAt[1]) > 6;
        if (moved || t >= m.until + (m.role === "mg" ? 20 : 30) * (0.6 + 0.8 * roll(this.seed, u.id, m.n, Math.floor(t / 20)))) {
          this.go(sq, m, s, down ? "crawl" : "creep", down ? 1.2 : 2.5, t + 1, dir);
          m.aim = void 0;
          m.el = 1;
          continue;
        }
        if (Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) > 1) continue;
        m.v = 0;
        m.el = home && !out ? void 0 : 1;
        if (t < m.until && m.pose !== "creep" && m.pose !== "crawl") continue;
        const r = roll(this.seed, u.id, m.n, t, 6);
        if (home && e && gap > (S.hold ?? (m.role === "mg" ? BOOK.hold.mg : BOOK.hold.rifle[band]))) {
          m.pose = m.role === "mg" ? "hide" : "wait";
          m.aim = void 0;
          m.until = t + 1;
          continue;
        }
        const aim = this.aimFor(sq, m, e, away), dn = aim ? Math.hypot(aim[0] - m.x, aim[1] - m.y) : 999;
        if (down && m.role !== "mg" && roll(this.seed, u.id, m.n, Math.floor(t / 2), 94) >= BOOK.suppress.keep) {
          m.pose = "cower";
          m.aim = void 0;
          m.until = t + 1 + r * 1.5;
        } else if (dn < G2 && m.role === "rifle" && r < 0.4) {
          m.pose = "throw_grenade";
          m.shot = t;
          m.aim = aim;
          m.until = t + 1.6;
        } else if (r > 0.9 && m.role !== "mg" && !out) {
          m.pose = "wait";
          m.aim = void 0;
          m.until = t + 1.2;
        } else {
          m.pose = aim ? this.firePose(m) : "wait";
          m.aim = aim;
          m.until = t + 2 + 2.5 * r;
          if (out && aim) m.rapid = m.until;
        }
        if (home && m.cover === "none" && u.dug) m.until = Math.min(m.until, t + 1);
      }
    }
    /**
     * Where a man just down crawls aside to (the drill book: the enemy is aiming where he dropped): a metre or three to
     * one side, the other where a mate lies there; none where both are taken.
     */
    aside(sq, m, n) {
      const u = sq.u, [rx, ry] = right(u.face), A = BOOK.aside, stand = this.memo(sq).stand;
      const d = A[1] + (A[2] - A[1]) * roll(this.seed, u.id, m.n, n, 97), s0 = roll(this.seed, u.id, m.n, n, 96) < 0.5 ? -1 : 1;
      for (const sd of [s0, -s0]) {
        const p = this.leash(sq, [m.x + rx * d * sd, m.y + ry * d * sd], -4);
        if (!stand.some((o) => o !== m && Math.hypot(o.spot[0] - p[0], o.spot[1] - p[1]) < 1.6 && Math.hypot(o.x - p[0], o.y - p[1]) < 3)) return p;
      }
      return void 0;
    }
    /**
     * One smoke round (the drill book, while book.smoke.on) from a man of the base of fire (the platoon's headquarters
     * first: its light mortar) onto the enemy's front corner on the assault's flank, the screen's rounds side by side
     * along his front (i of n). No harm: it only lands (the 3D draws the screen, drifting with the battle's wind).
     */
    smoke(sq, e, base, i, n) {
      const u = sq.u, b = this.b, t = b.t, R = DR.rounds;
      const squads = platoonOf(u.nat).squads, by2 = base.find((m) => (m.sub ?? 0) >= squads && m.role === "rifle") ?? base.find((m) => m.role === "rifle");
      if (!by2) return;
      const [rx, ry] = right(u.face), [ex, ey] = right(e.face), [gx, gy] = fwd(e.face), z = Math.round(t * 10);
      const side = (ex * rx + ey * ry >= 0 ? 1 : -1) * sq.flank, front = (gx * (u.x - e.x) + gy * (u.y - e.y) >= 0 ? 1 : -1) * e.d / 2;
      const along = side * e.w * 0.4 + (i - (n - 1) / 2) * BOOK.smoke.spread;
      const ax = e.x + ex * along + gx * front, ay = e.y + ey * along + gy * front;
      const range = Math.hypot(ax - by2.x, ay - by2.y), [i0, i1] = R.indirect.mortar, sg = i0 + i1 * range;
      const x = clamp2(ax + gauss(roll(this.seed, u.id, by2.n, z, 87), roll(this.seed, u.id, by2.n, z, 88)) * sg * 0.35, 1, b.field.w - 1);
      const y = clamp2(ay + gauss(roll(this.seed, u.id, by2.n, z, 89), roll(this.seed, u.id, by2.n, z, 90)) * sg * 0.35, 1, b.field.h - 1);
      const r = { t: Math.round(t * 10) / 10, l: Math.round((t + range / R.speed.mortar) * 10) / 10, by: u.id, k: by2.k, w: "smoke", on: e.id, x: Math.round(x * 10) / 10, y: Math.round(y * 10) / 10, r: "miss" };
      if (by2.j !== void 0) r.j = by2.j;
      this.book(r);
      by2.v = 0;
      by2.spot = [by2.x, by2.y];
      by2.pose = "wait";
      by2.aim = [ax, ay];
      by2.until = t + 1.5;
    }
    /**
     * Smoke on call (release 2, ruling 242 S6): a screen the rules just laid (LandBattle.lay), drawn as the rounds that lay
     * it, one round for each BOOK.smoke.per metres of its line (at most BOOK.smoke.most; looks only), one a think from the
     * block's base of fire, as the book's own smoke is fired (smoke). A screen laid while one is still being drawn takes its
     * place.
     */
    screen(s) {
      const sq = this.squads.get(s.unit);
      if (!sq) return;
      const n = clamp2(Math.ceil(Math.hypot(s.x1 - s.x0, s.y1 - s.y0) / BOOK.smoke.per), 1, BOOK.smoke.most);
      sq.screenQ = { s, i: 0, n };
    }
    /**
     * The next round of the screen being drawn: from the platoon's headquarters (its light mortar) or a rifleman standing,
     * else from the block's front, onto its place along the line ((i + 1/2) of n), scattered a little as a mortar's round;
     * it hangs from its landing to the screen's end (hang). No harm, at no block (on -1). The shooter's stamp is the round's
     * only change to his drill: he stands and fires it.
     */
    screenRound(sq) {
      const q = sq.screenQ, u = sq.u, b = this.b, t = b.t, R = DR.rounds, s = q.s;
      if (q.i >= q.n || !live(u) || t >= s.until) {
        delete sq.screenQ;
        return;
      }
      const men = this.up(sq), squads = platoonOf(u.nat).squads;
      const by2 = men.find((m) => (m.sub ?? 0) >= squads && m.role === "rifle") ?? men.find((m) => m.role === "rifle");
      const k = (q.i + 0.5) / q.n, ax = s.x0 + (s.x1 - s.x0) * k, ay = s.y0 + (s.y1 - s.y0) * k, z = Math.round(t * 10);
      const [fx, fy] = fwd(u.face), ox = by2 ? by2.x : u.x + fx * u.d / 2, oy = by2 ? by2.y : u.y + fy * u.d / 2, n = by2 ? by2.n : 0;
      const range = Math.hypot(ax - ox, ay - oy), [i0, i1] = R.indirect.mortar, sg = i0 + i1 * range;
      const x = clamp2(ax + gauss(roll(this.seed, u.id, n, z, 81), roll(this.seed, u.id, n, z, 82)) * sg * 0.35, 1, b.field.w - 1);
      const y = clamp2(ay + gauss(roll(this.seed, u.id, n, z, 83), roll(this.seed, u.id, n, z, 84)) * sg * 0.35, 1, b.field.h - 1);
      const l = Math.round((t + range / R.speed.mortar) * 10) / 10;
      const r = { t: Math.round(t * 10) / 10, l, by: u.id, w: "smoke", on: -1, x: Math.round(x * 10) / 10, y: Math.round(y * 10) / 10, r: "miss", hang: Math.max(0, Math.round((s.until - l) * 10) / 10) };
      if (by2) {
        r.k = by2.k;
        if (by2.j !== void 0) r.j = by2.j;
      }
      this.book(r);
      if (by2) {
        by2.v = 0;
        by2.spot = [by2.x, by2.y];
        by2.pose = "wait";
        by2.aim = [ax, ay];
        by2.until = t + 1.5;
      }
      if (++q.i >= q.n) delete sq.screenQ;
    }
    /** Whether a mate lies within arm's length of him, down. */
    crowded(sq, m) {
      return this.memo(sq).stand.some((o) => o !== m && Math.abs(o.x - m.x) < 0.9 && Math.abs(o.y - m.y) < 0.9 && o.v === 0);
    }
    /** Metres from a man to a block's ground (0 inside it). */
    toBox(m, e) {
      const [rx, ry] = right(e.face), [fx, fy] = fwd(e.face), dx = m.x - e.x, dy = m.y - e.y;
      return Math.hypot(Math.max(0, Math.abs(dx * rx + dy * ry) - e.w / 2), Math.max(0, Math.abs(dx * fx + dy * fy) - e.d / 2));
    }
    /**
     * One man of the assault (fightBook): in his turn he rushes, 10 to 20 m, swinging out onto the enemy's flank, and is
     * down again firing (a share crawling a metre or three aside first); in throwing range of a steady enemy he lies
     * throwing grenades and firing; once the enemy wavers he goes in by his army's drill. Under fire his group's heads go
     * down and its rushes wait.
     */
    assaultMan(sq, m, e, bound, bk, final, crawl, lv, wavering, dir) {
      const u = sq.u, t = this.b.t, G2 = DR.ranges.grenade, [rx, ry] = right(u.face), [fx, fy] = fwd(u.face);
      m.el = 2;
      if (MOVING.has(m.pose) && Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) > 1 && t < m.until + 1) return;
      const dE = this.toBox(m, e), down = lv > BOOK.suppress.down;
      const [a] = this.slot(sq, m, "line"), mx = m.x - u.x, my = m.y - u.y;
      const da = a + sq.flank * BOOK.flank.out - (mx * rx + my * ry), edge = (e.x - m.x) * fx + (e.y - m.y) * fy - e.d / 2;
      if (wavering && !down) {
        const sa = clamp2(da, -8, 8), p = [m.x + rx * sa + fx * (edge + 4), m.y + ry * sa + fy * (edge + 4)];
        if (final === "grenade" && dE < G2 && m.pose !== "throw_grenade" && (m.shot === void 0 || t - m.shot > 6) && roll(this.seed, u.id, m.n, Math.floor(t / 3), 98) < 0.5) {
          m.v = 0;
          m.spot = [m.x, m.y];
          m.pose = "throw_grenade";
          m.shot = t;
          m.aim = this.aimFor(sq, m, e);
          m.until = t + 1.6;
          return;
        }
        if (t < m.until && m.pose === "throw_grenade") return;
        if (final === "marching") {
          if (m.pose === "fire_stand" && t < m.until) return;
          if (m.pose !== "walk" || t >= m.until) {
            if (m.pose === "walk") {
              m.v = 0;
              m.spot = [m.x, m.y];
              m.pose = "fire_stand";
              m.aim = this.aimFor(sq, m, e);
              m.until = t + 0.6;
              m.rapid = t + 1;
              return;
            }
            this.go(sq, m, p, "walk", 1.6, t + 1.2 + 0.6 * roll(this.seed, u.id, m.n, t, 99), dir, 4);
            m.aim = void 0;
          }
          return;
        }
        this.go(sq, m, p, "advance", final === "charge" ? CATCH_UP : 5.5, t + 4, dir, 4);
        m.aim = void 0;
        return;
      }
      const first = m.lead || m.j === void 0 && m.role === "rifle" || bound === "body" && m.role === "mg";
      const off = -(this.turnFor(sq, m, bound) * bk.slot + (first ? 0 : 0.3 + (BOOK.stagger - 0.3) * roll(this.seed, u.id, grp(m), Math.floor((m.j ?? 0) / 2), 92)));
      const at = t + off + 100 * bk.cyc, n = Math.floor(at / bk.cyc), rushing = at - n * bk.cyc < bk.rush;
      const dl = edge - G2 * 0.7;
      if (rushing && m.rushed !== n && !down && dl > 2) {
        const F = BOOK.flank.rush, swing = F[0] + (F[1] - F[0]) * roll(this.seed, u.id, m.n, n, 3), step = 10 + 10 * roll(this.seed, u.id, m.n, n, 4);
        const sa = clamp2(da, -swing, swing), sl = clamp2(dl, 0, Math.max(3, step - Math.abs(sa) * 0.5));
        const heavy = lv > BOOK.suppress.heavy, slow = heavy || roll(this.seed, u.id, m.n, n, 93) < crawl;
        const d = Math.hypot(sa, sl), pace = heavy ? 1.2 : slow ? 2.6 : clamp2(d / (bk.rush * 0.85), 3.5, CATCH_UP);
        this.go(sq, m, [m.x + rx * sa + fx * sl, m.y + ry * sa + fy * sl], heavy ? "crawl" : slow ? "creep" : "advance", pace, t + (slow ? d / pace : bk.rush), dir, 6);
        m.rushed = n;
        m.aim = void 0;
        return;
      }
      if (rushing && m.rushed !== n) m.rushed = n;
      if (MOVING.has(m.pose)) {
        m.v = 0;
        m.spot = [m.x, m.y];
        m.cover = this.shelter(m.x, m.y, groundOf(this.b.field, m.x, m.y), dir, true, false).kind;
        const to2 = m.pose !== "crawl" && roll(this.seed, u.id, m.n, n, 95) < BOOK.aside[0] ? this.aside(sq, m, n) : void 0;
        if (to2) {
          m.spot = to2;
          m.pose = "crawl";
          m.v = 1.2;
          m.until = t + 2.5;
          m.aim = void 0;
          return;
        }
        m.until = t;
      }
      if (t < m.until) return;
      const to = this.crowded(sq, m) ? this.aside(sq, m, Math.round(t * 2)) : void 0;
      if (to) {
        m.spot = to;
        m.pose = "crawl";
        m.v = 1.2;
        m.until = t + 2;
        m.aim = void 0;
        return;
      }
      const aim = this.aimFor(sq, m, e), dn = aim ? Math.hypot(aim[0] - m.x, aim[1] - m.y) : 999, r = roll(this.seed, u.id, m.n, t, 9);
      m.v = 0;
      m.spot = [m.x, m.y];
      if (down && m.role !== "mg" && roll(this.seed, u.id, m.n, Math.floor(t / 2), 94) >= BOOK.suppress.keep) {
        m.pose = "cower";
        m.aim = void 0;
        m.until = t + 1 + r * 1.5;
      } else if (dn < G2 && m.role !== "mg" && r < (final === "grenade" ? 0.45 : 0.3)) {
        m.pose = "throw_grenade";
        m.shot = t;
        m.aim = aim;
        m.until = t + 1.6;
      } else {
        m.pose = aim ? this.firePose(m) : "wait";
        m.aim = aim;
        m.until = t + 1 + 1.5 * r;
        if (aim) m.rapid = m.until;
      }
    }
    /** Holding its ground, watching: in the best cover along its front, a sentry up at each end, the rest ready. */
    hold(sq) {
      const u = sq.u, t = this.b.t, dir = this.threatDir(sq), near = this.nearestGap(u);
      for (const m of this.up(sq)) {
        if (m.role === "officer") continue;
        const [a, l] = this.slot(sq, m, "line");
        const s = toWorld(u, u.face, a, l);
        if (sq.since === t || Math.hypot(s[0] - m.slotAt[0], s[1] - m.slotAt[1]) > 5 || m.v > 0 && t >= m.until) {
          this.go(sq, m, s, "creep", 2.5, t + 1, dir);
          continue;
        }
        if (Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) > 1) continue;
        m.v = 0;
        m.aim = void 0;
        if (m.role === "at" && m.hidden) continue;
        m.pose = m.role === "mg" ? "hide" : near < 600 ? "wait" : roll(this.seed, u.id, m.n, Math.floor(t / 8)) < 0.3 ? "idle" : "wait";
      }
    }
    /** A reserve resting out of the fight: in knots, in cover, ready; the headquarters at the middle. */
    rest(sq) {
      const u = sq.u, t = this.b.t, dir = this.threatDir(sq);
      for (const m of this.up(sq)) {
        const [a, l] = this.slot(sq, m, "rest");
        const s = toWorld(u, u.face, a, l);
        if (sq.since === t || Math.hypot(s[0] - m.slotAt[0], s[1] - m.slotAt[1]) > 5 || m.v > 0 && t >= m.until) {
          this.go(sq, m, s, "walk", 2, t + 1, dir);
          continue;
        }
        if (Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) > 1) continue;
        m.v = 0;
        m.aim = void 0;
        m.pose = m.k % 5 === 1 ? "wait" : roll(this.seed, u.id, m.n, Math.floor(t / 10)) < 0.5 ? "idle" : "wait";
      }
    }
    /**
     * Pinned: losing men fast, shelled, or wavering under fire. Down in cover; those caught in the open crawl to the nearest
     * shelter; about a third keep their heads down, the rest fire back, the machine guns hardest.
     */
    pinned(sq) {
      const u = sq.u, t = this.b.t, e = this.threat(u), dir = this.threatDir(sq);
      for (const m of this.up(sq)) {
        if (m.v > 0 && Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) > 1 && m.pose === "crawl") continue;
        const here = this.shelter(m.x, m.y, groundOf(this.b.field, m.x, m.y), dir, true, false);
        if (here.v < 0.2 && m.role !== "officer" && (t >= m.until || m.pose === "crawl")) {
          const best = this.seek(sq, m, [m.x, m.y], dir, sq.men, 14);
          if (best.v > here.v + 0.2) {
            m.spot = [best.x, best.y];
            m.cover = best.kind;
            m.pose = "crawl";
            m.v = 1.2;
            m.until = t + 6;
            m.aim = void 0;
            continue;
          }
        }
        m.spot = [m.x, m.y];
        m.cover = here.kind;
        m.v = 0;
        if (t < m.until && m.pose !== "crawl") continue;
        const r = roll(this.seed, u.id, m.n, t, 8);
        if (m.role === "mg") {
          m.pose = "lmg_burst";
          m.aim = this.aimAt(m, e);
          m.until = t + 2 + r * 2;
        } else if (m.role === "officer") {
          m.pose = "wait";
          m.aim = void 0;
          m.until = t + 2;
        } else if (r < 0.35) {
          m.pose = "cower";
          m.aim = void 0;
          m.until = t + 2 + r * 4;
        } else {
          m.pose = m.cover === "works" || m.cover === "building" ? "fire_kneel" : "fire_prone";
          m.aim = this.aimAt(m, e);
          m.until = t + 2 + r * 3;
        }
      }
    }
    /**
     * Falling back in order (a withdrawal, a block worn out at the brink, an army carried off by its break): by bounds,
     * one team facing the enemy and firing while the other runs back past it; at the double, a rolling rearguard.
     */
    fallback(sq) {
      const u = sq.u, t = this.b.t, N = this.nums(u), e = this.threat(u), dir = this.threatDir(sq), v = Math.max(1, sq.speed), h = sq.heading;
      const vLeap = 4.7 * N.rush / (N.rush + N.halt);
      sq.leap = sq.leap ? v < vLeap * 1.1 : v < vLeap * 0.9;
      for (const m of this.up(sq)) {
        const [a, l] = this.slot(sq, m, "line");
        const s = toWorld(u, u.face, a, l);
        const cyc = N.rush + N.halt, ph = (t + (m.team === 2 ? cyc / 2 : 0) + m.jx * 0.08 + 100 * cyc) % cyc;
        const cover = sq.leap ? ph >= N.rush : roll(this.seed, u.id, m.n, Math.floor(t / 2)) < 0.2;
        if (cover && m.role !== "officer" && e && this.b.gap(u, e) < 500) {
          m.v = 0;
          m.spot = [m.x, m.y];
          m.pose = m.role === "mg" ? "lmg_burst" : "fire_kneel";
          m.aim = this.aimAt(m, e);
        } else {
          m.aim = void 0;
          this.follow(sq, m, s, "run", 1.15);
        }
        m.until = t + 0.5;
        void dir;
      }
    }
    /** Routing: every man for himself, running for his own edge, spreading as he goes. */
    flee(sq) {
      const u = sq.u, t = this.b.t, back = u.side === "att" ? Math.PI : 0, v = Math.max(2, sq.speed);
      for (const m of this.up(sq)) {
        const spread = Math.min(1, (t - sq.since) / 15);
        const a = back + (roll(this.seed, u.id, m.n, 13) - 0.5) * 0.9 * spread;
        const lag = (roll(this.seed, u.id, m.n, 14) - 0.5) * 30 * spread;
        const across = (m.jx * 2 + (roll(this.seed, u.id, m.n, 15) - 0.5) * u.w) * (0.5 + spread * 0.7);
        const [rx, ry] = right(u.face);
        const s = [u.x + across * rx + Math.sin(a) * (10 + lag), u.y + across * ry + Math.cos(a) * (10 + lag)];
        m.aim = void 0;
        m.hidden = void 0;
        m.until = t + 0.5;
        this.follow(sq, m, s, "flee", 1.1);
      }
    }
    /** Rallied: the men gather on their officer, facing the enemy again. */
    rally(sq) {
      const u = sq.u, t = this.b.t, dir = this.threatDir(sq);
      const men = this.up(sq), lead = men.find((m) => m.role === "officer") ?? men[0];
      if (!lead) return;
      lead.v = 0;
      lead.spot = [lead.x, lead.y];
      lead.pose = "idle";
      lead.aim = void 0;
      men.forEach((m, i) => {
        if (m === lead) return;
        const ang = i / men.length * Math.PI * 2, r = sq.tier >= 1 ? 7 + 0.25 * men.length : 7;
        const p = [lead.x + Math.sin(ang) * r + dir[0] * 4, lead.y + Math.cos(ang) * r + dir[1] * 4];
        if (Math.hypot(m.spot[0] - p[0], m.spot[1] - p[1]) > 4) this.go(sq, m, p, "run", 4, t + 2, dir, 4);
        else if (Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) < 1) {
          m.v = 0;
          m.pose = "wait";
        }
      });
      void u;
    }
    /** Lorried infantry still aboard, cavalry still mounted: they ride where their block goes. */
    mounted(sq) {
      const u = sq.u, t = this.b.t;
      if (u.kind === "mot") {
        sq.veh.forEach((v, i) => {
          const s = toWorld(u, sq.heading, 0, u.d / 2 - 8 - i * 18);
          this.follow(sq, v, s, "drive");
          v.until = t + 1;
        });
        for (const m of this.up(sq)) {
          m.pose = "mounted";
          m.aim = void 0;
          m.cover = "none";
        }
        return;
      }
      for (const m of this.up(sq)) {
        const [a, l] = this.slot(sq, m, sq.phase === "column" ? "column" : "line");
        const s = toWorld(u, sq.phase === "column" ? sq.heading : u.face, a, l);
        m.aim = void 0;
        m.until = t + 1;
        this.follow(sq, m, s, "ride");
        if (m.v === 0) m.pose = "ride";
      }
    }
    /**
     * Off the lorries and horses. A trooper is down where he sits his horse, on his feet. Lorried men get down over their
     * lorry's tailboard one at a time, the back of her bed first, the two benches turn about (hopOut), the lorries standing
     * meanwhile; the block's phase then sends each man to his place.
     * (Down where they sat, they stood in the lorry's footprint, under her: Brian, 23 November 14:10Z, "troops are stuck
     * under the truck ... they then hop out and start engaging enemies".)
     */
    alight(sq) {
      sq.boarding = void 0;
      const t = this.b.t, aboard = [];
      for (const m of sq.men) {
        if (m.fell !== void 0 || m.pose !== "mounted" && m.pose !== "ride") continue;
        if (m.pose === "mounted" && sq.veh.length) {
          aboard.push(m);
          continue;
        }
        m.pose = "wait";
        m.v = 0;
        m.spot = [m.x, m.y];
        m.aim = void 0;
        m.cover = "none";
      }
      if (!aboard.length) return;
      const by2 = /* @__PURE__ */ new Map();
      for (const m of aboard) {
        const i = this.lorryIx(sq, m), l = by2.get(i);
        if (l) l.push(m);
        else by2.set(i, [m]);
      }
      for (const l of by2.values()) l.sort((a, c) => this.seatNo(sq, c) - this.seatNo(sq, a)).forEach((m, n) => {
        m.out = t + n * MOUNT.hop;
      });
      sq.hop = t;
      sq.left = void 0;
      for (const v of sq.veh) {
        v.spot = [v.x, v.y];
        v.slotAt = v.spot;
        v.pose = "parked";
        v.v = 0;
      }
    }
    /**
     * Getting down (alight): each man, his turn come, over his lorry's tailboard onto the ground a stride behind her, on
     * his bench's side, and off at a run to clear her tail (the left bench's men to her left, the right's to her right,
     * the back benches' nearest), kneeling there till the last of the block is down; the lorries stand meanwhile. A man
     * still aboard after MOUNT.hopMost seconds goes at once. True once every man is down.
     */
    hopOut(sq) {
      const t = this.b.t;
      let still = false;
      for (const v of sq.veh) {
        v.v = 0;
        v.pose = "parked";
        v.spot = [v.x, v.y];
      }
      for (const m of sq.men) {
        if (m.out === void 0) continue;
        if (m.fell !== void 0 || m.pose !== "mounted" || !sq.veh.length) {
          m.out = void 0;
          continue;
        }
        if (t < m.out && t - (sq.hop ?? t) < MOUNT.hopMost) {
          still = true;
          continue;
        }
        m.out = void 0;
        const v = this.lorryOf(sq, m), j = this.seatNo(sq, m), side = j % 2 ? 1 : -1, row = Math.floor(j / 2);
        [m.x, m.y] = toWorld(v, v.face, side * 0.45, -MOUNT.tail);
        m.face = wrap(v.face + Math.PI);
        m.went = 0;
        m.spot = toWorld(v, v.face, side * (1.6 + 0.5 * row), -MOUNT.tail - 1.5 - 0.4 * row);
        m.slotAt = m.spot;
        m.pose = "run";
        m.v = 3.5;
        m.aim = void 0;
        m.hidden = void 0;
        m.cover = "none";
        m.until = t + 1;
      }
      for (const m of sq.men) if (m.fell === void 0 && m.out === void 0 && !m.job && MOVING.has(m.pose) && Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) < 0.3) this.arrive(sq, m);
      if (still) return false;
      sq.hop = void 0;
      sq.left = t;
      return true;
    }
    /**
     * Down, and the block goes on faster than men on foot can follow: they mount again. Each lorried man makes for his own
     * lorry; each trooper for the horse his holder leads (his own four's holder, else the nearest one left; with none left
     * his horse is by him).
     */
    mountUp(sq) {
      const u = sq.u;
      sq.boarding = this.b.t;
      sq.hop = sq.left = void 0;
      for (const m of sq.men) m.out = void 0;
      for (const v of sq.veh) if (v.pose === "parked") v.pose = "drive";
      const men = sq.men.filter((m) => this.stands(sq, m));
      for (const m of men) {
        m.aid = void 0;
        m.hidden = void 0;
        m.aim = void 0;
        if (u.kind !== "mot" && m.role !== "holder") m.bus = this.holderOf(sq, m)?.k;
      }
    }
    /** The nearest of some men (lorries, holders) to a man, and its place in the list. */
    nearest(of, m) {
      let best, d = Infinity;
      of.forEach((o, i) => {
        const dd = Math.hypot(o.x - m.x, o.y - m.y);
        if (dd < d) {
          d = dd;
          best = [o, i];
        }
      });
      return best;
    }
    /**
     * The holder leading a trooper's horse now (his bus, a holder's number): that number's own man where he still stands
     * and is no one's prisoner, else a holder of his section (drawn as sections) still standing with it, as the holders
     * themselves go to every man of their number (board). None: his bus is gone with all its holders.
     * (A section's key holder down while his section's holders stood left the trooper with no man to run to: the release 2
     * drill threw at Poznan in 1939, a Polish cavalry block remounting in its rout; the design director's find, 23 Nov.)
     */
    busOf(sq, m) {
      if (m.bus === void 0) return void 0;
      let other2;
      for (const h of sq.men) {
        if (h.k !== m.bus || h.role !== "holder" || h.pr || !this.stands(sq, h)) continue;
        if (h.j === void 0) return h;
        other2 ?? (other2 = h);
      }
      return other2;
    }
    /** A trooper's horse holder: the one at the end of his four, else the nearest still standing. */
    holderOf(sq, m) {
      const held = sq.men.filter((h) => h.role === "holder" && !h.pr && this.stands(sq, h));
      return held.find((h) => Math.floor(h.k / 4) === Math.floor(m.k / 4)) ?? this.nearest(held, m)?.[0];
    }
    /** The lorry a lorried man rides in: his own, by his place on the tray. */
    lorryOf(sq, m) {
      return sq.veh[this.lorryIx(sq, m)];
    }
    /** Which of the block's lorries is his: by his place on the tray, or (drawn as sections) his section's own. */
    lorryIx(sq, m) {
      return (m.lor ?? (sq.tier >= 1 ? m.sec ?? m.k : m.k)) % sq.veh.length;
    }
    /** His seat in his lorry's bed, two benches a stride apart, front to back (so men getting down are never one knot). */
    seat(sq, m) {
      const v = this.lorryOf(sq, m), j = this.seatNo(sq, m);
      if (sq.tier >= 1) return toWorld(v, v.face, j % 2 ? 0.55 : -0.55, 1.4 - 0.6 * Math.floor(j / 2));
      return toWorld(v, v.face, j % 2 ? 0.55 : -0.55, 0.5 - 1.1 * Math.floor(j / 2));
    }
    /**
     * His seat's number in his lorry's bed (seat): two abreast, the odd ones on her right, front to back. Each lorry's men
     * are numbered once, in their sections' own order (a section to a lorry keeps its seats: its key man first); where
     * sections share a lorry (more sections than lorries) they sit turn about, so no two men have one seat (they sat on
     * one another: the seats' cloud test, 23 November).
     */
    seatNo(sq, m) {
      const own = (x) => sq.tier >= 1 ? x.seat ?? x.j ?? 0 : Math.floor((x.k - (sq.of !== void 0 ? DR.mech.base : 0)) / sq.veh.length);
      if (!sq.seats) {
        sq.seats = /* @__PURE__ */ new Map();
        const by2 = /* @__PURE__ */ new Map();
        for (const x of sq.men) {
          if (x.role === "holder") continue;
          const i = this.lorryIx(sq, x), l = by2.get(i);
          if (l) l.push(x);
          else by2.set(i, [x]);
        }
        for (const l of by2.values()) l.map((x, i) => ({ x, o: own(x), i })).sort((a, c) => a.o - c.o || a.i - c.i).forEach(({ x }, n) => sq.seats.set(x, n));
      }
      return sq.seats.get(m) ?? own(m);
    }
    /**
     * Where a man mounting runs to: his lorry, or his horse at his holder's side (the holder's string of horses spread
     * across his right and left, one per man he leads, in their order); null where he has no horse led for him.
     */
    mountAt(sq, m) {
      const u = sq.u;
      if (u.kind === "mot") return this.seat(sq, m);
      const h = this.busOf(sq, m);
      if (!h) return null;
      const led = sq.men.filter((x) => x.bus === h.k && x.role !== "holder" && this.stands(sq, x)).sort((a, c) => a.k - c.k || a.n - c.n);
      const j = Math.max(0, led.indexOf(m)), side = j % 2 ? 1 : -1, [rx, ry] = right(h.face);
      const off = side * (1.6 + 1.6 * Math.floor(j / 2));
      return [h.x + rx * off, h.y + ry * off];
    }
    /**
     * Mounting again (mountUp). Each lorry drives up to its men and halts while they climb aboard, then goes after the
     * block; each holder brings his horses up to the men he leads them for, who run to them and mount, he last, and each
     * rides on to his place as he is up. Once every man is up the block rides on (mounted).
     */
    board(sq) {
      const u = sq.u, t = this.b.t;
      const men = sq.men.filter((m) => this.stands(sq, m));
      const run = sq.phase === "flee" ? "flee" : "run";
      const afoot = (m, to) => {
        const dx = to.x - m.x, dy = to.y - m.y, at = this.mountAt(sq, m);
        m.aim = void 0;
        m.hidden = void 0;
        m.cover = "none";
        m.until = t + 1;
        const by2 = !!at && Math.hypot(at[0] - m.x, at[1] - m.y) < 2;
        const go = sq.slow < MOUNT.downAfter ? sq.phase === "flee" || m.pose === run || Math.hypot(dx, dy) < 30 || to.v === 0 || sq.net > 1 && (dx * sq.wx + dy * sq.wy) / sq.net > 5 : sq.phase === "flee" && !by2;
        if (go) {
          m.pose = run;
          m.v = 6;
        } else {
          m.pose = "wait";
          m.v = 0;
          m.spot = [m.x, m.y];
        }
      };
      let all = true;
      if (u.kind === "mot") {
        sq.veh.forEach((v, i) => {
          const off = men.filter((m) => m.pose !== "mounted" && this.lorryIx(sq, m) === i);
          v.cover = "none";
          v.until = t + 1;
          if (!off.length) {
            this.follow(sq, v, toWorld(u, sq.heading, 0, u.d / 2 - 8 - i * 18), "drive");
            return;
          }
          all = false;
          const cx = off.reduce((a, m) => a + m.x, 0) / off.length, cy = off.reduce((a, m) => a + m.y, 0) / off.length, d = Math.hypot(cx - v.x, cy - v.y);
          if (d > 6 && v.pose !== "parked") {
            v.spot = [cx, cy];
            v.slotAt = v.spot;
            v.pose = "drive";
            v.v = clamp2(d * 0.5, 3, 16);
          } else {
            v.spot = [v.x, v.y];
            v.slotAt = v.spot;
            v.pose = "parked";
            v.v = 0;
          }
        });
        for (const m of men) if (m.pose !== "mounted") afoot(m, this.lorryOf(sq, m));
      } else {
        const slotOf = (m) => {
          const [a, l] = this.slot(sq, m, sq.phase === "column" ? "column" : "line");
          return toWorld(u, sq.phase === "column" ? sq.heading : u.face, a, l);
        };
        for (const m of men) {
          if (m.role === "holder") continue;
          if (m.pose === "ride") {
            this.follow(sq, m, slotOf(m), "ride");
            if (m.v === 0) m.pose = "ride";
            m.until = t + 1;
            continue;
          }
          if (!this.busOf(sq, m)) m.bus = this.holderOf(sq, m)?.k;
          const h = this.busOf(sq, m);
          if (!h) {
            m.pose = "ride";
            m.v = 0;
            m.spot = [m.x, m.y];
            m.aim = void 0;
            m.cover = "none";
            continue;
          }
          all = false;
          afoot(m, h);
        }
        for (const h of men) {
          if (h.role !== "holder" || h.pr) continue;
          const mine = men.filter((m) => m.bus === h.k && m.role !== "holder" && m.pose !== "ride");
          h.aim = void 0;
          h.cover = "none";
          h.until = t + 1;
          if (!mine.length) {
            if (h.pose !== "ride") {
              h.pose = "ride";
              h.v = 0;
              h.spot = [h.x, h.y];
            }
            this.follow(sq, h, slotOf(h), "ride");
            if (h.v === 0) h.pose = "ride";
            continue;
          }
          all = false;
          const cx = mine.reduce((a, m) => a + m.x, 0) / mine.length, cy = mine.reduce((a, m) => a + m.y, 0) / mine.length, d = Math.hypot(cx - h.x, cy - h.y);
          const halted = h.pose === "horses" && h.v === 0;
          h.pose = "horses";
          if (d > 5 && !halted) {
            h.spot = [cx, cy];
            h.v = clamp2(d * 0.5, 3, 12);
          } else {
            h.spot = [h.x, h.y];
            h.v = 0;
          }
        }
      }
      if (all && sq.slow === 0) {
        sq.down = false;
        sq.boarding = void 0;
      }
    }
    /** A man mounting may climb in or swing up now: his lorry halted (his horse held still) and his block still going. */
    mayMount(sq, m) {
      if (sq.slow >= MOUNT.downAfter) return false;
      if (sq.u.kind === "mot") return this.lorryOf(sq, m).pose === "parked";
      const h = this.busOf(sq, m);
      return !!h && h.v === 0;
    }
    /**
     * Lorries: aboard, they carry the block; once it is down they pull back behind it and park in what cover there is; a
     * battery's tractor waits behind its guns, and leads them when they move. A horse holder takes his mates' horses back.
     */
    lorries(sq) {
      const u = sq.u, t = this.b.t;
      if (u.kind === "cav" && sq.down) {
        for (const m of sq.men) if (m.role === "holder" && !m.pr && this.stands(sq, m)) {
          const s = toWorld(u, u.face, m.jx * 4, -u.d / 2 - (sq.close ? 10 : 36));
          m.spot = s;
          m.slotAt = s;
          m.pose = "horses";
          m.v = Math.min(14, Math.max(3, sq.speed * 1.25));
          m.aim = void 0;
          m.cover = "none";
        }
      }
      if (u.kind !== "mot" || !sq.down) return;
      if (sq.left !== void 0 && t < sq.left + MOUNT.clear && sq.phase !== "flee") {
        for (const v of sq.veh) {
          v.v = 0;
          v.pose = "parked";
          v.spot = [v.x, v.y];
        }
        return;
      }
      sq.veh.forEach((v, i) => {
        if (sq.phase === "flee" || sq.phase === "fallback" || sq.close) {
          const s2 = toWorld(u, u.face, (i - 0.5) * 20, -u.d / 2 - (sq.close ? 30 : 40));
          this.follow(sq, v, s2, "drive", 1.2);
          return;
        }
        const s = toWorld(u, u.face, (i - (sq.veh.length - 1) / 2) * 24, -u.d / 2 - 110);
        if (Math.hypot(s[0] - v.slotAt[0], s[1] - v.slotAt[1]) > 25 || t < 0.2) {
          const best = this.seek(sq, v, s, this.threatDir(sq), sq.veh, 30);
          v.spot = [best.x, best.y];
          v.slotAt = s;
          v.cover = best.kind;
          v.pose = "drive";
          v.v = Math.min(16, Math.max(6, sq.speed * 1.3));
        } else if (Math.hypot(v.x - v.spot[0], v.y - v.spot[1]) < 1.5) {
          v.pose = "parked";
          v.v = 0;
        }
      });
    }
    /**
     * The anti-tank team's ambush: once enemy tanks come within range of a block holding or fighting where it stands, its
     * anti-tank man goes to the end of its line nearer their flank, lies hidden in the best cover there, and fires only
     * when they are close or are fighting his block; after each shot he moves. Foot in contact with tanks fighting alone
     * (rules: item 7, aloneAt) close in on them with grenades: the close-assault teams.
     */
    ambush(sq) {
      const u = sq.u, b = this.b, t = b.t;
      if (!fighting(u) || sq.phase === "fallback" || sq.phase === "flee" || sq.phase === "column") return;
      const at = this.up(sq).find((m) => m.role === "at");
      const tanks = b.units.filter((e) => e.side !== u.side && e.kind === "arm" && fighting(e) && !e.hid && b.gap(u, e) < DR.ranges.ambush);
      if (at) {
        if (!tanks.length || sq.speed > 0.6) at.hidden = void 0;
        else {
          const tk = tanks.reduce((m, e) => b.gap(u, e) < b.gap(u, m) ? e : m, tanks[0]);
          const [rx, ry] = right(u.face), side = Math.sign((tk.x - u.x) * rx + (tk.y - u.y) * ry) || 1;
          const end = toWorld(u, u.face, side * (u.w / 2 - 4), u.d / 2 - 6);
          const dir = [(tk.x - end[0]) / (Math.hypot(tk.x - end[0], tk.y - end[1]) || 1), (tk.y - end[1]) / (Math.hypot(tk.x - end[0], tk.y - end[1]) || 1)];
          const close = b.gap(u, tk) < 150 || tk.engaged === u.id;
          if (!at.hidden && !close) {
            this.go(sq, at, end, "creep", 2.5, t + 3, dir, 14);
            at.hidden = true;
            at.aim = void 0;
          } else if (at.hidden && Math.hypot(at.x - at.spot[0], at.y - at.spot[1]) < 1 && !close) {
            at.pose = "hide";
            at.v = 0;
          }
          if (close && t >= at.until) {
            const ts = this.squads.get(tk.id), tank = ts?.men.filter((x) => x.role === "tank" && this.stands(ts, x)).sort((p, q) => Math.hypot(p.x - at.x, p.y - at.y) - Math.hypot(q.x - at.x, q.y - at.y))[0];
            at.hidden = void 0;
            at.pose = "fire_kneel";
            at.v = 0;
            at.shot = t;
            at.aim = tank ? [tank.x, tank.y] : [tk.x, tk.y];
            at.until = t + 6 + 3 * roll(this.seed, u.id, t);
            const next = toWorld(u, u.face, side * (u.w / 2 - 10 - 15 * roll(this.seed, u.id, t, 2)), u.d / 2 - 6);
            at.slotAt = next;
          } else if (!at.hidden && close && Math.hypot(at.x - at.slotAt[0], at.y - at.slotAt[1]) > 3 && t >= at.until - 3) this.go(sq, at, at.slotAt, "creep", 2.5, t + 3, dir, 6);
        }
      }
      const lone = tanks.find((e) => e.aloneAt !== void 0 && t - e.aloneAt < 2 && (e.engaged === u.id || u.engaged === e.id));
      if (lone && isFoot2(u)) {
        const ts = this.squads.get(lone.id);
        const tank = ts?.men.filter((x) => x.role === "tank" && this.stands(ts, x))[0];
        if (tank) {
          const team = this.up(sq).filter((m) => m.role === "rifle" || m.role === "at").sort((p, q) => Math.hypot(p.x - tank.x, p.y - tank.y) - Math.hypot(q.x - tank.x, q.y - tank.y)).slice(0, 2);
          for (const m of team) {
            const d = Math.hypot(m.x - tank.x, m.y - tank.y);
            if (d > 14) {
              if (t >= m.until || m.pose !== "crawl") {
                const side = m.k % 2 ? 1 : -1, [rx, ry] = right(tank.face);
                m.spot = [tank.x + rx * side * 9, tank.y + ry * side * 9];
                m.pose = "crawl";
                m.v = 2;
                m.until = t + 8;
                m.hidden = void 0;
              }
            } else if (t >= m.until || m.pose === "crawl") {
              m.pose = "throw_grenade";
              m.v = 0;
              m.shot = t;
              m.aim = [tank.x, tank.y];
              m.until = t + 2.5;
            }
          }
        }
      }
    }
    /** The officer walks his line every so often, crouched, stopping by each team. */
    officer(sq) {
      const u = sq.u, t = this.b.t;
      if (sq.phase !== "fight" && sq.phase !== "hold" && sq.phase !== "startline") return;
      const o = this.up(sq).find((m) => m.role === "officer");
      if (!o || t < sq.officerAt) return;
      sq.officerAt = t + 8 + 8 * roll(this.seed, u.id, t);
      const team = Math.floor(t / 8) % 3;
      const mates = this.up(sq).filter((m) => m.team === team && m !== o);
      const by2 = mates.length ? mates[Math.floor(roll(this.seed, u.id, t, 1) * mates.length)] : null;
      const p = by2 ? [by2.spot[0] - Math.sin(u.face) * 4, by2.spot[1] - Math.cos(u.face) * 4] : toWorld(u, u.face, 0, -u.d / 2 + 6);
      this.go(sq, o, p, "creep", 2.5, t + 6, void 0, 5);
    }
    // ---- Tanks --------------------------------------------------------------------------------------------------------
    /**
     * Tanks. On the road, nose to tail. Coming on, in pairs: where the block goes slowly one pair halts to cover while the
     * other drives to its next firing place; fast, all drive and a tank halts to fire now and then. In a fight each takes
     * the best firing place near its own: hull-down behind a crest where there is one, else a wood's or a house's corner;
     * after firing from the open she backs off a few metres and comes up again (by style). In woods and streets with
     * their own foot near, they keep behind the foot's line. Alone among enemy foot they close up. Shaken, they back off to
     * cover; routing, they drive for their own edge. Each tank's place is where the rules would lay her wreck (sim.ts
     * wreckOffset), so she is never far from it; knocked out, she burns where she stood (falls, wreckSpot).
     */
    tanks(sq) {
      const u = sq.u, b = this.b, t = b.t, N = this.nums(u), e = this.threat(u), dir = this.threatDir(sq), ph = sq.phase, h = sq.heading;
      const tanks = this.up(sq).filter((m) => m.role === "tank");
      const lone = u.aloneAt !== void 0 && t - u.aloneAt < 2;
      const v = Math.max(1, sq.speed);
      const close = CLOSE2.includes(groundOf(b.field, u.x, u.y));
      const foot = b.units.filter((f) => f.side === u.side && fighting(f) && FOOT_KINDS.includes(f.kind) && b.gap(u, f) < 150);
      const [fx, fy] = fwd(u.face);
      let behind = Infinity;
      if (close && foot.length) for (const f of foot) behind = Math.min(behind, (f.x - u.x) * fx + (f.y - u.y) * fy + f.d / 2 - 15);
      tanks.forEach((m, i) => {
        m.buttoned = lone || e !== null && b.gap(u, e) < 120 || m.cmdDown !== void 0 || void 0;
        m.coax = this.byBook && !!e && FOOT_KINDS.includes(e.kind) && u.engaged !== void 0 && b.gap(u, e) < 600 || void 0;
        const n = sq.u.figures0 - 1 - m.k;
        const [wa, wl] = wreckOffset(u.id, n, u.w, u.d);
        if (ph === "flee") {
          m.aim = void 0;
          this.follow(sq, m, toWorld(u, u.face, wa, wl), "drive", 1.1);
          return;
        }
        if (ph === "column") {
          m.aim = void 0;
          this.follow(sq, m, toWorld(u, h, 0, u.d / 2 - 10 - i * 22), "drive");
          return;
        }
        let l = wl;
        if (behind < Infinity) l = Math.min(l, behind);
        const s = toWorld(u, u.face, wa, l);
        if (ph === "fallback") {
          this.follow(sq, m, s, "reverse");
          m.aim = e ? [e.x, e.y] : void 0;
          return;
        }
        if (ph === "deploy" || ph === "bound" && v >= 3.5) {
          const [hx, hy] = fwd(h), lag = (s[0] - m.x) * hx + (s[1] - m.y) * hy, at = t + roll(this.seed, u.id, m.n, 41) * 4;
          const halt = ph === "bound" && e && b.gap(u, e) < 600 && lag < 6 && at % 4 < 1.5 && roll(this.seed, u.id, m.n, Math.floor(at / 4)) < 0.25;
          if (halt) {
            m.spot = [m.x, m.y];
            m.v = 0;
            m.pose = this.tankShot(sq, m, e, N) ? "fire" : "halt";
            m.aim = this.tankAim(sq, m, e);
          } else {
            this.follow(sq, m, s, "drive");
            m.aim = e && b.gap(u, e) < 900 ? [e.x, e.y] : void 0;
          }
          return;
        }
        if (ph === "bound") {
          const pair = m.k < 2 ? 0 : 1, cyc = N.tankRush + N.tankHalt, off2 = pair ? cyc / 2 : 0;
          const moving = (t + off2 + 100 * cyc) % cyc < N.tankRush;
          if (moving) {
            if (m.pose !== "drive" && m.pose !== "reverse" || t >= m.until) {
              const [hx, hy] = fwd(h);
              this.go(sq, m, [s[0] + hx * v * cyc * 0.45, s[1] + hy * v * cyc * 0.45], "drive", v * cyc / N.tankRush, t + N.tankRush, dir);
            }
            m.aim = e ? [e.x, e.y] : void 0;
          } else if (Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) < 2 || m.pose !== "drive" && m.pose !== "reverse") {
            m.v = 0;
            m.spot = [m.x, m.y];
            m.pose = e && this.tankShot(sq, m, e, N) ? "fire" : "halt";
            m.aim = this.tankAim(sq, m, e);
          }
          return;
        }
        const shaken = ph === "pinned" || u.state === "wavering";
        const posts = N.scoot > 0 && !shaken && !lone && u.engaged !== void 0;
        if (!posts && m.cyc) {
          m.cyc = void 0;
          m.post = 0;
        }
        const T = DR.tanks, [rx, ry] = right(u.face), off = [0, 1, 0, -1][(m.post ?? 0) % 4] * (m.k % 2 ? 1 : -1) * T.post;
        const want = shaken ? [s[0] - dir[0] * 25, s[1] - dir[1] * 25] : [s[0] + rx * off, s[1] + ry * off];
        if (Math.hypot(want[0] - m.slotAt[0], want[1] - m.slotAt[1]) > (m.cyc ? 30 : 10) || m.pose === "drive" && t >= m.until && !m.cyc) {
          m.cyc = void 0;
          this.go(sq, m, want, shaken ? "reverse" : "drive", shaken ? 2 : 4, t + 4, dir);
          m.aim = e ? [e.x, e.y] : void 0;
          return;
        }
        if (m.cyc) {
          const there = Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) < 2;
          m.aim = e ? [e.x, e.y] : void 0;
          if (!there && t < m.until) return;
          if (m.cyc === "back") {
            m.post = (m.post ?? 0) + 1;
            const o = [0, 1, 0, -1][m.post % 4] * (m.k % 2 ? 1 : -1) * T.post, p = [s[0] + rx * o, s[1] + ry * o];
            m.cyc = "over";
            m.slotAt = p;
            m.spot = this.leash(sq, [p[0] - dir[0] * T.back, p[1] - dir[1] * T.back], -4);
            m.pose = "drive";
            m.v = 3.5;
            m.until = t + 12;
            return;
          }
          if (m.cyc === "over") {
            const best = this.seek(sq, m, m.slotAt, dir, sq.men);
            m.cyc = "up";
            m.spot = [best.x, best.y];
            m.cover = best.kind;
            m.pose = "drive";
            m.v = 2.5;
            m.until = t + 8;
            return;
          }
          m.cyc = void 0;
          m.shots = 0;
        }
        if (m.pose === "reverse" && t < m.until) return;
        if (m.pose === "reverse") {
          m.spot = [m.slotAt[0], m.slotAt[1]];
          const best = this.seek(sq, m, m.slotAt, dir, sq.men);
          m.spot = [best.x, best.y];
          m.cover = best.kind;
          m.pose = "drive";
          m.v = 3;
          m.until = t + 4;
          return;
        }
        if (Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) > 1.5) return;
        m.v = 0;
        if (!e || u.engaged === void 0 && b.gap(u, e) > 600) {
          m.pose = "halt";
          m.aim = e ? [e.x, e.y] : void 0;
          return;
        }
        m.aim = this.tankAim(sq, m, e);
        const need2 = T.shots[0] + Math.floor(roll(this.seed, u.id, m.n, m.post ?? 0, 23) * (T.shots[1] - T.shots[0] + 1));
        if (posts && (m.shots ?? 0) >= need2 && m.shot !== void 0 && t - m.shot >= 1.5) {
          m.cyc = "back";
          m.spot = this.leash(sq, [m.x - dir[0] * T.back, m.y - dir[1] * T.back], -4);
          m.pose = "reverse";
          m.v = 2.5;
          m.until = t + 8;
          return;
        }
        if (u.engaged !== void 0 && this.tankShot(sq, m, e, N)) {
          m.pose = "fire";
          m.shots = (m.shots ?? 0) + 1;
        } else m.pose = "halt";
      });
      for (const m of tanks) if (m.pose === "halt" && m.shot !== void 0 && t - m.shot < 1.5) m.pose = "fire";
    }
    /**
     * Whether this tank fires now (each about every 6 to 10 s while her block fights; the drill book: at foot, a round
     * every BOOK.tank.every seconds at a weapon, her machine guns doing the rest).
     */
    tankShot(sq, m, e, N) {
      if (!e || sq.u.engaged === void 0) return false;
      const r = roll(this.seed, sq.u.id, m.n, 31), T = BOOK.tank.every;
      const t = this.b.t, every = this.byBook && e.kind !== "arm" ? T[0] + (T[1] - T[0]) * r : 6 + 4 * r;
      if (m.shot !== void 0 && t - m.shot < every) return false;
      m.shot = t;
      void N;
      return true;
    }
    // ---- Guns ---------------------------------------------------------------------------------------------------------
    /**
     * A battery. Moving, its guns are towed behind its tractor in column. Standing, they unlimber in a line behind the
     * block's middle, each in the best place near its own: behind a crest (firing over it at what it cannot see) or a
     * wood's edge. They fire by the style's drill: together, as fast as each can, or one after another down the line;
     * over open sights when the enemy is close or its tanks are near. Shelled, the crews go down a moment. A spotter goes
     * up with the friendly foot nearest the target while the guns fire far.
     */
    guns(sq) {
      const u = sq.u, b = this.b, t = b.t, N = this.nums(u), dir = this.threatDir(sq), h = sq.heading;
      const e = u.aim !== void 0 ? b.unit(u.aim) ?? null : u.engaged !== void 0 ? b.unit(u.engaged) ?? null : null;
      const guns = this.up(sq).filter((m) => m.role === "gun"), tractor = sq.veh[0];
      const own = sq.tier >= 1 && sq.veh.length > 1;
      if (sq.phase === "limbered" || sq.phase === "flee" || sq.phase === "fallback") {
        guns.forEach((g, i) => {
          g.aim = void 0;
          this.follow(sq, g, toWorld(u, h, 0, -10 - i * (own ? 26 : 18)), "towed");
          if (g.v === 0) g.pose = "towed";
        });
        if (own) guns.forEach((g) => {
          if (g.ml) return;
          const v = this.lorryOf(sq, g);
          this.follow(sq, v, toWorld(g, g.face, 0, 7), "drive");
        });
        else if (tractor) {
          this.follow(sq, tractor, toWorld(u, h, 0, 8), "drive");
        }
        this.spotter(sq, null, false);
        return;
      }
      sq.veh.forEach((tr, i) => {
        if (!own && i > 0) return;
        const s = toWorld(u, u.face, own ? (i - (sq.veh.length - 1) / 2) * 16 : 0, -u.d / 2 - 50);
        if (Math.hypot(s[0] - tr.slotAt[0], s[1] - tr.slotAt[1]) > 20) {
          tr.spot = s;
          tr.slotAt = s;
          tr.pose = "drive";
          tr.v = 4;
        } else if (Math.hypot(tr.x - tr.spot[0], tr.y - tr.spot[1]) < 2) {
          tr.pose = "parked";
          tr.v = 0;
        }
      });
      const firing = u.engaged !== void 0 && !!e;
      const direct = !!e && (b.gap(u, e) < LAND.guns.closeAt || e.kind === "arm" && b.gap(u, e) < 800);
      const hit = t - sq.shelledAt < DR.pin.shelled || sq.harm > DR.pin.harm;
      let fireNow = () => false;
      const ranging = this.byBook && firing && !direct && e ? this.mission(sq, guns, e, 1, N) : void 0;
      if (this.byBook && firing && !direct) fireNow = (g) => g.due !== void 0 && t >= g.due;
      else if (firing && t >= sq.salvoAt) {
        if (N.salvo === "battery") {
          sq.salvoAt = t + N.gunEvery;
          fireNow = () => true;
        } else if (N.salvo === "ripple") {
          const i0 = sq.ripple % Math.max(1, guns.length);
          sq.ripple++;
          sq.salvoAt = t + N.gunEvery / Math.max(1, guns.length);
          fireNow = (_g, i) => i === i0;
        } else {
          sq.salvoAt = t + 0.5;
          fireNow = (g) => g.shot === void 0 || t - g.shot >= N.gunEvery * (0.7 + 0.3 * roll(this.seed, u.id, g.n, Math.floor(t / 10)));
        }
      }
      guns.forEach((g, i) => {
        const [a, l] = this.slot(sq, g, "line");
        const s = toWorld(u, u.face, a, l);
        if (Math.hypot(s[0] - g.slotAt[0], s[1] - g.slotAt[1]) > 12) {
          this.go(sq, g, s, "towed", 3, t + 4, dir);
          g.aim = void 0;
          return;
        }
        if (Math.hypot(g.x - g.spot[0], g.y - g.spot[1]) > 1.5) return;
        g.v = 0;
        g.hidden = hit && roll(this.seed, u.id, g.n, Math.floor(t)) < 0.5 ? true : void 0;
        if (firing && fireNow(g, i) && !g.hidden) {
          g.shot = t;
          g.pose = "fire";
          g.due = void 0;
        } else if (g.pose !== "fire" || g.shot !== void 0 && t - g.shot > 2.5) g.pose = "deployed";
        g.aim = !e ? void 0 : ranging?.[0] === g ? ranging[1] : this.liftAim(sq, g, e, "gun");
        g.buttoned = direct || void 0;
      });
      this.spotter(sq, e, firing && !!e && b.gap(u, e) > DR.ranges.fo);
    }
    /**
     * A support team (release 2; Brian, 21 November 2026, 03:49Z: "attack from the back of the battle"): a machine-gun
     * team's guns on their tripods or a mortar team's tubes, each with its crew, carried by hand (no tractor). On the move
     * the crews carry them in file behind the block's middle; standing, they set up in a line, each in the best place near
     * its own. The machine guns fire on whatever their block fights, each gun its own long bursts (Drill.fire); the mortars
     * fire by the style's drill as a battery does, faster than field guns, always over the crest, and send a spotter up
     * with the foot when they fire far. Shelled, the crews go down a moment.
     */
    teams(sq) {
      const u = sq.u, b = this.b, t = b.t, N = this.nums(u), dir = this.threatDir(sq), h = sq.heading, T = DR.teams, mg = u.team === "mg";
      const e = u.aim !== void 0 ? b.unit(u.aim) ?? null : u.engaged !== void 0 ? b.unit(u.engaged) ?? null : null;
      const crews = this.up(sq).filter((m) => CREWED.has(m.role));
      if (sq.phase === "limbered" || sq.phase === "flee" || sq.phase === "fallback") {
        crews.forEach((g, i) => {
          g.aim = void 0;
          g.hidden = void 0;
          this.follow(sq, g, toWorld(u, h, i % 2 ? 1.5 : -1.5, -4 - i * T.file), sq.phase === "flee" ? "flee" : "walk");
        });
        this.spotter(sq, null, false);
        return;
      }
      const firing = u.engaged !== void 0 && !!e;
      const hit = t - sq.shelledAt < DR.pin.shelled || sq.harm > DR.pin.harm;
      let fireNow = () => false;
      const ranging = this.byBook && !mg && firing && e ? this.mission(sq, crews, e, BOOK.mission.mortar, N) : void 0;
      if (this.byBook && !mg && firing) fireNow = (g) => g.due !== void 0 && t >= g.due;
      else if (!mg && firing && t >= sq.salvoAt) {
        const every = N.gunEvery * T.mortar.every;
        if (N.salvo === "battery") {
          sq.salvoAt = t + every;
          fireNow = () => true;
        } else if (N.salvo === "ripple") {
          const i0 = sq.ripple % Math.max(1, crews.length);
          sq.ripple++;
          sq.salvoAt = t + every / Math.max(1, crews.length);
          fireNow = (_g, i) => i === i0;
        } else {
          sq.salvoAt = t + 0.5;
          fireNow = (g) => g.shot === void 0 || t - g.shot >= every * (0.7 + 0.3 * roll(this.seed, u.id, g.n, Math.floor(t / 10)));
        }
      }
      crews.forEach((g, i) => {
        const [a, l] = this.slot(sq, g, "line");
        const s = toWorld(u, u.face, a, l);
        if (Math.hypot(s[0] - g.slotAt[0], s[1] - g.slotAt[1]) > 12) {
          this.go(sq, g, s, "run", T.pace, t + 4, dir);
          g.aim = void 0;
          return;
        }
        if (Math.hypot(g.x - g.spot[0], g.y - g.spot[1]) > 1.5) return;
        g.v = 0;
        g.hidden = hit && roll(this.seed, u.id, g.n, Math.floor(t)) < 0.5 ? true : void 0;
        if (mg) g.pose = firing && !g.hidden ? "fire" : "deployed";
        else if (firing && fireNow(g, i) && !g.hidden) {
          g.shot = t;
          g.pose = "fire";
          g.due = void 0;
        } else if (g.pose !== "fire" || g.shot !== void 0 && t - g.shot > 2.5) g.pose = "deployed";
        g.aim = !e ? void 0 : ranging?.[0] === g ? ranging[1] : this.liftAim(sq, g, e, mg ? "mg" : "mortar");
      });
      this.spotter(sq, e, !mg && firing && !!e && b.gap(u, e) > DR.ranges.fo);
    }
    /**
     * A battery's or a mortar team's mission at block e (the drill book, tactics.ts missionStep): steps it, sets each
     * gun's time for the salvo called (all within a second or two; rippled down the line where its style ripples), and
     * gives the gun to fire a ranging round and where it lands (over or short of the target along the line of fire).
     * k: the times' share (a mortar team's quicker).
     */
    mission(sq, guns, e, k, N) {
      const u = sq.u, t = this.b.t, z = Math.round(t * 2);
      const [ms, shoot] = missionStep(sq.ms, e.id, t, k, (i) => roll(this.seed, u.id, z, i, 81));
      sq.ms = ms;
      const ready = guns.filter((g2) => !g2.hidden && Math.hypot(g2.x - g2.spot[0], g2.y - g2.spot[1]) <= 1.5);
      if (!shoot || !ready.length) return void 0;
      if (shoot === "salvo") {
        ready.forEach((g2, i) => {
          g2.due = t + (N.salvo === "ripple" ? i * 0.5 : 1.5 * roll(this.seed, u.id, g2.n, z, 82));
        });
        return void 0;
      }
      const g = ready[0], dx = e.x - g.x, dy = e.y - g.y, d = Math.hypot(dx, dy) || 1, o = (shoot === "over" ? 1 : -1) * (e.d / 2 + BOOK.mission.range);
      g.due = t;
      return [g, [clamp2(e.x + dx / d * o, 1, this.b.field.w - 1), clamp2(e.y + dy / d * o, 1, this.b.field.h - 1)]];
    }
    /**
     * The battery's spotter and his signaller: with the friendly foot block nearest the guns' target, on the highest
     * ground or the tallest house within its stretch, lying with his glasses on the target. Where no friendly foot is near
     * the target, at the battery's own front, on its highest ground. Out once the guns first fire far, the pair stays out
     * to the end: when the shoot moves to another block, or its host goes on, they go up to the new place (running, the
     * last 30 m crawling) and are never made anew; while the guns are quiet they lie where they are, watching.
     */
    spotter(sq, e, on) {
      const u = sq.u, b = this.b, t = b.t, f = b.field;
      if (!on || !e) {
        sq.foOn = void 0;
        if (sq.fo) {
          for (const m of sq.fo) if (m.v === 0) {
            m.aim = void 0;
            m.pose = m.k === 200 ? "observe" : "wait";
          }
        }
        return;
      }
      const friends = b.units.filter((x) => x.side === u.side && fighting(x) && FOOT_KINDS.includes(x.kind) && b.gap(x, e) < 900);
      let host = friends.reduce((m, x) => !m || b.gap(x, e) < b.gap(m, e) ? x : m, null);
      const cur = friends.find((x) => x.id === sq.foWith);
      if (cur && host && b.gap(cur, e) < b.gap(host, e) + 100) host = cur;
      const base = host ?? u;
      const gone = sq.foAt ? Math.hypot(base.x - sq.foAt[0], base.y - sq.foAt[1]) : Infinity;
      if (!sq.fo || sq.foWith !== base.id || sq.foOn !== e.id || gone > 60) {
        const taken = [...this.squads.values()].filter((o) => o !== sq && o.fo && o.u.side === u.side).map((o) => o.fo[0].slotAt);
        let best = [base.x, base.y], score = -Infinity;
        for (let i = 0; i < 25; i++) {
          const a = (i % 5 - 2) * base.w / 5, l = (Math.floor(i / 5) - 2) * base.d / 3;
          const p = toWorld(base, base.face, a, l);
          const g = groundOf(f, p[0], p[1]);
          if (LAND.pace.foot[g] <= 0 || g === "water" || g === "blown") continue;
          if (taken.some((q) => Math.hypot(q[0] - p[0], q[1] - p[1]) < 12)) continue;
          const sc = heightOf(f, p[0], p[1]) + storeysOf(f, p[0], p[1]) * 3 + (g === "woods" ? 1 : 0) - Math.hypot(p[0] - e.x, p[1] - e.y) * 2e-3;
          if (sc > score) {
            score = sc;
            best = p;
          }
        }
        const face = bearing2(best[0], best[1], e.x, e.y), [rx, ry] = right(face);
        const at = (dx) => [best[0] + rx * dx, best[1] + ry * dx];
        if (!sq.fo) {
          const mk = (k, dx) => {
            const s = at(dx);
            return { k, n: k, role: "fo", team: 0, x: s[0] - Math.sin(face) * 30, y: s[1] - Math.cos(face) * 30, face, pose: "creep", v: 2, spot: s, cover: "none", until: t + 1, slotAt: best, jx: 0, jy: 0 };
          };
          sq.fo = [mk(200, 0), mk(201, 2.5)];
        } else if (Math.hypot(best[0] - sq.fo[0].slotAt[0], best[1] - sq.fo[0].slotAt[1]) > 15) {
          sq.fo.forEach((m, i) => {
            m.spot = at(i * 2.5);
            m.slotAt = best;
            m.v = 2;
          });
        }
        for (const m of sq.fo) m.cover = this.shelter(m.spot[0], m.spot[1], groundOf(f, m.spot[0], m.spot[1]), [Math.sin(face), Math.cos(face)], true, false).kind;
        sq.foWith = base.id;
        sq.foOn = e.id;
        sq.foAt = [base.x, base.y];
      }
      if (this.byBook && sq.ms && sq.ms.on === e.id && sq.fo[1].call !== sq.ms.call) {
        sq.fo[1].call = sq.ms.call;
        sq.fo[1].callAt = [e.x, e.y];
      }
      for (const m of sq.fo) {
        const d = Math.hypot(m.x - m.spot[0], m.y - m.spot[1]);
        if (d < 1 && m.v === 0) {
          m.pose = m.k === 200 ? "observe" : "wait";
          m.aim = [e.x, e.y];
        } else if (d > 30) {
          m.pose = "run";
          m.v = 6;
          m.aim = void 0;
        } else if (d >= 1) {
          m.pose = "creep";
          m.v = 2;
          m.aim = void 0;
        }
      }
    }
    // ---- Moving each man a step ---------------------------------------------------------------------------------------
    walk(sq) {
      const u = sq.u, f = this.b.field, t = this.b.t;
      let e;
      const crowd = sq.tier >= 1 ? /* @__PURE__ */ new Map() : null;
      if (crowd) for (const m of sq.men) {
        if (m.fell !== void 0 || m.job || VEHICLE.has(m.role) || CARRIED.has(m.pose)) continue;
        const key = Math.floor(m.x / SPACE.cell) * 65536 + Math.floor(m.y / SPACE.cell);
        let l = crowd.get(key);
        if (!l) crowd.set(key, l = []);
        l.push(m);
      }
      const step = (m) => {
        if (m.fell !== void 0) {
          m.went = 0;
          return;
        }
        const x0 = m.x, y0 = m.y;
        walkOne(m);
        m.went = Math.hypot(m.x - x0, m.y - y0) / DT;
        if (m.role === "tank") m.way = Math.max(m.went, (m.way ?? 0) - TANK_SHED * DT);
      };
      const walkOne = (m) => {
        if (m.v > 0 && !MOVING.has(m.pose) && !CARRIED.has(m.pose)) {
          m.v = 0;
          m.spot = [m.x, m.y];
        }
        if (m.role === "tank") m.v = Math.min(m.v, tankTop(sq));
        const veh = m.role === "tank" || m.role === "lorry" || m.role === "gun";
        const rate = (m.role === "tank" ? TURN.tank : veh ? TURN.wheels : m.pose === "ride" ? TURN.ride : TURN.foot) * DT;
        const via = !veh && MOVING.has(m.pose) && this.b.gates.length ? this.via(sq, m) : null;
        const dx = (via ? via[0] : m.spot[0]) - m.x, dy = (via ? via[1] : m.spot[1]) - m.y, d = Math.hypot(dx, dy);
        if (m.v > 0 && (d > 0.05 || MOVING.has(m.pose))) {
          let s = Math.min(d, m.v * DT);
          if (d > 0.3) {
            const way = Math.atan2(dx, dy);
            if (m.role === "tank" && m.pose === "drive" && d < 30 && Math.abs(wrap(way - m.face)) > 1.9) {
              m.pose = "reverse";
              m.v = Math.min(m.v, 4);
              s = Math.min(d, m.v * DT);
            }
            const want = way + (m.pose === "reverse" ? Math.PI : 0), f0 = m.face;
            m.face = turnTo(m.face, want, rate);
            const c = Math.cos(wrap(want - m.face));
            s = Math.min(d, m.v * DT * (c <= 0 ? 0 : veh ? Math.max(c, 0.15) : c));
            if (m.role === "lorry") s = Math.min(s, Math.max(0, 16 * DT - 0.85 * Math.abs(wrap(m.face - f0))));
          }
          if (m.role === "tank") s = Math.min(s, ((m.way ?? 0) + TANK_GAIN * DT) * DT);
          if (d > 0) {
            let ux = dx / d, uy = dy / d;
            if (crowd && !veh && s > 0.02 && s < d - 1e-9 && MOVING.has(m.pose)) {
              let rx = 0, ry = 0, slow = 1;
              const cx = Math.floor(m.x / SPACE.cell), cy = Math.floor(m.y / SPACE.cell);
              for (let i = -1; i <= 1; i++) for (let j = -1; j <= 1; j++) for (const q of crowd.get((cx + i) * 65536 + cy + j) ?? []) {
                if (q === m || q.fell !== void 0) continue;
                const ox = m.x - q.x, oy = m.y - q.y, od = Math.hypot(ox, oy);
                if (od >= SPACE.men || ox * ux + oy * uy > 0.5 * od) continue;
                const w = (SPACE.men - od) / SPACE.men, along = od > 1e-6 ? (ox * ux + oy * uy) / od : -1;
                let px = od > 1e-6 ? ox / od - along * ux : 0, py = od > 1e-6 ? oy / od - along * uy : 0;
                const pl = Math.hypot(px, py);
                if (pl < 0.2) {
                  const side = roll(this.seed, u.id, m.n, 59) < 0.5 ? 1 : -1;
                  px = uy * side;
                  py = -ux * side;
                } else {
                  px /= pl;
                  py /= pl;
                }
                rx += px * w;
                ry += py * w;
                const mj = m.j ?? 0, qj = q.j ?? 0;
                if (ox * ux + oy * uy < 0 && (mj > qj || mj === qj && m.n > q.n)) slow = Math.min(slow, Math.max(0.35, od / SPACE.men));
              }
              s *= slow;
              if (rx || ry) {
                let bx = ux + rx * 1.5, by2 = uy + ry * 1.5;
                const bl = Math.hypot(bx, by2) || 1;
                bx /= bl;
                by2 /= bl;
                const turnBy = Math.atan2(ux * by2 - uy * bx, ux * bx + uy * by2), lim = Math.PI / 6, a = clamp2(turnBy, -lim, lim);
                const ca = Math.cos(a), sa = Math.sin(a);
                [ux, uy] = [ux * ca - uy * sa, ux * sa + uy * ca];
              }
            }
            const nx = m.x + ux * s, ny = m.y + uy * s;
            m.x = clamp2(nx, 1, f.w - 1);
            m.y = clamp2(ny, 1, f.h - 1);
            if (s > 0.05 && Math.hypot(nx - m.x, ny - m.y) > 0.8 * s && MOVING.has(m.pose)) {
              m.spot = [m.x, m.y];
              this.arrive(sq, m);
            }
          }
          if (s >= d - 1e-9 && !via && MOVING.has(m.pose) && m.role !== "lorry" && sq.phase !== "column" && sq.phase !== "limbered" && sq.boarding === void 0) this.arrive(sq, m);
        } else {
          let want;
          if (m.aim) want = bearing2(m.x, m.y, m.aim[0], m.aim[1]);
          else if (m.v === 0 && fighting(u)) {
            if (e === void 0) e = this.threat(u);
            if (e) want = bearing2(m.x, m.y, e.x, e.y);
          }
          if (want !== void 0) m.face = turnTo(m.face, want, rate);
        }
        const extra = m.role === "lorry" ? 120 : 0;
        const [lx, ly] = this.leash(sq, [m.x, m.y], extra);
        const gd = Math.hypot(lx - m.x, ly - m.y);
        if (gd === 0) return;
        const fast = m.role === "tank" ? tankTop(sq) : veh ? 16 : m.pose === "ride" || m.pose === "mounted" ? 14 : CATCH_UP, need2 = Math.min(fast, Math.max(3, sq.speed * 1.25));
        const own = m.role !== "holder" && m.pose !== "ride" && m.pose !== "mounted" && sq.boarding === void 0;
        if (own && gd > 2 && m.role !== "lorry") {
          const [a, l] = this.slot(sq, m, sq.phase === "rest" ? "rest" : "line");
          m.slotAt = toWorld(u, u.face, a, l);
          m.spot = this.leash(sq, m.slotAt, -4);
        } else if (sq.boarding === void 0) m.spot = this.leash(sq, m.spot, extra ? extra - 4 : -4);
        const back = (p) => m.role === "tank" && Math.hypot(p[0] - m.x, p[1] - m.y) < 30 && Math.abs(wrap(Math.atan2(p[0] - m.x, p[1] - m.y) - m.face)) > 1.9;
        if (own && (!MOVING.has(m.pose) ? gd > 0.05 : gd > 2 && m.v < (m.pose === "reverse" ? Math.min(need2, 4) : need2))) {
          if (gd <= 2 || m.role === "lorry") m.spot = this.leash(sq, m.slotAt, extra ? extra - 4 : -4);
          m.aim = void 0;
          m.hidden = void 0;
          const rev = back(m.spot);
          m.v = rev ? Math.min(need2, 4) : need2;
          m.pose = rev ? "reverse" : m.role === "tank" || m.role === "lorry" ? "drive" : m.role === "gun" ? "towed" : sq.phase === "flee" ? "flee" : sq.phase === "bound" || sq.phase === "fight" ? "advance" : "run";
          m.until = t + 1;
        }
        this.ring(sq, m, extra || (u.kind === "mot" || u.kind === "cav" ? RIDERS_RING : 0));
      };
      for (const m of sq.veh) step(m);
      for (const m of sq.men) {
        if (m.fell !== void 0) {
          m.went = 0;
          continue;
        }
        if (m.job) continue;
        if (m.pose === "mounted" && (u.kind === "mot" || m.role === "crew") && sq.veh.length) {
          const lorry = this.lorryOf(sq, m), x0 = m.x, y0 = m.y, p = this.seat(sq, m);
          m.x = p[0];
          m.y = p[1];
          m.face = turnTo(m.face, lorry.face, TURN.foot * DT);
          m.went = Math.hypot(m.x - x0, m.y - y0) / DT;
          m.spot = p;
          m.v = lorry.v;
          continue;
        }
        if (sq.boarding !== void 0 && m.role !== "holder" && !CARRIED.has(m.pose) && this.stands(sq, m)) {
          const at = this.mountAt(sq, m);
          if (at) {
            const x0 = m.x, y0 = m.y;
            m.spot = at;
            step(m);
            if (Math.hypot(at[0] - m.x, at[1] - m.y) < (u.kind === "mot" ? MOUNT.climb : MOUNT.saddle)) {
              if (this.mayMount(sq, m)) {
                m.pose = u.kind === "mot" ? "mounted" : "ride";
                m.v = 0;
                m.aim = void 0;
                m.cover = "none";
                if (u.kind === "mot") {
                  m.x = at[0];
                  m.y = at[1];
                  m.went = Math.hypot(m.x - x0, m.y - y0) / DT;
                }
                m.spot = [m.x, m.y];
              } else if ((m.went ?? 0) < 0.5 && MOVING.has(m.pose)) {
                m.pose = "wait";
                m.v = 0;
                m.spot = [m.x, m.y];
              }
            }
            continue;
          }
        }
        step(m);
      }
      if (sq.fo) for (const m of sq.fo) {
        const dx = m.spot[0] - m.x, dy = m.spot[1] - m.y, d = Math.hypot(dx, dy);
        m.went = 0;
        if (m.v > 0) {
          let s = Math.min(d, m.v * DT);
          if (d > 0.3) {
            const want = Math.atan2(dx, dy);
            m.face = turnTo(m.face, want, TURN.foot * DT);
            s *= Math.max(0, Math.cos(wrap(want - m.face)));
          }
          if (d > 0) {
            m.x += dx / d * s;
            m.y += dy / d * s;
            m.went = s / DT;
          }
          if (s >= d - 1e-9) {
            m.v = 0;
            m.pose = m.k === 200 ? "observe" : "wait";
            m.until = t + 1;
          }
        } else if (m.aim) m.face = turnTo(m.face, bearing2(m.x, m.y, m.aim[0], m.aim[1]), TURN.foot * DT);
      }
    }
    /** The ring round the block's box: the routing block's (its men scatter wider), so when it rallies they come in on their feet. */
    ring(sq, m, extra) {
      const u = sq.u, mrg = 70 + extra, R = Math.hypot(u.w / 2 + mrg, u.d / 2 + mrg), cx = m.x - u.x, cy = m.y - u.y, r = Math.hypot(cx, cy);
      if (r > R) {
        m.x = u.x + cx * R / r;
        m.y = u.y + cy * R / r;
      }
    }
    // ---- What the 3D game reads ---------------------------------------------------------------------------------------
    /**
     * Every block's men, as one side sees them: an enemy block not yet identified (fog, b464(2)) sends none, so the 3D
     * game draws its plain stand. Fallen men stay to the end of the fight where they fell.
     */
    // ---- Rounds: every shot with its odds, landing where it lands ----------------------------------------------------
    /**
     * The rounds fired in the last few seconds (DR.rounds.keep), oldest first, each numbered n in the order fired (read
     * those past the last n you took). As a side sees them where one is given: rounds at an enemy block it does not know
     * are left out, and an unseen shooter is not named.
     */
    rounds(as) {
      const b = this.b, out = [];
      for (const r of this.shots) {
        const on = b.unit(r.on), by2 = r.by !== void 0 ? b.unit(r.by) : void 0;
        if (as !== void 0 && on && on.side !== as && !b.known(on)) continue;
        const o = { ...r };
        if (as !== void 0 && by2 && by2.side !== as && !b.known(by2)) {
          delete o.by;
          delete o.k;
          delete o.j;
        }
        out.push(o);
      }
      return out;
    }
    /**
     * Each shot of this step (a tank's, a gun's, an anti-tank round, a grenade: a new stamp) becomes a round; and each
     * rifleman and machine gun in a firing pose fires his next shot, burst by burst.
     */
    fire(sq) {
      const u = sq.u, t = this.b.t, R = DR.rounds;
      if (!live(u)) return;
      for (const m of sq.veh.length ? [...sq.men, ...sq.veh] : sq.men) {
        if (m.k < 100 ? !this.stands(sq, m) : m.fell !== void 0) continue;
        if (m.shot !== void 0 && m.shot !== m.fired) {
          m.fired = m.shot;
          const w = m.role === "tank" ? "tank" : m.role === "gun" ? "gun" : m.role === "mortar" ? "mortar" : m.pose === "throw_grenade" ? "grenade" : m.role === "at" ? "at" : void 0;
          if (w && m.aim && m.shot > t - 0.05) this.round(sq, m, w, m.aim);
        }
        const hmg = m.role === "hmg" && m.pose === "fire", coax = m.role === "tank" && !!m.coax && m.pose !== "fire";
        if (m.aim && (hmg || coax || SMALL_ARMS.has(m.pose) && m.role !== "at" && !VEHICLE.has(m.role))) {
          const w = m.role === "mg" || hmg || coax ? "mg" : "rifle", [lo, hi] = hmg ? DR.teams.hmg.burst : R.smallArms[w];
          const k = (coax ? 2 : 1) * (m.rapid !== void 0 && t < m.rapid ? BOOK.rapid : 1);
          if (m.burst === void 0) m.burst = t + k * hi * roll(this.seed, u.id, m.n, t * 10, 41);
          else if (t >= m.burst) {
            m.burst = t + k * (lo + (hi - lo) * roll(this.seed, u.id, m.n, t * 10, 42));
            this.round(sq, m, w, m.aim);
          }
        } else m.burst = void 0;
      }
    }
    /** The enemy block a point is in (or nearest, within 60 m of its ground). */
    blockAt(side, p) {
      let on, gap = 60;
      for (const e of this.b.units) {
        if (e.side === side || !live(e)) continue;
        const g = Math.max(0, Math.hypot(e.x - p[0], e.y - p[1]) - Math.max(e.w, e.d) / 2);
        if (g < gap) {
          gap = g;
          on = e;
        }
      }
      return on;
    }
    /** A block's models that can be struck: its men and tanks and guns standing, and its lorries. */
    standing(sq) {
      return this.memo(sq).struck;
    }
    /** Where a model will be dt battle seconds on, going on as it goes now (a round lands at Round.l). */
    ahead(v, dt) {
      const go = (v.went ?? 0) * dt * (v.pose === "reverse" ? -1 : 1);
      if (go === 0) return [v.x, v.y];
      const [fx, fy] = fwd(v.face);
      return [clamp2(v.x + fx * go, 1, this.b.field.w - 1), clamp2(v.y + fy * go, 1, this.b.field.h - 1)];
    }
    /** Whether a point lies within r of a vehicle standing anywhere on the field (where it will be in dt), or of a wreck. */
    onVehicle(x, y, r, dt) {
      for (const sq of this.squads.values()) {
        const u = sq.u;
        if (Math.hypot(u.x - x, u.y - y) > Math.max(u.w, u.d) / 2 + 80) continue;
        for (const q of this.standing(sq)) {
          if (!VEHICLE.has(q.role)) continue;
          const [qx, qy] = this.ahead(q, dt);
          if (Math.hypot(qx - x, qy - y) < r) return true;
        }
      }
      for (const z of this.b.wrecks) {
        const [zx, zy] = this.wreckSpot(z);
        if (Math.hypot(zx - x, zy - y) < r) return true;
      }
      return false;
    }
    /** The face of a vehicle toward a point: its front, its left or right side, or its rear. */
    faceOf(v, x, y) {
      const rel = wrap(bearing2(v.x, v.y, x, y) - v.face), a = Math.abs(rel);
      return a <= Math.PI / 4 ? "front" : a >= 3 * Math.PI / 4 ? "rear" : rel > 0 ? "right" : "left";
    }
    /** Where on a face a round strikes: [across, height], a tank's turret DR.rounds.turret of the time. */
    spotOn(v, ...ns) {
      const a = roll(this.seed, ...ns, 51), c = roll(this.seed, ...ns, 52), up = roll(this.seed, ...ns, 53);
      const h = v.role === "tank" ? up < DR.rounds.turret ? 0.7 + 0.25 * c : 0.15 + 0.45 * c : v.role === "lorry" ? 0.2 + 0.6 * c : 0.2 + 0.5 * c;
      return [Math.round((a - 0.5) * 160) / 100, Math.round(h * 100) / 100];
    }
    /** Puts a round in the list, and its scar on what it struck (kept to the end, the first DR.rounds.marks of them). */
    book(r, v, mark) {
      this.shots.push({ ...r, n: ++this.fired });
      if (v && mark && r.fc && r.u !== void 0 && r.h !== void 0) {
        const mk = v.marks ?? (v.marks = []);
        if (mk.length < DR.rounds.marks) mk.push([r.fc, r.u, r.h, mark, Math.round(r.l * 10) / 10]);
      }
    }
    /**
     * One round from a man, a tank or a gun at a point: what it is fired at (the enemy model nearest the point; for a
     * tank's, a launcher's or a gun's, a vehicle where one is near), whether it strikes (DR.rounds: the odds by range, a
     * moving shooter or target, cover, the crew), and where it lands. It never kills: only the rules take a model
     * (killRound). A man is never struck short of that: a round at men lands near.
     */
    round(sq, m, w, aim) {
      const u = sq.u, t = this.b.t, R = DR.rounds;
      const on = this.blockAt(u.side, aim), tq = on && this.squads.get(on.id);
      if (!on || !tq) return;
      const alive = this.standing(tq);
      if (!alive.length) return;
      const heavy = w === "tank" || w === "at" || w === "gun" || w === "mortar";
      const pool = heavy && alive.some((x2) => VEHICLE.has(x2.role) && Math.hypot(x2.x - aim[0], x2.y - aim[1]) < 60) ? alive.filter((x2) => VEHICLE.has(x2.role)) : alive;
      let v = pool[0], dd = Infinity;
      for (const x2 of pool) {
        const d = Math.hypot(x2.x - aim[0], x2.y - aim[1]);
        if (d < dd) {
          dd = d;
          v = x2;
        }
      }
      const range = Math.hypot(v.x - m.x, v.y - m.y);
      if (range > (w === "mg" || w === "rifle" ? 900 : 4e3)) return;
      const l = Math.round((t + range / R.speed[w]) * 10) / 10, k0 = Math.round(t * 10);
      const base = { t: Math.round(t * 10) / 10, l, by: u.id, k: m.k, w, on: on.id, x: 0, y: 0, r: "miss" };
      if (m.j !== void 0) base.j = m.j;
      if (w === "mg" && m.role === "tank") base.g = this.tankGun(u, m, aim, k0);
      const [lx, ly] = range > 0.01 ? [(v.x - m.x) / range, (v.y - m.y) / range] : fwd(m.face);
      const fly = l - t;
      if (w === "gun" && !m.buttoned || w === "mortar") {
        const [i0, i1] = w === "mortar" ? R.indirect.mortar : R.indirect.sigma;
        const sg = i0 + i1 * Math.hypot(aim[0] - m.x, aim[1] - m.y);
        const ea2 = gauss(roll(this.seed, u.id, m.n, k0, 61), roll(this.seed, u.id, m.n, k0, 62)) * sg, ec2 = gauss(roll(this.seed, u.id, m.n, k0, 63), roll(this.seed, u.id, m.n, k0, 64)) * sg * 0.4;
        let x2 = clamp2(aim[0] + lx * ea2 + ly * ec2, 1, this.b.field.w - 1), y2 = clamp2(aim[1] + ly * ea2 - lx * ec2, 1, this.b.field.h - 1);
        const hitV = alive.find((q) => {
          if (q.role !== "tank") return false;
          const [qx, qy] = this.ahead(q, fly);
          return Math.hypot(qx - x2, qy - y2) < R.indirect.onVehicle;
        });
        if (hitV) {
          const [a, h] = this.spotOn(hitV, u.id, m.n, k0), [hx, hy] = this.ahead(hitV, fly);
          const glance = roll(this.seed, u.id, m.n, k0, 65) < R.glance.top;
          this.duck(hx, hy);
          const cm = this.commanderHit(hitV, BOOK.tankMg.splinter, u.id, m.n, k0);
          this.book({ ...base, ok: hitV.k, x: Math.round(hx * 10) / 10, y: Math.round(hy * 10) / 10, r: glance ? "glance" : "worn", fc: "top", u: a, h, ...cm ? { cm: 1 } : {} }, hitV, "scorch");
          return;
        }
        for (let i = 0; i < 6 && this.onVehicle(x2, y2, R.indirect.onVehicle, fly); i++) {
          x2 = clamp2(x2 + lx * 3, 1, this.b.field.w - 1);
          y2 = clamp2(y2 + ly * 3, 1, this.b.field.h - 1);
        }
        this.duck(x2, y2);
        this.book({ ...base, x: Math.round(x2 * 10) / 10, y: Math.round(y2 * 10) / 10 });
        return;
      }
      const isV = VEHICLE.has(v.role);
      const small = w === "mg" || w === "rifle";
      const may = isV && (v.role === "tank" || small);
      let p = 0;
      if (may) {
        const [best, half] = R.odds[w];
        p = best / (1 + (range / half) ** 2);
        if ((m.went ?? 0) > 0.5) p *= R.shooterMoving;
        if ((v.went ?? 0) > 0.5) p *= R.targetMoving;
        p *= R.cover[v.cover] ?? 1;
        p *= R.xp[0] + R.xp[1] * clamp2(u.xp, 0, 1);
      }
      if (may && roll(this.seed, u.id, m.n, k0, 66) < p) {
        const fc = this.faceOf(v, m.x, m.y), [a, h] = this.spotOn(v, u.id, m.n, k0), [vx2, vy2] = this.ahead(v, fly);
        const at = { ...base, ok: v.k, x: Math.round(vx2 * 10) / 10, y: Math.round(vy2 * 10) / 10, fc, u: a, h };
        if (v.role === "tank" && small) this.book({ ...at, r: "glance", ...this.commanderHit(v, BOOK.tankMg.commander, u.id, m.n, k0) ? { cm: 1 } : {} });
        else if (small) this.book({ ...at, r: v.role === "lorry" ? "worn" : "glance" }, v, "pocks");
        else if (w === "grenade") this.book({ ...at, r: "glance" }, v, "scorch");
        else {
          const glance = roll(this.seed, u.id, m.n, k0, 67) < R.glance[fc];
          this.book({ ...at, r: glance ? "glance" : "worn" }, v, glance ? "gouge" : "hole");
        }
        return;
      }
      const [s0, s1, c0, c1] = R.spread[w];
      let ea = gauss(roll(this.seed, u.id, m.n, k0, 68), roll(this.seed, u.id, m.n, k0, 69)) * (s0 + s1 * range);
      let ec = gauss(roll(this.seed, u.id, m.n, k0, 70), roll(this.seed, u.id, m.n, k0, 71)) * (c0 + c1 * range);
      const least = isV ? 2.5 : 0.6, off = Math.hypot(ea, ec);
      if (off < least) {
        const g = off > 1e-6 ? least / off : 1;
        ea = off > 1e-6 ? ea * g : least;
        ec *= g;
      }
      const [vx, vy] = this.ahead(v, fly);
      let x = clamp2(vx + lx * ea + ly * ec, 1, this.b.field.w - 1), y = clamp2(vy + ly * ea - lx * ec, 1, this.b.field.h - 1);
      for (let i = 0; i < 6 && this.onVehicle(x, y, 2.5, fly); i++) {
        x = clamp2(x + lx * 3, 1, this.b.field.w - 1);
        y = clamp2(y + ly * 3, 1, this.b.field.h - 1);
      }
      if (heavy) this.duck(x, y);
      this.book({ ...base, x: Math.round(x * 10) / 10, y: Math.round(y * 10) / 10 });
    }
    /**
     * Whether the man (model k, or a section's man as the block's count of losses) a tank block took off the enemy's
     * foot went to her machine guns, not her main gun's shells (BOOK.tankMg.kill: more in the open). The drill book only;
     * fixed by the blocks and the count, never the battle's dice. Looks only: the rules took him either way.
     */
    mgKill(by2, t, k) {
      if (!this.byBook || by2.kind !== "arm" || !FOOT_KINDS.includes(t.kind) || t.kind === "fort") return false;
      const close = CLOSE2.includes(groundOf(this.b.field, t.x, t.y)) || t.dug;
      return roll(this.seed, by2.id, t.id, k, 97) < (close ? BOOK.tankMg.kill.cover : BOOK.tankMg.kill.open);
    }
    /**
     * Which of a tank's machine guns fires this burst (BOOK.tankMg): the commander's on the roof only in the armies that
     * carried one, only with him up in his hatch, a share of the time; the hull gunner's at what is before her bow; else
     * the coaxial beside the main gun.
     */
    tankGun(u, m, at, n) {
      const T = BOOK.tankMg;
      if (!m.buttoned && m.cmdDown === void 0 && T.roof.includes(u.nat) && roll(this.seed, u.id, m.n, n, 98) < T.roofShare) return "roof";
      const off = Math.abs(wrap(bearing2(m.x, m.y, at[0], at[1]) - m.face)) * (180 / Math.PI);
      return off <= T.hullArc ? "hull" : "coax";
    }
    /** A round striking a tank strikes her commander, up in his hatch, this share of the time: he drops inside, and she buttons up. */
    commanderHit(v, odds, ...ns) {
      if (v.role !== "tank" || v.buttoned || v.cmdDown !== void 0 || roll(this.seed, ...ns, 99) >= odds) return false;
      v.cmdDown = this.b.t;
      v.buttoned = true;
      return true;
    }
    /**
     * The round that cost a model the rules took: from the striking block's tank or gun (a shell, Drill.shelled, landing
     * at p) or, on foot, its man nearest who could have fired it (on a tank: its anti-tank man, else a grenade). It is
     * the hit, landing as it is fired; on a tank it leaves the hole it went in by.
     */
    killRound(by2, vq, v, p, mate = false, crush) {
      const t = this.b.t, tq = by2 ? this.squads.get(by2.id) : void 0;
      const at = p ?? [v.x, v.y];
      let s, w = p ? by2?.kind === "arm" ? "tank" : by2?.team === "mor" ? "mortar" : "gun" : "rifle";
      if (tq) {
        const shells = (x) => x.role === "tank" || x.role === "gun" || x.role === "mortar";
        const mg = !p && by2?.kind === "arm";
        const can = this.standing(tq).filter((x) => x.role !== "lorry" && x.role !== "holder" && (mg ? x.role === "tank" : p ? shells(x) : !shells(x)));
        const pref = v.role === "tank" && !p && by2?.team !== "mg" ? can.filter((x) => x.role === "at") : can.filter((x) => SMALL_ARMS.has(x.pose) || shells(x) || x.role === "hmg");
        const from = pref.length ? pref : can;
        let dd = Infinity;
        for (const x of from) {
          const d = Math.hypot(x.x - at[0], x.y - at[1]);
          if (d < dd) {
            dd = d;
            s = x;
          }
        }
      }
      if (!p && s) w = s.role === "hmg" || s.role === "tank" ? "mg" : v.role === "tank" ? s.role === "at" ? "at" : "grenade" : s.role === "mg" ? "mg" : "rifle";
      if (crush) {
        s = crush;
        w = "track";
      }
      const r = { t: Math.round(t * 10) / 10, l: Math.round(t * 10) / 10, w, on: vq.u.id, ok: v.k, x: Math.round(at[0] * 10) / 10, y: Math.round(at[1] * 10) / 10, r: "kill" };
      if (v.j !== void 0) r.oj = v.j;
      if (by2) r.by = by2.id;
      if (mate) r.m = 1;
      if (s) {
        r.k = s.k;
        if (s.j !== void 0) r.j = s.j;
        if (w === "mg" && s.role === "tank" && by2) r.g = this.tankGun(by2, s, at, Math.round(t * 10));
        if (w !== "rifle" && w !== "mg" && w !== "track" && !mate) {
          s.shot = t;
          s.fired = t;
          s.aim = [v.x, v.y];
          if (s.role === "gun" || s.role === "mortar") s.pose = "fire";
        }
      }
      if (VEHICLE.has(v.role)) {
        const fc = w === "gun" || w === "mortar" ? "top" : s ? this.faceOf(v, s.x, s.y) : "front", [a, h] = this.spotOn(v, vq.u.id, v.n, Math.round(t * 10), 9);
        if (v.role === "tank") r.fate = fateOf(vq.esc ? null : this.b.vehicle(vq.u), fc === "front" ? "front" : fc === "rear" ? "rear" : "side", roll(this.seed, vq.u.id, v.n, 96), roll(this.seed, vq.u.id, v.n, 95));
        this.book({ ...r, fc, u: a, h }, v, "hole");
      } else this.book(r);
    }
    view(as) {
      const b = this.b, out = [];
      const r13 = (v) => Math.round(v * 10) / 10, r22 = (v) => Math.round(v * 100) / 100;
      const one = (m) => {
        const o = { k: m.k, r: m.hasSec && m.role === "rifle" || m.lead ? "leader" : m.role, x: r13(m.x), y: r13(m.y), f: r22(wrap(m.face)), p: m.pose, v: MOVING.has(m.pose) || CARRIED.has(m.pose) ? r13(m.went ?? 0) : 0, c: m.cover };
        if (m.j !== void 0) o.j = m.j;
        if (m.aim) o.a = [r13(m.aim[0]), r13(m.aim[1])];
        if (m.shot !== void 0) o.s = r13(m.shot);
        if (m.fell !== void 0) o.d = r13(m.fell);
        if (m.hidden) o.h = 1;
        if (m.buttoned) o.b = 1;
        if (m.marks?.length) o.mk = m.marks.map((x) => [...x]);
        if (m.el && this.byBook) o.e = m.el;
        if (m.call !== void 0) o.cl = r13(m.call);
        if (m.wd !== void 0) o.wd = r13(m.wd);
        if (m.cy) o.cy = m.cy.map((c) => [r13(c[0]), r13(c[1]), c[2], c[3]]);
        if (m.gone !== void 0) o.gone = r13(m.gone);
        if (m.back !== void 0) o.back = r13(m.back);
        if (m.br) o.br = 1;
        if (m.call !== void 0 && m.callAt) o.cs = [r13(m.callAt[0]), r13(m.callAt[1])];
        if (m.sw) o.sw = { ...m.sw };
        if (m.ml) {
          o.ml = m.ml;
          if (m.role === "gun") {
            o.r = "tank";
            o.p = m.pose === "towed" ? o.v > 0 ? "drive" : "halt" : m.pose === "deployed" || m.pose === "parked" || m.pose === "idle" ? "halt" : m.pose;
            delete o.h;
          }
        }
        return o;
      };
      const byId = /* @__PURE__ */ new Map();
      for (const sq of this.squads.values()) {
        const u = sq.u;
        const lead = this.leads.get(u.side), mark = lead && lead[0] === u.id ? lead[1] : void 0;
        if (as !== void 0 && u.side !== as && !b.known(u)) continue;
        if (u.state === "withdrawn" && !sq.men.some((m) => m.fell !== void 0)) continue;
        const men = sq.men.filter((m) => m.fell !== void 0 || m.job !== void 0 || this.stands(sq, m) && live(u)).map((m) => {
          const o = one(m);
          if (m.pose === "mounted" && m.fell === void 0 && sq.veh.length) o.sv = [this.lorryOf(sq, m).k, this.seatNo(sq, m)];
          if (mark !== void 0 && m.fell === void 0 && grp(m) === mark && m.role !== "lorry" && m.role !== "holder") o.ld = 1;
          return o;
        });
        for (const v of sq.veh) if (live(u)) men.push(one(v));
        if (sq.of !== void 0) {
          const own = byId.get(sq.of);
          if (own) own.men.push(...men);
          continue;
        }
        const blk = { id: u.id, ph: sq.phase, men };
        byId.set(u.id, blk);
        if (sq.fo && live(u)) {
          blk.fo = sq.fo.map(one);
          blk.foWith = sq.foWith;
          blk.foOn = sq.foOn;
        }
        if (b.phones && b.phonesSeen(as)) {
          const ln = this.linemen(u.id);
          if (ln.length) blk.ln = ln;
        }
        out.push(blk);
      }
      return out;
    }
    /**
     * Field telephone lines (release 2; LandBattle.phones): the linemen out on this block's wire now, one for each mend.
     * He leaves the headquarters at the mend's t0 with the cable reel ('reel'), walking the wire at foot pace to the cut,
     * kneels there splicing ('splice') from t1, as he gets there, until the mend's t, then walks back along it ('walk') and
     * is gone once he is home. Worked out from the mend alone, as the clock stands: looks only, and nothing here draws.
     * Numbered k 300 on by the mend's man (the spotter's pair are 200 and 201).
     */
    linemen(id) {
      const b = this.b, t = b.t, out = [], pace = LAND.speed.inf;
      const r13 = (v) => Math.round(v * 10) / 10, r22 = (v) => Math.round(v * 100) / 100;
      for (const m of b.wireMends) {
        const w = b.wires[m.wire - 1];
        if (!w || w.unit !== id || t < m.t0) continue;
        const [hx, hy] = w.hq, d = Math.hypot(m.x - hx, m.y - hy), home = m.t + d / pace;
        if (t >= home) continue;
        const out1 = bearing2(hx, hy, m.x, m.y);
        const k = t < m.t1 ? m.t1 > m.t0 ? (t - m.t0) / (m.t1 - m.t0) : 1 : t < m.t ? 1 : d > 0 ? Math.max(0, 1 - (t - m.t) * pace / d) : 0;
        const p = t < m.t1 ? "reel" : t < m.t ? "splice" : "walk";
        out.push({ k: 300 + m.man, r: "rifle", x: r13(hx + (m.x - hx) * k), y: r13(hy + (m.y - hy) * k), f: r22(wrap(p === "walk" ? out1 + Math.PI : out1)), p, v: p === "splice" ? 0 : pace, c: "none", ln: 1 });
      }
      return out;
    }
    /** The block's men, for tests and the browser page. */
    squad(id) {
      const sq = this.squads.get(id);
      return sq ? { phase: sq.phase, men: sq.men, veh: sq.veh, fo: sq.fo } : void 0;
    }
  };
  function rolesFor(kind, tray, n, team) {
    const out = [];
    for (let k = 0; k < n; k++) {
      if (kind === "arm") {
        out.push(["tank", k % 2 ? 2 : 1]);
        continue;
      }
      if (kind === "art") {
        out.push([team === "mg" ? "hmg" : team === "mor" ? "mortar" : "gun", 0]);
        continue;
      }
      if (tray >= 8) {
        if (k === 0) out.push(["officer", 0]);
        else if (k === 1) out.push(["mg", 1]);
        else if (k === 2) out.push(["mg", 2]);
        else if (k === 3) out.push([kind === "fort" ? "mg" : "at", kind === "fort" ? 1 : 0]);
        else {
          const team2 = k % 2 ? 2 : 1;
          if (kind === "eng" && k >= tray - 2) out.push(["sapper", team2]);
          else if (kind === "cav" && k % 4 === 3) out.push(["holder", team2]);
          else out.push(["rifle", team2]);
        }
      } else out.push(k === 0 ? ["mg", 1] : ["rifle", k % 2 ? 2 : 1]);
    }
    return out;
  }
  var grp = (m) => (m.sec ?? m.k) * 8 + (m.sub ?? 0);
  function platoonOf(nat) {
    const P = DR.sections.platoon, p = P[nat] ?? P.default;
    return { squads: Math.max(1, Math.min(7, p.squads)), hq: Math.max(1, p.hq) };
  }
  var menOnScreen = (r) => r === "hmg" || r === "mortar" ? 3 : VEHICLE.has(r) ? 0 : 1;
  function sectionOf(nat, key) {
    const S = DR.sections, out = [];
    const add = (r, c, pr = false, scout = false) => {
      for (let i = 0; i < c; i++) out.push([r, pr, scout]);
    };
    switch (key) {
      case "officer":
        add("rifle", S.officer - 1, true);
        break;
      case "mg":
        add("loader", 2, true);
        add("rifle", S.mg - 3);
        break;
      case "at":
        add("loader", S.at - 1, true);
        break;
      case "sapper":
        add("sapper", 2);
        add("rifle", S.sapper - 3);
        break;
      case "holder":
        add("holder", S.holder - 1, true);
        break;
      case "gun":
        add("crew", S.gun - 1, true);
        break;
      case "hmg":
        add("loader", S.hmg - 3, true);
        break;
      case "mortar":
        add("loader", S.mortar - 3, true);
        break;
      case "rifle": {
        const R = S.rifle[nat] ?? S.rifle.default;
        add("mg", 1);
        add("loader", R.loaders, true);
        add("rifle", R.scouts, false, true);
        add("rifle", R.men - 2 - R.loaders - R.scouts);
        break;
      }
      default:
        break;
    }
    return out;
  }
  function wreckOffset(id, n, w, d) {
    return [(mix(id * 977 + n * 131 + 7) - 0.5) * w * 0.9, (mix(id * 613 + n * 389 + 3) - 0.5) * d * 0.8];
  }

  // ../../src/battle/land/own-teams.ts
  var OWN_TEAM_SHARE = 0.04;
  var OWN_TEAM_KINDS = ["inf", "mtn", "mot"];
  var OWN_TEAM_ID = -1000001;
  var WORD = { mg: ["machine guns", "MGs"], mor: ["mortars", "mortars"] };
  var isOwnTeam = (f) => f.corps <= OWN_TEAM_ID;
  var brings = (f) => f.corps >= 0 && OWN_TEAM_KINDS.includes(f.kind) && f.share > 0 && f.men > 0;
  function sideWith(s, next, parents) {
    const made = [];
    const forces = s.forces.map((f) => {
      if (!brings(f)) return f;
      const keep = 1 - 2 * OWN_TEAM_SHARE;
      for (const kind of ["mg", "mor"]) {
        const id = next - made.length, men = Math.max(1, Math.round(f.men * OWN_TEAM_SHARE));
        const name = `${f.name} ${WORD[kind][0]}`, short = `${f.name} ${WORD[kind][1]}`;
        parents.set(id, f.corps);
        made.push({
          corps: id,
          name: f.name,
          nat: f.nat,
          kind,
          divs: 1,
          men,
          str: f.str,
          org: f.org,
          xp: f.xp,
          ...f.mor !== void 0 ? { mor: f.mor } : {},
          ...f.dug ? { dug: true } : {},
          ...f.cross === false ? { cross: false } : {},
          ...f.wear !== void 0 ? { wear: f.wear } : {},
          ...f.fresh !== void 0 ? { fresh: f.fresh } : {},
          divisions: [{ name, short, str: f.str, xp: f.xp, men }],
          share: f.share * OWN_TEAM_SHARE,
          battles: f.battles ?? 0
        });
      }
      return {
        ...f,
        share: f.share * keep,
        men: Math.round(f.men * keep),
        ...f.divisions ? { divisions: f.divisions.map((d) => ({ ...d, men: Math.round(d.men * keep) })) } : {}
      };
    });
    return made.length ? { ...s, forces: [...forces, ...made] } : s;
  }
  function ownTeams(t, seen = []) {
    if ([...t.att.forces, ...t.def.forces].some(isOwnTeam)) return { ticket: t, seen: [...seen] };
    const parents = /* @__PURE__ */ new Map();
    const att = sideWith(t.att, OWN_TEAM_ID, parents);
    const def = sideWith(t.def, OWN_TEAM_ID - parents.size, parents);
    if (!parents.size) return { ticket: t, seen: [...seen] };
    const known = new Set(seen);
    return { ticket: { ...t, att, def }, seen: [...seen, ...[...parents].filter(([, parent]) => known.has(parent)).map(([id]) => id)] };
  }
  function ownTeamParents(t) {
    const out = /* @__PURE__ */ new Map();
    for (const s of [t.att, t.def]) {
      const teams = s.forces.filter(isOwnTeam), parents = s.forces.filter((f) => !isOwnTeam(f) && brings(f));
      if (!teams.length || teams.length !== 2 * parents.length) continue;
      teams.forEach((f, i) => out.set(f.corps, parents[i >> 1].corps));
    }
    return out;
  }
  function ownTeamsLook(look) {
    if ("ticket" in look && look.ticket) {
      const o = ownTeams(look.ticket, look.live?.seen ?? []);
      return o.ticket === look.ticket ? look : { ...look, ticket: o.ticket, live: { ...look.live ?? {}, seen: o.seen } };
    }
    return ownTeams(look).ticket;
  }

  // ../../src/battle/land/sim.ts
  var other = (s) => s === "att" ? "def" : "att";
  var HEDGE_CELL = 10;
  var OPEN_GROUND = ["open", "field", "road", "bridge", "marsh"];
  var GATE_CELL = 25;
  var GATE_REACH = 120;
  var HEDGE_BAR = 0.75;
  var HEDGE_GAP = 1.25;
  function segHit(ax, ay, bx, by2, cx, cy, dx, dy) {
    const rx = bx - ax, ry = by2 - ay, sx = dx - cx, sy = dy - cy, den = rx * sy - ry * sx;
    if (Math.abs(den) < 1e-12) return null;
    const t = ((cx - ax) * sy - (cy - ay) * sx) / den, u = ((cx - ax) * ry - (cy - ay) * rx) / den;
    return t >= 0 && t <= 1 && u >= 0 && u <= 1 ? t : null;
  }
  function segGap(px, py, ax, ay, bx, by2) {
    const dx = bx - ax, dy = by2 - ay, l = dx * dx + dy * dy;
    const k = l > 0 ? Math.max(0, Math.min(1, ((px - ax) * dx + (py - ay) * dy) / l)) : 0;
    return Math.hypot(px - (ax + k * dx), py - (ay + k * dy));
  }
  function coveredTanks(t, units) {
    const A = LAND.alone, out = /* @__PURE__ */ new Set();
    for (const s of ["att", "def"]) {
      const fs = t[s].forces, w = (k) => fs.filter((f) => k(f.kind)).reduce((a, f) => a + f.divs * f.str, 0);
      const foot = w((k) => A.foot.includes(k) || s === "def" && k === "fort"), tanks = w((k) => k === "arm");
      if (foot <= 0 || tanks <= 0) continue;
      const per = s === "def" ? A.cover.def : A.cover.cards.includes(t.place.terrain) ? A.cover.close : A.cover.open;
      const share = Math.min(1, foot * per / tanks);
      const blocks = units.filter((u) => u.side === s && u.kind === "arm").sort((a, c) => a.id - c.id);
      const all = blocks.reduce((a, u) => a + (u.worth ?? 1), 0);
      let got = 0;
      for (const u of blocks) {
        if (got + (u.worth ?? 1) > share * all + 1e-9) break;
        got += u.worth ?? 1;
        out.add(u.id);
      }
    }
    return out;
  }
  var inRiver = (g) => g === "water" || g === "blown";
  var live2 = (u) => u.state !== "gone" && u.state !== "withdrawn";
  var fighting2 = (u) => live2(u) && u.state !== "routing";
  var weightOf = (u) => u.priced ?? u.power0;
  var ownArm2 = (u) => u.team === "mg" || u.team === "mor";
  var INWARD = { south: [0, 1, 0], north: [0, -1, Math.PI], west: [1, 0, Math.PI / 2], east: [-1, 0, 1.5 * Math.PI] };
  function joinEdge(side, toward, own) {
    if (toward === void 0 || !Number.isFinite(toward)) return side === "att" ? "south" : "north";
    const sn = Math.sin(toward), cs = Math.cos(toward);
    let e = Math.abs(cs) >= Math.abs(sn) ? cs > 0 ? "north" : "south" : sn > 0 ? "east" : "west";
    if (e === (side === "att" ? "north" : "south")) e = sn > 1e-9 ? "east" : "west";
    if (own && (e === "west" || e === "east") && e !== own) e = "south";
    return e;
  }
  var mix2 = (n) => {
    let h = n >>> 0;
    h ^= h >>> 16;
    h = Math.imul(h, 2246822507);
    h ^= h >>> 13;
    h = Math.imul(h, 3266489909);
    h ^= h >>> 16;
    return (h >>> 0) / 4294967296;
  };
  function breakLine(why, who, past = false) {
    if (why.by === "running") return past ? `${who} broke: half of it was running or gone, and the rest would not stand alone.` : `${who} breaks: half of it is running or gone, and the rest will not stand alone.`;
    const pays = why.side === "att" ? "as much as an attack will pay before it is called off" : "as much as a defence will pay before it gives up the ground";
    const still = why.stillSteady === "all" ? ", though every part of it was still steady" : why.stillSteady === "most" ? ", though most of it was still steady" : "";
    return `${who} ${past ? "broke when it had lost" : "breaks: it has lost"} about ${Math.round(why.worn * 100)}% of its men along the whole line, ${pays}${still}.`;
  }
  function warnLine(why) {
    return why.nearer === "running" ? "Your army is near breaking: nearly half of it is running or gone. Pull back the worn, or hand it to the staff." : `Your army is near breaking: its losses along the whole line are near what ${why.side === "att" ? "an attack" : "a defence"} will pay. Withdraw, or hand it to the staff.`;
  }
  var LandBattle = class {
    constructor(ticket) {
      __publicField(this, "domain", "land");
      __publicField(this, "ticket");
      __publicField(this, "field");
      __publicField(this, "t", 0);
      __publicField(this, "units");
      __publicField(this, "over", null);
      __publicField(this, "ai");
      /**
       * Each side's computer general's style (styles.ts; the tactics panel, item 1), read once from the ticket: null where
       * the player commands the side, which, handed to the computer, fights as the plain general always has.
       */
      __publicField(this, "style");
      /** The attack's plan as its computer general set it at its first thought (ai.ts), for the fight and the 3D game. */
      __publicField(this, "plan", {});
      __publicField(this, "rng");
      __publicField(this, "events", []);
      /** Everything said this battle, for the after-action card. */
      __publicField(this, "log", []);
      __publicField(this, "power");
      /**
       * Whether a side holds at the brink by its fresh blocks: an army of LAND.fresh.upTo blocks or fewer, its works not
       * counted (b480, b484), unless it is the defence behind fortress works or in a fort, which the card prices already
       * (b488).
       */
      __publicField(this, "freshHold");
      __publicField(this, "nextThink", 0);
      __publicField(this, "broke", { att: false, def: false });
      /** When the first blow fell between the lines (guns firing from far back do not count): null until then. */
      __publicField(this, "contactAt", null);
      /** The tanks knocked out on the field, where each stands, to the end of the fight (b477): what the view sends as zones. */
      __publicField(this, "wrecks", []);
      /**
       * The craters the shells have left on the ground, to the end of the fight (shells.ts): sent as zones beside the wrecks.
       * Looks only: nothing in the fight reads them. Numbered -1, -2 and on, so no crater has a wreck's id.
       */
      __publicField(this, "craters", []);
      /** The player has been warned that their army is near breaking (ruling 226(3)). */
      __publicField(this, "warned", { att: false, def: false });
      /** Each army's bar, how near breaking it stands, as last judged: what a withdrawal after contact pays by (b474). */
      __publicField(this, "bar", { att: 0, def: 0 });
      /** Seconds of battle time the fresh hold has kept each army at the brink, as judged: for the words on why it broke (b496(2)). */
      __publicField(this, "heldFor", { att: 0, def: 0 });
      /**
       * The attack card did not know the defence (b453): an enemy block shows the player only where it stands until one of
       * his has fought it (b464(2); known). Set by whoever opens the field. Words and pictures only: the fight never reads it.
       */
      __publicField(this, "fog", false);
      /** Under fog, the corps the attack card identified: their blocks show as the card shows them (b476(3); cardKnows). */
      __publicField(this, "seen", /* @__PURE__ */ new Set());
      /**
       * The men inside the blocks, the squad brain (drill.ts): null until something that draws them asks (drillOn: the 3D
       * bridge, the browser page). Looks only: the fight never reads it, and it never draws on the battle's dice.
       */
      __publicField(this, "drill", null);
      /**
       * The side placing its guns and support teams before the clock starts (release 2; Brian, 21 Nov 03:49Z: "units you
       * can place at the begining of a battle on your side"): null unless something asks (setupOn), as the drill is.
       * While it is set the battle does not step; land:place moves one of that side's gun blocks, land:begin ends it.
       */
      __publicField(this, "setup", null);
      /** Where the placing side may put a block: its zone, grown to take in every block of its own where it was drawn up. */
      __publicField(this, "room", null);
      /** The setup step places every block of the side, not only its guns and teams (release 2, the general's controls). */
      __publicField(this, "placeAll", false);
      /**
       * Direct command (release 2; Brian, 22 November: "I want to be able to click a unit and point where I want them to
       * go", and 03:09Z "do 1 2 and 3"). perUnit: a block the player orders is his (LandUnit.taken), and his staff thinks
       * for the rest of his side (while ai[side] is on), never for his. radio: his orders reach a block by radio, after
       * LAND.radio's delay. Both off unless whoever opens the field turns them on; off, nothing here is read.
       */
      __publicField(this, "perUnit", false);
      __publicField(this, "radio", false);
      /** True while a staff (ai.ts) is giving its orders: those are the computer's, never taken in hand nor sent by radio. */
      __publicField(this, "staffing", false);
      /**
       * Hidden in the woods (release 2; Brian, 22 November 04:18Z: ambushes; LAND.hide): off unless whoever opens the field
       * turns it on (hideOn). Off, no block is ever hidden and nothing here is read.
       */
      __publicField(this, "hide", false);
      /** The clock has started with hide on: who is hidden is settled, and the computer's ambushes laid. */
      __publicField(this, "hidStarted", false);
      /**
       * Night patrol (release 2; Brian, 22 November 17:33Z: "lets do all three but make it where we can toggle it off if
       * someone doesnt want it"; the design director, 18:36Z, Q6 to Q8; LAND.patrol; "Night patrol" below): the night
       * before, the attack sends a patrol that may find the defence's most forward hidden post, or be caught and alert the
       * defence. Off unless whoever opens the field turns it on (the 2D page's ?patrol, the bridge's { patrol: true }), and
       * it needs hiding on: with hiding off no patrol goes out and nothing is drawn. Off, nothing below is read.
       */
      __publicField(this, "patrol", false);
      /** The night's patrol, as it was settled when the clock started (night patrol): null until then, and where none went out. */
      __publicField(this, "nightPatrol", null);
      /** The block the player sends out on patrol, by his order while he places his army (night patrol): null where none. */
      __publicField(this, "patrolBy", null);
      /** The field's hedges and their gates, as the 3D game handed them over (hedgesOn): empty where it has none. */
      __publicField(this, "hedges", []);
      __publicField(this, "gates", []);
      /** Which 10 m squares lie on a hedge line (LAND.hide.hedge.near), for hiding: null with no hedges. */
      __publicField(this, "hedgeMap", null);
      /** The hedge lines' segments, [ax, ay, bx, by], by the GATE_CELL square they pass through: for the men's way through the gates. */
      __publicField(this, "hedgeSegs", /* @__PURE__ */ new Map());
      /** The sides that have been ambushed: their attack fires on the cover ahead from then (LAND.hide.search). */
      __publicField(this, "ambushed", /* @__PURE__ */ new Set());
      /**
       * Measured armour (release 2; Brian, 22 November 04:03Z to 04:16Z; armour.ts, LAND.armour): a tank's or a gun's blow on
       * a tank weighs what its gun gets through the face it strikes at that range. Off unless whoever opens the field turns
       * it on; off, nothing here is read.
       */
      __publicField(this, "measured", false);
      /**
       * Hold until relieved (release 2; Brian, 22 November 12:48Z; "Hold until relieved" below): a block on foot may be told
       * to dig in and wait for a corps coming up from next door. Off unless whoever opens the field turns it on (the 2D
       * page's ?relief, the bridge's { relief: true }); off, nothing below is read.
       */
      __publicField(this, "relief", false);
      /**
       * Support teams in battle (release 2, the Battle rules card's "teams"; the design director's ruling of 25 November
       * 2026, 17:52Z, on the ground thread's own-teams-causes-1125.md). On, wherever team blocks are on the field (a
       * campaign battle's own or built teams, a set piece's battalions):
       *   - a team's nerve multiplier (LAND.teams' nerve, by its blow's aspect) weighs its own harm only (step). Off, as
       *     it was: every striker's multiplier is added and the sum weighs all the harm on the block that tick, so a team
       *     with 2% of its side's power doubled what everything landing with it cost in nerve; blocks ran early with
       *     strength left, stopped being worn, and battles ran to the clock. The line's and the guns' are added as fitted,
       *     and LAND.teams is untouched.
       *   - the defence's staff never counts a team block firing from far back as a neighbour in a fight (ai.ts defend).
       *   - a team is its own division's, never a gun (the design director, 25 November 20:03Z, on the ground thread's
       *     teams-traces-1125.md). At the brink a force's own team block stands in its army's sums as its division's line
       *     stands (wornShare, brokenShare, freshShare: ownLine). Off, as it was: a team far back, never worn, was whole
       *     power in the sum, so the line had to be worn that much more before its army was spent. A team block with no
       *     division of its own (a set piece's battalion, a brigade the player built) counts as itself, on or off.
       *   - a team fires on the target in its reach its own weapon harms most, the nearest among those it harms alike
       *     (targetOf), and the attack's staff no longer lays it on the main effort's target as it lays guns (ai.ts
       *     planned). Off, as it was: laid with the guns, a machine gun spent half its fire on tanks.
       * A battle with no team block on the field is the same on or off. Off unless whoever opens the field turns it on.
       */
      __publicField(this, "teamsRule", false);
      /**
       * Units face the fire that hurts most (release 2, the Battle rules card's `facing`, off; the design director's ruling
       * of 25 November 2026, 21:54Z, on the ground thread's trace of two cities).
       *
       * Off, as it always was: any blow that lands on a block's front holds it for the tick ("face what it is fighting,
       * unless held by an enemy in front"), and a held block may not turn to meet another enemy. Any striker, at any range,
       * however light the blow and whatever the block's cover: so a machine gun a kilometre off held a block in a building
       * to its facing while a line came in on its side, where blows count half as much again. Ruled a fault: the hold
       * read no cover.
       *
       * On: a block faces what costs it most. Of the enemies that struck it this tick, the one whose blow costs it most
       * holds it: in front, the block stands as it faces; on its side or behind, the block turns to meet it. A blow a
       * building's cover has cut low holds it only while nothing hurts it more. No new figure: the cost is the harm the
       * fight already reckons, after cover. It is weighed as it would land face on (the facing's own weight and the tick's
       * luck taken out), so the costliest enemy is the same one whichever way the block faces and two enemies of about
       * equal weight never turn it back and forth: facing the heavier is what costs it least. Struck by no one, it faces
       * what it is fighting, as before.
       *
       * It moves every battle (guns hold too), so it has its own row and its own gate, and `teams` goes on only with it on
       * (live.ts). It goes on only with standRule, below. Off unless whoever opens the field turns it on.
       */
      __publicField(this, "faceRule", false);
      /**
       * A stand is kept (release 2, the Battle rules card's "stand"; the art director's number of 26 November 2026 00:50Z
       * and the design director's ruling of 01:02Z).
       *
       * Off, as it always was: a block on an attack order closes "until it is well in reach, then stands and fights", and
       * that is asked afresh every tick with no memory of the tick before. With its target hovering at that edge the block
       * stands for a tick and wheels to what it fights, marches for a tick and wheels back to its way, and so on: a rock of
       * a few degrees several times a second, or a wide swing every few seconds (counted at its gate: the reversals of a
       * block's wheel within 5 s of the one before). Ruled a fault, for every block on an attack order.
       *
       * On: a block that has stood to fight its target stands while that target is within its hold, and closes again only
       * when the target is further off than that. No new figure: the hold is the stand's own (lanePoint: half way from
       * where a block closes to the end of its reach, how far off its target a block may stand and still fight).
       *
       * It moves every battle, so it has its own row and its own gate, and `facing` and `teams` go on only with it on
       * (live.ts). Off unless whoever opens the field turns it on.
       */
      __publicField(this, "standRule", false);
      /**
       * What it chose it keeps while that still holds (the design director, 26 November 2026 01:28Z: one rule with the
       * stand above, on the same row and switch, read only from the game's own facts, with no new figure; and 02:30Z:
       * yes to `step`, and a key each for the two halves of a block's place beside a friend). More choices a block made
       * afresh every tick with no memory of the tick before, and so turned back and forth between. Each has its own key
       * here so the rule's gate can count them alone and together; all are on with the rule, and none is read with it off.
       *
       * way: on its way round ground it cannot enter (over a bridge), a block makes for the furthest turning point it can
       * walk straight to. Shoved a step to one side at the bridge's end it lost that line and turned for the nearer point,
       * found the line again a step on and turned back, and so on for as long as a friend stood beside it. On: it goes on
       * for the turning point it chose for as long as the ground lets it step straight for it, as a block on a move order
       * goes on for the next point of its way; where the ground stops that step it falls back to the furthest point it
       * still has a straight line to and follows that line until it sees further than it fell back to; and while the point
       * it fell back to is under its own box it steps to it as it faces the one after, with no wheel to a way of a few
       * metres. (The ground is its way's own: foot that will not wade is stopped at the water and a tank held back by the
       * hedges at the cells a hedge line bars, as a tank is at a river.)
       *
       * side: a block on the march slides a friend in its way aside and slides aside itself, by the side the friend lies
       * across its front. A friend dead ahead lies now a hair to its left and now a hair to its right as the block's own
       * heading wavers, so the two were slid one way and the other tick by tick. On: the side it slid to pass that friend
       * the tick before is the side it slides now, while the two still overlap and the ground still lets them.
       *
       * beside: the place of its stand before its target. A friend it stood aside for holds the same ground still (where
       * it is, or the stand its own attack has it make for), whether a tick finds that friend halted or stepping (one that
       * waits and steps by turns held the one ground one tick and the other, or none, the next, and the stand hopped
       * between two places 30 m and more apart).
       *
       * back: a block that has carried out a move order and is then eased a few metres off its place by its friends is
       * sent there again by its general. It wheeled to that way of a few metres, marched it and wheeled back to its line's
       * facing, and the friends that had eased it eased it again. (Stepping back to the place without the wheel, as this
       * keep was first built, set it against those friends tick by tick: a step back, a shove off, for as long as they
       * stood there.) On: a move order sent again to the place it already holds (the place of the move it last carried
       * out, and still under its own box) is the same order, and that order it has carried out: it stands where it is,
       * beside the friends that eased it, and turns only to the facing its line is given.
       *
       * step: the same of a block on an attack order that has stood to fight. Eased a few metres off its stand with its
       * target at the edge of its hold, it wheeled to that way of a few metres, marched it and wheeled back to what it
       * fights. On: while its stand is still under its own box it steps back to it as it faces what it fights, and its
       * stand is kept the while.
       */
      __publicField(this, "standKeeps", { way: true, side: true, beside: true, back: true, step: true });
      /** faceRule: of the enemies that struck each block this tick, the costliest, what its blow cost face on, and whether it landed on the front. */
      __publicField(this, "costliest", /* @__PURE__ */ new Map());
      /** Each own team block's parent force (own-teams.ts ownTeamParents), by the forces' ids: read once from the ticket. */
      __publicField(this, "teamParent");
      /** The computer's tank blocks going round to a tank block's flank (ai.ts): where to, at whom, since when. */
      __publicField(this, "flanking", /* @__PURE__ */ new Map());
      /** Each block's tank or gun model, as the rules weigh it (armour.ts vehicleOf): null for the rest. */
      __publicField(this, "models", /* @__PURE__ */ new Map());
      /**
       * The player's orders on the air (radio): each block's, with the battle time it lands; 'smoke' (smoke on call, release
       * 2) a smoke order, which rides beside the block's other order on the air, neither replacing it nor replaced by it. An
       * order waiting for a down wire (field telephone lines) also carries the order it is (key) and its stamp (held).
       */
      __publicField(this, "pending", []);
      /**
       * Neighbours join the fight (release 2): the blocks of corps coming up from next door, not yet on the field, earliest
       * first: each with its corps' ticket, the battle second it comes on, the edge and the place there. Empty where none
       * comes, and then nothing below reads it.
       */
      __publicField(this, "coming", []);
      /** The battle second each joining corps came on. */
      __publicField(this, "joinedAt", /* @__PURE__ */ new Map());
      /** The corps that come up from next door. */
      __publicField(this, "joinCorps", /* @__PURE__ */ new Set());
      /**
       * The rider ahead (release 2; Brian, 22 November 12:48Z: "a courier arrives first; clicking him shows the relief's
       * strength and arrival time"; the design director, 13:43Z): off unless whoever opens the field turns it on (the 2D
       * page's ?rider, the bridge's { rider: true }); off, nothing reads him. Looks and words only: the fight never does.
       */
      __publicField(this, "rider", false);
      /** Each joining corps' rider, worked out with its time (comeOn): where he sets out and stands, and when. */
      __publicField(this, "riders", []);
      /** The joining corps a block has waited for this battle (hold until relieved): each relieves one block a battle. */
      __publicField(this, "waitedFor", /* @__PURE__ */ new Set());
      /**
       * Hedge cutters (release 2; Brian, 22 November 14:19Z; ruling 242; LAND.cutters; "Hedge cutters" below): a tank block
       * carries cutters, may be told to cut a gap through a hedge where the player picks, and the staffs' cut where their
       * way crosses one. Off unless whoever opens the field turns it on (the 2D page's ?cutters, the bridge's { cutters:
       * true }); off, nothing below is read.
       */
      __publicField(this, "cutters", false);
      /** Every cut begun this battle (hedge cutters), in the order begun. Empty while the switch is off. */
      __publicField(this, "cuts", []);
      /**
       * Smoke on call (release 2; Brian, 22 November 14:19Z; ruling 242; LAND.smoke; "Smoke on call" below): a block on foot
       * whose army has the means may lay a smoke screen before it, and both staffs lay one as they go in. Off unless whoever
       * opens the field turns it on (the 2D page's ?smoke, the bridge's { smoke: true }); off, nothing below is read.
       */
      __publicField(this, "smoke", false);
      /**
       * The bocage (release 2; ruling 242 C1-B, as amended 17:45Z: "unfitted tanks cross only at gaps"; "The bocage" below):
       * on a field with hedges, a tank block without hedge cutters crosses a hedge line only at a gap in it, a gateway or a
       * gap cut and open, and finds its way round to one. Its own switch, so cutters alone stays the fight it was (C1-B:
       * "under its own switch, measured against the card"). Off unless whoever opens the field turns it on (the 2D page's
       * ?bocage, the bridge's { bocage: true }); off, nothing below is read.
       */
      __publicField(this, "bocage", false);
      /** The bocage: the field's cells a hedge line bars to a tank block without cutters (barrier()); null until asked, and again whenever a gap opens. */
      __publicField(this, "bars", null);
      /** How many times the barred cells have been laid afresh (a gap opened): a way found before is looked for again. */
      __publicField(this, "barsLaid", 0);
      /** Every screen laid this battle (smoke on call), in the order laid: standing while t < until. Empty while the switch is off. */
      __publicField(this, "screens", []);
      /**
       * Field telephone lines (release 2; Brian, 22 November 17:33Z: "lets do all three but make it where we can toggle it off
       * if someone doesnt want it"; the design director's rulings, 18:36Z, PHONES Q1 to Q3; LAND.phones; "Field telephone
       * lines" below): the defence's blocks are on the telephone from its headquarters; guns, mortars and tanks cut the wires,
       * and an order to a block whose wire is down waits its army's runner or wireless, the player's and every staff's alike.
       * Off unless whoever opens the field turns it on (the 2D page's ?phones, the bridge's { phones: true }); off, nothing
       * below is read.
       */
      __publicField(this, "phones", false);
      /** The defence's wires (field telephone lines), laid as the clock starts, numbered from 1. Empty while the switch is off. */
      __publicField(this, "wires", []);
      /** Every line cut, every lineman sent, and every order that waited for a down wire, in order. Empty while the switch is off. */
      __publicField(this, "wireCuts", []);
      __publicField(this, "wireMends", []);
      __publicField(this, "wireHeld", []);
      /** Each block's wire, by its id; and the wires' own dice (never the battle's), from when they are laid. */
      __publicField(this, "wireBy", /* @__PURE__ */ new Map());
      __publicField(this, "phoneRng", null);
      /**
       * Stretcher bearers (release 2; Brian, 22 November 17:33Z: "lets do all three but make it where we can toggle it off if
       * someone doesnt want it"; the design director's rulings Q4 and Q5, 18:36Z; LAND.bearers; "Stretcher bearers" below):
       * a block's wounded lie for its bearers, who carry them back to its aid post, its own blows the weaker by the men away;
       * its light cases are back in the line after their dressing. Off unless whoever opens the field turns it on (the 2D
       * page's ?bearers, the bridge's { bearers: true }); off, nothing below is read.
       */
      __publicField(this, "bearers", false);
      /**
       * Shell blasts (release 2; Brian, 23 November 05:04Z): the men a block loses to shells fall together, the men near one
       * burst, as one shell takes them (Drill.thin, DRILL.blast). Looks only: the drill reads it, the fight never. Off unless
       * whoever opens the field turns it on (the bridge's { blast: true }, the Battle rules card's Shell blasts row).
       */
      __publicField(this, "blast", false);
      /**
       * Infantry with tanks (release 2; Brian, 23 November 05:05Z): foot near a friendly tank advance behind her, no man is
       * run over by his own side's vehicles (they step out of the way), and an enemy tank can run a man over (Drill.escort,
       * clearOfVehicles, thin; DRILL.escort). Off unless turned on (the bridge's { escort: true }, the Battle rules card's
       * Infantry with tanks row). With it, tanks overrun foot in the open (LAND.overrun, overruns): harm unchanged.
       */
      __publicField(this, "escort", false);
      /**
       * Armoured infantry (release 2; Brian, 23 November 07:01Z): an armoured block draws its division's riflemen riding
       * behind its tanks in their army's carriers, half-tracks or lorries, who get down near the enemy (Drill.addMech;
       * DRILL.mech). Looks only: the drill reads it, the fight never. Off unless turned on (the bridge's { mech: true }, the
       * Battle rules card's Armoured infantry row).
       */
      __publicField(this, "mech", false);
      /**
       * Self-propelled guns (release 2; Brian, 23 November 07:01Z, yes to "a tank destroyer and a self-propelled gun for each
       * major army"): some of a battery's guns are drawn as its army's self-propelled howitzer, with no tractor and no crew
       * on foot (Drill.spgOf; DRILL.spg). Looks only. Off unless turned on (the bridge's { spg: true }, the Battle rules
       * card's Self-propelled guns row); a campaign battle's only (set pieces stay as the historian drew them).
       */
      __publicField(this, "spg", false);
      /**
       * Tank destroyers (release 2, the same yes; the design director, 23 November 07:28Z, (a)): one model in an armoured
       * block's mix is drawn as its army's tank destroyer or assault gun, the block's own figures and fight unchanged
       * (Drill.tdOf; DRILL.td). Looks only. Off unless turned on (the bridge's { td: true }, the Battle rules card's Tank
       * destroyers row); a campaign battle's only.
       */
      __publicField(this, "td", false);
      /**
       * Armoured cars and AA vehicles (release 2; Brian, 23 November 07:01Z, item 3; the design director, 07:47Z): a block's
       * armoured cars out ahead on its flanks till it meets the enemy, then behind it; its AA vehicles behind it (Drill.addEscorts;
       * DRILL.cars, DRILL.aa). Looks only. Off unless turned on (the bridge's { cars: true } and { aa: true }, the Battle rules
       * card's Armoured cars and AA vehicles rows); a campaign battle's only.
       */
      __publicField(this, "cars", false);
      __publicField(this, "aa", false);
      /**
       * Jeeps, field cars and motorcycles (release 2; Brian, 23 November 07:01Z, item 3; the historian's rows, 08:18Z): a
       * block's light vehicles out ahead with its screen or parked behind it, a motorcycle there riding despatch
       * (Drill.lightOf; DRILL.light). Looks only. Off unless turned on (the bridge's { light: true }, the Battle rules card's
       * Jeeps and motorcycles row); a campaign battle's only.
       */
      __publicField(this, "light", false);
      /** Overrun (LAND.overrun): each foot block overrun in its contact now, and the tank block that drove through it. */
      __publicField(this, "overrun", /* @__PURE__ */ new Map());
      /** Each block's bearers and wounded (Litter), from its first loss. Empty while the switch is off. */
      __publicField(this, "litters", /* @__PURE__ */ new Map());
      /**
       * The light cases back in the line (stretcher bearers): when, the block and how much strength, a block's returns in one
       * battle second taken together (at the first one's time). Empty while the switch is off.
       */
      __publicField(this, "returns", []);
      // Air support (see "Air support" below). Nothing here moves where neither side has fighters on call.
      /** Each side's fighters on call, where it has any. */
      __publicField(this, "airOn", { att: null, def: null });
      /** Strikes each side has still to call; while it has any, its blows hold back the card's share for them. */
      __publicField(this, "airLeft", { att: 0, def: 0 });
      /** Battle time each side's next strike is ready: never before the armies meet. */
      __publicField(this, "airNext", { att: Infinity, def: Infinity });
      /** What each side's blows have held back, owed to the enemy block each fell on, as it would be on open ground. */
      __publicField(this, "airBank", { att: /* @__PURE__ */ new Map(), def: /* @__PURE__ */ new Map() });
      /** With it, the nerve it would have cost (held back times the nerve of the blows it was held from): paid with it. */
      __publicField(this, "airNerve", { att: /* @__PURE__ */ new Map(), def: /* @__PURE__ */ new Map() });
      /** The strikes called and not yet over. */
      __publicField(this, "strikes", []);
      /** Strikes called so far, by side: strike n is flown by the side's group n in turn. */
      __publicField(this, "airCalls", { att: 0, def: 0 });
      /** Strikes not yet flown when the battle went to the dice: the staff fly them in the settlement, and the bill counts them. */
      __publicField(this, "airSettled", { att: 0, def: 0 });
      __publicField(this, "flightIds", 0);
      /**
       * Each side's ledger, in open-ground terms: held back from its blows; landed on enemy blocks; wasted (on empty ground,
       * or held over its own men); and dealt, what the landed share took off them for the ground they stood on. What is
       * neither landed nor wasted is unspent: in the bank or in the air when the day ended.
       */
      __publicField(this, "airLedger", { att: { withheld: 0, landed: 0, wasted: 0, dealt: 0 }, def: { withheld: 0, landed: 0, wasted: 0, dealt: 0 } });
      /** The day's luck: the attack's blows are multiplied by it, the defence's divided. */
      __publicField(this, "fortune");
      /** The share of the attack's tanks and lorries, by strength, that must cross the river: 0 where it has none (b494). */
      __publicField(this, "armourWet");
      /** The defence's line ends on a city's own river up the field (b526), or a second river along its flank (b596), all of it on one bank: a flank not on the card. */
      __publicField(this, "flankOnRiver", false);
      /** What the held defence's take is multiplied by for that flank: LAND.hold.stream up the field, LAND.hold.join beside a second river. */
      __publicField(this, "flankHold", 1);
      /**
       * What waits' cover spared, being paid out: each held block's, at the pace it was spared, shared by men with the blocks
       * that relieved it (the design director, 13:43Z). Empty with no waits, and then nothing reads it.
       */
      __publicField(this, "repay", []);
      /**
       * The way there: straight, unless ground it cannot enter lies across it (a river away from its bridges, rock, marsh
       * for a lorry, a bunker). Then round it, the quickest way over the ground (a road is quicker than a field), cut to as
       * few turns as can be walked straight between: over a bridge, round a bend of the river, round an outcrop. Where it
       * cannot get there at all it goes as near as it can, and stands. A block set down in ground it cannot enter (a tank
       * among rock) first walks out of it the shortest way. An attack's way is asked for afresh every tick, so the way found
       * is kept while the block makes for much the same place, and the block takes the furthest turning point it can walk
       * straight to (b418: tanks and lorries never crossed rivers, and stood for good at rock and marsh).
       */
      __publicField(this, "ways", /* @__PURE__ */ new Map());
      /** By how it moves: the cells within a few of ground it can hardly cross or not at all (a river, rock, marsh). */
      __publicField(this, "hardNear", {});
      __publicField(this, "ashore", null);
      /**
       * Nobody stands in anybody else: where two blocks' ground overlaps they are eased apart along the shallower way, a
       * few metres a second, onto ground they can stand on. Blocks side by side in a line, or one behind the other, are
       * left alone; enemies keep a few metres between their fronts. For the screens and the models as much as for the
       * fight: two divisions never share one patch of ground.
       */
      /** standKeeps.side: by pair of friends, the way across the first's front they last slid apart, and the count of the tick they did. */
      __publicField(this, "sides", /* @__PURE__ */ new Map());
      __publicField(this, "spreads", 0);
      /** The player's group attacks' 'together' groups, numbered above the computer's (ai.ts uses block ids and 100000). */
      __publicField(this, "groupIds", 2e5);
      /** Tank blocks the card counted as covered by their foot (item 7): set when the armies are made. */
      __publicField(this, "covered", /* @__PURE__ */ new Set());
      /** Tank blocks bitten for being alone this battle, for the tests and the 3D game's mark. */
      __publicField(this, "bitten", /* @__PURE__ */ new Set());
      /** A bold defence's held blocks walking behind their line (ai.ts walk()): the x each was last sent to. */
      __publicField(this, "shifted", /* @__PURE__ */ new Map());
      /** Whether the player has been told of the enemy reserve's walk (once a battle). */
      __publicField(this, "toldShift", false);
      /** The cells within LAND.mouth of a bridge, worked out once. */
      __publicField(this, "mouths", null);
      this.ticket = ticket;
      this.rng = new Rng(ticket.seed);
      this.style = { att: styleFor(ticket, "att"), def: styleFor(ticket, "def") };
      const priced = ticket.reckoning.teamFire && [...ticket.att.forces, ...ticket.def.forces].some(isOwnTeam) ? ticket.reckoning.teamFire : void 0;
      const A = ticket.reckoning.A / (priced ? Math.max(1e-6, priced.att) : 1), D2 = ticket.reckoning.A / Math.max(1e-6, ticket.reckoning.odds) / (priced ? Math.max(1e-6, priced.def) : 1);
      this.power = { att: A, def: D2 };
      this.teamParent = ownTeamParents(ticket);
      const att = makeBlocks("att", ticket.att, A, 1, void 0, ticket.turn);
      const def = makeBlocks("def", ticket.def, D2, att.length + 1, void 0, ticket.turn);
      this.units = [...att, ...def];
      const joining = joinBlocks(ticket, this.power, att.length + def.length + 1);
      this.covered = coveredTanks(ticket, this.units);
      const works = ticket.place.fort >= 1 || def.some((u) => u.kind === "fort");
      this.freshHold = { att: att.filter((u) => u.kind !== "fort" && !u.team).length <= LAND.fresh.upTo, def: !works && def.filter((u) => u.kind !== "fort" && !u.team).length <= LAND.fresh.upTo };
      for (const u of this.units) {
        const c = crewFor(u, ticket[u.side].forces.find((f) => f.corps === u.corps)?.battles ?? 0);
        if (c) u.crew = c;
      }
      for (const u of this.units) {
        const c = u.team ? u.team === "mor" ? "mortar" : void 0 : calFor(u.kind, u.nat, ticket.turn);
        if (c) u.cal = c;
      }
      for (const u of joining) {
        const c = crewFor(u, ticket[u.side].joiners?.find((f) => f.corps === u.corps)?.battles ?? 0);
        if (c) u.crew = c;
        const k = u.team ? u.team === "mor" ? "mortar" : void 0 : calFor(u.kind, u.nat, ticket.turn);
        if (k) u.cal = k;
      }
      const dry = ticket.place.dryEnd ?? "west";
      const bend = ticket.place.river && att.some((u) => u.dry) && att.some((u) => !u.dry) ? { x: bendAt(att, zonesOf(FIELD.w, FIELD.h).att, dry), dry } : void 0;
      if (!bend) for (const u of att) delete u.dry;
      for (const u of joining) if (u.side === "def" || !ticket.place.river) delete u.dry;
      const armour = att.filter((u) => MOTION[u.kind] === "tracks" || MOTION[u.kind] === "wheels"), all = armour.reduce((a, u) => a + u.power0, 0);
      this.armourWet = all > 0 ? armour.filter((u) => !u.dry).reduce((a, u) => a + u.power0, 0) / all : 0;
      this.field = buildField(ticket.place, ticket.seed, bend);
      if (streetsOf(ticket.place)) this.ticket = { ...ticket, limit: Math.round(ticket.limit * (1 + LAND.city.clock * builtShare(this.field))) };
      deploy(this.units, "att", this.field);
      deploy(this.units, "def", this.field);
      if (joining.length) this.comeOn(joining);
      const St = this.field.stream, held = this.units.filter((u) => u.side === "def");
      if (St?.lie === "up") {
        const xAt = (y) => St.course.reduce((m, p) => Math.abs(p[1] - y) < Math.abs(m[1] - y) ? p : m)[0];
        const west = held.filter((u) => u.x < xAt(u.y)).length;
        this.flankOnRiver = west === 0 || west === held.length;
        if (this.flankOnRiver) this.flankHold = LAND.hold.stream;
      } else if (St?.lie === "across" && St.join) {
        const xs = St.join.course.map((p) => p[0]).filter((x) => x >= 0 && x <= this.field.w), west = St.join.course[0][0] < this.field.w / 2;
        this.flankOnRiver = xs.length > 0 && held.every((u) => west ? u.x > Math.max(...xs) : u.x < Math.min(...xs));
        if (this.flankOnRiver) this.flankHold = LAND.hold.join;
      }
      for (const u of this.units) u.cover = groundAt(this.field, u.x, u.y);
      this.ai = { att: !ticket.att.human, def: !ticket.def.human };
      const [d0, d1] = ticket.reckoning.dice;
      const roll2 = d0 + (d1 - d0) * new Rng(hashString(ticket.attempt ? `fortune:${ticket.seed}:${ticket.attempt}` : `fortune:${ticket.seed}`)).next();
      this.fortune = 1 + LAND.fortune * (roll2 - 1);
      for (const s of ["att", "def"]) {
        const a = ticket[s].air;
        if (a && a.share > 0 && a.strikes > 0 && a.groups.length) {
          this.airOn[s] = a;
          this.airLeft[s] = Math.floor(a.strikes);
        }
      }
    }
    /**
     * What a defender standing on its own ground takes of a blow: set per ground, and for a river or fortress works,
     * so that the field counts the ground once, as the card's odds already have (ruling 226(4)). The river's only where
     * the block striking crossed it: one that came on over dry ground has no river drawn before it to make up for (b464(5)).
     * And only as far as the attack's tanks and lorries had to cross with it (armourWet): an attack on foot crosses at no
     * cost the field must make up, so a river never makes an attack better than none (b494).
     */
    holdTake(u, t) {
      const p = this.ticket.place, H = LAND.hold;
      const streets = streetsOf(p);
      return (H.take[p.terrain] ?? 0.4) * (p.river && !u.dry ? 1 + (H.river - 1) * this.armourWet : 1) * (p.fort > 0 && t.joined === void 0 ? H.works[Math.min(3, p.fort)] : 1) * (streets ? H.streets[`${p.terrain} ${streets}`] ?? H.streets[streets] : 1) * this.flankHold;
    }
    say(e) {
      const ev = { t: Math.round(this.t * 10) / 10, ...e };
      if (!ev.cue && ONE_SHOTS[ev.what]) ev.cue = ONE_SHOTS[ev.what];
      const u = ev.unit !== void 0 ? this.unit(ev.unit) : void 0;
      if (u && !this.known(u)) {
        const t = ev.what === "contact" && u.engaged !== void 0 ? this.unit(u.engaged) : void 0;
        if (t && this.ticket[t.side].human) ev.text = `${this.ticket[u.side].adj} troops are attacking your ${t.name}.`;
        else {
          this.log.push({ ...ev, text: "", cue: void 0 });
          return;
        }
      }
      this.events.push(ev);
      this.log.push(ev);
    }
    unit(id) {
      return this.units.find((u) => u.id === id);
    }
    /** Every block of the battle: those on the field, then those of corps still coming up from next door (release 2). */
    allBlocks() {
      return this.coming.length ? [...this.units, ...this.coming.map((c) => c.u)] : this.units;
    }
    // ---- Neighbours join the fight (release 2) ----------------------------------------------------------------------
    //
    // The design director's ruling of 22 November 03:15Z: a corps next door that joins a battle is priced on the
    // campaign card, and the field only draws it coming up. It reaches its side's line at (1 - joined) of the battle's
    // own clock, worked out from its march weeks, never a fixed minute or a draw: it stands off the field until its own
    // march from the edge facing the province it comes from would bring it there then, and walks on from that edge as a
    // b407 reserve already sent, to where its side's line is struck hardest. Until then nothing on the field sees it,
    // strikes it or is sent at it; the bar and the break count it at the card's figure.
    /**
     * Holds each joiner off the field: its time, its edge and each block's place there, side by side along the edge.
     * Nothing here draws on the dice.
     */
    comeOn(joining) {
      const B = this.field.bend, F = this.field, J = LAND.join;
      for (const corps of [...new Set(joining.map((u) => u.corps))]) {
        const us = joining.filter((u) => u.corps === corps), u0 = us[0], side = u0.side;
        const f = this.ticket[side].joiners.find((x2) => x2.corps === corps);
        const own = side === "att" && B ? u0.dry ? B.dry : B.dry === "west" ? "east" : "west" : void 0;
        const edge = joinEdge(side, f.toward, own);
        const lanes = own && B && edge === "south" ? own === "west" ? [10, B.x - BEND.clear] : [B.x + BEND.clear, F.w - 10] : null;
        const [x, y] = this.edgeAt(side, edge, f.toward, u0.w, lanes);
        const along = edge === "south" || edge === "north", [lo, hi] = lanes ?? [10, F.w - 10];
        const own0 = this.units.filter((v) => v.side === side);
        const cx = own0.length ? own0.reduce((a, v) => a + v.x, 0) / own0.length : x, cy = own0.length ? own0.reduce((a, v) => a + v.y, 0) / own0.length : y;
        const walk2 = Math.hypot(x - cx, y - cy) / Math.max(1, LAND.speed[u0.kind]);
        const at = Math.round(Math.max(0, (1 - u0.joined) * this.ticket.limit - walk2) * 1e3) / 1e3;
        if (at > 0) {
          this.riders.push({ corps, side, f, from: [x, y], mid: [cx, cy], to: [x, y], there: 0, comes: at, edge });
          this.riderTo(this.riders[this.riders.length - 1]);
        }
        us.forEach((u, i) => {
          const off = (i - (us.length - 1) / 2) * (u.w + J.gap);
          const px = along ? clamp(x + off, lo + u.w / 2, Math.max(lo + u.w / 2, hi - u.w / 2)) : x, py = along ? y : clamp(y + off, 40, F.h - 40);
          this.coming.push({ u, f, edge, x: px, y: py, lanes, at });
        });
        this.joinCorps.add(corps);
      }
      this.coming.sort((a, c) => a.at - c.at || a.u.id - c.u.id);
    }
    /** Where on its edge a joining corps comes on: across a back edge by the way it comes, at a flank its side's own depth. */
    edgeAt(side, edge, toward, w, lanes) {
      const F = this.field, z = F.zones[side], J = LAND.join;
      if (edge === "west" || edge === "east") return [edge === "west" ? w / 2 + 10 : F.w - w / 2 - 10, (z[1] + z[3]) / 2];
      const [lo, hi] = lanes ?? [z[0], z[2]];
      const q = toward !== void 0 && Number.isFinite(toward) ? toward : 0;
      return [clamp(F.w / 2 + F.w / 2 * Math.sin(q), lo + w / 2, Math.max(lo + w / 2, hi - w / 2)), edge === "south" ? J.edge : F.h - J.edge];
    }
    /** The joiners whose time has come: each corps' blocks walk on together, and its coming is told. */
    arrive() {
      const due = [];
      while (this.coming.length && this.t >= this.coming[0].at - 1e-9) due.push(this.coming.shift());
      for (const corps of [...new Set(due.map((c) => c.u.corps))]) {
        const g = due.filter((c) => c.u.corps === corps);
        for (const c of g) this.walkOn(c);
        this.joinedAt.set(corps, Math.round(this.t * 10) / 10);
        const u = g[0].u, f = g[0].f, side = u.side;
        this.say({
          what: "land:joined",
          unit: u.id,
          side,
          x: u.x,
          y: u.y,
          text: `${this.ticket[side].human ? f.name : `The ${this.ticket[side].adj} ${f.name}`} comes up from next door${f.arrive ? `, ${f.arrive === "early" ? "early" : f.arrive === "midway" ? "midway through the fight" : "late in the day"}` : ""}.`
        });
      }
    }
    /**
     * Sets one joining block on its edge, clear of the river and of every block, and starts it in: a reserve already
     * sent (b407; its guns stand as guns), walking its LAND.join.walk metres onto the field, its ground where it stops.
     */
    walkOn(c) {
      const u = c.u, J = LAND.join, [nx, ny, face] = INWARD[c.edge];
      [u.x, u.y] = this.clearSpot(u, c.x, c.y, nx, ny, c.lanes);
      u.face = face;
      u.cover = groundAt(this.field, u.x, u.y);
      if (u.kind !== "art") u.reserve = "sent";
      this.units.push(u);
      this.drill?.join(u);
      const o = { kind: "move", x: u.x + nx * J.walk, y: u.y + ny * J.walk, face: u.side === "att" ? 0 : Math.PI };
      this.send(u, o);
      u.home = [o.x, o.y];
    }
    /**
     * The first spot at or in from (x, y) where a block can stand: on ground it can stand on, out of the water, and clear
     * of every block on the field. Tried in a fixed order (in from the edge a cell at a time, and along it either way), so
     * the same battle always finds the same spot; where none is clear it keeps (x, y) and the march sorts it out.
     */
    clearSpot(u, x, y, nx, ny, lanes) {
      const F = this.field, J = LAND.join, m = Math.max(u.w, u.d), cell = F.cell;
      const [lo, hi] = lanes ?? [10, F.w - 10];
      const fit = (px, py) => [clamp(px, lo + m / 2, Math.max(lo + m / 2, hi - m / 2)), clamp(py, 40, F.h - 40)];
      const clear = (px, py) => {
        if (this.wet(u, px, py, false) || wetUnder(F, { x: px, y: py, w: m, d: m })) return false;
        for (const o of this.units) if (live2(o) && Math.abs(o.x - px) < (o.w + m) / 2 + LAND.placeClear && Math.abs(o.y - py) < (o.d + m) / 2 + LAND.placeClear) return false;
        return true;
      };
      for (let k = 0; k <= J.inward; k++) for (let j = 0; j <= 2 * J.across; j++) {
        const a = j === 0 ? 0 : (j % 2 ? 1 : -1) * Math.ceil(j / 2);
        const [px, py] = fit(x + nx * k * cell + ny * a * cell, y + ny * k * cell + nx * a * cell);
        if (clear(px, py)) return [px, py];
      }
      return fit(x, y);
    }
    /** Where a rider rides to and stands, and when he is there: worked out as the clock is set, and again once the setup step has placed the guns. */
    riderTo(r) {
      r.to = this.riderSpot(r.mid[0], r.mid[1]);
      r.there = Math.round(Math.hypot(r.to[0] - r.from[0], r.to[1] - r.from[1]) / Math.max(...Object.values(LAND.speed)) * 1e3) / 1e3;
    }
    /** Where a rider stands: the nearest point to (x, y), outward a half cell at a time, out of the water and off a bridge, clear of every block. */
    riderSpot(x, y) {
      const F = this.field, c = LAND.placeClear;
      const ok2 = (px, py) => {
        if (px < 10 || py < 10 || px > F.w - 10 || py > F.h - 10) return false;
        const g = groundAt(F, px, py);
        return !inRiver(g) && g !== "bridge" && !this.units.some((o) => live2(o) && Math.abs(o.x - px) < o.w / 2 + c && Math.abs(o.y - py) < o.d / 2 + c);
      };
      const r13 = (v) => Math.round(v * 10) / 10;
      if (ok2(r13(x), r13(y))) return [r13(x), r13(y)];
      for (let r = F.cell / 2; r <= 1e3; r += F.cell / 2) for (let a = 0; a < 16; a++) {
        const px = r13(x + Math.sin(a * Math.PI / 8) * r), py = r13(y + Math.cos(a * Math.PI / 8) * r);
        if (ok2(px, py)) return [px, py];
      }
      return [r13(x), r13(y)];
    }
    /**
     * The riders the player sees: his own side's only (a courier's news is his own army's), from the first step until his
     * corps comes up; none whose corps would beat him there (he comes on with it), none once the day is over.
     */
    riderView() {
      if (!this.rider || !this.riders.length || this.over || this.t <= 0) return {};
      const still = new Set(this.coming.map((c) => c.u.corps));
      const on = this.riders.filter((r) => this.ticket[r.side].human && still.has(r.corps) && r.there <= r.comes);
      if (!on.length) return {};
      const r33 = (v) => Math.round(v * 1e3) / 1e3, r13 = (v) => Math.round(v * 10) / 10;
      const KIND = { inf: "infantry", mtn: "mountain", cav: "cavalry", eng: "engineer", mot: "motorized", arm: "armoured", art: "artillery", fort: "fortress" };
      const mmss = (t) => `${Math.floor(t / 60)}:${String(Math.floor(t % 60)).padStart(2, "0")}`;
      return {
        riders: on.map((r) => {
          const k = r.there > 0 ? Math.min(1, this.t / r.there) : 1;
          const us = this.coming.filter((c) => c.u.corps === r.corps).map((c) => c.u).sort((a, b) => a.id - b.id);
          const men = us.reduce((a, u) => a + u.men0, 0), str = men > 0 ? us.reduce((a, u) => a + u.str0 * u.men0, 0) / men : r.f.str;
          const divs = us.map((u) => u.short), kind = KIND[r.f.kind] ?? r.f.kind;
          const staff = this.ai[r.side], line = (1 - (us[0]?.joined ?? 1)) * this.ticket.limit;
          const back = r.edge === (r.side === "att" ? "south" : "north"), by2 = back ? "from behind your line" : `on your ${r.edge} flank`;
          const list = divs.length > 1 ? `${divs.slice(0, -1).join(", ")} and ${divs[divs.length - 1]}` : divs[0] ?? "";
          const says = `${r.f.name}: ${divs.length} ${kind} division${divs.length === 1 ? "" : "s"} (${list}), ${Math.round(men).toLocaleString("en-GB")} men, strength ${Math.round(str * 100)}. It comes up ${by2} at ${mmss(r.comes)}${staff ? ` and reaches your line at about ${mmss(line)}` : ", and waits there for your orders"}.`;
          return {
            corps: r.corps,
            side: r.side,
            x: r13(r.from[0] + (r.to[0] - r.from[0]) * k),
            y: r13(r.from[1] + (r.to[1] - r.from[1]) * k),
            face: r33(bearing(r.from[0], r.from[1], r.to[0], r.to[1])),
            speed: k < 1 ? Math.max(...Object.values(LAND.speed)) : 0,
            there: r33(r.there),
            comes: r33(r.comes),
            edge: r.edge,
            ...staff ? { line: r33(line) } : {},
            name: r.f.name,
            kind: r.f.kind,
            divs,
            men: Math.round(men),
            str: r33(str),
            says
          };
        })
      };
    }
    /** The joiners still coming, as the view shows them: those of the player's side and those the player's staff knows. */
    comingView() {
      if (!this.coming.length || this.over) return {};
      const shown = this.coming.filter((c) => this.known(c.u));
      return shown.length ? { coming: shown.map((c) => ({ id: c.u.id, side: c.u.side, corps: c.u.corps, kind: c.u.kind, at: c.at, edge: c.edge, x: c.x, y: c.y, ...c.f.arrive ? { arrive: c.f.arrive } : {} })) } : {};
    }
    // ---- Hold until relieved (release 2) -----------------------------------------------------------------------------
    //
    // Brian, 22 November 12:48Z ("Do 2 and 3"): "a unit ordered to wait for help digs in harder, but breaks faster if help
    // never comes"; the design director's rulings of 13:43Z. A block on foot told to wait for a corps coming up from next
    // door stands where it is, fires on what comes into its reach and never advances; its staff never orders it. While it
    // keeps to its spot it takes the field's light cover (LAND.cover[LAND.wreck.cover], the wrecks' own figure, within
    // LAND.dug.keep, never better than its ground gives), and what that cover spares it is kept: the field moves when harm
    // lands, never how much (b415, b418). It lands with the block if it breaks; else, once the cover ends, it is paid out at
    // the pace it was spared, shared by men with the blocks that relieved it, never as one blow on men whose friends have
    // just come; whatever is left lands at the end of the fight. Help is due when the corps' nearest block could reach it
    // at its own pace, the joiners' own reckoning (comeOn), never a draw; it is relieved once a block of a joining corps
    // stands beside it (LAND.alone.near). Past due unrelieved, or its corps out of the fight, help never came: the men are
    // let down, and break where a block starts to waver (LAND.nerve.waver), not at the rout's mark, until relieved. The
    // player's own order to move, attack, line up or withdraw ends the wait without that mark: men re-ordered are not let
    // down. Each joining corps relieves one block a battle, the player's and every staff's alike (ai.ts relieve). Nothing
    // here draws on the dice.
    /**
     * Whether this battle has corps coming up from next door, on the field yet or not; with a side, whether that side has
     * (a side's own help: the player is never told by it of an enemy's that his staff has not identified).
     */
    hasJoiners(side) {
      if (side === void 0) return this.joinCorps.size > 0;
      return this.coming.some((c) => c.u.side === side) || this.units.some((u) => u.side === side && u.joined !== void 0);
    }
    /** A block of a joining corps that helps this one: its side's, of another corps, in the fight, not guns or a team. */
    helps(r, u) {
      return r.side === u.side && r.joined !== void 0 && r.corps !== u.corps && fighting2(r) && r.kind !== "art";
    }
    /** Help stands beside this block: a block of a joining corps within LAND.alone.near of it, edge to edge. */
    relieved(u) {
      return this.units.some((r) => this.helps(r, u) && this.gap(r, u) <= LAND.alone.near);
    }
    /** This corps still has a block that is not guns coming up or in the fight: its help may yet come. */
    canHelp(corps) {
      return this.coming.some((c) => c.u.corps === corps && c.u.kind !== "art") || this.units.some((r) => r.corps === corps && r.joined !== void 0 && fighting2(r) && r.kind !== "art");
    }
    /**
     * When help could reach this block, and whose: of its side's joining corps not yet waited for, the one whose nearest
     * block (not guns) could be there soonest, from where it stands, or from its place on its edge at its time while it is
     * still coming, at its own pace over open ground (comeOn's own reckoning). Null where none can come.
     */
    dueAt(u, skip) {
      let corps = -1, due = Infinity;
      const take = (c, d) => {
        if (d < due - 1e-9 || Math.abs(d - due) <= 1e-9 && c < corps) {
          corps = c;
          due = d;
        }
      };
      for (const c of this.coming) {
        if (c.u.side !== u.side || c.u.corps === u.corps || c.u.kind === "art" || this.waitedFor.has(c.u.corps) || skip?.has(c.u.corps)) continue;
        take(c.u.corps, Math.max(this.t, c.at) + Math.hypot(c.x - u.x, c.y - u.y) / Math.max(1, LAND.speed[c.u.kind]));
      }
      for (const r of this.units) if (this.helps(r, u) && !this.waitedFor.has(r.corps) && !skip?.has(r.corps)) take(r.corps, this.t + Math.hypot(r.x - u.x, r.y - u.y) / Math.max(1, LAND.speed[r.kind]));
      return corps < 0 ? null : { corps, due: Math.round(due * 1e3) / 1e3 };
    }
    /**
     * Why this block may not wait for help now, in the player's words; null where it may (words for the UI director).
     * 'skip': corps already spoken for by blocks before it in the same order (waitWhyAll).
     */
    waitWhy(u, skip) {
      if (!this.relief || !this.hasJoiners(u.side)) return "No help is coming up from next door.";
      if (!fighting2(u) || u.kind === "fort") return "It is out of the fight.";
      if (MOTION[u.kind] !== "foot") return "Only men on foot dig in to wait for help.";
      if (u.waited) return u.waits && !u.waits.free ? "It is waiting for help already." : "It has waited for help once today.";
      if (!diggable(coverAt(this.field, u.x, u.y))) return "There is no digging in here.";
      if (this.relieved(u)) return "Help is beside it already.";
      const d = this.dueAt(u, skip);
      if (!d) {
        const any = this.coming.some((c) => c.u.side === u.side && c.u.corps !== u.corps && c.u.kind !== "art") || this.units.some((r) => this.helps(r, u));
        return any ? "Each corps coming up relieves one block, and each is spoken for." : "No help is coming up from next door.";
      }
      if (d.due > this.ticket.limit) return "Help could not reach it before the day is out.";
      return null;
    }
    /**
     * Why each of these blocks, told together and in this order, may not wait: each one that may speaks for its corps
     * before the next is asked, as their orders will land (one block a joining corps).
     */
    waitWhyAll(us) {
      const claimed = /* @__PURE__ */ new Set();
      return us.map((u) => {
        const why = this.waitWhy(u, claimed);
        if (why === null) claimed.add(this.dueAt(u, claimed).corps);
        return why;
      });
    }
    /** The joining corps' name as the dispatches give it. */
    corpsName(side, corps) {
      const f = this.ticket[side].joiners?.find((x) => x.corps === corps);
      return f ? this.ticket[side].human ? f.name : `the ${this.ticket[side].adj} ${f.name}` : "help";
    }
    /** The wait order landing (by radio, the player's): where it may no longer wait, it holds as the core's hold does. */
    wait(u) {
      u.order = null;
      u.path = [];
      u.keepFace = null;
      delete u.aim;
      delete u.search;
      delete u.searchR;
      delete u.cut;
      if (this.waitWhy(u) !== null) return;
      const d = this.dueAt(u);
      u.waits = { corps: d.corps, due: d.due, at: [u.x, u.y], since: this.t };
      u.waited = true;
      this.waitedFor.add(d.corps);
      if (this.ticket[u.side].human) this.say({ what: "land:waits", unit: u.id, side: u.side, text: `${this.named(u)} digs in to wait for ${this.corpsName(u.side, d.corps)}.`, x: u.x, y: u.y });
    }
    /**
     * Each tick, before the staffs think: a waiting block with help beside it is relieved; one whose help is overdue, or
     * whose corps can no longer come, has waited in vain.
     */
    waitOn() {
      for (const u of this.units) {
        if (!u.waits || !fighting2(u)) continue;
        if (this.relieved(u)) this.endWait(u, "relieved");
        else if (!u.waits.late && (this.t >= u.waits.due - 1e-9 || !this.canHelp(u.waits.corps))) this.endWait(u, "late");
      }
    }
    /**
     * A wait ends, or help is late. Late, the block is still waiting: it keeps its spot, its cover and what that cover has
     * spared it, and now carries the mark of help that never came. Relieved, its wait is over and the mark lifts. The
     * player's own order, or giving ground at the brink (b496(1)), ends it, keeping the mark only where help was late
     * already (men let down stay let down until relieved or broken: the design director, 13:43Z). What its cover spared
     * lands at once if it broke, else, once the wait is over, is paid out from then at the pace it was spared (repay),
     * shared with the blocks that relieved it.
     */
    endWait(u, how) {
      const w = u.waits;
      if (!w) return;
      if (how === "late") w.late = true;
      else if ((how === "off" || how === "gives") && w.late) w.free = true;
      else delete u.waits;
      if (how === "broke") this.landNow(u);
      else if (how !== "late" && u.debt !== void 0) {
        const by2 = how === "relieved" ? this.units.filter((r) => this.helps(r, u) && this.gap(r, u) <= LAND.alone.near).map((r) => r.id) : [];
        const d = u.debt, n = u.debtN ?? 0;
        delete u.debt;
        delete u.debtN;
        if (d > 0) this.repay.push({ held: u.id, by: by2, left: d, nerve: n / d, rate: d / Math.max(DT, this.t - w.since) });
      }
      if (!this.ticket[u.side].human) return;
      if (how === "relieved") this.say({ what: "land:relieved", unit: u.id, side: u.side, text: `${this.corpsName(u.side, w.corps)} has reached ${this.named(u)}.`, x: u.x, y: u.y });
      if (how === "late") this.say({ what: "land:late", unit: u.id, side: u.side, text: `No help has reached ${this.named(u)}; its men are losing heart.`, x: u.x, y: u.y });
    }
    /** Each tick, after the blows: a tick's worth of each repayment lands. */
    repayNow() {
      for (const p of this.repay) {
        const d = Math.min(p.left, p.rate * DT);
        p.left -= d;
        this.repayOn(p, d);
      }
      this.repay = this.repay.filter((p) => p.left > 1e-12);
    }
    /**
     * The blocks a repayment lands on, each with its share by the men it has now: the held block and its relievers, on the
     * field or not (one that has left still pays its share, off its strength, as the air's dues are paid: leaving never
     * escapes it, nor hands it to the others).
     */
    repayShares(p) {
      const on = [p.held, ...p.by].map((id) => this.unit(id)).filter((x) => !!x);
      const men = on.map((x) => x.men0 * (Math.max(0, x.str) / Math.max(1e-9, x.str0))), all = men.reduce((a, m) => a + m, 0);
      return on.map((x, i) => [x, all > 0 ? men[i] / all : 1 / on.length]);
    }
    /** Lands d of a repayment on its blocks: on the field as a blow, off it off their strength (harm). */
    repayOn(p, d) {
      if (d <= 0) return;
      for (const [x, k] of this.repayShares(p)) this.harm(x, d * k, p.nerve);
    }
    /** What a wait's cover spared a block, kept or still being paid out, lands on it at once (it broke). */
    landNow(u) {
      const d = u.debt, n = u.debtN ?? 0;
      delete u.debt;
      delete u.debtN;
      if (d !== void 0 && d > 0) this.harm(u, d, n / d);
      for (const p of this.repay) if (p.held === u.id && p.left > 0) {
        const left = p.left;
        p.left = 0;
        this.harm(u, left, p.nerve);
      }
      this.repay = this.repay.filter((p) => p.left > 1e-12);
    }
    /**
     * Kept harm landing on a block: where it still stands, as a blow from whoever had struck it most (whose shells its lost
     * men go to: looks only); off the field, off its strength alone.
     */
    harm(u, d, nerve) {
      if (d <= 0) return;
      if (!live2(u)) {
        this.offStrength(u, d);
        return;
      }
      const owed = u.owed ?? {};
      let by2;
      for (const id of Object.keys(owed)) {
        const s = this.unit(Number(id));
        if (s && live2(s) && (!by2 || owed[s.id] > owed[by2.id])) by2 = s;
      }
      this.suffer(u, d, nerve, by2 ? [[by2, d]] : [], false, true);
    }
    /** Harm off a block's strength alone, as the air's dues are paid off the field (offField). */
    offStrength(u, d) {
      u.str = Math.max(0, u.str - d / Math.max(1e-9, u.power0) * u.str0);
      u.figures = u.str <= 0 ? 0 : Math.max(1, Math.ceil(u.tray * u.str));
    }
    /** The day is done: what waits' cover spared, kept or still being paid out, lands before the count. */
    payWaits() {
      if (!this.relief) return;
      for (const p of this.repay) {
        const left = p.left;
        p.left = 0;
        this.repayOn(p, left);
      }
      this.repay = [];
      for (const u of this.units) if (u.debt !== void 0) {
        const d = u.debt, n = u.debtN ?? 0;
        delete u.debt;
        delete u.debtN;
        if (d > 0) this.harm(u, d, n / d);
      }
    }
    /** What blocks on the field are owed, were the day settled now: the fighters on call's, and every wait's (the staff's chance). */
    dueNow() {
      let due = this.airDue();
      if (!this.relief) return due;
      const add = (id, d) => {
        due ?? (due = /* @__PURE__ */ new Map());
        due.set(id, (due.get(id) ?? 0) + d);
      };
      for (const u of this.units) if (u.debt && live2(u)) add(u.id, u.debt);
      for (const p of this.repay) for (const [x, k] of this.repayShares(p)) if (live2(x)) add(x.id, p.left * k);
      return due;
    }
    // ---- Hedge cutters (release 2) ----------------------------------------------------------------------------------
    //
    // Brian, 22 November 14:19Z ("Let's do 1 and 2": "bladed Shermans punch a gap through a hedgerow where the player
    // picks"); the design director's ruling 242, C1-A to C4-A (LAND.cutters). A tank block carries cutters from July 1944
    // on a field with hedges, whatever its army. Told to cut at a point on a hedge line, it drives for that point and on
    // through it till its rear stands LAND.cutters.near metres beyond, an order like any other (by radio, the player's,
    // and gated again as it lands). As its front reaches the hedge the cut begins (t0, a land:cut event), and LAND.cutters.secs later
    // a gap stands open there (t1): a Gate of kind 'cut' the drawn men of every block on either side walk through from
    // then. A block its staff runs cuts wherever its own way crosses a hedge line with no gap near, with no order at all
    // (C4-A); the player's own blocks only where he orders it. Looks only (C1-A): the gates are read by the drill alone
    // (drill.ts via and gateOn: no number of the fight reads this.gates, and gateOn's only caller is the drill), the
    // block's way is its move's own, and nothing here draws on the dice; so the fight is the same, cut or not. With the
    // bocage switch on as well (C1-B, "The bocage" below), the gates are read by the way a tank block without cutters
    // finds (barrier()): there a gap open is a way through for any tank, and the fight changes, measured.
    /** Whether this block carries hedge cutters: the switch on, a tank block, from LAND.cutters.from, on a field with hedges. */
    cutterOf(u) {
      return this.cutters && u.kind === "arm" && this.ticket.turn >= LAND.cutters.from && this.hedges.length > 0;
    }
    /**
     * Why this block may not cut through the hedge at (x, y) now, in the player's words (129(d)); null where it may. With
     * no point, whether it may cut anywhere. Whose block it is, is the caller's to ask (the bridge's words).
     */
    cutWhy(u, x, y) {
      if (!this.cutters) return "Your tanks carry no hedge cutters.";
      if (!fighting2(u) || u.kind === "fort") return "It is out of the fight.";
      if (u.kind !== "arm") return "Only tanks carry hedge cutters.";
      if (this.ticket.turn < LAND.cutters.from) return "Hedge cutters come in July 1944.";
      if (!this.hedges.length) return "There are no hedges on this field.";
      if (x === void 0 && y === void 0) return null;
      const px = Number(x), py = Number(y);
      if (!Number.isFinite(px) || !Number.isFinite(py) || px < 0 || py < 0 || px > this.field.w || py > this.field.h) return "Off the field.";
      if (!this.hedgeNear(px, py, LAND.cutters.near)) return "There is no hedge there.";
      return null;
    }
    /**
     * The nearest point of a hedge line to (x, y) within r metres, and the way the line runs there (radians from east, as
     * a Gate's yaw); null where none passes that near.
     */
    hedgeNear(x, y, r) {
      let best = null, bd = r;
      const seen = /* @__PURE__ */ new Set();
      for (let c = Math.floor((x - r) / GATE_CELL); c <= Math.floor((x + r) / GATE_CELL); c++)
        for (let q = Math.floor((y - r) / GATE_CELL); q <= Math.floor((y + r) / GATE_CELL); q++)
          for (const s of this.hedgeSegs.get(c * 4096 + q) ?? []) {
            if (seen.has(s)) continue;
            seen.add(s);
            const dx = s[2] - s[0], dy = s[3] - s[1], l = dx * dx + dy * dy;
            if (l <= 0) continue;
            const k = Math.max(0, Math.min(1, ((x - s[0]) * dx + (y - s[1]) * dy) / l)), px = s[0] + k * dx, py = s[1] + k * dy, d = Math.hypot(x - px, y - py);
            if (d <= bd) {
              bd = d;
              best = { x: px, y: py, yaw: Math.atan2(dy, dx) };
            }
          }
      return best;
    }
    /**
     * The cut order landing (by radio, the player's): where it may no longer cut there, it holds, as the core's hold does
     * (as wait does). Else it drives for the nearest point of the hedge line and on through it, the cut its order's.
     */
    cutGo(u, x, y) {
      if (this.cutWhy(u, x, y) !== null) {
        u.order = null;
        u.path = [];
        u.keepFace = null;
        delete u.aim;
        delete u.search;
        delete u.searchR;
        delete u.cut;
        return;
      }
      const p = this.hedgeNear(x, y, LAND.cutters.near);
      const k = Math.hypot(p.x - u.x, p.y - u.y), [ux, uy] = k > 1e-6 ? [(p.x - u.x) / k, (p.y - u.y) / k] : ahead(u.face);
      const on = u.d / 2 + LAND.cutters.near;
      this.send(u, { kind: "move", x: p.x + ux * on, y: p.y + uy * on });
      u.cut = { x: p.x, y: p.y, yaw: p.yaw };
    }
    /** The point lies on this block's front or inside its ground: its front has reached it. */
    fronted(u, x, y) {
      const [fx, fy] = ahead(u.face), dx = x - u.x, dy = y - u.y;
      const along = dx * fx + dy * fy, across = Math.abs(dx * fy - dy * fx);
      return along <= u.d / 2 + 0.5 && along >= -u.d / 2 && across <= u.w / 2;
    }
    /** A block its staff runs: the computer's, or one of the player's his staff fights (one he has not taken in hand). */
    staffRuns(u) {
      return this.ai[u.side] && !u.taken;
    }
    /** A gap within LAND.cutters.width of (x, y): a gate, or a cut under way. */
    gapNear(x, y) {
      const W = LAND.cutters.width;
      return this.gates.some((g) => Math.hypot(g.x - x, g.y - y) <= W) || this.cuts.some((c) => !c.open && Math.hypot(c.x - x, c.y - y) <= W);
    }
    /**
     * Each tick, once the blocks have moved ('before': where each stood; 'moved': where its own move took it, before the
     * blocks were eased apart): a block whose front has reached the hedge it was sent to cut begins its cut, unless a gap
     * stands there already (another block's cut, or a gateway: it goes through that); one whose way there has ended short
     * of it drops it. A block its staff runs that carries cutters, whose own way this tick (its move, never a shove from a
     * neighbour) crossed a hedge line with no gap near the crossing, cuts there. Geometry only.
     */
    cutOn(before, moved) {
      for (const u of this.units) {
        if (u.cut) {
          const c = u.cut;
          if (!fighting2(u)) delete u.cut;
          else if (this.fronted(u, c.x, c.y)) {
            delete u.cut;
            if (!this.gapNear(c.x, c.y)) this.cutBegin(u, c.x, c.y, c.yaw);
          } else if (u.order?.kind !== "move") delete u.cut;
          continue;
        }
        if (!live2(u) || !this.staffRuns(u) || !this.cutterOf(u)) continue;
        const b0 = before.get(u.id), b1 = moved.get(u.id);
        if (!b0 || !b1 || b0[0] === b1[0] && b0[1] === b1[1]) continue;
        const p = this.hedgeCross(b0[0], b0[1], b1[0], b1[1]);
        if (!p || this.gapNear(p[0], p[1])) continue;
        const d = this.hedgeDir(p[0], p[1]);
        this.cutBegin(u, p[0], p[1], d ? Math.atan2(d[1], d[0]) : 0);
      }
    }
    /**
     * A cut begins: kept, and told (a land:cut event, with when the gap opens, the hedge's yaw and the gap's width). Its
     * words go to the player for his own blocks; an enemy's is told with no words, for the 3D game alone (as every event
     * of a block he has not met is kept from him).
     */
    cutBegin(u, x, y, yaw) {
      const C = LAND.cutters, r13 = (v) => Math.round(v * 10) / 10, r33 = (v) => Math.round(v * 1e3) / 1e3;
      const c = { t0: r33(this.t), t1: r33(this.t + C.secs), unit: u.id, side: u.side, x: r13(x), y: r13(y), yaw: r33(yaw), width: C.width, known: this.known(u), open: false };
      this.cuts.push(c);
      const words = this.ticket[u.side].human || !this.ticket[other(u.side)].human;
      this.say({ what: "land:cut", unit: u.id, side: u.side, text: words ? `${this.named(u)} cuts a gap through the hedge.` : "", x: c.x, y: c.y, extra: { t1: c.t1, yaw: c.yaw, width: c.width } });
    }
    /** Each tick, as the clock moves on: a cut whose time is up stands open as a gate, for the drawn men from now. */
    openCuts() {
      for (const c of this.cuts) {
        if (c.open || this.t < c.t1 - 1e-9) continue;
        c.open = true;
        this.gates.push({ x: c.x, y: c.y, yaw: c.yaw, width: c.width, kind: "cut" });
        if (this.bocage) {
          this.bars = null;
          this.barsLaid++;
        }
      }
    }
    /** The cuts a side sees: its own, and those of enemy blocks it knew as they cut; every one with no side (a recording). */
    cutView(as) {
      if (!this.cutters || !this.cuts.length) return {};
      const l = this.cuts.filter((c) => as === void 0 || c.side === as || c.known);
      return l.length ? { cuts: l.map((c) => ({ t0: c.t0, t1: c.t1, unit: c.unit, x: c.x, y: c.y, yaw: c.yaw, width: c.width })) } : {};
    }
    // ---- The bocage (release 2) ---------------------------------------------------------------------------------------
    //
    // The design director's ruling 242 C1-B, built as amended 17:45Z from the historian ("unfitted tanks cross only at
    // gaps"): the device's real point (the readings, C1-B: tanks without cutters "diving into the ditches nose first, to
    // be stuck there and become easy targets"; the historian's K21). On a field with hedges, with the bocage switch on, a
    // tank block that carries no cutters (before July 1944, or with the cutters switch off) crosses a hedge line only at a
    // gap in it: a gateway the 3D game laid, or a gap a cutter tank has cut and that stands open ("Once a gap is open, any
    // vehicle uses it"). It finds its way round to one, as round a river to its bridge (going 'hedged': the hedge lines
    // bar the cells about them, barrier()), and is never eased over one. A tank block carrying cutters goes on as before:
    // its cut is drawn (C1-A), and takes none of its time (the cut's LAND.cutters.secs stays the drawing's: the historian's
    // bound of 3 real minutes or less, a breach and the drive to the next bank, holds it). Lorries, guns and foot are not
    // held: the ruling holds tanks. Only the way changes: nothing here draws on the dice, and a block's way is found the
    // same each time. It changes the fight (slower tanks in the bocage): measured against off (the cutters and smoke
    // tests' (10b), LONG=1).
    /** Whether a straight way from (x0, y0) to (x1, y1) crosses a hedge line where it is barred (barrier()): not at a gap. */
    barredCross(x0, y0, x1, y1) {
      const bar = this.barrier(), len = Math.hypot(x1 - x0, y1 - y0);
      for (let skip = 0, p = this.hedgeCross(x0, y0, x1, y1); p; p = this.hedgeCross(x0, y0, x1, y1, skip)) {
        if (bar[this.cellAt(p[0], p[1])]) return true;
        skip = Math.hypot(p[0] - x0, p[1] - y0) + 0.01;
        if (skip >= len) break;
      }
      return false;
    }
    /** Held back by hedges (the bocage on): a tank block without cutters, on a field with hedges. */
    heldBack(u) {
      return this.bocage && u.kind === "arm" && this.hedges.length > 0 && !this.cutterOf(u);
    }
    /**
     * The field's cells a hedge line bars to a tank block without cutters: each cell whose middle lies within HEDGE_BAR
     * cells of a hedge line, but for those within HEDGE_GAP cells of a gap in it (this.gates: a gateway, or a cut open).
     * Laid when first asked and again once a gap opens.
     */
    barrier() {
      if (this.bars) return this.bars;
      const f = this.field, C = f.cols, R = f.rows, cs = f.cell, bar = new Uint8Array(C * R), r = HEDGE_BAR * cs;
      const mid = (i) => [i % C * cs + cs / 2, Math.floor(i / C) * cs + cs / 2];
      for (const h of this.hedges) for (let k = 1; k < h.points.length; k++) {
        const [ax, ay] = h.points[k - 1], [bx, by2] = h.points[k];
        const c0 = Math.max(0, Math.floor((Math.min(ax, bx) - r) / cs)), c1 = Math.min(C - 1, Math.floor((Math.max(ax, bx) + r) / cs));
        const r0 = Math.max(0, Math.floor((Math.min(ay, by2) - r) / cs)), r13 = Math.min(R - 1, Math.floor((Math.max(ay, by2) + r) / cs));
        for (let q = r0; q <= r13; q++) for (let c = c0; c <= c1; c++) if (segGap(c * cs + cs / 2, q * cs + cs / 2, ax, ay, bx, by2) <= r) bar[q * C + c] = 1;
      }
      const gr = HEDGE_GAP * cs;
      for (const g of this.gates) {
        const c0 = Math.max(0, Math.floor((g.x - gr) / cs)), c1 = Math.min(C - 1, Math.floor((g.x + gr) / cs));
        const r0 = Math.max(0, Math.floor((g.y - gr) / cs)), r13 = Math.min(R - 1, Math.floor((g.y + gr) / cs));
        for (let q = r0; q <= r13; q++) for (let c = c0; c <= c1; c++) {
          const [mx, my] = mid(q * C + c);
          if (Math.hypot(mx - g.x, my - g.y) <= gr) bar[q * C + c] = 0;
        }
      }
      return this.bars = bar;
    }
    // ---- Smoke on call (release 2) ----------------------------------------------------------------------------------
    //
    // Brian, 22 November 14:19Z ("a section lays a smoke line to cover a crossing, when and where the player orders it");
    // the design director's ruling 242, S1-A to S6, as amended 17:45Z (LAND.smoke). A block on foot, or lorried infantry,
    // whose army has the means lays a screen along a line he draws: clipped to its own front about the line's middle,
    // which must lie within its army's reach of its front (an army whose means is set down at its own front lays it there,
    // for a line drawn on its ground or within LAND.smoke.front of its front), at once as the order lands (by radio, the
    // player's, on its own place on the air beside his other order, gated again then); one at a time, its army's count a
    // battle. While it stands (t < until), a direct blow whose line from the shooter's middle to the target's middle
    // crosses it, either side's screen, strikes at LAND.smoke.share, never on top of the vague share nor of fire on cover
    // at a block still hidden (blow); and, with hiding on, no hidden block is seen through it (spotHidden). Fire from far
    // back is not touched: guns beyond LAND.guns.closeAt, mortar teams, and the air (the share a side holds back for its
    // air strikes is held back from the blow whole, as if no screen stood). It gives no cover, does no harm and shakes no
    // one. Both staffs lay one as a foot block of theirs goes in (ai.ts smokeIn). The drill draws each screen as the rounds
    // that lay it, in place of its own salvoes (S6).
    // The dice: where a screen acts it draws nothing and spares no draw. A screened blow draws its luck as it would; the
    // veil (spotHidden) draws nothing. What follows from it changes the fight, and so the draws that come after, as every
    // rule that changes the fight does: a weaker blow breaks a block later, and a block kept hidden by the veil (S1-A as the
    // build spec reads it: nobody seen through a screen) is not fired on until it is found, so the blows it would have
    // drawn are not drawn. That is the screen's effect, measured with the rest (S5-A), not a draw of its own.
    /** This block's section smoke: the switch on, a block on foot or lorried infantry (not a support team) of an army with the means. */
    smokeOf(u) {
      if (!this.smoke || u.team || MOTION[u.kind] !== "foot" && u.kind !== "mot") return null;
      return smokeMeans(u.nat);
    }
    /** The screen this block laid that still stands, if any. */
    screenOf(u) {
      return this.screens.find((s) => s.unit === u.id && this.t < s.until);
    }
    /** Metres from (x, y) to this block's front edge. */
    frontGap(u, x, y) {
      const [fx, fy] = ahead(u.face), cx = u.x + fx * u.d / 2, cy = u.y + fy * u.d / 2, hw = u.w / 2;
      return segGap(x, y, cx - fy * hw, cy + fx * hw, cx + fy * hw, cy - fx * hw);
    }
    /**
     * Why this block may not lay smoke now, in the player's words (129(d)); null where it may. With a line (x0, y0 to x1,
     * y1, or only its middle x0, y0), whether it may lay it there; with none, whether it may lay one at all. Whose block it
     * is, is the caller's to ask (the bridge's words).
     */
    smokeWhy(u, x0, y0, x1, y1) {
      if (!this.smoke) return "There is no smoke to lay in this battle.";
      if (!fighting2(u) || u.kind === "fort") return "It is out of the fight.";
      if (u.team || MOTION[u.kind] !== "foot" && u.kind !== "mot") return "Only troops on foot lay smoke.";
      const m = smokeMeans(u.nat);
      if (!m) return "Your army has no smoke to lay.";
      if ((u.smokeLeft ?? m.count ?? LAND.smoke.count) <= 0) return "They have no smoke left.";
      if (this.screenOf(u)) return "Their last screen still stands.";
      if (x0 === void 0 && y0 === void 0) return null;
      const a = Number(x0), b = Number(y0), c = x1 === void 0 ? a : Number(x1), d = y1 === void 0 ? b : Number(y1);
      if (![a, b, c, d].every(Number.isFinite)) return "Off the field.";
      const mx = (a + c) / 2, my = (b + d) / 2;
      if (m.reach > 0 && this.frontGap(u, mx, my) > m.reach + 1e-9) return `They can lay smoke up to ${m.reach} m ahead.`;
      if (m.reach <= 0 && !this.fronted(u, mx, my) && this.frontGap(u, mx, my) > LAND.smoke.front + 1e-9) return "They set their smoke down along their own front.";
      return null;
    }
    /**
     * Where this block lays its screen for the line drawn: along it, at most its own front long, about the line's middle
     * (a point: its front long, square to its face); set down along its own front where its army's means is (reach 0).
     */
    smokeLine(u, l, m) {
      const [fx, fy] = ahead(u.face), r13 = (v) => Math.round(v * 10) / 10;
      let mx, my, dx = fy, dy = -fx, half = u.w / 2;
      if (m.reach <= 0) {
        mx = u.x + fx * u.d / 2;
        my = u.y + fy * u.d / 2;
      } else {
        const [x0, y0, x1, y1] = l, len = Math.hypot(x1 - x0, y1 - y0);
        mx = (x0 + x1) / 2;
        my = (y0 + y1) / 2;
        if (len > 1e-6) {
          dx = (x1 - x0) / len;
          dy = (y1 - y0) / len;
          half = Math.min(len / 2, u.w / 2);
        }
      }
      return [r13(mx - dx * half), r13(my - dy * half), r13(mx + dx * half), r13(my + dy * half)];
    }
    /**
     * The smoke order landing (by radio, the player's): where it may no longer lay it there, nothing is laid and it goes
     * on as it was. Else the screen is laid at once, its count goes down, and it is told (land:smoke); the drill draws its
     * rounds. Its order is not touched: a block lays smoke as it goes.
     */
    lay(u, l) {
      if (this.smokeWhy(u, l[0], l[1], l[2], l[3]) !== null) return;
      const m = this.smokeOf(u), [x0, y0, x1, y1] = this.smokeLine(u, l, m);
      const s = { id: this.screens.length + 1, side: u.side, unit: u.id, x0, y0, x1, y1, t: Math.round(this.t * 1e3) / 1e3, until: Math.round((this.t + m.hang) * 1e3) / 1e3 };
      this.screens.push(s);
      u.smokeLeft = (u.smokeLeft ?? m.count ?? LAND.smoke.count) - 1;
      this.say({ what: "land:smoke", unit: u.id, side: u.side, text: `${this.named(u)} lays a smoke screen.`, x: Math.round((x0 + x1) / 2 * 10) / 10, y: Math.round((y0 + y1) / 2 * 10) / 10, extra: { screen: s.id, until: s.until } });
      this.drill?.screen(s);
    }
    /** A live screen lies across the line from (ax, ay) to (bx, by). */
    screened(ax, ay, bx, by2) {
      for (const s of this.screens) if (this.t < s.until && segHit(ax, ay, bx, by2, s.x0, s.y0, s.x1, s.y1) !== null) return true;
      return false;
    }
    /** A blow struck straight at what it sees (foot, tanks, a machine-gun team, a gun over open sights), not from far back (guns beyond their close range, a mortar team). */
    direct(u, t) {
      if (u.team === "mor") return false;
      return !(u.kind === "art" && !u.team && this.gap(u, t) >= gunsOf(u).closeAt);
    }
    /** The screens standing now, as a side sees them: every one, the laying block named where the side has met it. */
    screenView(as) {
      if (!this.smoke || !this.screens.length) return {};
      const on = this.screens.filter((s) => this.t < s.until);
      if (!on.length) return {};
      return {
        screens: on.map((s) => {
          const u = this.unit(s.unit), named = as === void 0 || s.side === as || !u || this.known(u);
          return { id: s.id, side: s.side, ...named ? { unit: s.unit } : {}, x0: s.x0, y0: s.y0, x1: s.x1, y1: s.y1, t: s.t, until: s.until };
        })
      };
    }
    // ---- Field telephone lines (release 2) ----------------------------------------------------------------------------
    //
    // Brian, 22 November 17:33Z ("lets do all three but make it where we can toggle it off"); the design director's
    // rulings of 18:36Z, PHONES Q1 to Q3 (LAND.phones). As the clock starts, every block of the defence on the field has a
    // wire from its headquarters (the middle of the defence zone's back edge); a corps that comes up from next door later
    // has none, and its orders go as ever. The wire runs straight to its block's middle and follows it: moving never cuts
    // it. Each blow on the block from an enemy gun block or mortar team cuts a line at LAND.phones.cut, drawn from the
    // wires' own stream ('phones:' and the seed), only for such blows and only while the wire has a line up; the cut lies
    // where the wire comes nearest the block's front. An enemy tank block whose step, its middle from where it stood to
    // where it stands, crosses the wire cuts a line outright, where it crosses. A wire is down once all its lines are cut
    // (Germany's double lines need two cuts). While it is down, every order to its block, the player's (radio switch on or
    // off) or a staff's, waits its army's fallback from when it was given: the wireless, LAND.radio's wait (longer for a
    // block whose headquarters is hit, as the radio switch reckons it), or a runner, the metres from the headquarters over
    // LAND.phones.runner. A later order takes its place and waits afresh, but a staff giving the same order again (it
    // thinks every second) leaves the one on its way be. At a wire's first cut with no lineman out on it, one leaves the
    // headquarters, walks the wire to the cut at foot pace and splices it in LAND.phones.splice seconds: the mend clears
    // every cut on the wire, and whatever was waiting for it goes at once. He is looks only: never struck, no harm. The
    // rules' own moves (the joiners walking on, a block giving ground at the brink) are no orders and never wait. Nothing
    // here draws on the battle's dice.
    /** The defence's headquarters, where every wire runs from: the middle of its zone's back edge (field.ts zonesOf). */
    phonesHq() {
      const z = this.field.zones.def, r13 = (v) => Math.round(v * 10) / 10;
      return [r13((z[0] + z[2]) / 2), r13(z[3])];
    }
    /** This block's wire, where it has one (the switch on, a block of the defence on the field as the clock started). */
    wireOf(u) {
      return this.phones ? this.wireBy.get(u.id) ?? null : null;
    }
    /** This block's wire is down: every line of it cut, and no lineman has mended it yet. */
    wireDown(u) {
      const w = this.wireOf(u);
      return !!w && w.cut >= w.lines;
    }
    /** How orders go to this block while its wire is down: its army's fallback in the battle's year (LAND.phones.fallback). */
    fallbackOf(u) {
      return phoneFallback(u.nat, turnToDate(this.ticket.turn).year);
    }
    /**
     * How long an order to this block waits while its wire is down, battle seconds: the wireless, LAND.radio's wait as the
     * radio switch reckons it (its headquarters hit: wavering or under hqAt of its strength); or a runner from the
     * headquarters to where it stands.
     */
    fallbackWait(u) {
      if (this.fallbackOf(u) === "radio") {
        const R = LAND.radio, hit = u.state === "wavering" || u.str < R.hqAt * u.str0;
        return R.delay * (hit ? R.cut : 1) * this.radioOf(u);
      }
      const [hx, hy] = this.phonesHq();
      return Math.hypot(u.x - hx, u.y - hy) / LAND.phones.runner;
    }
    /** Each tick, before the orders land and the staffs think: the wires laid as the clock starts, and the mends whose time has come. */
    phonesOn() {
      if (!this.phoneRng) this.layWires();
      for (const w of this.wires) if (w.out && this.t >= w.out.t - 1e-9) this.mend(w);
    }
    /** The clock starts: a wire to every block of the defence on the field, its own dice made (the day's luck's way). */
    layWires() {
      this.phoneRng = new Rng(hashString(this.ticket.attempt ? `phones:${this.ticket.seed}:${this.ticket.attempt}` : `phones:${this.ticket.seed}`));
      const hq = this.phonesHq();
      for (const u of this.units) {
        if (u.side !== "def" || !live2(u)) continue;
        const w = { id: this.wires.length + 1, side: u.side, unit: u.id, hq, lines: phoneLines(u.nat), cut: 0, out: null };
        this.wires.push(w);
        this.wireBy.set(u.id, w);
      }
    }
    /**
     * A blow struck on a block at its wire's end: from an enemy gun block or mortar team, while the wire has a line up, it
     * cuts one at LAND.phones.cut, where the wire comes nearest the block's front. Machine guns, foot and tanks never.
     */
    wireBlow(u, t) {
      const w = this.wireBy.get(t.id);
      if (!w || w.cut >= w.lines || u.side === w.side || !(u.team === "mor" || u.kind === "art" && !u.team)) return;
      if (this.phoneRng.next() >= LAND.phones.cut) return;
      const [fx, fy] = ahead(t.face), px = t.x + fx * t.d / 2, py = t.y + fy * t.d / 2;
      const dx = t.x - w.hq[0], dy = t.y - w.hq[1], l = dx * dx + dy * dy;
      const k = l > 0 ? Math.max(0, Math.min(1, ((px - w.hq[0]) * dx + (py - w.hq[1]) * dy) / l)) : 1;
      this.wireCut(w, w.hq[0] + k * dx, w.hq[1] + k * dy, u);
    }
    /** Each tick, once the blocks have moved: an enemy tank block whose step crossed a wire with a line up cuts one there. */
    wireTanks(was) {
      for (const [u, x0, y0] of was) {
        if (u.x === x0 && u.y === y0) continue;
        for (const w of this.wires) {
          const e = w.cut < w.lines && w.side !== u.side ? this.unit(w.unit) : void 0;
          if (!e || !live2(e)) continue;
          const k = segHit(x0, y0, u.x, u.y, w.hq[0], w.hq[1], e.x, e.y);
          if (k !== null) this.wireCut(w, x0 + (u.x - x0) * k, y0 + (u.y - y0) * k, u);
        }
      }
    }
    /**
     * A line of the wire is cut: kept, and told to the defence (land:wire). At the wire's first cut with no lineman out, one
     * leaves the headquarters now; he reaches the cut at foot pace and the wire is mended LAND.phones.splice seconds later.
     */
    wireCut(w, x, y, by2) {
      const r13 = (v) => Math.round(v * 10) / 10, r33 = (v) => Math.round(v * 1e3) / 1e3;
      w.cut++;
      const c = { t: r33(this.t), wire: w.id, x: r13(x), y: r13(y), by: by2.id };
      this.wireCuts.push(c);
      if (!w.out) {
        const t1 = r33(this.t + Math.hypot(c.x - w.hq[0], c.y - w.hq[1]) / LAND.speed.inf);
        w.out = { t: r33(t1 + LAND.phones.splice), wire: w.id, man: this.wireMends.length + 1, t0: c.t, t1, x: c.x, y: c.y };
        this.wireMends.push(w.out);
      }
      const u = this.unit(w.unit);
      if (!u || !this.phoneWords()) return;
      const down = w.cut >= w.lines, how = this.fallbackOf(u) === "radio" ? "by radio" : "by runner";
      this.say({
        what: "land:wire",
        unit: u.id,
        side: u.side,
        x: c.x,
        y: c.y,
        extra: { wire: w.id, down: down ? 1 : 0, by: this.fallbackOf(u), mend: w.out.t },
        text: down ? `The telephone line to ${this.named(u)} is cut. Orders go ${how} until it is mended.` : `One of the ${w.lines === 2 ? "two telephone lines" : "telephone lines"} to ${this.named(u)} is cut. Orders still go by the ${w.lines - w.cut === 1 ? "other" : "others"}.`
      });
    }
    /** A lineman's splice is done: every cut on the wire is mended, and whatever was waiting for it goes at once. */
    mend(w) {
      const m = w.out, r33 = (v) => Math.round(v * 1e3) / 1e3;
      w.out = null;
      w.cut = 0;
      const u = this.unit(w.unit);
      const now = this.pending.filter((p) => p.id === w.unit && p.held);
      if (now.length) {
        this.pending = this.pending.filter((p) => !(p.id === w.unit && p.held));
        if (u && now.some((p) => !p.smoke)) delete u.orderAt;
        if (u && fighting2(u)) for (const p of now) {
          p.go();
          p.held.went = r33(this.t);
        }
      }
      if (u && this.phoneWords()) this.say({ what: "land:mend", unit: u.id, side: u.side, x: m.x, y: m.y, extra: { wire: w.id, man: m.man }, text: `The line to ${this.named(u)} is mended.` });
    }
    /** The wires' events go to the defence alone: told where it is the player's, or nobody's (a recording), never to a player attacking. */
    phoneWords() {
      return this.ticket.def.human || !this.ticket.att.human;
    }
    /**
     * An order for a block whose wire is down: it waits its army's fallback from when it was given (from). A staff giving
     * the very same order again leaves the one on its way be; any other order takes its place and waits afresh. Stamped
     * (wireHeld) as it is given, and as it goes.
     */
    byFallback(u, go, from, c, smoke = false) {
      const r33 = (v) => Math.round(v * 1e3) / 1e3, key = c ? JSON.stringify(c) : void 0;
      const slot = (p) => p.id === u.id && !p.smoke === !smoke;
      const was = this.pending.find(slot);
      if (this.staffing && was?.held && key !== void 0 && was.key === key) return;
      const held = { t: r33(from), unit: u.id, due: r33(from + this.fallbackWait(u)) };
      this.wireHeld.push(held);
      this.pending = this.pending.filter((p) => !slot(p));
      if (held.due <= this.t + 1e-9) {
        if (!smoke) delete u.orderAt;
        go();
        held.went = r33(this.t);
        return;
      }
      this.pending.push(smoke ? { at: held.due, id: u.id, go, smoke, key, held } : { at: held.due, id: u.id, go, key, held });
      if (!smoke) u.orderAt = held.due;
    }
    /**
     * The wires as a side sees them: the defence its own, and everyone everything where the view is unfogged (no side, or
     * the card knew the defence). A cut's striker is named only where the side has met it.
     */
    phonesView(as) {
      if (!this.phonesSeen(as)) return {};
      const hq = this.phonesHq(), r13 = (v) => Math.round(v * 10) / 10;
      return {
        phones: {
          hq: [...hq],
          wires: this.wires.map((w) => {
            const u = this.unit(w.unit);
            return { id: w.id, side: w.side, pts: [[...w.hq], [r13(u.x), r13(u.y)]], hq: [...w.hq], unit: w.unit, lines: w.lines, cut: w.cut };
          }),
          cuts: this.wireCuts.map((c) => {
            const by2 = this.unit(c.by);
            return { t: c.t, wire: c.wire, x: c.x, y: c.y, ...as === void 0 || !by2 || by2.side === as || this.known(by2) ? { by: c.by } : {} };
          }),
          mends: this.wireMends.map((m) => ({ ...m })),
          held: this.wireHeld.map((h) => ({ ...h }))
        }
      };
    }
    /** Whether a side sees the wires (phonesView): once they are laid, the defence, and everyone where the view is unfogged. */
    phonesSeen(as) {
      return this.phones && this.phoneRng !== null && (as === void 0 || as === "def" || !this.fog);
    }
    /**
     * Whether the player knows this block (b464(2)): his own always; the other side's where the card knew the defence,
     * or the card identified its corps (b476(3)), once one of his has fought it or it has struck one of his in contact
     * (found), and once the battle is over.
     */
    known(u) {
      return !this.fog || this.ticket[u.side].human || this.seen.has(u.corps) || !!u.found || this.over !== null;
    }
    /** A block's name in a dispatch: the side nobody plays carries its country, so it never reads like the player's own (ruling 226(7)). */
    named(u) {
      return this.ticket[u.side].human ? u.name : `${this.ticket[u.side].adj} ${u.name}`;
    }
    /** A practice ground is no place in the campaign: its sides are the player's and theirs, not compass words. */
    get practice() {
      return this.ticket.place.region < 0;
    }
    /** "The Polish army", or on a practice ground "Your army" or "Their army" (the UI director, after the retest). */
    army(s) {
      return this.practice ? this.ticket[s].human ? "Your army" : "Their army" : `The ${this.ticket[s].adj} army`;
    }
    /** "Your army" for the player's own, else as army() names it: the break lines' words (the UI director, 14:44Z). */
    whose(s) {
      return this.ticket[s].human ? "Your army" : this.army(s);
    }
    /** "the Polish defence", or on a practice ground "your defence" or "the defence". */
    sideOf(s, noun) {
      return this.practice ? `${this.ticket[s].human ? "your" : "the"} ${noun}` : `the ${this.ticket[s].adj} ${noun}`;
    }
    // ---- Orders -----------------------------------------------------------------------------------------------------
    command(c, by2) {
      if (this.over) return false;
      const mine = (ids) => ids.map((i) => this.unit(i)).filter((u) => !!u && u.side === by2 && fighting2(u) && u.kind !== "fort" && !(this.staffing && u.waits && !u.waits.free));
      const player = !this.staffing && this.ticket[by2].human;
      const hands = (us) => {
        if (this.perUnit && player) for (const u of us) this.take(u);
      };
      const air = this.radio && player && this.t > 0;
      const said = c.at, from = typeof said === "number" && Number.isFinite(said) ? Math.min(this.t, said) : this.t;
      const give = (u, go, smoke = false) => this.order(u, go, air, from, c, smoke);
      switch (c.do) {
        case "move": {
          const us = mine(c.units);
          if (!us.length) return false;
          hands(us);
          const cx = us.reduce((s, u) => s + u.x, 0) / us.length, cy = us.reduce((s, u) => s + u.y, 0) / us.length;
          for (const u of us) {
            const o = { kind: "move", x: c.x + (us.length > 1 ? u.x - cx : 0), y: c.y + (us.length > 1 ? u.y - cy : 0), face: c.face, ...c.run ? { run: true } : {} };
            give(u, () => this.send(u, o));
          }
          this.say({ what: "contact", side: by2, text: "" });
          this.events.pop();
          this.log.pop();
          return true;
        }
        case "line": {
          const us = mine(c.units);
          if (!us.length) return false;
          hands(us);
          for (const p of lineUp(us, by2, c.x0, c.y0, c.x1, c.y1, 150)) {
            const u = this.unit(p.id);
            give(u, () => this.send(u, { kind: "move", x: p.x, y: p.y, face: p.face }));
          }
          return true;
        }
        case "attack": {
          const t = this.unit(c.target);
          const us = mine(c.units);
          if (!t || t.side === by2 || !live2(t) || t.hid || !us.length) return false;
          hands(us);
          const cx = us.reduce((s, u) => s + u.x, 0) / us.length, cy = us.reduce((s, u) => s + u.y, 0) / us.length;
          const axis = bearing(cx, cy, t.x, t.y);
          const [rx, ry] = ahead(axis + Math.PI / 2);
          const across = [...us].sort((a, b) => (a.x - cx) * rx + (a.y - cy) * ry - ((b.x - cx) * rx + (b.y - cy) * ry));
          const step = across.reduce((s, u) => s + u.w, 0) / across.length + 30;
          const foes = this.units.filter((e) => e.side !== by2 && fighting2(e) && !e.hid);
          const together = c.together !== void 0 ? { together: c.together } : us.length > 1 && LAND.together.player ? { together: ++this.groupIds } : {};
          across.forEach((u, i) => {
            const lane = (i - (us.length - 1) / 2) * step;
            if (us.length > 1 && Math.abs(lane) > t.w / 2 + u.w / 2) {
              const px = t.x + rx * lane, py = t.y + ry * lane;
              const e = foes.reduce((m, f) => dist(f.x, f.y, px, py) < dist(m.x, m.y, px, py) ? f : m, t);
              if (e !== t) {
                const o2 = { kind: "attack", x: e.x, y: e.y, target: e.id, press: true, lane: lane - ((e.x - t.x) * rx + (e.y - t.y) * ry), axis, ...together };
                give(u, () => this.send(u, o2));
                return;
              }
            }
            const o = { kind: "attack", x: t.x, y: t.y, target: t.id, press: us.length > 1, ...us.length > 1 ? { lane, axis } : {}, ...together };
            give(u, () => this.send(u, o));
          });
          return true;
        }
        case "hold": {
          const us = mine(c.units);
          hands(us);
          for (const u of us) give(u, () => {
            u.order = null;
            u.path = [];
            u.keepFace = null;
            delete u.aim;
            delete u.search;
            delete u.searchR;
            delete u.cut;
          });
          return us.length > 0;
        }
        case "withdraw": {
          const us = mine(c.units);
          hands(us);
          for (const u of us) give(u, () => this.send(u, { kind: "withdraw", x: u.x, y: by2 === "att" ? 0 : this.field.h }));
          return us.length > 0;
        }
        case "withdrawAll": {
          if (c.side !== by2) return false;
          this.payAir();
          this.payWaits();
          this.bar[by2] = this.breaking(by2);
          for (const u of this.units) if (u.side === by2 && fighting2(u)) {
            u.state = "withdrawn";
            u.act = "down";
          }
          this.finish(other(by2), "withdrew");
          return true;
        }
        case "auto": {
          if (c.side !== by2 && !this.ai[c.side]) return false;
          this.ai[c.side] = c.on;
          if (c.on) {
            for (const u of this.units) if (u.side === c.side) {
              delete u.taken;
              delete u.cut;
            }
          }
          return true;
        }
        case "dice": {
          this.toDice();
          return true;
        }
        default: {
          if (c.do === "land:air") {
            const r = [c.x0, c.y0, c.x1, c.y1].map(Number);
            return r.every(Number.isFinite) && this.callAir(by2, r);
          }
          if (c.do === "land:place") {
            const many = c.places;
            if (Array.isArray(many)) {
              const us = (c.units ?? []).map((id) => this.unit(Number(id))), at = many.map((p) => Array.isArray(p) ? p.map(Number) : []);
              if (by2 !== this.setup || us.some((v) => !v) || this.placeManyWhy(us, at) !== null) return false;
              us.forEach((v, i) => {
                const f2 = at[i][2];
                this.place(v, at[i][0], at[i][1], f2 !== void 0 && Number.isFinite(f2) ? f2 : void 0);
              });
              return true;
            }
            const u = this.unit(Number(c.units?.[0])), x = Number(c.x), y = Number(c.y);
            if (!u || this.placeWhy(u, x, y) !== null || by2 !== this.setup) return false;
            const f = c.face === void 0 ? void 0 : Number(c.face);
            this.place(u, x, y, f !== void 0 && Number.isFinite(f) ? f : void 0);
            return true;
          }
          if (c.do === "land:patrol") {
            const u = this.unit(Number(c.unit ?? c.units?.[0]));
            if (!this.patrol || !u || u.side !== by2 || by2 !== this.setup || this.patrolWhy(u) !== null) return false;
            this.patrolBy = u.id;
            return true;
          }
          if (c.do === "land:wait") {
            if (!this.relief) return false;
            const all = mine(c.units ?? []), whys = this.waitWhyAll(all);
            const us = all.filter((_, i) => whys[i] === null);
            if (!us.length) return false;
            hands(us);
            for (const u of us) give(u, () => this.wait(u));
            return true;
          }
          if (c.do === "land:cut") {
            if (!this.cutters) return false;
            const x = Number(c.x), y = Number(c.y);
            if (!Number.isFinite(x) || !Number.isFinite(y)) return false;
            const us = mine(c.units ?? []).filter((u) => this.cutWhy(u, x, y) === null);
            if (!us.length) return false;
            hands(us);
            const p = this.hedgeNear(x, y, LAND.cutters.near), ax = Math.cos(p.yaw), ay = Math.sin(p.yaw);
            const cx = us.reduce((s, u) => s + u.x, 0) / us.length, cy = us.reduce((s, u) => s + u.y, 0) / us.length;
            for (const u of us) {
              const k = (u.x - cx) * ax + (u.y - cy) * ay, q = us.length > 1 ? this.hedgeNear(p.x + ax * k, p.y + ay * k, LAND.cutters.near) : null;
              const [px, py] = q ? [q.x, q.y] : [x, y];
              give(u, () => this.cutGo(u, px, py));
            }
            return true;
          }
          if (c.do === "land:smoke") {
            if (!this.smoke) return false;
            const l = [c.x0, c.y0, c.x1 ?? c.x0, c.y1 ?? c.y0].map(Number);
            if (!l.every(Number.isFinite)) return false;
            const us = mine(c.units ?? []).filter((u) => this.smokeWhy(u, l[0], l[1], l[2], l[3]) === null);
            if (!us.length) return false;
            for (const u of us) give(u, () => this.lay(u, l), true);
            return true;
          }
          if (c.do === "land:staff") {
            const us = mine(c.units ?? []).filter((u) => u.taken);
            for (const u of us) {
              delete u.taken;
              delete u.cut;
            }
            return us.length > 0;
          }
          if (c.do === "land:begin") {
            if (by2 !== this.setup) return false;
            this.setup = null;
            this.room = null;
            for (const r of this.riders) this.riderTo(r);
            return true;
          }
          if (c.do === "land:fire" && c.target === void 0) {
            const x = Number(c.x), y = Number(c.y);
            const us = mine(c.units ?? []).filter((u) => u.kind !== "fort");
            if (!Number.isFinite(x) || !Number.isFinite(y) || !us.length) return false;
            hands(us);
            for (const u of us) give(u, () => {
              u.order = null;
              u.path = [];
              u.keepFace = null;
              delete u.aim;
              u.search = [x, y];
              delete u.searchR;
              delete u.cut;
            });
            return true;
          }
          if (c.do === "land:fire") {
            const t = this.unit(Number(c.target));
            const guns = mine(c.units ?? []).filter((u) => u.kind === "art");
            if (!t || t.side === by2 || !live2(t) || t.hid || !guns.length) return false;
            hands(guns);
            for (const u of guns) give(u, () => {
              u.order = null;
              u.path = [];
              u.keepFace = null;
              u.aim = t.id;
              delete u.cut;
            });
            return true;
          }
          return false;
        }
      }
    }
    /** The player takes a block in hand (direct command): his staff leaves it be; a held reserve he orders is committed. */
    take(u) {
      u.taken = true;
      if (u.reserve === "held") u.reserve = "sent";
    }
    /**
     * An order for one block: at once, or by radio (direct command, release 2), landing LAND.radio's delay later, longer
     * when its headquarters is hit. A later order on the air replaces the one still on the way. A smoke order (smoke on
     * call, release 2: 'smoke') has a place of its own on the air: it replaces only a smoke order of that block's still on
     * the way, never its move or attack, nor is replaced by one, so "move, then smoke to cover it" lands both; and it
     * leaves the block's orderAt (its move's or attack's) alone.
     */
    order(u, go, air, from = this.t, c, smoke = false) {
      if (this.phones && this.t > 0 && this.wireDown(u)) {
        this.byFallback(u, go, from, c, smoke);
        return;
      }
      if (!air) {
        go();
        return;
      }
      const R = LAND.radio, hit = u.state === "wavering" || u.str < R.hqAt * u.str0;
      const at = Math.round((from + R.delay * (hit ? R.cut : 1) * this.radioOf(u)) * 1e3) / 1e3;
      this.pending = this.pending.filter((p) => p.id !== u.id || !p.smoke !== !smoke);
      if (at <= this.t + 1e-9) {
        if (!smoke) delete u.orderAt;
        go();
        return;
      }
      this.pending.push(smoke ? { at, id: u.id, go, smoke } : { at, id: u.id, go });
      if (!smoke) u.orderAt = at;
    }
    /** The player's orders on the air that have reached their blocks (radio): carried out, if the block still fights. */
    landOrders() {
      if (!this.pending.length) return;
      const due = this.pending.filter((p) => p.at <= this.t + 1e-9);
      if (!due.length) return;
      this.pending = this.pending.filter((p) => p.at > this.t + 1e-9);
      for (const p of due) {
        const u = this.unit(p.id);
        if (!u) continue;
        if (!p.smoke) delete u.orderAt;
        if (fighting2(u)) {
          p.go();
          if (p.held) p.held.went = Math.round(this.t * 1e3) / 1e3;
        }
      }
    }
    send(u, o) {
      if (u.waits && !u.waits.free) this.endWait(u, "off");
      o.x = clamp(o.x, u.w / 2, this.field.w - u.w / 2);
      if (o.kind !== "withdraw") o.y = clamp(o.y, 10, this.field.h - 10);
      if (o.kind === "move" && this.wet(u, o.x, o.y, false) && groundAt(this.field, o.x, o.y) !== "bridge") {
        search: for (let r = 8; r <= 240; r += 8) for (let a = 0; a < 16; a++) {
          const x = clamp(o.x + Math.sin(a * Math.PI / 8) * r, u.w / 2, this.field.w - u.w / 2), y = clamp(o.y + Math.cos(a * Math.PI / 8) * r, 10, this.field.h - 10);
          if (!this.wet(u, x, y, false)) {
            o.x = x;
            o.y = y;
            break search;
          }
        }
      }
      let there = false;
      if (this.standRule) {
        const h = u.heldAt;
        there = this.standKeeps.back && o.kind === "move" && !!h && h[0] === o.x && h[1] === o.y && this.under(u, o.x, o.y);
        if (!there) u.heldAt = void 0;
      }
      u.order = o;
      u.keepFace = o.face ?? null;
      delete u.aim;
      delete u.search;
      delete u.searchR;
      delete u.cut;
      const t = o.kind === "attack" && o.target !== void 0 ? this.unit(o.target) : void 0;
      const at = t ? this.lanePoint(u, t, this.closeTo(u)) : [o.x, o.y];
      u.path = at && !there ? this.route(u, at[0], at[1]) : [];
    }
    /** A point under the block's own box, as it faces. */
    under(u, x, y) {
      const [fx, fy] = ahead(u.face), [rx, ry] = ahead(u.face + Math.PI / 2), dx = x - u.x, dy = y - u.y;
      return Math.abs(dx * rx + dy * ry) <= u.w / 2 && Math.abs(dx * fx + dy * fy) <= u.d / 2;
    }
    /** How near a block closes on what it attacks before it stands and fights: well inside its reach. */
    closeTo(u) {
      return reachOf(u) * (u.kind === "art" ? 0.85 : 0.6);
    }
    // (A joiner over dry ground keeps off the water only where the river bends away from the dry lanes; with no bend it wades.)
    going(u) {
      if (this.bocage && this.heldBack(u)) return "hedged";
      return u.dry && MOTION[u.kind] === "foot" && (u.joined === void 0 || this.field.bend) ? "ashore" : MOTION[u.kind];
    }
    /** Whether a block going this way may be at (x, y): ground it can enter, and for a tank held back by the hedges no cell a hedge line bars. */
    bears(m, x, y) {
      return this.paceOf(m)[groundAt(this.field, x, y)] > 0 && !(m === "hedged" && this.barrier()[this.cellAt(x, y)]);
    }
    paceOf(m) {
      return m === "ashore" ? this.ashore ?? (this.ashore = { ...LAND.pace.foot, water: 0, blown: 0 }) : m === "hedged" ? LAND.pace.tracks : LAND.pace[m];
    }
    route(u, x, y) {
      const m = this.going(u), f = this.field;
      if (m === "fixed") return [[x, y]];
      const out = this.exit(m, u.x, u.y);
      if (out) return [...out, [x, y]];
      if (this.dry(m, u.x, u.y, x, y)) {
        this.ways.delete(u.id);
        return [[x, y]];
      }
      const goal = this.cellAt(x, y);
      const near = (a, b) => Math.abs(a % f.cols - b % f.cols) <= 2 && Math.abs(Math.floor(a / f.cols) - Math.floor(b / f.cols)) <= 2;
      let w = this.ways.get(u.id), fresh = false;
      for (; ; ) {
        if (!w || w.m !== m || !near(w.goal, goal) || m === "hedged" && w.bars !== this.barsLaid) {
          w = { m, goal, ...this.findWay(m, u.x, u.y, x, y), ...m === "hedged" ? { bars: this.barsLaid } : {} };
          this.ways.set(u.id, w);
          fresh = true;
        }
        const pts = w.pts, end = pts[pts.length - 1];
        let k = pts.length - 1;
        while (k >= 0 && !this.dry(m, u.x, u.y, pts[k][0], pts[k][1])) k--;
        const lost = k < 0 || w.whole && k === pts.length - 1 && dist(u.x, u.y, end[0], end[1]) < 10 && !this.dry(m, end[0], end[1], x, y);
        if (lost && !fresh) {
          w = void 0;
          continue;
        }
        if (k < 0) return [[u.x, u.y]];
        if (this.standRule && this.standKeeps.way) {
          w.blind = false;
          if (w.at !== void 0 && k > w.at) w.fell = false;
          else if (w.at !== void 0 && k < w.at && !w.fell) {
            if (w.stopped || w.at >= pts.length) w.fell = true;
            else {
              k = w.at;
              w.blind = true;
            }
          }
          w.stopped = false;
          w.at = k;
        }
        const way = pts.slice(k);
        if (w.whole && this.dry(m, end[0], end[1], x, y) && (end[0] !== x || end[1] !== y)) way.push([x, y]);
        return way;
      }
    }
    cellAt(x, y) {
      const f = this.field;
      return clamp(Math.floor(y / f.cell), 0, f.rows - 1) * f.cols + clamp(Math.floor(x / f.cell), 0, f.cols - 1);
    }
    /**
     * The way a block standing in ground it cannot enter walks out: cell by cell, side to side, over that same kind of
     * ground (never into water it cannot cross unless it stands in water already) to the nearest cell it can enter.
     * Nothing where it stands on ground it can enter.
     */
    exit(m, x, y) {
      const f = this.field, C = f.cols, pace = this.paceOf(m), s = this.cellAt(x, y), bar = m === "hedged" ? this.barrier() : null;
      const go = (i) => pace[GROUNDS[f.ground[i]]] > 0 && !(bar && bar[i]);
      const mine = (i) => !bar || !this.barredCross(x, y, i % C * f.cell + f.cell / 2, Math.floor(i / C) * f.cell + f.cell / 2);
      if (go(s)) return null;
      const wet = (i) => GROUNDS[f.ground[i]] === "water" || GROUNDS[f.ground[i]] === "blown";
      const inWet = wet(s), back = /* @__PURE__ */ new Map([[s, -1]]);
      const mid = (i) => [i % C * f.cell + f.cell / 2, Math.floor(i / C) * f.cell + f.cell / 2];
      let ring = [s];
      while (ring.length) {
        const next = [];
        let best = -1, bd = Infinity;
        for (const i of ring) for (const [dc, dr] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
          const c = i % C + dc, r = Math.floor(i / C) + dr, j = r * C + c;
          if (c < 0 || c >= C || r < 0 || r >= f.rows || back.has(j)) continue;
          back.set(j, i);
          if (go(j) && mine(j)) {
            const d = dist(x, y, ...mid(j));
            if (d < bd || d === bd && j < best) {
              bd = d;
              best = j;
            }
          } else if (inWet || !wet(j)) next.push(j);
        }
        if (best >= 0) {
          const way = [];
          for (let i = best; i !== s; i = back.get(i)) way.unshift(mid(i));
          return way;
        }
        ring = next;
      }
      return null;
    }
    /**
     * The quickest way over the field's cells from one point to another for a block that moves this way, side to side or
     * corner to corner (never across the corner of ground it cannot enter), then cut to the fewest turning points it can
     * walk straight between. Where the place cannot be reached, the way ends at the nearest cell that can, and says so.
     */
    findWay(m, sx, sy, x, y) {
      const f = this.field, C = f.cols, n = f.ground.length, pace = this.paceOf(m), bar = m === "hedged" ? this.barrier() : null;
      const top = Math.max(...Object.values(pace)), s = this.cellAt(sx, sy), g = this.cellAt(x, y), gc = g % C, gr = Math.floor(g / C);
      const go = (i) => bar && bar[i] ? 0 : pace[GROUNDS[f.ground[i]]];
      const h = (i) => {
        const dc = Math.abs(i % C - gc), dr = Math.abs(Math.floor(i / C) - gr);
        return (Math.max(dc, dr) + (Math.SQRT2 - 1) * Math.min(dc, dr)) / top;
      };
      const cost = new Float64Array(n).fill(Infinity), back = new Int32Array(n).fill(-1), done = new Uint8Array(n);
      const hk = [], hv = [];
      const less = (a, b) => hk[a] < hk[b] || hk[a] === hk[b] && hv[a] < hv[b];
      const swap = (a, b) => {
        [hk[a], hk[b]] = [hk[b], hk[a]];
        [hv[a], hv[b]] = [hv[b], hv[a]];
      };
      const push = (i, key) => {
        hk.push(key);
        hv.push(i);
        for (let k = hk.length - 1; k > 0; ) {
          const p = k - 1 >> 1;
          if (!less(k, p)) break;
          swap(k, p);
          k = p;
        }
      };
      const pop = () => {
        const top0 = hv[0], lk = hk.pop(), lv = hv.pop();
        if (hk.length) {
          hk[0] = lk;
          hv[0] = lv;
          for (let k = 0; ; ) {
            const l = 2 * k + 1, r = l + 1;
            let b = k;
            if (l < hk.length && less(l, b)) b = l;
            if (r < hk.length && less(r, b)) b = r;
            if (b === k) break;
            swap(k, b);
            k = b;
          }
        }
        return top0;
      };
      cost[s] = 0;
      push(s, h(s));
      let best = s, bh = h(s);
      while (hk.length) {
        const i = pop();
        if (done[i]) continue;
        done[i] = 1;
        if (h(i) < bh) {
          bh = h(i);
          best = i;
        }
        if (i === g) break;
        const c = i % C, r = Math.floor(i / C);
        for (let dr = -1; dr <= 1; dr++) for (let dc = -1; dc <= 1; dc++) {
          if (!dr && !dc || c + dc < 0 || c + dc >= C || r + dr < 0 || r + dr >= f.rows) continue;
          const j = i + dr * C + dc;
          if (done[j] || go(j) <= 0) continue;
          if (dr && dc && (go(i + dc) <= 0 || go(i + dr * C) <= 0)) continue;
          const c2 = cost[i] + (dr && dc ? Math.SQRT2 : 1) / Math.max(0.12, go(j));
          if (c2 < cost[j]) {
            cost[j] = c2;
            back[j] = i;
            push(j, c2 + h(j));
          }
        }
      }
      const cs = f.cell, mid = (i) => [i % C * cs + cs / 2, Math.floor(i / C) * cs + cs / 2];
      const raw = [];
      for (let i = best; i !== -1 && i !== s; i = back[i]) raw.unshift(mid(i));
      if (best === g && raw.length) raw[raw.length - 1] = [x, y];
      if (!raw.length) return best === g ? { pts: [[x, y]], whole: true } : { pts: [[sx, sy]], whole: false };
      const pts = [];
      let ax = sx, ay = sy;
      for (let j = 1; j < raw.length; j++) {
        if (!this.dry(m, ax, ay, raw[j][0], raw[j][1])) {
          [ax, ay] = raw[j - 1];
          pts.push(raw[j - 1]);
        }
      }
      pts.push(raw[raw.length - 1]);
      return { pts, whole: best === g };
    }
    /**
     * Whether a block that moves this way can go straight from one point to the other over nothing it cannot enter:
     * every cell the line passes through after the one it starts in, not samples along it, so the line never clips the
     * corner of a water or rock cell. A line through the very corner of two cells goes from one to the other across it.
     */
    dry(m, x0, y0, x1, y1) {
      const f = this.field, cs = f.cell;
      const cOf = (v) => clamp(Math.floor(v / cs), 0, f.cols - 1), rOf = (v) => clamp(Math.floor(v / cs), 0, f.rows - 1);
      let c = cOf(x0), r = rOf(y0);
      const c1 = cOf(x1), r13 = rOf(y1), dx = x1 - x0, dy = y1 - y0, sx = Math.sign(dx), sy = Math.sign(dy);
      const stepX = dx ? cs / Math.abs(dx) : Infinity, stepY = dy ? cs / Math.abs(dy) : Infinity;
      let tx = dx ? (sx > 0 ? (c + 1) * cs - x0 : x0 - c * cs) / Math.abs(dx) : Infinity;
      let ty = dy ? (sy > 0 ? (r + 1) * cs - y0 : y0 - r * cs) / Math.abs(dy) : Infinity;
      const pace = this.paceOf(m), bar = m === "hedged" ? this.barrier() : null;
      for (let n = Math.abs(c1 - c) + Math.abs(r13 - r); n > 0 && (c !== c1 || r !== r13); n--) {
        if (Math.abs(tx - ty) < 1e-9) {
          tx += stepX;
          ty += stepY;
          c += sx;
          r += sy;
          n--;
        } else if (tx < ty) {
          tx += stepX;
          c += sx;
        } else {
          ty += stepY;
          r += sy;
        }
        if (c < 0 || r < 0 || c >= f.cols || r >= f.rows) return true;
        if (pace[GROUNDS[f.ground[r * f.cols + c]]] <= 0) return false;
        if (bar && bar[r * f.cols + c]) return false;
      }
      return true;
    }
    // ---- The tick -----------------------------------------------------------------------------------------------------
    /** The squad brain, made the first time it is asked for (drill.ts). */
    drillOn(opts) {
      if (this.drill) return this.drill;
      const { scale, ...rest } = opts ?? {};
      const n = scale === true ? this.scale() : typeof scale === "number" ? scale : void 0;
      return this.drill = new Drill(this, n !== void 0 ? { ...rest, scale: n } : rest);
    }
    /**
     * The battle's one N (GR1, LAND.scale): every man both sides bring (their blocks' men0, as the battle opens) over the
     * figure budget, rounded up to a multiple of the step; 1 when they fit the budget whole. Sedan, Normandy, Prokhorovka
     * and Kursk give 20, 20, 35 and 125. Looks only. menOf: a block's men as counted (a fogged field counts what the
     * player can know: the bridge's battleStart); a figure that is no number counts none. budget: the figures it is set
     * for (the 2D page's dots: LAND.scale.page). The blocks of corps still coming up from next door count too.
     */
    scale(menOf = (u) => u.men0, budget = LAND.scale.budget) {
      const S = LAND.scale, men = this.allBlocks().reduce((s, u) => {
        const m = menOf(u);
        return s + (Number.isFinite(m) ? Math.max(0, m) : 0);
      }, 0);
      const raw = men / budget;
      return raw <= 1 ? 1 : Math.ceil(raw / S.step) * S.step;
    }
    /**
     * Opens the setup step for one side (release 2): only before the first step. Its guns and support teams may then be
     * placed anywhere in its own zone (or where its own blocks were drawn up), on dry ground they can stand on, clear of
     * every other block; the computer's stand where they were drawn up. Every other order is taken as before the clock
     * starts. False if the battle has begun.
     */
    setupOn(side, all = false) {
      if (this.t > 0 || this.over) return false;
      this.placeAll = all;
      const z = this.field.zones[side];
      const mine = this.units.filter((u) => u.side === side);
      this.room = [
        Math.min(z[0], ...mine.map((u) => u.x - u.w / 2)),
        Math.min(z[1], ...mine.map((u) => u.y - u.d / 2)),
        Math.max(z[2], ...mine.map((u) => u.x + u.w / 2)),
        Math.max(z[3], ...mine.map((u) => u.y + u.d / 2))
      ];
      this.setup = side;
      return true;
    }
    /** Where the placing side may put its guns and teams, [x0, y0, x1, y1] metres; null outside the setup step. */
    setupRoom() {
      return this.room ? [...this.room] : null;
    }
    /**
     * Why a block may not stand there in the setup step, or null where it may (release 2). 'but': blocks placed in the
     * same act, which are not counted where they stand now (placeManyWhy counts them where they will stand).
     */
    placeWhy(u, x, y, but) {
      return this.placeable(u) ?? this.standWhy(u, x, y, this.room, but);
    }
    /** Why a block may not be placed at all in the setup step, wherever it would stand, or null where it may be (release 2). */
    placeable(u) {
      if (!this.setup || u.side !== this.setup) return "The battle has begun.";
      if (this.placeAll ? u.kind === "fort" || !fighting2(u) : u.kind !== "art" || !fighting2(u)) return this.placeAll ? "Fortifications stay where they are built." : "Only guns and support teams are placed.";
      if (u.dug || u.waits) return "It is dug in where it stands.";
      return null;
    }
    /**
     * Why several blocks may not be placed as one act in the setup step (release 2), or null where they all may: each
     * block by its own checks (placeWhy), the others of the act counted where they would stand and not where they stand
     * now. Names the first block that cannot go. A control of the setup step: nothing in the fight reads it.
     */
    placeManyWhy(us, at) {
      if (!us.length || us.length !== at.length) return { id: us[0]?.id ?? -1, why: "Nothing to place." };
      const act = new Set(us);
      if (act.size !== us.length) return { id: us[0].id, why: "A block stands in one place only." };
      for (let i = 0; i < us.length; i++) {
        const u = us[i], x = Number(at[i][0]), y = Number(at[i][1]);
        const why = this.placeWhy(u, x, y, act);
        if (why !== null) return { id: u.id, why };
        for (let j = 0; j < i; j++) {
          const o = us[j];
          if (Math.abs(at[j][0] - x) < (o.w + u.w) / 2 + LAND.placeClear && Math.abs(at[j][1] - y) < (o.d + u.d) / 2 + LAND.placeClear) return { id: u.id, why: "Another block stands there." };
        }
      }
      return null;
    }
    /** Why a block may not stand there inside its side's ground 'r' (the setup step's own checks), or null where it may. */
    standWhy(u, x, y, r, but) {
      if (!Number.isFinite(x) || !Number.isFinite(y)) return "Off the field.";
      const hw = u.w / 2, hd = u.d / 2;
      if (x - hw < r[0] || x + hw > r[2] || y - hd < r[1] || y + hd > r[3]) return "Outside your ground.";
      if (this.wet(u, x, y, false) || wetUnder(this.field, { ...u, x, y })) return "Not on water, a bridge or ground it cannot stand on.";
      const B = this.field.bend;
      if (B && u.side === "att" && (Math.sign(x - B.x) !== Math.sign(u.x - B.x) || Math.abs(x - B.x) < BEND.clear + hw)) return "Its corps keeps to its own side of the river.";
      const St = this.field.stream;
      if (St?.lie === "up") {
        const xAt = (yy) => St.course.reduce((m, p) => Math.abs(p[1] - yy) < Math.abs(m[1] - yy) ? p : m)[0];
        if (Math.sign(x - xAt(y)) !== Math.sign(u.x - xAt(u.y))) return "It stays on its own bank of the river.";
      }
      for (const o of this.units) {
        if (o === u || !live2(o) || but?.has(o)) continue;
        if (Math.abs(o.x - x) < (o.w + u.w) / 2 + LAND.placeClear && Math.abs(o.y - y) < (o.d + u.d) / 2 + LAND.placeClear) return "Another block stands there.";
      }
      return null;
    }
    /** Puts a block where the setup step placed it: drawn up there, facing the enemy (or the way it was turned), with no order. */
    place(u, x, y, face) {
      u.x = x;
      u.y = y;
      u.face = face ?? (u.side === "att" ? 0 : Math.PI);
      u.order = null;
      u.path = [];
      u.keepFace = face ?? null;
      delete u.aim;
      delete u.search;
      delete u.searchR;
      delete u.stand;
      delete u.standFor;
      delete u.standFace;
      delete u.closing;
      delete u.cut;
      this.ways.delete(u.id);
      u.home = [x, y];
      u.cover = groundAt(this.field, x, y);
      if (this.hide && !this.hidStarted) this.markHid(u);
      this.drill?.relay(u);
    }
    // ---- Measured armour (release 2; armour.ts) ----------------------------------------------------------------------
    /** The tank or gun model a block fights with in this battle's month, where the sheet knows it: null for the rest. */
    vehicle(u) {
      if (!this.models.has(u.id)) this.models.set(u.id, u.team ? null : vehicleOf(u.kind, u.nat, this.ticket.turn));
      return this.models.get(u.id);
    }
    /**
     * Names the regiment blocks as their armies did (release 2; the historian's ruling, 22 November: units.ts
     * regimentWord): "Infanterie-Regiment 33" of the "11th Infantry Division", a British brigade, an American Combat
     * Command. Before the clock starts only; false once it has begun. Looks only: no number in the fight reads a name.
     */
    namesOn() {
      if (this.t > 0 || this.over) return false;
      for (const side of ["att", "def"]) armyNames(this.ticket[side], this.units.filter((u) => u.side === side), this.ticket.turn);
      return true;
    }
    /**
     * The model a block is drawn with, where the 3D set piece picks its own (Normandy's first German tank block is a
     * Tiger): the rules weigh it as that model from then. False where the block or the model is unknown to the sheet (a
     * tank block needs a model the sheet has duels for); null hands it back to vehicleOf.
     */
    vehicleIs(id, model) {
      const u = this.unit(id) ?? this.coming.find((c) => c.u.id === id)?.u;
      if (!u) return false;
      if (model === null) {
        this.models.delete(id);
        return true;
      }
      if (!ARMOUR.models[model] || u.kind === "arm" && !ARMOUR.duels[model]) return false;
      this.models.set(id, model);
      return true;
    }
    /** What a blow from u on tank block t weighs for t's armour on that face at that range (1 at par); null where unknown. */
    armourAt(u, t, face, range) {
      const s = this.vehicle(u), v = this.vehicle(t);
      return s && v ? armourWeight(s, v, face, range, this.ticket.turn, LAND.armour.lo, LAND.armour.hi) : null;
    }
    // ---- Hidden in the woods (release 2; Brian, 22 November 04:18Z; LAND.hide) -------------------------------------
    /**
     * Turns concealment on: before the clock starts only. Every block standing in woods shows as hidden at once (the setup
     * step shows his so as he places them); who is hidden is settled when the clock starts. False once it has begun.
     */
    hideOn() {
      if (this.t > 0 || this.over) return false;
      this.hide = true;
      for (const u of this.units) this.markHid(u);
      return true;
    }
    /** Whether a side can see a block: its own always; the enemy's unless it is hidden in the woods. */
    sees(side, e) {
      return e.side === side || !e.hid;
    }
    /**
     * The field's hedges (the historian's check, 22 November: the Normandy bocage): the hedge lines and their gates as the
     * 3D game lays them on this field, metres from its south-west corner. Before the clock starts only; a block on a hedge
     * line then hides as in woods (LAND.hide). False once it has begun.
     */
    hedgesOn(hedges, gates = []) {
      if (this.t > 0 || this.over) return false;
      const W = this.field.w, H = this.field.h, C = HEDGE_CELL, near = LAND.hide.hedge.near;
      const cols = Math.ceil(W / C), rows = Math.ceil(H / C), map = new Uint8Array(cols * rows);
      const keep = [];
      for (const h of hedges) {
        const pts = (h.points ?? []).filter((p) => Array.isArray(p) && Number.isFinite(p[0]) && Number.isFinite(p[1]));
        if (pts.length < 2 || !pts.some(([x, y]) => x >= 0 && x <= W && y >= 0 && y <= H)) continue;
        keep.push({ kind: h.kind, points: pts });
        for (let i = 1; i < pts.length; i++) {
          const [ax, ay] = pts[i - 1], [bx, by2] = pts[i];
          const c0 = Math.max(0, Math.floor((Math.min(ax, bx) - near) / C)), c1 = Math.min(cols - 1, Math.floor((Math.max(ax, bx) + near) / C));
          const r0 = Math.max(0, Math.floor((Math.min(ay, by2) - near) / C)), r13 = Math.min(rows - 1, Math.floor((Math.max(ay, by2) + near) / C));
          for (let r = r0; r <= r13; r++) for (let c = c0; c <= c1; c++) if (segGap((c + 0.5) * C, (r + 0.5) * C, ax, ay, bx, by2) <= near) map[r * cols + c] = 1;
        }
      }
      this.hedges = keep;
      this.gates = gates.filter((g) => Number.isFinite(g.x) && Number.isFinite(g.y) && g.x >= 0 && g.x <= W && g.y >= 0 && g.y <= H);
      this.hedgeMap = keep.length ? map : null;
      this.bars = null;
      this.hedgeSegs.clear();
      for (const h of keep) for (let i = 1; i < h.points.length; i++) {
        const [ax, ay] = h.points[i - 1], [bx, by2] = h.points[i], seg = [ax, ay, bx, by2];
        for (let c = Math.floor(Math.min(ax, bx) / GATE_CELL); c <= Math.floor(Math.max(ax, bx) / GATE_CELL); c++)
          for (let r = Math.floor(Math.min(ay, by2) / GATE_CELL); r <= Math.floor(Math.max(ay, by2) / GATE_CELL); r++) {
            const k = c * 4096 + r, l = this.hedgeSegs.get(k);
            if (l) l.push(seg);
            else this.hedgeSegs.set(k, [seg]);
          }
      }
      if (this.hide) for (const u of this.units) this.markHid(u);
      return true;
    }
    /**
     * Where a straight way from (x0, y0) to (x1, y1) first meets a hedge line, more than 'skip' metres out: null where it
     * meets none (the 3D game's men go through the gates: drill.ts).
     */
    hedgeCross(x0, y0, x1, y1, skip = 0) {
      if (!this.hedgeSegs.size) return null;
      const len = Math.hypot(x1 - x0, y1 - y0);
      if (len <= skip) return null;
      let best = null, bestK = Infinity;
      const seen = /* @__PURE__ */ new Set();
      for (let c = Math.floor(Math.min(x0, x1) / GATE_CELL); c <= Math.floor(Math.max(x0, x1) / GATE_CELL); c++)
        for (let r = Math.floor(Math.min(y0, y1) / GATE_CELL); r <= Math.floor(Math.max(y0, y1) / GATE_CELL); r++)
          for (const s of this.hedgeSegs.get(c * 4096 + r) ?? []) {
            if (seen.has(s)) continue;
            seen.add(s);
            const k = segHit(x0, y0, x1, y1, s[0], s[1], s[2], s[3]);
            if (k !== null && k * len > skip && k < bestK) {
              bestK = k;
              best = [x0 + (x1 - x0) * k, y0 + (y1 - y0) * k];
            }
          }
      return best;
    }
    /**
     * The gate a man going from (x0, y0) to (x1, y1) goes through, where his straight way meets a hedge (more than 'skip'
     * metres out): the gate nearest where it meets it, within GATE_REACH, that he can walk to straight. Null where his way
     * meets no hedge, or no gate serves: he climbs the bank there.
     */
    gateOn(x0, y0, x1, y1, skip = 0) {
      const at = this.hedgeCross(x0, y0, x1, y1, skip);
      if (!at || Math.hypot(at[0] - x1, at[1] - y1) < 0.75) return null;
      const near = this.gates.map((g) => ({ g, d: Math.hypot(g.x - at[0], g.y - at[1]) })).filter((x) => x.d <= GATE_REACH).sort((a, b) => a.d - b.d);
      for (const { g } of near) {
        const to = this.hedgeCross(x0, y0, g.x, g.y, skip);
        if (!to || Math.hypot(to[0] - g.x, to[1] - g.y) <= LAND.hide.hedge.gap) return g;
      }
      return null;
    }
    /** The way the hedge line nearest (x, y) runs there, a unit vector: null where none passes within LAND.hide.hedge.near. */
    hedgeDir(x, y) {
      let best = Infinity, dir = null;
      const c = Math.floor(x / GATE_CELL), r = Math.floor(y / GATE_CELL);
      for (let i = -1; i <= 1; i++) for (let j = -1; j <= 1; j++) for (const s of this.hedgeSegs.get((c + i) * 4096 + r + j) ?? []) {
        const l = Math.hypot(s[2] - s[0], s[3] - s[1]), d = segGap(x, y, s[0], s[1], s[2], s[3]);
        if (l > 0 && d < best) {
          best = d;
          dir = [(s[2] - s[0]) / l, (s[3] - s[1]) / l];
        }
      }
      return best <= LAND.hide.hedge.near ? dir : null;
    }
    /** On a hedge line (within LAND.hide.hedge.near of one), where the field has hedges. */
    onHedge(x, y) {
      if (!this.hedgeMap || x < 0 || y < 0 || x >= this.field.w || y >= this.field.h) return false;
      return this.hedgeMap[Math.floor(y / HEDGE_CELL) * Math.ceil(this.field.w / HEDGE_CELL) + Math.floor(x / HEDGE_CELL)] === 1;
    }
    /** Standing in woods (fieldworks dug into woods too): where a block can hide. */
    inWoods(u) {
      return coverAt(this.field, u.x, u.y) === "woods";
    }
    /** Cover a block can hide in: woods, or a hedge line. */
    inCover(u) {
      return this.inWoods(u) || this.onHedge(u.x, u.y);
    }
    /** Hidden if it stands in woods or on a hedge line, before the clock starts. Fieldworks are known where they are built. */
    markHid(u) {
      if (live2(u) && u.kind !== "fort" && this.inCover(u)) u.hid = true;
      else delete u.hid;
    }
    /**
     * How close the enemy must come before a hidden block opens fire unbidden: Infinity where it never holds (guns). In
     * true forest, foot and machine guns hold to LAND.hide.forest: they could see no further.
     */
    ambushAt(u) {
      const H = LAND.hide, a = u.team === "mg" ? H.ambush.mg : H.ambush[MOTION[u.kind]];
      if (a <= 0) return Infinity;
      return !this.onHedge(u.x, u.y) && this.inWoods(u) ? Math.min(a, H.forest) : a;
    }
    /**
     * An army's wait for an order by radio, as a multiple of LAND.radio.delay (the historian's, 22 November): its foot's by
     * nation and year; a tank block's as its model's share with a set, the rest as its foot.
     */
    radioOf(u) {
      const year = turnToDate(this.ticket.turn).year;
      const foot = (LAND.radio.nations[u.nat] ?? []).find(([to]) => year <= to)?.[1] ?? 1;
      if (u.kind !== "arm") return foot;
      const m = this.vehicle(u), r = m ? ARMOUR.models[m]?.radio : void 0;
      return r === void 0 ? foot : r + (1 - r) * foot;
    }
    /** The clock starts: the computer's defence lays its ambushes, then every block in woods is hidden. */
    hideStart() {
      this.hidStarted = true;
      if (this.ai.def && !this.ticket.def.human) this.layAmbush("def");
      for (const u of this.units) this.markHid(u);
      if (this.patrol) this.patrolOut();
    }
    /**
     * The computer's defence puts a block or two of its line into woods on the enemy's way in, before the clock starts
     * (LAND.hide.staff): foot of its line (not its held reserve, nor a block dug in), each to the furthest-forward wood
     * within 'ahead' metres before it or 'back' behind it and 'across' to either side, inside its own ground as the
     * player's setup step allows his, clear of every other block. Where no woods lie near, none goes. 'most' blocks at
     * most, of those 'may' allows (a caught patrol's one more post: night patrol); the blocks it put there.
     */
    layAmbush(side, most = LAND.hide.staff.blocks, may = () => true) {
      const H = LAND.hide.staff, dir = side === "att" ? 1 : -1;
      const z = this.field.zones[side];
      const own = this.units.filter((u) => u.side === side && live2(u));
      const room = [
        Math.min(z[0], ...own.map((u) => u.x - u.w / 2)),
        Math.min(z[1], ...own.map((u) => u.y - u.d / 2)),
        Math.max(z[2], ...own.map((u) => u.x + u.w / 2)),
        Math.max(z[3], ...own.map((u) => u.y + u.d / 2))
      ];
      const foot = own.filter((u) => MOTION[u.kind] === "foot" && !u.team && !u.dug && u.reserve !== "held" && !u.taken && !this.inCover(u) && may(u));
      const picks = [];
      const cell = this.hedgeMap ? Math.min(this.field.cell, HEDGE_CELL) : this.field.cell;
      for (const u of foot) {
        let best = null;
        for (let dy = -H.back; dy <= H.ahead; dy += cell) for (let dx = -H.across; dx <= H.across; dx += cell) {
          const x = u.x + dx, y = u.y + dir * dy;
          if (coverAt(this.field, x, y) !== "woods" && !this.onHedge(x, y)) continue;
          if (best && (dy < best.ahead || dy === best.ahead && Math.abs(dx) >= Math.abs(best.x - u.x))) continue;
          if (this.standWhy(u, x, y, room) !== null) continue;
          best = { x, y, ahead: dy };
        }
        if (best) picks.push({ u, ...best });
      }
      picks.sort((a, b) => b.ahead - a.ahead || a.u.id - b.u.id);
      const laid = [];
      for (const p of picks) {
        if (laid.length >= most) break;
        if (this.standWhy(p.u, p.x, p.y, room) !== null) continue;
        this.place(p.u, p.x, p.y);
        laid.push(p.u);
      }
      return laid;
    }
    /**
     * Found (release 2): no longer hidden from the other side, and known to the player if it is the enemy's (b464(2)).
     * 'fired': it opened fire on 'by'; 'seen': 'by' came close enough to see it; 'out': it left the woods; 'struck': hit
     * from the air; 'patrol': the night before, by the patrol 'by' sent out (night patrol, release 2), before any shot.
     * Told to the player as his blocks would report it.
     */
    reveal(u, how, by2) {
      if (!u.hid) return;
      delete u.hid;
      if (how === "fired") u.unloc = Math.round((this.t + LAND.hide.locate.secs) * 1e3) / 1e3;
      const mineFound = this.ticket[u.side].human, theirs = this.ticket[other(u.side)].human;
      if (theirs) u.found = true;
      const where = this.inWoods(u) ? "the woods" : "the hedges";
      const text = mineFound ? how === "fired" ? `${this.named(u)} springs its ambush from ${where}.` : how === "seen" ? `${this.named(u)} has been seen in ${where}.` : how === "heard" ? `${this.named(u)} has been heard moving in ${where}.` : how === "out" ? `${this.named(u)} has left cover and can be seen.` : how === "searched" ? `${this.named(u)} is flushed out of ${where} by fire.` : how === "patrol" ? `An enemy patrol found ${this.named(u)} in ${where} in the night.` : `${this.named(u)} is struck in ${where}: the enemy knows it is there.` : theirs ? how === "fired" && by2 ? `Enemy troops hidden in ${where} open fire on your ${by2.name}.` : (how === "seen" || how === "heard") && by2 ? `Your ${by2.name} has found enemy troops in ${where}.` : how === "searched" && by2 ? `Your ${by2.name} has flushed enemy troops out of ${where}.` : how === "patrol" ? "Our night patrol found a hidden post." : how === "out" ? "Enemy troops come out of cover." : `Enemy troops found in ${where}.` : "";
      this.say({ what: "land:found", unit: u.id, side: u.side, text, x: u.x, y: u.y, extra: { how, ...by2 ? { by: by2.id } : {} } });
    }
    /**
     * Each tick: a hidden block that has left cover, or that an enemy has come close enough to see (or to hear: a tank or
     * lorry on the move), is found; one found by its fire is located once that long has passed or an enemy is that close.
     */
    spotHidden() {
      const H = LAND.hide;
      for (const u of this.units) {
        const sight = H.sight[MOTION[u.kind]];
        if (u.unloc !== void 0 && (this.t >= u.unloc || !live2(u) || this.units.some((e) => e.side !== u.side && fighting2(e) && this.gap(u, e) <= sight))) delete u.unloc;
        if (!u.hid) continue;
        if (!live2(u)) {
          delete u.hid;
          continue;
        }
        if (!this.inCover(u)) {
          this.reveal(u, "out");
          continue;
        }
        const heard = u.speed > 0.5 ? H.heard[MOTION[u.kind]] : 0;
        const veiled = (e) => this.smoke && this.screens.length > 0 && this.screened(e.x, e.y, u.x, u.y);
        const by2 = this.units.find((e) => e.side !== u.side && fighting2(e) && this.gap(u, e) <= Math.max(sight, heard) && (this.gap(u, e) <= heard || !veiled(e)));
        if (by2) this.reveal(u, this.gap(u, by2) <= sight && !veiled(by2) ? "seen" : "heard", by2);
      }
    }
    // ---- Night patrol (release 2) -----------------------------------------------------------------------------------
    //
    // Brian, 22 November 17:33Z ("lets do all three but make it where we can toggle it off if someone doesnt want it");
    // the design director, 18:36Z, Q6 to Q8 (LAND.patrol). The night before, the attack sends one patrol out from a
    // block of its line on foot: the computer's always, from its most forward (doctrine: Q8); the player's only by his
    // order while he places his army (land:patrol). The defence never patrols. It needs hiding on (only hidden posts can
    // be found): it is settled once in hideStart, after the computer's ambushes are laid and every block in cover is
    // hidden and before either staff first thinks, so the attack's first plan counts what it found. Two draws on its own
    // stream ('patrol:' and the seed, and the attempt), always both and found first, never on the battle's dice: found,
    // the defence's most forward hidden post is found before any shot (it never springs its ambush); caught, the defence
    // is alerted, ready but not stronger (Q7, b418): one more of its blocks takes a hidden post by the computer's own
    // ambush rule (the player's defence too, laid for him and named in his warning), a computer defence's reserve starts
    // toward the attack at once as a bold one does (ai.ts defend; a player's moves his own), and the patrol's block loses
    // the patrol's men from its strength (counted as lost; its nerve untouched). The flares over it are looks only.
    /** Why this block may not go out on patrol tonight, in the player's words (129(d)); null where it may. */
    patrolWhy(u) {
      if (!this.patrol) return "There are no night patrols in this battle.";
      if (!this.hide) return "Nothing hides in this battle, so there is nothing for a patrol to find.";
      if (u.side !== "att") return "Only the attacker sends out a patrol.";
      if (this.t > 0 || this.over || this.hidStarted) return "The battle has begun: a patrol goes out the night before.";
      if (this.setup !== u.side) return "A patrol is sent while you place your army, before the battle begins.";
      if (this.patrolBy !== null) return "One patrol a night: one is already going out.";
      if (!fighting2(u)) return "It is out of the fight.";
      if (u.team || MOTION[u.kind] !== "foot") return "Only men on foot go out on patrol.";
      return null;
    }
    /** A side's computer defence alerted by the patrol it caught (night patrol): its reserve starts toward the attack at once (ai.ts). */
    alertedStaff(side) {
      return this.patrol && side === "def" && !this.ticket.def.human && this.nightPatrol?.alerted === true;
    }
    /** The night's patrol goes out, and what it found and whether it was caught are settled (hideStart). */
    patrolOut() {
      const P = LAND.patrol, side = "att", foe = "def";
      const foot = (u) => !!u && u.side === side && fighting2(u) && !u.team && MOTION[u.kind] === "foot";
      const by2 = this.ticket[side].human ? this.patrolBy === null ? void 0 : this.unit(this.patrolBy) : this.units.filter((u) => foot(u) && u.reserve !== "held").sort((a, b) => b.y - a.y || a.id - b.id)[0];
      if (!foot(by2)) return;
      const rng = new Rng(hashString(this.ticket.attempt ? `patrol:${this.ticket.seed}:${this.ticket.attempt}` : `patrol:${this.ticket.seed}`));
      const found = rng.next() < P.found, caught = rng.next() < P.caught;
      const post = this.units.filter((u) => u.side === foe && live2(u) && u.hid).sort((a, b) => a.y - b.y || a.id - b.id)[0];
      const men = patrolSize(by2.nat);
      const r13 = (v) => Math.round(v * 10) / 10;
      const near = found && post ? post : this.units.filter((u) => u.side === foe && live2(u) && !u.hid).sort((a, b) => dist(a.x, a.y, by2.x, by2.y) - dist(b.x, b.y, by2.x, by2.y) || a.id - b.id)[0];
      const to = near ? [r13(near.x), r13(Math.max(0, near.y - near.d / 2 - 40))] : [r13(by2.x), r13(by2.y + by2.d / 2 + 200)];
      const from = [r13(by2.x), r13(by2.y + by2.d / 2)];
      const w = (j) => (mix2(Math.imul(this.ticket.seed >>> 0, 2246822507) + j) - 0.5) * 60;
      const tm = Math.round((P.night.t0 + P.night.t1) / 2), at = (f, j) => [r13(from[0] + (to[0] - from[0]) * f + w(j)), r13(from[1] + (to[1] - from[1]) * f)];
      const route = [
        [P.night.t0, from[0], from[1]],
        [Math.round((P.night.t0 + tm) / 2), ...at(0.5, 1)],
        [tm, to[0], to[1]],
        [Math.round((tm + P.night.t1) / 2), ...at(0.5, 2)],
        [P.night.t1, from[0], from[1]]
      ];
      const night = {
        side,
        unit: by2.id,
        men,
        t0: P.night.t0,
        t1: P.night.t1,
        from,
        to,
        route,
        found: null,
        alerted: caught,
        post: null,
        lost: 0,
        flares: []
      };
      this.nightPatrol = night;
      if (found && post) {
        night.found = { unit: post.id, x: r13(post.x), y: r13(post.y), kind: post.team ?? post.kind };
        this.reveal(post, "patrol", by2);
      }
      if (caught) {
        const laid = this.layAmbush(foe, 1, (u) => !u.order);
        for (const u of laid) this.markHid(u);
        const extra = laid.find((u) => u.hid);
        if (extra) night.post = extra.id;
        const lost = Math.min(by2.str, men * by2.str0 / Math.max(1, by2.men0));
        by2.str -= lost;
        by2.figures = by2.str <= 0 ? 0 : Math.max(1, Math.ceil(by2.tray * by2.str));
        night.lost = lost;
        const k = (i, j) => mix2(Math.imul(this.ticket.seed >>> 0, 2654435761) + i * 8 + j);
        for (let i = 0; i < 3; i++) {
          const t = Math.round(P.night.t0 + (P.night.t1 - P.night.t0) * ((i + k(i, 1)) / 3));
          night.flares.push([t, r13(Math.min(this.field.w, Math.max(0, to[0] + (k(i, 2) - 0.5) * 240))), r13(Math.min(this.field.h, Math.max(0, to[1] + (k(i, 3) - 0.5) * 120)))]);
        }
        if (this.ticket[foe].human) {
          const where = extra ? ` ${extra.name} has taken a hidden post in ${this.inWoods(extra) ? "the woods" : "the hedges"}.` : "";
          this.say({ what: "land:patrol", side: foe, ...extra ? { unit: extra.id } : {}, text: `We caught an enemy patrol in the night. An attack is coming.${where}`, x: extra ? r13(extra.x) : to[0], y: extra ? r13(extra.y) : to[1], extra: { caught: true, ...extra ? { post: extra.id } : {} } });
        }
      }
      if (this.ticket[side].human && (caught || !found || !post)) {
        const text = caught ? "Our patrol was caught in the night. They know we're coming." : "Our patrol came back with nothing to report.";
        this.say({ what: "land:patrol", side, unit: by2.id, text, x: to[0], y: to[1], extra: { caught, found: !!night.found } });
      }
    }
    /** The night's patrol as a side sees it: the attack all of it; the defence only once alerted, where it caught it and the flares. */
    patrolView(as) {
      const p = this.nightPatrol;
      if (!this.patrol || !p) return {};
      const flares = p.flares.map((f) => [f[0], f[1], f[2]]);
      if (as === void 0 || as === p.side) {
        return { patrol: { side: p.side, unit: p.unit, t0: p.t0, t1: p.t1, men: p.men, from: [p.from[0], p.from[1]], to: [p.to[0], p.to[1]], route: p.route.map((r) => [r[0], r[1], r[2]]), found: p.found ? { x: p.found.x, y: p.found.y, kind: p.found.kind } : null, alerted: p.alerted, flares } };
      }
      return p.alerted ? { patrol: { side: p.side, t0: p.t0, t1: p.t1, men: p.men, to: [p.to[0], p.to[1]], found: null, alerted: true, flares } } : {};
    }
    step() {
      if (this.setup) return;
      if (this.over) {
        this.drill?.step();
        return;
      }
      if (this.hide && !this.hidStarted) this.hideStart();
      if (this.phones) this.phonesOn();
      if (this.coming.length && this.t >= this.coming[0].at - 1e-9) this.arrive();
      this.landOrders();
      if (this.relief && this.joinCorps.size) this.waitOn();
      if (this.t >= this.nextThink) {
        this.nextThink = this.t + LAND.think;
        this.staffing = true;
        for (const s of ["att", "def"]) if (this.ai[s]) thinkFor(this, s);
        this.staffing = false;
      }
      for (const u of this.units) {
        u.pinned = false;
        if (live2(u)) {
          u.still += DT;
          u.clear += DT;
        }
      }
      if (this.costliest.size) this.costliest.clear();
      const hurt = /* @__PURE__ */ new Map();
      const kept = this.airOn.att || this.airOn.def ? /* @__PURE__ */ new Map() : null;
      for (const u of this.units) {
        if (!fighting2(u)) continue;
        const t = this.targetOf(u);
        u.engaged = t?.id;
        if (!t) {
          if (u.hid && u.search && this.hide) this.reveal(u, "fired");
          continue;
        }
        const blind = !!t.hid;
        if (this.contactAt === null && u.kind !== "art") this.contactAt = this.t;
        if (u.hid) {
          u.sprung = this.t;
          this.reveal(u, "fired", t);
        }
        if (u.sprung === this.t) this.ambushed.add(t.side);
        if (this.ticket[u.side].human || u.kind !== "art" || this.gap(u, t) < LAND.guns.closeAt) {
          u.found = true;
          if (!blind) t.found = true;
        }
        if (!u.met) {
          u.met = true;
          if (u.side === "att" || !t.met) this.say({ what: "contact", unit: u.id, side: u.side, text: `${this.named(u)} is in action.`, x: u.x, y: u.y });
        }
        const blow = this.blow(u, t);
        const struck = blow.dmg;
        if (this.phones && this.wires.length) this.wireBlow(u, t);
        if (blind) {
          const F = LAND.hide.search;
          blow.dmg *= F.blow;
          if (this.rng.next() < 1 - (1 - F.odds) ** (DT / F.every)) this.reveal(t, "searched", u);
        }
        if (this.airLeft[u.side] > 0) {
          const full = blow.dmg / blow.screen, held = full * this.airOn[u.side].share, raw = held / blow.ground;
          blow.dmg = (full - held) * blow.screen;
          this.airBank[u.side].set(t.id, (this.airBank[u.side].get(t.id) ?? 0) + raw);
          this.airLedger[u.side].withheld += raw;
          kept.set(t.id, (kept.get(t.id) ?? 0) + raw);
        }
        const h = hurt.get(t.id) ?? { dmg: 0, nerve: 0, from: [], blind: true, team: 0, teamNerve: 0 };
        this.tally(h, u, blow);
        if (!blind) h.blind = false;
        h.from.push([u, blow.dmg]);
        hurt.set(t.id, h);
        if (this.faceRule) {
          const cost = struck > 0 ? blow.faceOn * (blow.dmg / struck) : 0, top = this.costliest.get(t.id);
          if (!top || cost > top.cost) this.costliest.set(t.id, { by: u.id, cost, front: blow.front });
        } else if (blow.front) t.pinned = true;
        if (blow.aspect !== "front" && this.t - t.flankedAt > 20) {
          t.flankedAt = this.t;
          if (t.side === "att" ? this.ticket.att.human : this.ticket.def.human) this.say({ what: "flanked", unit: t.id, side: t.side, text: `${this.named(t)} is hit from the ${blow.aspect === "rear" ? "rear" : "flank"}.`, x: t.x, y: t.y });
        }
      }
      if (this.faceRule) {
        for (const [id, top] of this.costliest) if (top.front) this.unit(id).pinned = true;
      }
      if (this.teamsRule) {
        for (const h of hurt.values()) if (h.team > 0 && h.dmg > 0) h.nerve = ((h.dmg - h.team) * h.nerve + h.teamNerve) / h.dmg;
      }
      if (kept) for (const [id, raw] of kept) {
        const h = hurt.get(id), side = other(this.unit(id).side);
        this.airNerve[side].set(id, (this.airNerve[side].get(id) ?? 0) + raw * h.nerve);
      }
      for (const [id, h] of hurt) this.suffer(this.unit(id), h.dmg, h.nerve, h.from, h.blind);
      if (this.repay.length) this.repayNow();
      if (this.airOn.att || this.airOn.def) this.flyAir();
      if (this.bearers) this.carry();
      for (const u of this.units) if (live2(u)) this.nerve(u);
      const at = () => new Map(this.units.map((u) => [u.id, [u.x, u.y]]));
      const before = this.cutters && this.hedges.length ? at() : null;
      const tanks = this.phones && this.wires.length ? this.units.filter((u) => u.side === "att" && u.kind === "arm" && live2(u)).map((u) => [u, u.x, u.y]) : null;
      if (this.escort) this.overruns();
      for (const u of this.units) if (live2(u)) this.move(u);
      const moved = before ? at() : null;
      this.spread();
      if (before) this.cutOn(before, moved);
      if (tanks) this.wireTanks(tanks);
      if (this.hide) this.spotHidden();
      this.t = Math.round((this.t + DT) * 1e3) / 1e3;
      if (this.cuts.length) this.openCuts();
      this.judge();
      this.drill?.step();
    }
    spread() {
      const us = this.units.filter((u) => fighting2(u) && u.kind !== "fort");
      const can = (u, ux, uy, push) => {
        const x = clamp(u.x + ux * push, 5, this.field.w - 5), y = clamp(u.y + uy * push, 5, this.field.h - 5), pace = LAND.pace[MOTION[u.kind]];
        const there = groundAt(this.field, x, y), here = groundAt(this.field, u.x, u.y);
        if (inRiver(there) && !inRiver(here)) return false;
        const to = pace[there];
        if (this.bocage && this.going(u) === "hedged" && this.barredCross(u.x, u.y, x, y)) return false;
        return to > LAND.queue.hard || to > 0 && to >= pace[here];
      };
      const go = (u, ux, uy, push) => {
        if (can(u, ux, uy, push)) {
          u.x = clamp(u.x + ux * push, 5, this.field.w - 5);
          u.y = clamp(u.y + uy * push, 5, this.field.h - 5);
        }
      };
      const keep = this.standRule && this.standKeeps.side, now = keep ? ++this.spreads : 0;
      for (let i = 0; i < us.length; i++) for (let j = i + 1; j < us.length; j++) {
        const a = us[i], b = us[j];
        const dx = b.x - a.x, dy = b.y - a.y;
        if (Math.abs(dx) > 300 || Math.abs(dy) > 300) continue;
        const [fx, fy] = ahead(a.face), [rx, ry] = ahead(a.face + Math.PI / 2);
        const lat = dx * rx + dy * ry, dep = dx * fx + dy * fy;
        const c = Math.abs(Math.cos(b.face - a.face)), s = Math.abs(Math.sin(b.face - a.face));
        const pl = a.w / 2 + b.w / 2 * c + b.d / 2 * s - Math.abs(lat);
        const pd = a.d / 2 + b.w / 2 * s + b.d / 2 * c + (a.side === b.side ? 0 : LAND.apart) - Math.abs(dep);
        if (pl <= 0 || pd <= 0) continue;
        if (a.side !== b.side && this.inOverrun(a, b)) continue;
        if (a.side === b.side && (this.narrows(a, a.x, a.y) || this.narrows(b, b.x, b.y))) {
          const into = (u, ox, oy) => u.pressing !== void 0 && ox * Math.sin(u.pressing) + oy * Math.cos(u.pressing) > 0;
          const pa = into(a, dx, dy), pb = into(b, -dx, -dy);
          if (pa || pb) {
            const goA = pa && (!pb || this.toGo(a) < this.toGo(b) || this.toGo(a) === this.toGo(b) && a.id < b.id);
            const mover = goA ? a : b, y = goA ? b : a, sy = goA ? 1 : -1;
            const p = Math.min(Math.min(pl, pd), (mover.speed + LAND.spread) * DT);
            const sl = Math.sign(lat) || 1, sd = Math.sign(dep) || 1, d1 = Math.hypot(dx, dy) || 1;
            const ways = pl < pd ? [[rx * sl, ry * sl], [fx * sd, fy * sd]] : [[fx * sd, fy * sd], [rx * sl, ry * sl]];
            ways.push([dx / d1, dy / d1]);
            const w = ways.find(([wx, wy]) => can(y, wx * sy, wy * sy, p));
            if (w) {
              go(y, w[0] * sy, w[1] * sy, p);
              y.yieldTo = mover.id;
              continue;
            }
            if (can(mover, -rx * sl * sy, -ry * sl * sy, p)) {
              go(mover, -rx * sl * sy, -ry * sl * sy, p);
              continue;
            }
          }
        }
        if (a.side === b.side && (a.pressing !== void 0 || b.pressing !== void 0) && !this.narrows(a, a.x, a.y) && !this.narrows(b, b.x, b.y)) {
          const p = Math.min(pl / 2, (Math.max(a.speed, b.speed) + LAND.spread) * DT);
          let sl = Math.sign(lat) || (a.id < b.id ? 1 : -1);
          const pair = keep ? `${a.id}:${b.id}` : "", was = keep ? this.sides.get(pair) : void 0;
          if (was && was.n === now - 1) {
            const ks = Math.sign(rx * was.x + ry * was.y);
            if (ks !== 0 && can(a, -rx * ks, -ry * ks, p) && can(b, rx * ks, ry * ks, p)) sl = ks;
          }
          if (can(a, -rx * sl, -ry * sl, p) && can(b, rx * sl, ry * sl, p)) {
            go(a, -rx * sl, -ry * sl, p);
            go(b, rx * sl, ry * sl, p);
            if (keep) this.sides.set(pair, { x: rx * sl, y: ry * sl, n: now });
            if (a.pressing !== void 0 && b.pressing !== void 0) {
              const wa2 = this.toGo(a) > this.toGo(b) || this.toGo(a) === this.toGo(b) && a.id > b.id;
              (wa2 ? a : b).yieldTo = (wa2 ? b : a).id;
            }
            continue;
          }
        }
        let along = pl < pd;
        if (along) {
          const sl = Math.sign(lat) || (a.id < b.id ? 1 : -1), p = Math.min(pl / 2, LAND.spread * DT);
          if (!can(a, -rx * sl, -ry * sl, p) && !can(b, rx * sl, ry * sl, p)) along = false;
        }
        const sg = Math.sign(along ? lat : dep) || (a.id < b.id ? 1 : -1);
        const ux = (along ? rx : fx) * sg, uy = (along ? ry : fy) * sg;
        const push = Math.min((along ? pl : pd) / 2, LAND.spread * DT);
        const wa = a.side !== b.side && inRiver(groundAt(this.field, a.x, a.y)), wb = a.side !== b.side && inRiver(groundAt(this.field, b.x, b.y));
        if (!wa) go(a, -ux, -uy, wb ? 2 * push : push);
        if (!wb) go(b, ux, uy, wa ? 2 * push : push);
      }
    }
    /** Who this block strikes: its ordered target if in reach, else the enemy in reach most in front of it. */
    targetOf(u) {
      const marching = u.order && u.order.kind !== "attack" && u.path.length > 0;
      if (u.kind === "art" && marching) return null;
      if (u.order?.kind === "withdraw") return null;
      let reach = reachOf(u);
      if (u.aim !== void 0) {
        const a = this.unit(u.aim);
        if (a && live2(a) && a.side !== u.side && !a.hid && this.gap(u, a) <= reach) return a;
      }
      if (u.hid && u.order?.kind !== "attack") reach = Math.min(reach, this.ambushAt(u));
      let best = null, score = Infinity;
      const T = this.teamsRule && ownArm2(u) ? LAND.teams[u.team] : null;
      let harm = 0;
      for (const e of this.units) {
        if (e.side === u.side || !live2(e) || e.hid) continue;
        const d = this.gap(u, e);
        if (d > reach) continue;
        if (u.order?.kind === "attack" && u.order.target === e.id) return e;
        const off = Math.abs(turn(u.face, bearing(u.x, u.y, e.x, e.y)));
        const s = d * (1 + off) * (e.state === "routing" ? 2 : 1);
        if (T) {
          const h = (e.kind === "arm" ? T.armour : 1) * (d < T.closeAt ? T.close : 1);
          if (h < harm - 1e-9 || h <= harm + 1e-9 && s >= score) continue;
          harm = h;
          score = s;
          best = e;
          continue;
        }
        if (s < score) {
          score = s;
          best = e;
        }
      }
      if (!best && u.search && this.hide) {
        const [sx, sy] = u.search, R = u.searchR ?? LAND.hide.search.reach;
        for (const e of this.units) {
          if (e.side === u.side || !live2(e) || !e.hid || dist(e.x, e.y, sx, sy) > R + Math.max(e.w, e.d) / 2) continue;
          const d = this.gap(u, e);
          if (d <= reach && d < score) {
            score = d;
            best = e;
          }
        }
      }
      return best;
    }
    /**
     * How far a tank or lorry block stands ahead of its group's slowest foot block, toward the enemy (going in together);
     * null where the group has no foot left fighting.
     */
    aheadOfFoot(u, group) {
      const dir = u.side === "att" ? 1 : -1;
      let slow = null;
      for (const f of this.units) {
        if (f.side !== u.side || !fighting2(f) || MOTION[f.kind] !== "foot" || f.order?.together !== group) continue;
        const p = dir * f.y;
        if (slow === null || p < slow) slow = p;
      }
      return slow === null ? null : dir * u.y - slow;
    }
    /**
     * Tanks alone (the tactics panel, item 7; LAND.alone): what a tank block takes of this foot block's blow for having
     * left its own infantry on close ground, or against foot in fieldworks: the card's own figure for tanks alone there,
     * or 1. Only a tank block the card counted as covered by its foot (covered), never one it already charged as alone.
     */
    alone(t, u) {
      const A = LAND.alone;
      if (!this.covered.has(t.id) || !A.foot.includes(u.kind)) return 1;
      const g = coverAt(this.field, t.x, t.y), gu = groundAt(this.field, u.x, u.y);
      const works = A.works.includes(gu) || u.dug && dist(u.x, u.y, u.home[0], u.home[1]) <= LAND.dug.keep;
      const kind = A.streets.includes(g) ? "streets" : A.woods.includes(g) ? "woods" : works ? "works" : null;
      if (!kind) return 1;
      const foot = (f) => fighting2(f) && A.foot.includes(f.kind);
      if (this.units.some((f) => f.side === t.side && foot(f) && (f.engaged === u.id || u.engaged === f.id))) return 1;
      if (this.units.some((f) => f.side === t.side && foot(f) && this.gap(t, f) <= A.near)) return 1;
      this.bitten.add(t.id);
      t.aloneAt = this.t;
      return A.take[t.side][kind];
    }
    /**
     * The enemy's reserve walking behind its line toward the stretch one of the player's blocks is coming at (ai.ts; ruling
     * 239's rider, 03:20Z): told once, as that block of his reports what it sees, never naming the enemy's (b464's fog
     * rule). Words: the UI director's (land-slice-3-review.md, 20 Nov 05:02).
     */
    reserveSeen(side, toward) {
      if (this.toldShift || toward.side === side || !this.ticket[toward.side].human) return;
      this.toldShift = true;
      this.say({ what: "land:reserve", unit: toward.id, side: toward.side, text: `Your ${this.named(toward)} reports movement behind the enemy line, shifting toward it.`, x: toward.x, y: toward.y });
    }
    // ---- Overrun (release 2) -------------------------------------------------------------------------------------------
    //
    // Brian, 23 November 05:05Z: "vechiles can run over enemy soldiers though which would kill them"; the design director,
    // 05:48Z: through the block's ground to its far edge and back, once per foot block per contact, open ground only, no
    // enemy weapon that can hurt a tank in its own direct-fire reach of the way, the foot's own anti-tank weapon counted,
    // harm unchanged. LAND.overrun.
    /** Whether one of the two is driving through the other's ground. */
    inOverrun(a, b) {
      return a.ovr?.on === b.id || b.ovr?.on === a.id;
    }
    /** Whether a block's foot carries its own anti-tank weapon in the battle's month (LAND.overrun.at, by its army). */
    ownAntiTank(u) {
      if (u.kit?.at !== void 0) return u.kit.at;
      const O = LAND.overrun, d = turnToDate(this.ticket.turn), when = d.year + d.month / 12;
      const nat = O.commonwealth.includes(u.nat) ? "UK" : u.nat;
      return (O.at[nat] ?? []).some(([from, to]) => when >= from && when < to);
    }
    /**
     * How far an enemy block's weapons that can hurt a tank reach by direct fire (null: none): a tank block's reach, a
     * gun's over open sights, a fort's, foot with its own anti-tank weapon its reach. Support teams' bullets and bombs: none.
     */
    tankReach(e) {
      if (e.team) return null;
      if (e.kind === "arm" || e.kind === "fort") return reachOf(e);
      if (e.kind === "art") return gunsOf(e).closeAt;
      if (LAND.alone.foot.includes(e.kind) && this.ownAntiTank(e)) return reachOf(e);
      return null;
    }
    /**
     * Whether an enemy weapon that can pierce a hull reaches a block now (tankReach: a tank's, a gun's over open sights, a
     * fort's, foot with its own anti-tank weapon), the overrun's threat test. Looks only for the drill: armoured infantry
     * aboard their half-tracks ride on till one does (the design director, 23 November 07:06Z(2); Drill.think).
     */
    hullThreat(u) {
      for (const e of this.units) {
        if (e.side === u.side || !fighting2(e)) continue;
        const r = this.tankReach(e);
        if (r !== null && this.gap(u, e) <= r) return true;
      }
      return false;
    }
    /** The tank blocks that may, begin their overruns (each tick, before the blocks move). */
    overruns() {
      const foot = (f) => fighting2(f) && LAND.alone.foot.includes(f.kind);
      for (const [id] of this.overrun) {
        const f = this.unit(id);
        if (!f || !fighting2(f) || !this.units.some((e) => e.side !== f.side && e.kind === "arm" && fighting2(e) && this.gap(e, f) <= reachOf(e))) this.overrun.delete(id);
      }
      for (const u of this.units) {
        if (u.ovr || u.kind !== "arm" || u.team || !fighting2(u) || u.state === "wavering" || u.gives || u.fellBack || u.engaged === void 0) continue;
        if (u.order && u.order.kind !== "attack") continue;
        const f = this.unit(u.engaged);
        if (!f || f.side === u.side || !foot(f) || f.hid || this.overrun.has(f.id) || this.gap(u, f) > reachOf(u)) continue;
        if (f.dug && dist(f.x, f.y, f.home[0], f.home[1]) <= LAND.dug.keep || this.ownAntiTank(f)) continue;
        if (!this.units.some((g) => g.side === u.side && foot(g) && this.gap(u, g) <= LAND.alone.near)) continue;
        const dx = f.x - u.x, dy = f.y - u.y, len = Math.hypot(dx, dy) || 1;
        const to = [clamp(f.x + dx / len * (f.d / 2), 5, this.field.w - 5), clamp(f.y + dy / len * (f.d / 2), 5, this.field.h - 5)];
        const run = dist(u.x, u.y, to[0], to[1]);
        let open = true;
        for (let s = 0; s <= run && open; s += 10) {
          const x = u.x + (to[0] - u.x) * s / Math.max(1, run), y = u.y + (to[1] - u.y) * s / Math.max(1, run), g = groundAt(this.field, x, y);
          if (!OPEN_GROUND.includes(coverAt(this.field, x, y)) || inRiver(g) || this.onHedge(x, y) || LAND.pace.tracks[g] <= LAND.queue.hard) open = false;
        }
        if (!open || this.bocage && this.barredCross(u.x, u.y, to[0], to[1])) continue;
        const threat = this.units.some((e) => {
          if (e.side === u.side || e.id === f.id || !fighting2(e)) return false;
          const r = this.tankReach(e);
          if (r === null) return false;
          const ex = e.x - u.x, ey = e.y - u.y, k = clamp((ex * (to[0] - u.x) + ey * (to[1] - u.y)) / Math.max(1, run * run), 0, 1);
          const near = dist(e.x, e.y, u.x + (to[0] - u.x) * k, u.y + (to[1] - u.y) * k) - (Math.max(e.w, e.d) + Math.max(u.w, u.d)) / 4;
          return near <= r;
        });
        if (threat) continue;
        u.ovr = { on: f.id, to, back: [u.x, u.y], leg: "in" };
        u.path = [];
        this.overrun.set(f.id, u.id);
        const mine = this.ticket[u.side].human, theirs = this.ticket[f.side].human;
        if (mine || theirs) this.say({ what: "land:overrun", unit: mine ? u.id : f.id, side: mine ? u.side : f.side, text: mine ? `${this.named(u)} drives through the enemy's line.` : `Enemy tanks drive through ${this.named(f)}'s line.`, x: f.x, y: f.y });
      }
    }
    /**
     * A tank block on its overrun this tick: on through the foot's ground to its far edge, then backing out, facing the
     * enemy, to where it stood. False when the overrun is over or called off (it broke, wavered, or was ordered away).
     */
    drive(u) {
      const O = u.ovr;
      if (!fighting2(u) || u.state === "wavering" || u.gives || u.fellBack || u.order && u.order.kind !== "attack") {
        u.ovr = void 0;
        return false;
      }
      const f = this.unit(O.on);
      if (O.leg === "in" && (!f || !fighting2(f))) O.leg = "back";
      const to = O.leg === "in" ? O.to : O.back;
      const d = dist(u.x, u.y, to[0], to[1]);
      if (d < 1) {
        if (O.leg === "back") {
          u.ovr = void 0;
          return false;
        }
        O.leg = "back";
        u.speed = 0;
        u.act = "fight";
        return true;
      }
      const head = bearing(u.x, u.y, to[0], to[1]), on = O.leg === "in";
      if (on) {
        if (Math.abs(turn(u.face, head)) > 0.6) {
          this.turnTo(u, head);
          u.speed = 0;
          u.act = "turn";
          return true;
        }
        this.turnTo(u, head);
      }
      const g = groundAt(this.field, u.x, u.y);
      const v = LAND.speed.arm * Math.max(0.12, LAND.pace.tracks[g]) * (on ? 1 : LAND.overrun.reverse);
      const step = Math.min(d, v * DT), [ax, ay] = ahead(head);
      u.x = clamp(u.x + ax * step, 5, this.field.w - 5);
      u.y = clamp(u.y + ay * step, 5, this.field.h - 5);
      u.speed = step / DT;
      if (on) u.pressing = head;
      u.act = "march";
      u.cover = groundAt(this.field, u.x, u.y);
      return true;
    }
    /** Metres between two blocks' edges, roughly. */
    gap(a, b) {
      return Math.max(0, dist(a.x, a.y, b.x, b.y) - (Math.max(a.w, a.d) + Math.max(b.w, b.d)) / 4);
    }
    /**
     * What a block takes of a blow for the ground it stands on (its cover, the shelter of wrecks beside men on foot, tanks
     * blind in the streets): the part of what it takes that is where it stands, not whose ground it is.
     */
    groundTake(t, at = [t.x, t.y]) {
      const gt = coverAt(this.field, at[0], at[1]);
      let take = LAND.cover[gt];
      const ws = this.wreckShare(t);
      if (ws > 0) take = take * (1 - ws) + Math.min(take, LAND.cover[LAND.wreck.cover]) * ws;
      if (t.kind === "arm" && LAND.tanksClose.ground.includes(gt)) take *= LAND.tanksClose.take;
      return take;
    }
    /**
     * One blow's harm and nerve added to what its target takes this tick (step's hurt; anything else that lands part of
     * a blow on a block that tick must come through here too). The strikers' nerve multipliers are added, as fitted.
     * Support teams in battle (teamsRule): a team's is not added to the others'; it is kept with its own harm (team,
     * teamNerve), and step makes the one multiplier of them once every blow is in.
     */
    tally(h, u, blow) {
      h.dmg += blow.dmg;
      if (this.teamsRule && ownArm2(u)) {
        h.team += blow.dmg;
        h.teamNerve += blow.dmg * blow.nerve;
      } else h.nerve += blow.nerve;
    }
    /**
     * One block's blow on another this tick: its weight (dmg), its nerve's multiplier, whether it lands on the front and
     * from which aspect, the part of what the target takes that is its ground (ground), and the share a smoke screen across
     * it left of it (screen: 1 where none; smoke on call, release 2), so the air's part of it can be held back whole.
     */
    blow(u, t) {
      const pair = this.inOverrun(u, t);
      const uAt = pair && u.ovr ? u.ovr.back : [u.x, u.y], tAt = pair && t.ovr ? t.ovr.back : [t.x, t.y];
      const gu = coverAt(this.field, uAt[0], uAt[1]);
      let p = u.power0 * (0.35 + 0.65 * (u.str / u.str0)) * LAND.rate;
      if (this.bearers) p *= this.atHand(u);
      if (u.state === "wavering") p *= LAND.wavering;
      const rearguard = this.givingGround(t);
      if (u.order && u.path.length && u.speed > 0.5 || rearguard) p *= LAND.moving;
      if (u.winded !== void 0 && this.t < u.winded) p *= LAND.double.blows;
      let nerveMul = 1;
      const SP = LAND.hide.surprise;
      if (u.sprung !== void 0 && this.t - u.sprung < SP.secs) {
        p *= SP.punch;
        nerveMul *= t.speed > 0.2 && OPEN_GROUND.includes(coverAt(this.field, t.x, t.y)) && !this.onHedge(t.x, t.y) ? SP.open : SP.nerve;
      }
      if (u.team === "mor" && this.hide && this.units.some((f) => f.side === u.side && f.sprung !== void 0 && this.t - f.sprung < SP.secs && f.engaged === t.id)) p *= SP.mortar;
      const vague = t.unloc !== void 0 && this.t < t.unloc;
      if (vague) p *= LAND.hide.locate.fire;
      const screen = this.smoke && !vague && !t.hid && this.screens.length && this.direct(u, t) && this.screened(u.x, u.y, t.x, t.y) ? LAND.smoke.share : 1;
      p *= screen;
      if (u.kind === "art") {
        const G2 = gunsOf(u), close = this.gap(u, t) < G2.closeAt;
        p *= close ? G2.close : G2.punch;
        if (!close) nerveMul *= G2.nerve;
        if (u.team && t.kind === "arm") p *= LAND.teams[u.team].armour;
      }
      if (u.kind === "arm" && LAND.tanksClose.ground.includes(gu)) p *= LAND.tanksClose.punch;
      const rise = heightAt(this.field, uAt[0], uAt[1]) - heightAt(this.field, tAt[0], tAt[1]);
      if (rise > LAND.height.over) p *= LAND.height.up;
      else if (rise < -LAND.height.over) p *= LAND.height.down;
      const aspect = rearguard || pair ? "front" : aspectOf(t.face, t.x, t.y, u.x, u.y);
      const [am, nm] = LAND.aspect[aspect];
      let amx = 1;
      const plate = this.measured && t.kind === "arm" && (u.kind === "arm" || u.kind === "art" && !u.team && this.gap(u, t) < gunsOf(u).closeAt);
      if (plate) amx = this.armourAt(u, t, faceOfAspect(aspect), this.gap(u, t)) ?? 1;
      const ground = this.groundTake(t, tAt);
      let take = ground;
      if (t.kind === "arm" && LAND.alone.on) take *= this.alone(t, u);
      if (t.side === "def" && dist(t.x, t.y, t.home[0], t.home[1]) < LAND.hold.within) take *= this.holdTake(u, t);
      if (t.order?.kind === "move" && t.order.run && t.speed > 0.5) take *= LAND.double.exposed;
      const luck = 1 + LAND.jitter * (this.rng.next() * 2 - 1);
      const dmg = p * am * amx * take * luck * (u.side === "att" ? this.fortune : 1 / this.fortune) * DT;
      let faceOn = 0;
      if (this.faceRule) {
        const plateOn = plate && aspect !== "front" ? this.armourAt(u, t, "front", this.gap(u, t)) ?? 1 : amx;
        faceOn = p * LAND.aspect.front[0] * plateOn * take * (u.side === "att" ? this.fortune : 1 / this.fortune) * DT;
      }
      return { dmg, nerve: nm * nerveMul, front: aspect === "front", aspect, ground, screen, faceOn };
    }
    /**
     * 'from' is who struck it this tick and how hard. Each model it loses goes to the one of them owed most (LandUnit.owed:
     * the harm each striker has done it, less a model's worth for each model given), the first of equals; a gun or a tank
     * given a model lands a shell there. So guns are seen firing in their share of the harm, not only where they did the
     * most in a tick (the design director, b507). Looks only: nothing in the fight reads it.
     */
    suffer(t, dmg, nerveMul, from = [], blind = false, bare = false) {
      if (t.hid && dmg > 0 && !blind && !bare) this.reveal(t, "struck");
      if (t.waits && !t.waits.free && !bare && dmg > 0 && dist(t.x, t.y, t.waits.at[0], t.waits.at[1]) <= LAND.dug.keep) {
        const take = this.groundTake(t), f = Math.min(take, LAND.cover[LAND.wreck.cover]) / Math.max(1e-9, take);
        if (f < 1) {
          const spared = dmg * (1 - f);
          t.debt = (t.debt ?? 0) + spared;
          t.debtN = (t.debtN ?? 0) + spared * nerveMul;
          dmg *= f;
        }
      }
      const share = dmg / Math.max(1e-9, t.power0);
      const before = t.str, had = t.figures;
      t.str = Math.max(0, t.str - share * t.str0);
      const lost = (before - t.str) / t.str0;
      const steady2 = 1 - LAND.nerve.xp * t.xp - LAND.nerve.general * this.generalOf(t.side);
      t.mor = Math.max(0, t.mor - lost * LAND.nerve.perLoss[t.side] * nerveMul * Math.max(0.3, steady2));
      t.still = 0;
      t.figures = t.str <= 0 ? 0 : Math.max(1, Math.ceil(t.tray * t.str));
      if (this.bearers) {
        this.wounded(t, before - t.str);
        t.figures = Math.min(had, t.figures);
      }
      const owed = t.owed ?? (t.owed = {}), model = t.power0 / Math.max(1e-9, t.tray * t.str0);
      for (const [u, d] of from) owed[u.id] = (owed[u.id] ?? 0) + d;
      for (let k = t.figures; k < had; k++) {
        const w = t.kind === "arm" ? this.wreck(t) : null;
        let by2;
        for (const [u] of from) if (!by2 || owed[u.id] > owed[by2.id]) by2 = u;
        if (!by2) {
          this.drill?.fromAir(t.id, k);
          continue;
        }
        owed[by2.id] -= model;
        if (SHELLS_FROM.includes(by2.kind) && by2.team !== "mg" && !(by2.kind === "arm" && this.drill?.mgKill(by2, t, k))) this.shell(by2, t, k, w);
        else this.drill?.struckBy(by2.id, t.id, k);
      }
    }
    /**
     * A shell that cost a block its model k (counted from 0 in its models' order): it lands at that model's place within
     * the block's frontage and depth, or on the tank it knocked out, at her wreck (shells.ts). Fixed by the block and the
     * model, never drawn from the battle's random stream. A 'shell' event for the 3D game, sent once and never logged: it
     * says nothing, and names the block that fired only where the player knows it (b464(2); a block he does not know that
     * lands a shell on his is always guns firing from far back, since any other striker is known by the blow).
     */
    shell(by2, t, k, w) {
      let x, y;
      const man = this.drill?.manAt(t.id, k);
      if (man) {
        x = Math.round(clamp(man[0], 2, this.field.w - 2) * 10) / 10;
        y = Math.round(clamp(man[1], 2, this.field.h - 2) * 10) / 10;
      } else if (w) {
        x = w.x;
        y = w.y;
      } else {
        const across = (mix2(t.id * 787 + k * 271 + 11) - 0.5) * t.w * 0.9, along = (mix2(t.id * 449 + k * 523 + 5) - 0.5) * t.d * 0.8;
        const c = Math.cos(t.face), s = Math.sin(t.face);
        x = Math.round(clamp(t.x + across * c + along * s, 2, this.field.w - 2) * 10) / 10;
        y = Math.round(clamp(t.y - across * s + along * c, 2, this.field.h - 2) * 10) / 10;
      }
      const cal = by2.cal ?? "field", at = Math.round(this.t * 10) / 10, armour = t.kind === "arm";
      const ground = groundAt(this.field, x, y);
      const ev = { t: at, what: "shell", text: "", x, y, cal, on: t.id, armour, ground, reach: REACH[cal] };
      if (this.known(by2)) ev.by = by2.id;
      this.events.push(ev);
      this.drill?.shelled(x, y, by2.id, t.id, k);
      if (!armour && !inRiver(ground)) {
        this.craters.push({ id: -(this.craters.length + 1), kind: "crater", x, y, r: CRATER[cal] / 2, until: this.ticket.limit, at, cal });
      }
    }
    /**
     * A tank knocked out: her wreck stays where she stood, within the block's frontage, to the end of the fight (b477). Where
     * on the tray is fixed by the block and the count, never by the battle's dice, so the fight's own luck does not move.
     */
    wreck(t) {
      const n = this.wrecks.filter((z2) => z2.unit === t.id).length;
      const [across, along] = wreckOffset(t.id, n, t.w, t.d);
      const c = Math.cos(t.face), s = Math.sin(t.face);
      const x = clamp(t.x + across * c + along * s, 2, this.field.w - 2), y = clamp(t.y - across * s + along * c, 2, this.field.h - 2);
      const z = { id: this.wrecks.length + 1, kind: "wreck", x: Math.round(x * 10) / 10, y: Math.round(y * 10) / 10, r: LAND.wreck.r, until: this.ticket.limit, side: t.side, at: Math.round(this.t * 10) / 10, unit: t.id };
      this.wrecks.push(z);
      return z;
    }
    /** The share of a block on foot that wrecks shelter: one model's worth for each within its frontage or beside it (b477). */
    wreckShare(u) {
      if (!this.wrecks.length || MOTION[u.kind] !== "foot") return 0;
      const c = Math.cos(u.face), s = Math.sin(u.face), hw = u.w / 2 + LAND.wreck.beside, hd = u.d / 2 + LAND.wreck.beside;
      let n = 0;
      for (const z of this.wrecks) {
        const dx = z.x - u.x, dy = z.y - u.y;
        if (Math.abs(dx * c - dy * s) <= hw && Math.abs(dx * s + dy * c) <= hd) n++;
      }
      return Math.min(1, n / Math.max(1, u.tray));
    }
    generalOf(s) {
      return this.ticket[s].general?.level ?? 0;
    }
    nerve(u) {
      const N = LAND.nerve;
      const enemyNear = this.units.some((e) => e.side !== u.side && fighting2(e) && dist(e.x, e.y, u.x, u.y) < N.clear);
      if (enemyNear) u.clear = 0;
      if (u.state === "routing") {
        if (u.clear > N.quiet) u.mor = Math.min(u.mor0, u.mor + N.runnerRecover * DT);
        if (!this.broke[u.side] && u.clear > N.quiet && u.mor >= N.rally && u.str > N.shattered) {
          u.state = "wavering";
          u.order = null;
          u.path = [];
          u.act = "idle";
          u.gives = false;
          this.say({ what: "rallied", unit: u.id, side: u.side, text: `${this.named(u)} has rallied.`, x: u.x, y: u.y });
        }
        return;
      }
      if (u.still > N.still) u.mor = Math.min(u.mor0, u.mor + N.recover * DT);
      if (u.str <= N.shattered * u.str0 || u.mor < (u.waits?.late ? N.waver : N.rout)) {
        this.rout(u);
        return;
      }
      const was = u.state;
      u.state = u.mor < N.waver ? "wavering" : "steady";
      if (was === "steady" && u.state === "wavering") this.say({ what: "wavering", unit: u.id, side: u.side, text: `${this.named(u)} is wavering.`, x: u.x, y: u.y });
    }
    rout(u) {
      if (u.state === "routing") return;
      u.state = "routing";
      u.order = null;
      u.path = [];
      u.keepFace = null;
      u.engaged = void 0;
      this.say({ what: "routing", unit: u.id, side: u.side, text: `${this.named(u)} breaks and runs.`, x: u.x, y: u.y });
      for (const f of this.units) if (f !== u && f.side === u.side && fighting2(f) && dist(f.x, f.y, u.x, u.y) < LAND.nerve.near) f.mor = Math.max(0, f.mor - LAND.nerve.friendRuns);
      if (u.waits) this.endWait(u, "broke");
      else if (this.repay.length) this.landNow(u);
    }
    /**
     * The way a halted block wants to face, or null for as it faces: the facing its line was given, unless what it meets
     * is well off that; what it meets is the enemy it is fighting, or (faceRule) the one whose blow cost it most this tick.
     */
    facing(u) {
      const e = u.engaged !== void 0 ? this.unit(u.engaged) : void 0;
      let want = u.keepFace;
      const top = this.faceRule ? this.costliest.get(u.id) : void 0, by2 = top ? this.unit(top.by) : void 0;
      const meet = by2 && live2(by2) ? by2 : e;
      if (meet && !u.pinned) {
        const off = Math.abs(turn(u.keepFace ?? u.face, bearing(u.x, u.y, meet.x, meet.y)));
        if (u.keepFace === null || off > Math.PI / 3) want = bearing(u.x, u.y, meet.x, meet.y);
      }
      return want;
    }
    move(u) {
      const k = u.kind;
      u.pressing = void 0;
      if (u.ovr && this.drive(u)) return;
      const gave = u.yieldTo !== void 0 ? this.unit(u.yieldTo) : void 0;
      u.yieldTo = void 0;
      let to = null, run = false, sidle = false;
      if (u.state === "routing") {
        to = [u.x, u.side === "att" ? -50 : this.field.h + 50];
        run = true;
      } else if (u.order?.kind === "withdraw") {
        u.path = this.dry(this.going(u), u.x, u.y, u.x, u.order.y) && !this.exit(this.going(u), u.x, u.y) ? [[u.x, u.order.y]] : this.route(u, u.order.x, u.order.y);
        to = u.path[0];
      } else if (u.order?.kind === "attack") {
        let t = this.unit(u.order.target);
        if (u.order.press && t && !fighting2(t) && (!this.ai[u.side] || u.taken)) {
          const next = this.units.filter((e) => e.side !== u.side && fighting2(e) && dist(e.x, e.y, u.x, u.y) < LAND.press).reduce((m2, e) => !m2 || dist(e.x, e.y, u.x, u.y) < dist(m2.x, m2.y, u.x, u.y) ? e : m2, null);
          if (next) {
            const ax2 = u.order.axis, lane = ax2 === void 0 ? void 0 : (u.x - next.x) * Math.sin(ax2 + Math.PI / 2) + (u.y - next.y) * Math.cos(ax2 + Math.PI / 2);
            this.send(u, { kind: "attack", x: next.x, y: next.y, target: next.id, press: true, ...ax2 === void 0 ? {} : { lane, axis: ax2 } });
            t = next;
          }
        }
        if (!t || !live2(t)) {
          u.order = null;
          u.path = [];
        } else {
          const want = this.closeTo(u);
          const far = this.standRule && u.stoodFor === t.id ? want + (reachOf(u) - want) / 2 : want;
          const at = this.gap(u, t) > far || this.wet(u, u.x, u.y) ? this.lanePoint(u, t, want) : null;
          if (at) {
            u.path = this.route(u, at[0], at[1]);
            to = u.path[0];
            if (this.standRule) {
              sidle = this.standKeeps.step && u.stoodFor === t.id && !!u.stand && at[0] === u.stand[0] && at[1] === u.stand[1] && u.path.length === 1 && this.under(u, at[0], at[1]);
              if (!sidle) u.stoodFor = void 0;
            }
          } else {
            u.path = [];
            to = null;
            if (this.standRule) u.stoodFor = t.id;
          }
        }
      } else if (u.order && u.path.length) {
        to = u.path[0];
        if (u.order.kind === "move" && !this.dry(this.going(u), u.x, u.y, to[0], to[1])) {
          u.path = this.route(u, u.order.x, u.order.y);
          to = u.path[0];
        }
      }
      if (to && dist(u.x, u.y, to[0], to[1]) < 0.5 && !run) {
        if (u.path.length > 1 && u.path[0] === to) {
          u.x = to[0];
          u.y = to[1];
          u.path.shift();
          to = u.path[0];
        } else if (u.order?.kind === "move") {
          u.path.shift();
          to = u.path[0] ?? null;
        } else to = null;
      }
      if (u.engaged !== void 0 && u.order?.kind === "move" && u.kind !== "art") {
        const e = this.unit(u.engaged);
        if (e && this.gap(u, e) < 60) to = null;
      }
      const wading = !run && inRiver(groundAt(this.field, u.x, u.y));
      if (wading && !to) to = this.shore(u);
      if (!to) {
        u.speed = 0;
        const e = u.engaged !== void 0 ? this.unit(u.engaged) : void 0;
        const want = this.facing(u);
        if (u.order?.kind === "move" && !u.path.length) {
          if (this.standRule) u.heldAt = [u.order.x, u.order.y];
          u.order = null;
        }
        if (want !== null && Math.abs(turn(u.face, want)) > 0.02) {
          this.turnTo(u, want);
          u.act = "turn";
        } else u.act = e ? "fight" : "idle";
        if (u.state === "routing") u.act = "rout";
        return;
      }
      const head = bearing(u.x, u.y, to[0], to[1]);
      if (gave && !run && !this.wet(u, u.x, u.y, false) && Math.sin(head) * (gave.x - u.x) + Math.cos(head) * (gave.y - u.y) > 0) {
        u.speed = 0;
        u.act = "idle";
        return;
      }
      if (!run && u.order?.together !== void 0 && (k === "arm" || k === "mot") && !u.met && Math.cos(head) * (u.side === "att" ? 1 : -1) > 0 && !this.wet(u, u.x, u.y, false)) {
        const ahead0 = this.aheadOfFoot(u, u.order.together);
        if (ahead0 !== null && ahead0 >= STYLE.lead) {
          this.turnTo(u, head);
          u.speed = 0;
          u.act = "idle";
          return;
        }
      }
      const off = turn(u.face, head);
      const fell = this.standRule && this.standKeeps.way && !run && u.path.length > 1 && to === u.path[0] ? this.ways.get(u.id) : void 0;
      const via = !!fell && fell.fell === true && fell.at !== void 0 && fell.pts[fell.at] === to && this.under(u, to[0], to[1]);
      const back = this.standRule && !run && (sidle || via);
      if (!run && !wading && !back && Math.abs(off) > 0.6) {
        this.turnTo(u, head);
        u.speed = 0;
        u.act = "turn";
        return;
      }
      if (run) u.face = head;
      else if (!back) this.turnTo(u, head);
      else {
        const want = via ? bearing(u.x, u.y, u.path[1][0], u.path[1][1]) : this.facing(u);
        if (want !== null) this.turnTo(u, want);
      }
      const m = MOTION[k];
      const g = groundAt(this.field, u.x, u.y);
      let pace = LAND.pace[m][g];
      if (k === "mtn" && (g === "rough" || g === "rock")) pace = Math.min(1, pace * LAND.mountainFoot);
      const up = inRiver(g) ? 0 : heightAt(this.field, u.x + Math.sin(head) * 20, u.y + Math.cos(head) * 20) - heightAt(this.field, u.x, u.y);
      pace *= up > 0 ? Math.max(0.4, 1 - up / 20 * 1.5) : 1;
      const top = u.team ? LAND.teams[u.team].speed : LAND.speed[k];
      const double = !run && u.order?.kind === "move" && !!u.order.run;
      if (double) u.winded = this.t + LAND.double.winded;
      let v = top * Math.max(0.12, pace) * (run ? 1.15 : 1) * (double ? LAND.double.pace[m] : 1) * (u.state === "wavering" ? 0.85 : 1);
      const d = dist(u.x, u.y, to[0], to[1]);
      const stepLen = Math.min(d, v * DT);
      const [ax, ay] = ahead(head);
      let nx = u.x + ax * stepLen, ny = u.y + ay * stepLen;
      if (this.standRule && this.standKeeps.way && !run) {
        const kept = this.ways.get(u.id);
        if (kept?.blind && kept.m !== m && kept.m === this.going(u) && this.bears(kept.m, u.x, u.y) && !this.bears(kept.m, nx, ny)) {
          kept.stopped = true;
          u.speed = 0;
          u.act = "idle";
          return;
        }
      }
      if (LAND.pace[m][groundAt(this.field, nx, ny)] <= 0 && LAND.pace[m][g] > 0 && !run) {
        if (this.standRule && this.standKeeps.way) {
          const kept = this.ways.get(u.id);
          if (kept) kept.stopped = true;
        }
        let moved = false;
        for (const sw of [0.5, -0.5, 1, -1]) {
          const [bx, by2] = ahead(head + sw);
          const tx = u.x + bx * stepLen, ty = u.y + by2 * stepLen;
          if (LAND.pace[m][groundAt(this.field, tx, ty)] > 0) {
            nx = tx;
            ny = ty;
            moved = true;
            break;
          }
        }
        if (!moved) {
          u.speed = 0;
          u.act = "idle";
          return;
        }
      }
      u.x = clamp(nx, 5, this.field.w - 5);
      u.y = run ? ny : clamp(ny, 5, this.field.h - 5);
      u.speed = stepLen / DT;
      if (!run) u.pressing = head;
      u.act = run ? "rout" : v > top * 1.05 ? "run" : "march";
      u.cover = groundAt(this.field, u.x, u.y);
      if (d <= Math.max(6, stepLen + 0.01)) {
        u.path.shift();
        if (!u.path.length && u.order?.kind === "move") {
          if (this.standRule) u.heldAt = [u.order.x, u.order.y];
          u.order = null;
          u.speed = 0;
        }
      }
      if (u.y < 0 || u.y > this.field.h) {
        if (u.state === "routing") {
          u.state = "gone";
          u.act = "down";
          u.figures = 0;
          this.say({ what: "gone", unit: u.id, side: u.side, text: `${this.named(u)} has fled the field.` });
        } else if (u.order?.kind === "withdraw") {
          u.state = "withdrawn";
          u.act = "down";
          this.say({ what: "withdrawn", unit: u.id, side: u.side, text: `${this.named(u)} has fallen back off the field.` });
        }
      } else if (u.order?.kind === "withdraw" && (u.side === "att" ? u.y <= 6 : u.y >= this.field.h - 6)) {
        u.state = "withdrawn";
        u.act = "down";
        this.say({ what: "withdrawn", unit: u.id, side: u.side, text: `${this.named(u)} has fallen back off the field.` });
      }
    }
    /** Within LAND.queue.near metres of ground this block can hardly cross or not at all: at a bridge, a pass, a gap. */
    narrows(u, x, y) {
      const m = this.going(u), f = this.field, C = f.cols, R = f.rows;
      let near = this.hardNear[m];
      if (!near) {
        near = this.hardNear[m] = new Uint8Array(C * R);
        const n = Math.round(LAND.queue.near / f.cell), pace = this.paceOf(m);
        for (let i = 0; i < C * R; i++) {
          if (pace[GROUNDS[f.ground[i]]] > LAND.queue.hard) continue;
          const c0 = i % C, r0 = Math.floor(i / C);
          for (let r = Math.max(0, r0 - n); r <= Math.min(R - 1, r0 + n); r++) for (let c = Math.max(0, c0 - n); c <= Math.min(C - 1, c0 + n); c++) near[r * C + c] = 1;
        }
      }
      return near[this.cellAt(x, y)] === 1;
    }
    /** How far a block has still to go on its way. */
    toGo(u) {
      let d = 0, x = u.x, y = u.y;
      for (const [px, py] of u.path) {
        d += dist(x, y, px, py);
        x = px;
        y = py;
      }
      return d;
    }
    /**
     * Where a block in a lane makes for: in front of its target as the attack came on, out to the side by its lane, and
     * for a lane wider than the target's front, forward round its flank. With no lane, or once it stands at that point and
     * is still out of reach, straight at the target. Null once it stands at that point within reach: it stands and fights.
     */
    lanePoint(u, t, want) {
      const o = u.order;
      const lone = o.lane === void 0 || o.axis === void 0;
      const axis = lone ? bearing(u.x, u.y, t.x, t.y) : o.axis, lane = lone ? 0 : o.lane;
      const key = lone ? `${t.id}` : `${t.id}/${lane}/${axis}`;
      const hold = want + (reachOf(u) - want) / 2;
      const off = (x, y) => dist(x, y, t.x, t.y) - (Math.max(u.w, u.d) + Math.max(t.w, t.d)) / 4;
      const there = (s) => {
        if (dist(u.x, u.y, s[0], s[1]) >= 12) return s;
        if (this.gap(u, t) <= hold && !this.wet(u, u.x, u.y)) return null;
        u.closing = true;
        return [t.x, t.y];
      };
      const old = u.stand && u.standFor === key ? u.stand : null;
      if (old && u.closing) return [t.x, t.y];
      const kept = !!old && off(old[0], old[1]) <= hold && !this.wet(u, old[0], old[1]);
      if (old && kept && !this.claimed(u, old[0], old[1], u.standFace ?? bearing(old[0], old[1], t.x, t.y))) return there(old);
      const [fx, fy] = ahead(axis), [rx, ry] = ahead(axis + Math.PI / 2);
      const lim = t.w / 2 + u.w / 2;
      const side0 = Math.min(Math.abs(lane), lim) * Math.sign(lane);
      const back = Math.max(0, want * 0.8 + t.d / 2 + u.d / 2 - Math.max(0, Math.abs(lane) - lim));
      let p = [t.x + rx * side0 - fx * back, t.y + ry * side0 - fy * back];
      if (this.wet(u, p[0], p[1])) {
        const room = Math.max(0, back - (t.d / 2 + u.d / 2 + LAND.apart));
        let dry = null;
        for (let k = 8; k <= Math.max(room, 8) && !dry; k += 8) {
          const q = [p[0] + fx * Math.min(k, room), p[1] + fy * Math.min(k, room)];
          if (!this.wet(u, q[0], q[1])) dry = q;
        }
        for (let k = 8; k <= 240 && !dry; k += 8) {
          const q = [p[0] - fx * k, p[1] - fy * k];
          if (!this.wet(u, q[0], q[1])) dry = q;
        }
        if (dry) p = dry;
      }
      const face = bearing(p[0], p[1], t.x, t.y), [sx, sy] = ahead(face + Math.PI / 2);
      const fit = (q) => off(q[0], q[1]) <= hold && !this.wet(u, q[0], q[1]) && !this.units.some((e) => e.side !== u.side && live2(e) && dist(e.x, e.y, q[0], q[1]) < (Math.max(e.w, e.d) + Math.max(u.w, u.d)) / 2);
      const asides = [];
      for (let k = 0; k < 4; k++) {
        const f = this.claimed(u, p[0], p[1], face);
        if (!f) break;
        asides.push({ id: f.id, st: f.st });
        const lat = (p[0] - f.x) * sx + (p[1] - f.y) * sy, mine = (u.x - f.x) * sx + (u.y - f.y) * sy;
        const side = Math.abs(lat) >= 1 ? Math.sign(lat) : Math.sign(mine) || Math.sign(lane) || (u.id < f.id ? -1 : 1);
        const by2 = (u.w + f.w) / 2 + LAND.room;
        const near = [p[0] + sx * side * (by2 - Math.abs(lat)), p[1] + sy * side * (by2 - Math.abs(lat))];
        const far = [p[0] - sx * side * (by2 + Math.abs(lat)), p[1] - sy * side * (by2 + Math.abs(lat))];
        const q = fit(near) ? near : fit(far) ? far : null;
        if (!q) break;
        p = q;
      }
      if (old && kept && this.claimed(u, p[0], p[1], face)) return there(old);
      if (this.standRule) u.standBy = asides.length ? asides : void 0;
      u.stand = p;
      u.standFor = key;
      u.standFace = face;
      u.closing = false;
      return there(p);
    }
    /**
     * The friend whose ground this block's box would stand on at x, y facing so (with LAND.room between), or null: one
     * standing there, or one whose attack has it stand there and is nearer to it than this block is to x, y.
     */
    claimed(u, x, y, face) {
      const [fx, fy] = ahead(face), [rx, ry] = ahead(face + Math.PI / 2), mine = dist(u.x, u.y, x, y);
      for (const f of this.units) {
        if (f === u || f.side !== u.side || !fighting2(f)) continue;
        const was = this.standRule && this.standKeeps.beside ? u.standBy?.find((b) => b.id === f.id) : void 0;
        let at = was ? !was.st ? [f.x, f.y] : f.order?.kind === "attack" && f.stand ? f.stand : null : null, st = !!was && was.st;
        if (!at) {
          st = false;
          if (f.speed <= 0.5) at = [f.x, f.y];
          else if (f.order?.kind === "attack" && f.stand) {
            const theirs = dist(f.x, f.y, f.stand[0], f.stand[1]);
            if (theirs < mine || theirs === mine && f.id < u.id) {
              at = f.stand;
              st = true;
            }
          }
        }
        if (!at) continue;
        const dx = at[0] - x, dy = at[1] - y;
        if (Math.abs(dx) > 300 || Math.abs(dy) > 300) continue;
        const c = Math.abs(Math.cos(f.face - face)), s = Math.abs(Math.sin(f.face - face));
        const pl = u.w / 2 + f.w / 2 * c + f.d / 2 * s + LAND.room / 2 - Math.abs(dx * rx + dy * ry);
        const pd = u.d / 2 + f.w / 2 * s + f.d / 2 * c - Math.abs(dx * fx + dy * fy);
        if (pl > 0 && pd > 0) return { id: f.id, x: at[0], y: at[1], w: f.w, st };
      }
      return null;
    }
    /**
     * Ground a block may not stand on: the river, a bridge and the road off either end of it (kept clear for those
     * coming over), or ground it can hardly cross (LAND.queue.hard). A block sent to stand there by its own order
     * (mouth false) may stand at a bridge's end.
     */
    wet(u, x, y, mouth = true) {
      if (MOTION[u.kind] === "fixed") return false;
      const g = groundAt(this.field, clamp(x, 5, this.field.w - 5), clamp(y, 5, this.field.h - 5));
      if (g === "bridge" || g === "water" || g === "blown" || LAND.pace[MOTION[u.kind]][g] <= LAND.queue.hard) return true;
      if (!mouth) return false;
      if (!this.mouths) {
        const f = this.field, C = f.cols, R = f.rows, n = Math.round(LAND.mouth / f.cell);
        const m = this.mouths = new Uint8Array(C * R);
        for (let i = 0; i < C * R; i++) {
          const k = GROUNDS[f.ground[i]];
          if (k !== "bridge" && k !== "blown") continue;
          const c0 = i % C, r0 = Math.floor(i / C);
          for (let r = Math.max(0, r0 - n); r <= Math.min(R - 1, r0 + n); r++) for (let c = Math.max(0, c0 - n); c <= Math.min(C - 1, c0 + n); c++) m[r * C + c] = 1;
        }
      }
      return this.mouths[this.cellAt(x, y)] === 1;
    }
    /**
     * Out of the river (b464(5)): the nearest point off river ground the block can stand on, the bank before it rather
     * than the one behind unless that is much the nearer (a turn away counts as perTurn metres a radian: 30 while it
     * wades, none once the battle is over, so it is set down on the nearest bank). Where it stands already, that point.
     */
    shore(u, perTurn = 30) {
      const pace = LAND.pace[MOTION[u.kind]], f = this.field;
      const ok2 = (x, y) => {
        const g = groundAt(f, x, y);
        return !inRiver(g) && pace[g] > 0;
      };
      if (ok2(u.x, u.y)) return [u.x, u.y];
      let best = [u.x, u.y], score = Infinity;
      for (let r = 8; r <= 240 && r < score; r += 8) for (let a = 0; a < 16; a++) {
        const b = u.face + a * Math.PI / 8, x = clamp(u.x + Math.sin(b) * r, 5, f.w - 5), y = clamp(u.y + Math.cos(b) * r, 5, f.h - 5);
        const sc = r + perTurn * Math.abs(turn(u.face, b));
        if (sc < score && ok2(x, y)) {
          score = sc;
          best = [x, y];
        }
      }
      return best;
    }
    turnTo(u, want) {
      const r = LAND.turn[u.kind] * DT;
      const d = turn(u.face, want);
      u.face = Math.abs(d) <= r ? want : u.face + Math.sign(d) * r;
    }
    // ---- Stretcher bearers (release 2) --------------------------------------------------------------------------------
    //
    // Brian, 22 November 17:33Z ("lets do all three but make it where we can toggle it off if someone doesnt want it"); the
    // design director's rulings Q4 and Q5 (18:36Z), and the light cases (18:38Z); LAND.bearers. Of each loss of strength a
    // block suffers (suffer: the blows, the air, kept harm landing), LAND.bearers.killed are dead and the rest wounded. Of
    // the wounded, light.share are dressed at the block and back in the line light.dress seconds later (with no walk while
    // light.at is 0), if the block is still fighting then: running, fled or gone, it takes none back. They come back as
    // strength, up to what it started with, never raising its nerve: the only upward write to strength in the fight. The
    // rest are litter cases: while they lie waiting and its bearers (share of its strength at the start, by army) are home,
    // a block still fighting sends a trip, litter bearers a case, out to its aid post (LAND.bearers.aid behind where it
    // started, toward its own edge) and back at LAND.bearers.pace: 2 x aid / pace, 330 s. Bearers out are still counted in
    // its strength, its nerve, the day's count and its size: they only weaken its own blows, by the share of its strength
    // away (atHand). One land:bearers event a block, to the player's own side, as its bearers first go out. Nothing here
    // draws on the dice. The drill draws the men (drill.ts): the wounded, the bearers and the carries, looks only.
    /** Where a block's aid post stands: LAND.bearers.aid metres behind its start place, toward its own back edge, on the field. */
    aidOf(u) {
      const back = u.side === "att" ? -1 : 1;
      return [clamp(u.home[0], 0, this.field.w), clamp(u.home[1] + back * LAND.bearers.aid, 0, this.field.h)];
    }
    /** A block's bearers and wounded, as the rules count them (strength): none before its first loss, or with the switch off. */
    litter(u) {
      return this.bearers ? this.litters.get(u.id) : void 0;
    }
    /** A loss of strength d on a block: its share of wounded, the light cases to their dressing, the rest lying for the bearers. */
    wounded(u, d) {
      if (!(d > 0)) return;
      const B = LAND.bearers, L2 = B.light;
      let l = this.litters.get(u.id);
      if (!l) {
        l = { all: bearerShare(u.nat) * u.str0, out: 0, trips: [], waiting: 0, light: [], aid: this.aidOf(u), told: false };
        this.litters.set(u.id, l);
      }
      const hurt = d * (1 - killedShare(u.nat)), light = hurt * L2.share, foot = LAND.speed.inf;
      const at = Math.round((this.t + (L2.at > 0 ? L2.at / (foot * L2.out) : 0) + L2.dress + (L2.at > 0 ? L2.at / (foot * L2.back) : 0)) * 1e3) / 1e3;
      const last = l.light[l.light.length - 1];
      if (last && last.at === at) last.n += light;
      else if (light > 0) l.light.push({ at, n: light });
      l.waiting += hurt - light;
    }
    /**
     * Each tick, after the harm: the bearers whose trip is done are home; the light cases whose dressing is done are back
     * in the line, where their block still fights (up to its strength at the start); then, while litter cases wait and
     * bearers are home, a block still fighting sends them out: min(waiting, home / litter) cases, litter bearers each.
     */
    carry() {
      const B = LAND.bearers, trip = 2 * B.aid / B.pace, t = this.t;
      for (const u of this.units) {
        const l = this.litters.get(u.id);
        if (!l) continue;
        if (l.trips.length && l.trips[0].back <= t + 1e-9) {
          while (l.trips.length && l.trips[0].back <= t + 1e-9) l.trips.shift();
          l.out = l.trips.reduce((a, x) => a + x.by, 0);
        }
        while (l.light.length && l.light[0].at <= t + 1e-9) {
          const n = l.light.shift().n, was = u.str;
          if (!fighting2(u)) continue;
          u.str = Math.min(u.str0, u.str + n);
          if (u.str <= was) continue;
          let r;
          for (let i = this.returns.length - 1; i >= 0 && !r; i--) if (this.returns[i].unit === u.id) r = this.returns[i];
          if (r && Math.floor(r.t + 1e-9) === Math.floor(t + 1e-9)) r.n += u.str - was;
          else this.returns.push({ t: Math.round(t * 1e3) / 1e3, unit: u.id, n: u.str - was });
        }
        if (!fighting2(u) || l.waiting <= 1e-12) continue;
        const home = l.all - l.out;
        if (home <= 1e-12) continue;
        const cases = Math.min(l.waiting, home / B.litter), by2 = cases * B.litter;
        l.waiting -= cases;
        l.out += by2;
        l.trips.push({ back: Math.round((t + trip) * 1e3) / 1e3, by: by2 });
        if (!l.told) {
          l.told = true;
          if (this.ticket[u.side].human) this.say({ what: "land:bearers", unit: u.id, side: u.side, text: `${this.named(u)}'s stretcher bearers are carrying the wounded back.`, x: u.x, y: u.y });
        }
      }
    }
    /** The share of a block's strength at hand to strike with: all of it, less its bearers out carrying (none below nothing). */
    atHand(u) {
      const l = this.litters.get(u.id);
      if (!l || l.out <= 0 || u.str <= 0) return 1;
      return Math.max(0, u.str - l.out) / u.str;
    }
    /**
     * The bearers' part of the view, a side's own blocks (every side's with no side given, as a recording is): each block's
     * bearers out and litter cases waiting, in men (strength times its men at full strength), and the battle second the
     * next of its bearers are back (null with none out); the aid post of each block that has had wounded; and the light
     * cases back in the line in the two battle seconds before this one (when, the block, how many men), each told once by
     * its t and block: a battle second is shown only once it is over, so an entry never changes after it is shown (a
     * block's returns in one second are summed), and one of no whole man is left out. Every side's where the view is
     * unfogged (the card knew the defence), as the telephone lines are. Left out with the switch off, and before anyone
     * is hurt.
     */
    bearerView(as) {
      if (!this.bearers || !this.litters.size) return {};
      const r13 = (v) => Math.round(v * 10) / 10;
      const ours = (u) => !!u && (as === void 0 || u.side === as || !this.fog);
      const men = (u, s) => Math.round(s * u.men0 / Math.max(1e-9, u.str0));
      const bearers = [], aid = [], returns = [];
      for (const [id, l] of [...this.litters].sort((a, c) => a[0] - c[0])) {
        const u = this.unit(id);
        if (!ours(u)) continue;
        aid.push({ side: u.side, unit: id, x: r13(l.aid[0]), y: r13(l.aid[1]) });
        const out = men(u, l.out), waiting = men(u, l.waiting);
        if (out > 0 || waiting > 0) bearers.push({ unit: id, out, waiting, back: l.trips.length ? l.trips[0].back : null });
      }
      const now = Math.floor(this.t + 1e-9);
      for (const r of this.returns) {
        const u = this.unit(r.unit), sec = Math.floor(r.t + 1e-9);
        if (sec >= now || sec < now - 2 || !ours(u)) continue;
        const n = men(u, r.n);
        if (n > 0) returns.push({ t: r.t, unit: r.unit, n });
      }
      return { ...bearers.length ? { bearers } : {}, ...aid.length ? { aid } : {}, ...returns.length ? { returns } : {} };
    }
    // ---- The end --------------------------------------------------------------------------------------------------
    /** Each side's standing power as a share of both sides' at the start: the balance bar. */
    standing(due) {
      const tot = this.power.att + this.power.def;
      const s = { att: 0, def: 0 };
      for (const u of this.units) {
        if (!live2(u)) continue;
        const k = u.state === "routing" ? 0.3 : 1;
        const d = due?.get(u.id), str = d === void 0 ? u.str : Math.max(0, u.str - d / Math.max(1e-9, u.power0) * u.str0);
        s[u.side] += u.priced === void 0 ? u.power0 * (str / u.str0) * k / tot : Math.max(0, u.priced - u.power0 * (1 - str / u.str0)) * k / tot;
      }
      for (const c of this.coming) s[c.u.side] += c.u.priced / tot;
      return s;
    }
    /**
     * A force's own team block's division on the field (own-teams.ts): the line blocks of the force it was taken from.
     * Empty for every other block, and for a team block with no division of its own.
     */
    ownLine(u) {
      if (!ownArm2(u)) return [];
      const parent = this.teamParent.get(u.corps);
      return parent === void 0 ? [] : this.units.filter((p) => p.corps === parent && !p.team);
    }
    /**
     * The power of its force's own team blocks that stands with a line block (0 for a block whose force brought none):
     * each team block's shared among its division's line blocks by their power. The 8% a division gave its teams is
     * still the division's weight (the staff's plan, ai.ts makePlan, with teamsRule).
     */
    ownTeamsPower(u) {
      if (u.team || !this.teamParent.size) return 0;
      let n = 0;
      for (const t of this.units) {
        if (!ownArm2(t) || this.teamParent.get(t.corps) !== u.corps) continue;
        const line = this.ownLine(t), whole = line.reduce((a, p) => a + p.power0, 0);
        if (whole > 0 && line.includes(u)) n += t.power0 * (u.power0 / whole);
      }
      return n;
    }
    /**
     * Support teams in battle (teamsRule): of a force's own team block, the share that stands in a sum at the brink as
     * its division's line stands: `part` of each of its line blocks over their `whole`. Null where the block counts as
     * itself: the rule off, no team block, or one with no division of its own on the field.
     * A team takes its state (fresh, worn, running) from its division's line, but its own men still leave the sums as
     * they fall (the design director, 25 November 20:22Z: priced once, no ghost): what it has lost is worn, as any
     * block's is, and only what is left of it stands as its division stands. A team wiped out is all worn and counts
     * nothing in the running or the fresh, whatever its division.
     */
    asItsLine(u, whole, part) {
      if (!this.teamsRule || !ownArm2(u)) return null;
      const line = this.ownLine(u);
      if (!line.length) return null;
      const all = line.reduce((a, p) => a + whole(p), 0);
      return all > 0 ? line.reduce((a, p) => a + part(p), 0) / all : null;
    }
    brokenShare(s) {
      const running = (u) => u.state === "routing" || u.state === "gone";
      let gone = 0;
      for (const u of this.units) {
        if (u.side !== s) continue;
        const k = this.asItsLine(u, weightOf, (p) => running(p) ? weightOf(p) : 0);
        if (k !== null) gone += weightOf(u) * (u.str / u.str0) * k;
        else if (running(u)) gone += weightOf(u);
      }
      return gone / this.power[s];
    }
    /** The share of a side's power worn away on the field (strength lost, before anyone runs). */
    wornShare(s) {
      let worn = 0;
      for (const u of this.units) {
        if (u.side !== s) continue;
        const k = this.asItsLine(u, (p) => p.power0, (p) => p.power0 * (1 - p.str / p.str0)), left = u.str / u.str0;
        worn += u.power0 * (1 - left + (k === null ? 0 : left * k));
      }
      return worn / this.power[s];
    }
    /**
     * How near an army is to breaking, 0 to 1: its whole worn state weighted by strength against what an army of its
     * side will stand (LAND.spent), or half its power running, whichever is nearer. One block running counts only its
     * own share, so it never takes the army with it while the rest is fresh (ruling 226(2)). The army bar shows this.
     */
    breaking(s) {
      return this.brink(s).bar;
    }
    /** The bar, with the two shares it is reckoned from and whether the fresh hold keeps it at the brink. */
    brink(s) {
      const worn = this.wornShare(s), running = this.brokenShare(s), over = worn / LAND.spent[s];
      const holding = over >= 1 && this.freshShare(s) > LAND.fought;
      return { bar: Math.min(holding ? 0.99 : 1, Math.max(running / LAND.breaks, over)), worn, running, holding };
    }
    /**
     * Why an army's bar stands where it does (b496(2)): the two shares breaking() takes the nearer of, each against its
     * mark, its blocks as their cards read them, and the fresh hold. Worn across the whole line, a large army breaks at
     * LAND.spent of its power lost (13 in 100 for an attack) while every block's card may still read Steady: these are the
     * numbers for the words that say so. Read at the break for the result and the 'broken' event, and as it stands for the
     * view; nothing in the fight reads it.
     */
    breakCause(s) {
      const r33 = (x) => Math.round(x * 1e3) / 1e3;
      const b = this.brink(s), w = b.worn / LAND.spent[s], r = b.running / LAND.breaks;
      const mine = this.units.filter((u) => u.side === s), nearer = r >= w && r > 0 ? "running" : "worn";
      const on = mine.filter((u) => u.state !== "withdrawn"), p0 = on.reduce((a, u) => a + weightOf(u), 0);
      const steady2 = p0 > 0 ? on.filter((u) => u.state === "steady").reduce((a, u) => a + weightOf(u), 0) / p0 : 0;
      return {
        side: s,
        bar: r33(b.bar),
        by: Math.max(w, r) < 1 ? "none" : nearer,
        nearer,
        worn: r33(b.worn),
        spent: LAND.spent[s],
        running: r33(b.running),
        breaks: LAND.breaks,
        fresh: r33(this.freshShare(s)),
        fought: LAND.fought,
        small: this.freshHold[s],
        holding: b.holding,
        held: this.heldFor[s] > 0,
        heldFor: Math.round(this.heldFor[s] * 10) / 10,
        blocks: mine.length,
        blocksWorn: mine.filter((u) => u.str < u.str0 * LAND.fresh.str).length,
        wornAt: LAND.fresh.str,
        routing: mine.filter((u) => u.state === "routing").length,
        fled: mine.filter((u) => u.state === "gone").length,
        steady: r33(steady2),
        stillSteady: steady2 >= 0.9995 ? "all" : steady2 > 0.5 ? "most" : "less"
      };
    }
    /**
     * The share of a side's power standing in blocks that have not been in the fight (never met the enemy, and not worn
     * by a blow from afar) or, in an army of LAND.fresh.upTo blocks or fewer, that are fresh: steady, at nine tenths of
     * their strength or more, in contact or not. While more than half the army stands so, one worn block never takes it
     * with it (ruling 226(2); the design director, b476(1) and b480). Not for a defence behind works or in a fort: the
     * works keep its men fresh and the card prices the works already, so the field never prices them twice (b488).
     */
    freshShare(s) {
      let n = 0;
      for (const u of this.units) {
        if (u.side !== s) continue;
        const k = this.asItsLine(u, weightOf, (p) => fighting2(p) && this.fresh(p) ? weightOf(p) : 0);
        if (k !== null) n += weightOf(u) * (u.str / u.str0) * k;
        else if (fighting2(u) && this.fresh(u)) n += weightOf(u);
      }
      for (const c of this.coming) if (c.u.side === s) n += c.u.priced;
      return n / this.power[s];
    }
    /** A block that holds its army at the brink (freshShare): fresh, in a small army, or not yet in the fight. */
    fresh(u) {
      return this.freshHold[u.side] && u.state === "steady" && u.str >= u.str0 * LAND.fresh.str || !u.met && u.str >= u.str0 * 0.97;
    }
    /**
     * A small army held at the brink by its fresh blocks does not stand there to be ground down (the design director,
     * b496(1)): each block of it no longer fresh gives ground, falling back off the field while the fresh hold, and is
     * struck only on its rearguard as it goes (blow); so is one already running, behind the fresh. The fresh still hold
     * the army until most of it has fought. A block already falling back is not sent again, and says nothing; one the
     * player sends in again is left to the player (gives stays set until it rallies).
     */
    giveGround(s, left) {
      for (const u of this.units) if (u.side === s && u.state === "routing") u.gives = true;
      for (const u of left) {
        if (u.gives || this.fresh(u)) continue;
        u.gives = true;
        if (u.order?.kind === "withdraw") continue;
        if (u.waits) this.endWait(u, "gives");
        this.send(u, { kind: "withdraw", x: u.x, y: s === "att" ? 0 : this.field.h });
        u.engaged = void 0;
        this.say({ what: "land:gives", unit: u.id, side: s, text: `${this.named(u)} is worn out and falls back; the rest of its army holds.`, x: u.x, y: u.y });
      }
    }
    /** Giving ground at the brink, and still on its way back or running (an order of the player's may send it in again). */
    givingGround(u) {
      return !!u.gives && (u.order?.kind === "withdraw" || u.state === "routing");
    }
    judge() {
      for (const s of ["att", "def"]) {
        const left = this.units.filter((u) => u.side === s && fighting2(u));
        if (!left.length) {
          const how = this.units.some((u) => u.side === s && u.state === "withdrawn") && !this.units.some((u) => u.side === s && u.state === "routing") ? "withdrew" : "destroyed";
          this.finish(other(s), how, void 0, how === "destroyed" ? this.breakCause(s) : void 0);
          return;
        }
        const b = this.brink(s), near = b.bar;
        this.bar[s] = near;
        if (b.holding) this.heldFor[s] += DT;
        if (b.holding && this.freshHold[s]) this.giveGround(s, left);
        if (!this.warned[s] && near >= 0.75 && (this.ticket.att.human || this.ticket.def.human)) {
          this.warned[s] = true;
          const own = !!this.ticket[s].human, why = own ? this.breakCause(s) : void 0;
          this.say({ what: "nearBreak", side: s, text: why ? warnLine(why) : `${this.army(s)} is near breaking.`, ...why ? { brokeBy: why } : {} });
        }
        if (near >= 1) {
          this.broke[s] = true;
          const why = this.breakCause(s);
          for (const u of left) {
            u.fellBack = true;
            u.order = { kind: "withdraw", x: u.x, y: s === "att" ? 0 : this.field.h };
            u.path = [];
            u.keepFace = null;
            u.engaged = void 0;
          }
          this.say({ what: "broken", side: s, text: breakLine(why, this.whose(s)), brokeBy: { ...why } });
          this.finish(other(s), "rout", void 0, why);
          return;
        }
      }
      if (this.t >= this.ticket.limit) this.finish("def", "time");
    }
    /**
     * What a block keeps of its strength in the day's count: the fled leave LAND.count.gone behind them, runners and those
     * that fell back in order LAND.count.routing. A defence's runner caught in the water at the end is booked as fled (the
     * design director, b608(3)): it has not got back in order, and a stand with the water at its back carries its price at
     * the break. An attack's runner back in a river it came over counts as before: the card's crossing prices that.
     */
    kept(u) {
      const caught = u.side === "def" && u.state === "routing" && inRiver(groundAt(this.field, u.x, u.y));
      return u.state === "gone" || caught ? u.str * (1 - LAND.count.gone[u.side]) : u.state === "routing" || u.fellBack ? u.str * (1 - LAND.count.routing[u.side]) : u.str;
    }
    /** Each side's share of its starting power lost: strength worn away, and what runners and the fled leave behind them. */
    lostShares() {
      const lost = { att: 0, def: 0 };
      for (const u of this.units) {
        const kept = this.kept(u);
        lost[u.side] += u.power0 * (1 - kept / u.str0);
      }
      return { att: lost.att / this.power.att, def: lost.def / this.power.def };
    }
    finish(winner, how, R, why) {
      if (this.over) return;
      this.payAir();
      this.payWaits();
      const lost = this.lostShares();
      const men = { att: 0, def: 0 };
      for (const u of this.units) {
        const kept = this.kept(u);
        men[u.side] += Math.round(u.men0 * (1 - kept / u.str0));
      }
      const k = this.ticket.reckoning;
      const r = R ?? clampToDice(fieldRatio(winner, lost), k.odds, k.dice, winner);
      const verdict = r >= RATIO.carry ? "att" : "def";
      const forces = /* @__PURE__ */ new Map();
      for (const u of this.units) {
        const f = forces.get(u.corps) ?? { corps: u.corps, side: u.side, lost: 0, p: 0 };
        f.lost += u.power0 * (1 - u.str / u.str0);
        f.p += u.power0;
        forces.set(u.corps, f);
      }
      for (const c of this.coming) if (!forces.has(c.u.corps)) forces.set(c.u.corps, { corps: c.u.corps, side: c.u.side, lost: 0, p: 0 });
      const loser = other(winner);
      const pct = (p) => p < 1e-4 ? "none" : p < 5e-3 ? "less than 1%" : `about ${Math.round(p * 100)}%`;
      const cap = (t) => t[0].toUpperCase() + t.slice(1);
      const line = this.practice && this.ticket.def.human ? "Your line" : this.practice ? "Their line" : `The ${this.ticket.def.adj} line`;
      const told = how === "rout" && why && why.by !== "none";
      const lines = [
        how === "rout" ? this.brokeAfter(loser, told ? why : void 0) : how === "withdrew" ? `${this.army(loser)} withdrew from the field.` : how === "time" ? `${line} held until the day was done.` : how === "destroyed" ? `${this.army(loser)} was driven from the field.` : this.staffLine()
      ];
      if (how !== "objective") lines.push(lost.att < 1e-4 && lost.def < 1e-4 ? "Neither side lost any of its strength." : told ? `By the day's end, counting the rout, ${this.sideOf("att", "attack")} had lost ${pct(lost.att)} of its strength in all, ${this.sideOf("def", "defence")} ${pct(lost.def)}.` : `${cap(this.sideOf("att", "attack"))} lost ${pct(lost.att)} of its strength, ${this.sideOf("def", "defence")} ${pct(lost.def)}.`);
      else if (this.contactAt !== null) lines.push(`Before you handed the battle to the staff, ${this.sideOf("att", "attack")} had lost ${pct(lost.att)} of its strength on the field, and ${this.sideOf("def", "defence")} ${pct(lost.def)}.`);
      for (const u of this.units) if (live2(u) && u.state !== "routing") {
        u.act = "idle";
        u.speed = 0;
        u.engaged = void 0;
      }
      for (const u of this.units) if (live2(u) && inRiver(groundAt(this.field, u.x, u.y))) {
        [u.x, u.y] = this.shore(u, 0);
        u.cover = groundAt(this.field, u.x, u.y);
      }
      this.over = {
        winner: verdict,
        how,
        seconds: Math.round(this.t),
        lost,
        men,
        R: Math.round(r * 1e3) / 1e3,
        level: levelOf(r, men.att > men.def),
        lines,
        forces: [...forces.values()].map((f) => ({ corps: f.corps, side: f.side, lost: f.p > 0 ? Math.round(f.lost / f.p * 1e3) / 1e3 : 0, ...this.joinCorps.has(f.corps) ? { arrived: this.joinedAt.get(f.corps) ?? null } : {} })),
        ...how === "rout" || how === "destroyed" ? { broke: loser, ...why ? { brokeBy: why } : {} } : {},
        // Pulled out after the armies met: the rearguard and the stragglers are the rules' to bill, by its bar (b474).
        ...how === "withdrew" && this.contactAt !== null ? { pulled: { side: loser, bar: Math.round(Math.min(1, this.bar[loser]) * 1e3) / 1e3 } } : {},
        // What the fighters on call flew and lost, by group, for the campaign's bill (air support).
        ...this.airOn.att || this.airOn.def ? { air: this.airBill() } : {}
      };
      this.strikes = [];
      if (verdict !== winner) this.over.lines.push("The day went their way, but the campaign counts it as close as the odds allowed.");
      this.say({ what: "end", text: lines[0] });
    }
    /**
     * The staff's line, in its three forms (the UI director's words for ruling 226(5)): before any contact, the whole
     * attack; after it, the rest, and "what you had won stays won" only where the field had moved the player's way from
     * where the card put it, by five points of the balance or more.
     */
    staffLine() {
      const what = this.practice ? "battle" : `attack on ${this.ticket.place.name}`;
      if (this.contactAt === null) return `The staff settled the whole ${what}: the armies had not met.`;
      const me = this.ticket.def.human && !this.ticket.att.human ? "def" : "att";
      const bal = this.standing(), tot = this.power.att + this.power.def;
      const moved = bal[me] / Math.max(1e-9, bal.att + bal.def) >= this.power[me] / tot + 0.05;
      return `The staff settled the rest of the ${what} from where it stood${moved ? ": what you had won stays won" : ""}.`;
    }
    /**
     * How long it broke after: the field's own time from the first contact to the break (ruling 226(6)), to the half
     * minute (b464(6)). Whole minutes made everything from half a minute to a minute and a half "a minute", the span most
     * routs at odds the card calls sure or hopeless fall in.
     */
    brokeAfter(s, why) {
      const sec = this.t - (this.contactAt ?? this.t), who = this.army(s);
      const halves = Math.round(sec / 30), m = Math.floor(halves / 2), half = halves % 2 === 1;
      const n = ["", "a", "two", "three", "four", "five", "six"][m] ?? `${m}`;
      const span2 = halves === 0 ? "within half a minute" : `after ${m === 0 ? "half a minute" : m === 1 ? half ? "a minute and a half" : "a minute" : `${n}${half ? " and a half" : ""} minutes`}`;
      const when = `${who} broke ${span2} of fighting`;
      if (!why) return `${when}.`;
      const said = breakLine(why, who, true);
      const after = said.startsWith(`${who} broke`) ? said.slice(`${who} broke`.length) : `: ${said}`;
      return `${when}${after.startsWith(":") ? after : `,${after}`}`;
    }
    /** Each side's standing power against what it brought, as the staff reckons the day now. */
    standingShares(due) {
      const st = this.standing(due), tot = this.power.att + this.power.def;
      return { att: st.att / (this.power.att / tot), def: st.def / (this.power.def / tot) };
    }
    /**
     * The staff's chance now that the attack carries the ground, were the battle handed to them this moment: the dice's
     * own chance at the ratio standing, held to their run at the card's odds (what toDice would roll). For HAND IT TO THE
     * STAFF's question (ruling 226(5)).
     */
    chanceNow() {
      const k = this.ticket.reckoning, sh = this.standingShares(this.dueNow());
      if (k.odds * k.dice[0] >= RATIO.carry) return 1;
      if (k.odds * k.dice[1] < RATIO.carry) return 0;
      const now = k.odds * (Math.max(1e-6, sh.att) / Math.max(1e-6, sh.def));
      return Math.max(0, Math.min(1, (k.dice[1] - RATIO.carry / Math.max(1e-6, now)) / (k.dice[1] - k.dice[0])));
    }
    /**
     * The staff's chance at a card's own odds, the fight having moved the balance since the card: for the staff's band
     * where the card did not know the defence (b453), so HAND IT TO THE STAFF gives a range and never the true chance.
     */
    chanceAt(card) {
      const k = this.ticket.reckoning, bal = this.standing(this.dueNow()), r = bal.att / Math.max(1e-6, bal.def) / Math.max(1e-6, k.odds);
      if (card * k.dice[0] >= RATIO.carry) return 1;
      if (card * k.dice[1] < RATIO.carry) return 0;
      const now = card * r;
      return Math.max(0, Math.min(1, (k.dice[1] - RATIO.carry / Math.max(1e-6, now)) / (k.dice[1] - k.dice[0])));
    }
    /** Hand it to the staff (ruling 217(4), the button named in b348): the rest is rolled on the dice, at the ratio then standing. */
    toDice() {
      if (this.over) return;
      const k = this.ticket.reckoning;
      for (const s of ["att", "def"]) if (this.airOn[s]) this.airSettled[s] = this.airLeft[s];
      this.payAir();
      this.payWaits();
      const R = diceNow(k.odds, k.dice, this.standingShares(), this.rng.next());
      this.finish(R >= RATIO.carry ? "att" : "def", "objective", R);
    }
    // ---- Air support ------------------------------------------------------------------------------------------------
    //
    // Brian, 2026-11-18 18:58Z: "during ground battles where fighters are in range, you should be able to call in air
    // support where the fighters will strafe and drop bombs. the fighter squadron has to be on ground support mode where
    // they can be called in for missions like this that turn." The design director, b533: counted on the card, placed by
    // the player, shown as pictures. The campaign counts a side's fighters on call in its sky term (SideTicket.air.share:
    // the part of its strength they made); on the field, while the side has strikes left to call, its blows hold that
    // share back, owed to the block each fell on, and each strike pays it out where its run lands. So the field never
    // adds to the card (b418): the strikes carry part of the strength the card counted, and only where they land is the
    // player's. The first strike is ready LAND.air.first after the armies meet, each next LAND.air.every after the last
    // call; one left ready LAND.air.wait the staff call themselves, for either side, at the blocks owed most, paying
    // each what is owed (b533(c): the enemy's strikes are the staff's, never aimed sharper). A run is a strip of ground
    // its guns rake, never within LAND.air.close of the caller's own men (no friendly fire, b533(d)). Planes are pictures:
    // no flight model, no aiming, no gunnery (air-battle-paused); a plane lost is the campaign's price for a strike
    // (AirOnCall.groups[].lose), shown as it falls. Nothing here draws on the battle's dice: where neither side has
    // fighters on call, the battle is exactly as it was.
    /** Whether this side has fighters on call over this battle. */
    hasAir(s) {
      return this.airOn[s] !== null;
    }
    /** This side's strike in the air, if any: one flight's strike at a time. */
    flying(s) {
      return this.strikes.some((k) => k.side === s && !k.ended);
    }
    /** Whether this side has a strike ready to call now. */
    airReady(s) {
      return this.airLeft[s] > 0 && this.t >= this.airNext[s] && !this.flying(s) && !this.over;
    }
    /**
     * Why this side cannot call a strike down this run now, in the UI director's words to be (b533(d)), or null where it
     * can: none on call; all flown; before the armies meet; the next not ready yet, or one already in the air; or the run
     * too close to its own men to fire on enough of it.
     */
    airWhyNot(s, run) {
      if (!this.airOn[s]) return "No fighters on call";
      if (this.airLeft[s] <= 0) return "All strikes flown";
      if (this.contactAt === null) return "After the armies meet";
      if (this.flying(s)) return "A strike is on its way";
      if (this.t < this.airNext[s]) {
        const w = Math.ceil(this.airNext[s] - this.t);
        return `Ready in ${Math.floor(w / 60)}:${String(w % 60).padStart(2, "0")}`;
      }
      if (run && this.clearShare(s, run) < LAND.air.least) return "Too close to your own men";
      return null;
    }
    /**
     * The run a strike flies when the player points at a place rather than drawing one: down the frontage of the enemy
     * block nearest the point, from one end to the other (at least LAND.air.minRun long); with none within LAND.air.near,
     * a run of that length across the field through the point.
     */
    airRunAt(s, x, y) {
      const foes = this.units.filter((e) => e.side !== s && live2(e) && e.kind !== "fort" && this.known(e) && !e.hid);
      const near = foes.reduce((m, e) => dist(e.x, e.y, x, y) < (m ? dist(m.x, m.y, x, y) : LAND.air.near) ? e : m, null);
      return near ? this.frontageRun(near) : this.clipRun([x - LAND.air.minRun / 2, y, x + LAND.air.minRun / 2, y]);
    }
    /** A run drawn by hand, as it will be flown: kept on the field and to its longest (what callAir flies). */
    airRun(r) {
      return this.clipRun(r);
    }
    /** A run down a block's frontage, its whole width, at least LAND.air.minRun. */
    frontageRun(b) {
      const [cx, cy] = ahead(b.face + Math.PI / 2), half = Math.max(b.w, LAND.air.minRun) / 2;
      return this.clipRun([b.x - cx * half, b.y - cy * half, b.x + cx * half, b.y + cy * half]);
    }
    /** A run kept on the field and to its longest, about its middle. */
    clipRun(r) {
      let [x0, y0, x1, y1] = r;
      const len = dist(x0, y0, x1, y1);
      if (len > LAND.air.len) {
        const mx = (x0 + x1) / 2, my = (y0 + y1) / 2, k = LAND.air.len / len;
        x0 = mx + (x0 - mx) * k;
        y0 = my + (y0 - my) * k;
        x1 = mx + (x1 - mx) * k;
        y1 = my + (y1 - my) * k;
      }
      const w = this.field.w, h = this.field.h;
      return [clamp(x0, 0, w), clamp(y0, 0, h), clamp(x1, 0, w), clamp(y1, 0, h)].map((v) => Math.round(v * 10) / 10);
    }
    /** The points a run's guns rake, one a metre-step apart along it, at least two. */
    runPoints(r, n) {
      const out = [];
      for (let i = 0; i < n; i++) {
        const f = (i + 0.5) / n;
        out.push([r[0] + (r[2] - r[0]) * f, r[1] + (r[3] - r[1]) * f]);
      }
      return out;
    }
    /** Whether a point is within LAND.air.close of this side's own men: its guns hold fire there. */
    overOwn(s, x, y) {
      return this.units.some((u) => u.side === s && live2(u) && dist(u.x, u.y, x, y) - Math.max(u.w, u.d) / 2 < LAND.air.close);
    }
    /** The share of a run its guns may fire on, clear of the side's own men. */
    clearShare(s, r) {
      const pts = this.runPoints(r, 20);
      return pts.filter(([x, y]) => !this.overOwn(s, x, y)).length / pts.length;
    }
    /** The enemy blocks a run's guns rake at a point: those whose ground the strip LAND.air.wide across covers there. */
    underRun(s, x, y) {
      const half = LAND.air.wide / 2;
      return this.units.filter((e) => {
        if (e.side === s || !live2(e) || e.kind === "fort") return false;
        const dx = x - e.x, dy = y - e.y, c = Math.cos(e.face), sn = Math.sin(e.face);
        return Math.abs(dx * c - dy * sn) <= e.w / 2 + half && Math.abs(dx * sn + dy * c) <= e.d / 2 + half;
      });
    }
    /** Call a strike down this run (the player's), or the staff's where run is left out. False where it cannot be called. */
    callAir(s, run) {
      const r = run ? this.clipRun(run) : void 0;
      if (this.airWhyNot(s, r) !== null) return false;
      const on = this.airOn[s], bank = this.airBank[s];
      const legs = [];
      const flight = (r4, models) => {
        const len = Math.max(1, dist(r4[0], r4[1], r4[2], r4[3])), back = LAND.air.eta * LAND.air.speed;
        return { id: ++this.flightIds, side: s, nat: on.groups[this.airCalls[s] % on.groups.length].nat, year: this.year(), models, x: r4[0] - (r4[2] - r4[0]) / len * back, y: r4[1] - (r4[3] - r4[1]) / len * back, alt: LAND.air.alt, face: bearing(r4[0], r4[1], r4[2], r4[3]), phase: "in", run: r4 };
      };
      const leg = (f, aimed) => ({ flight: f, owed: /* @__PURE__ */ new Map(), nerve: /* @__PURE__ */ new Map(), total: 0, aimed, done: 0, struck: /* @__PURE__ */ new Set(), dropped: 0, released: 0, away: false });
      if (r) {
        let total = 0;
        for (const v of bank.values()) total += v;
        const l = leg(flight(r, LAND.air.models), true);
        l.owed = new Map(bank);
        l.nerve = new Map(this.airNerve[s]);
        l.total = total;
        legs.push(l);
      } else {
        const live1 = (id) => {
          const u = this.unit(id);
          return u && live2(u) ? u : void 0;
        };
        const owed = [...bank].filter(([id, v]) => v > 0 && live1(id)).sort((a, b) => b[1] - a[1] || a[0] - b[0]);
        if (!owed.length) return false;
        const top = owed.slice(0, LAND.air.legs), n = top.length;
        top.forEach(([id], i) => {
          const b = this.unit(id);
          legs.push(leg(flight(this.frontageRun(b), Math.floor(LAND.air.models / n) + (i < LAND.air.models % n ? 1 : 0)), false));
        });
        for (const [id, v] of owed) {
          const b = this.unit(id);
          let at = 0;
          top.forEach(([tid], i) => {
            const t = this.unit(tid), a = this.unit(top[at][0]);
            if (dist(t.x, t.y, b.x, b.y) < dist(a.x, a.y, b.x, b.y)) at = i;
          });
          legs[at].owed.set(id, v);
          legs[at].nerve.set(id, this.airNerve[s].get(id) ?? v);
          legs[at].total += v;
        }
        for (const [id, v] of bank) if (v > 0 && !live1(id)) this.offField(s, id, v);
      }
      bank.clear();
      this.airNerve[s].clear();
      const bombs = this.year() >= (LAND.air.bombsFrom[on.groups[this.airCalls[s] % on.groups.length].nat] ?? LAND.air.bombsFrom._);
      this.strikes.push({ side: s, by: r ? "you" : "staff", group: this.airCalls[s] % on.groups.length, nth: this.airCalls[s] + 1, called: this.t, legs, bombs, ended: false, lost: false });
      this.airCalls[s]++;
      this.airLeft[s]--;
      this.airNext[s] = this.airLeft[s] > 0 ? this.t + LAND.air.every : Infinity;
      const human = this.ticket[s].human;
      this.say({ what: "land:air-call", side: s, text: human && !r ? "The staff called in the fighters." : "", x: legs[0].flight.run[0], y: legs[0].flight.run[1], extra: { flights: legs.map((l) => l.flight.id), runs: legs.map((l) => l.flight.run), eta: LAND.air.eta, by: r ? "you" : "staff" } });
      return true;
    }
    /** The year the ticket's date falls in: which fighters fly, and whether they carry bombs. */
    year() {
      const m = /(\d{4})/.exec(this.ticket.date);
      return m ? Number(m[1]) : 1939;
    }
    /**
     * Each tick: the next strike comes ready once the armies have met; one left ready the staff call; and each strike in
     * the air comes in, makes its run, paying what it carries where it lands, and pulls away.
     */
    flyAir() {
      for (const s of ["att", "def"]) {
        const on = this.airOn[s];
        if (!on) continue;
        if (this.contactAt !== null && this.airNext[s] === Infinity && this.airLeft[s] > 0 && this.airCalls[s] === 0) this.airNext[s] = this.contactAt + LAND.air.first;
        if (this.airReady(s) && this.t >= this.airNext[s] + LAND.air.wait) this.callAir(s);
      }
      const hurt = /* @__PURE__ */ new Map();
      for (const k of this.strikes) if (!k.ended) this.flyStrike(k, hurt);
      for (const [id, [d, n]] of hurt) this.suffer(this.unit(id), d, d > 0 ? n / d * LAND.air.nerve : LAND.air.nerve, []);
      this.strikes = this.strikes.filter((k) => !k.ended);
    }
    /** One strike's tick, its legs flown together. What each leg lands this tick is added to 'hurt'. */
    flyStrike(k, hurt) {
      const A = LAND.air, at = k.called + A.eta, led = this.airLedger[k.side];
      let over = true;
      for (const l of k.legs) {
        const r = l.flight.run, len = Math.max(1, dist(r[0], r[1], r[2], r[3])), secs = Math.max(DT, len / A.speed), [dx, dy] = [(r[2] - r[0]) / len, (r[3] - r[1]) / len];
        const f = l.flight;
        if (this.t < at) {
          const back = (at - this.t) * A.speed, k0 = (at - this.t) / A.eta;
          f.phase = "in";
          f.x = r[0] - dx * back;
          f.y = r[1] - dy * back;
          f.alt = A.low + (A.alt - A.low) * k0;
          over = false;
          continue;
        }
        if (l.done < 1) {
          if (f.phase === "in") {
            f.phase = "run";
            this.say({ what: "land:air-in", side: k.side, text: "", x: r[0], y: r[1], extra: { flight: f.id, run: r, models: f.models, alt: A.low, nat: f.nat, year: f.year, heading: f.face }, cue: "fighter_pass" });
            this.say({ what: "land:strafe", side: k.side, text: "", x: (r[0] + r[2]) / 2, y: (r[1] + r[3]) / 2, extra: { flight: f.id, from: [r[0], r[1]], to: [r[2], r[3]], width: A.wide, secs: Math.round(secs * 10) / 10 }, cue: "machine_gun" });
          }
          const step = Math.min(1 - l.done, DT / secs), from = l.done;
          l.done = Math.min(1, l.done + step);
          f.x = r[0] + (r[2] - r[0]) * l.done;
          f.y = r[1] + (r[3] - r[1]) * l.done;
          f.alt = A.low;
          if (l.aimed) {
            const share = l.total * step, mx = r[0] + (r[2] - r[0]) * (from + step / 2), my = r[1] + (r[3] - r[1]) * (from + step / 2);
            const on = this.overOwn(k.side, mx, my) ? [] : this.underRun(k.side, mx, my);
            if (!on.length) led.wasted += share;
            else for (const e of on) this.land(k.side, e, share / on.length, hurt, l, l.total > 0 ? this.sumOf(l.nerve) / l.total : 1);
          } else {
            for (const [id, v] of l.owed) {
              const e = this.unit(id);
              if (live2(e)) this.land(k.side, e, v * step, hurt, l, v > 0 ? (l.nerve.get(id) ?? v) / v : 1);
              else this.offField(k.side, id, v * step);
            }
          }
          if (k.bombs) {
            const fbOf = (i) => 2 / 3 + (i + 0.5) / A.bombs / 3;
            for (let i = l.released; i < A.bombs; i++) {
              const fb = fbOf(i), fr = Math.max(0, fb - A.toss * A.speed / len);
              if (l.done < fr) break;
              this.say({ what: "land:bomb-drop", side: k.side, text: "", x: Math.round(f.x * 10) / 10, y: Math.round(f.y * 10) / 10, extra: { flight: f.id, from: { x: Math.round(f.x * 10) / 10, y: Math.round(f.y * 10) / 10, alt: A.low }, to: { x: Math.round((r[0] + (r[2] - r[0]) * fb) * 10) / 10, y: Math.round((r[1] + (r[3] - r[1]) * fb) * 10) / 10 }, at: Math.round((this.t + Math.max(0, fb - l.done) * secs) * 10) / 10 } });
              l.released = i + 1;
            }
            for (let i = l.dropped; i < l.released; i++) {
              const fb = fbOf(i);
              if (l.done >= fb) {
                this.bomb(k.side, r[0] + (r[2] - r[0]) * fb, r[1] + (r[3] - r[1]) * fb, f);
                l.dropped = i + 1;
              }
            }
          }
          if (l.done >= 1) {
            this.say({ what: "land:strafed", side: k.side, text: this.strafeWords(k.side, l), x: (r[0] + r[2]) / 2, y: (r[1] + r[3]) / 2, extra: { flight: f.id, on: [...l.struck].filter((id) => this.known(this.unit(id))) } });
          }
          over = false;
          continue;
        }
        const gone = this.t - (at + secs);
        if (gone < A.out) {
          f.phase = "out";
          f.x = r[2] + dx * gone * A.speed;
          f.y = r[3] + dy * gone * A.speed;
          f.alt = A.low + (A.alt - A.low) * (gone / A.out);
          over = false;
        } else l.away = true;
      }
      if (!k.lost && k.legs.every((l) => l.done >= 1)) {
        k.lost = true;
        this.planesLost(k);
      }
      if (over) k.ended = true;
    }
    /**
     * A share of a strike, in open-ground terms, landing on an enemy block: what it takes off it for its ground, and the
     * nerve it costs as the blows it was held from would have (nerve: their weight on nerve, a unit of what was held).
     */
    land(s, e, raw, hurt, l, nerve) {
      const led = this.airLedger[s];
      const d = raw * this.groundTake(e) * (e.kind === "arm" && l.aimed ? LAND.air.armour : 1);
      led.landed += raw;
      led.dealt += d;
      if (this.ticket[s].human) e.found = true;
      const h = hurt.get(e.id) ?? [0, 0];
      h[0] += d;
      h[1] += d * nerve;
      hurt.set(e.id, h);
      l.struck.add(e.id);
    }
    sumOf(m) {
      let n = 0;
      for (const v of m.values()) n += v;
      return n;
    }
    /** A bomb, pictures only: a 'shell' of class 'bomb' where it lands, and its crater (shells.ts). */
    bomb(s, x, y, f) {
      x = Math.round(clamp(x, 2, this.field.w - 2) * 10) / 10;
      y = Math.round(clamp(y, 2, this.field.h - 2) * 10) / 10;
      const hit = this.underRun(s, x, y)[0], on = hit && this.known(hit) ? hit : void 0, ground = groundAt(this.field, x, y), armour = on?.kind === "arm", at = Math.round(this.t * 10) / 10;
      const ev = { t: at, what: "shell", text: "", x, y, cal: "bomb", armour: !!armour, ground, reach: REACH.bomb, extra: { flight: f.id } };
      if (on) ev.on = on.id;
      this.events.push(ev);
      this.drill?.shelled(x, y);
      if (!armour && !inRiver(ground)) this.craters.push({ id: -(this.craters.length + 1), kind: "crater", x, y, r: CRATER.bomb / 2, until: this.ticket.limit, at, cal: "bomb" });
    }
    /**
     * The words a strafing run leaves in the dispatches, for the player's side only and never naming a block he does not
     * know (b464(2)): his own run, the blocks it struck; theirs, his blocks it struck. The UI director's words (21:18Z):
     * "strafed and bombed" where the run let bombs go.
     */
    strafeWords(s, l) {
      const me = this.ticket.att.human ? "att" : this.ticket.def.human ? "def" : null;
      if (!me) return "";
      const hit = [...l.struck].map((id) => this.unit(id)), did = l.dropped > 0 ? "strafed and bombed" : "strafed";
      if (s === me) {
        const seen = hit.filter((u) => this.known(u));
        return !hit.length ? "Your fighters found nothing under their run." : seen.length ? `Your fighters ${did} ${seen.map((u) => this.named(u)).join(" and ")}.` : `Your fighters ${did} the enemy line.`;
      }
      const mine = hit.filter((u) => u.side === me);
      return mine.length ? `${this.ticket[s].adj} fighters ${did} your ${mine.map((u) => u.name).join(" and your ")}.` : "";
    }
    /**
     * The planes a strike cost its group, as the campaign prices a strike (AirOnCall.groups[].lose: no dice, fought or
     * settled by the staff, the same): each whole plane that price comes to falls, a picture of the loss ('air:lost').
     */
    planesLost(k) {
      let n = 0;
      for (const g of this.airOn[k.side].groups) {
        const falls = Math.floor(g.lose * k.nth + 1e-9) - Math.floor(g.lose * (k.nth - 1) + 1e-9);
        for (let i = 0; i < falls; i++, n++) {
          const f = k.legs[n % k.legs.length].flight;
          const m = mix2(f.id * 7919 + n * 131 + 17), kind = m < 0.3 ? "explode" : m < 0.7 ? "burn" : "spin";
          this.say({ what: "air:lost", side: k.side, text: this.lostWords(k.side), x: Math.round(f.x * 10) / 10, y: Math.round(f.y * 10) / 10, extra: { flight: f.id, kind, chutes: kind !== "explode" && mix2(f.id * 613 + n * 7) < 0.5 ? 1 : 0, over: k.side === "att" ? "enemy" : "own", air: "ftr", alt: Math.round(f.alt) }, cue: "plane_down" });
        }
      }
    }
    /** A fighter down, in the player's dispatches (the UI director, 21:18Z): his own to ground fire, or theirs to his men. */
    lostWords(s) {
      const me = this.ticket.att.human ? "att" : this.ticket.def.human ? "def" : null;
      if (!me) return "";
      if (s === me) return "Ground fire brought down one of your fighters.";
      const adj = this.ticket[s].adj, word = adj === "Enemy" ? "enemy" : adj;
      return `Your men brought down ${/^[AEIOUaeiou]/.test(word) ? "an" : "a"} ${word} fighter.`;
    }
    /** What each side's fighters flew and lost, by group, for the campaign's bill (FieldResult.air). */
    airBill() {
      const out = {};
      for (const s of ["att", "def"]) {
        const on = this.airOn[s];
        if (!on) continue;
        const n = this.airCalls[s] + this.airSettled[s];
        out[s] = on.groups.map((g) => ({ group: g.id, sorties: n, lost: Math.round(g.lose * n * 1e3) / 1e3 }));
      }
      return out;
    }
    /**
     * Handed to the staff, or the day done: what the bank still owes and what strikes in the air still carry is paid
     * where it is owed, at once (the staff settle the rest from where the field stands, the air's share in it).
     */
    payAir() {
      if (!this.airOn.att && !this.airOn.def) return;
      const hurt = /* @__PURE__ */ new Map();
      for (const { s, id, raw, nerve } of this.airDues()) {
        const e = this.unit(id), led = this.airLedger[s];
        if (!e) {
          led.wasted += raw;
          continue;
        }
        if (!live2(e)) {
          this.offField(s, id, raw);
          continue;
        }
        const d = raw * this.groundTake(e), h = hurt.get(id) ?? [0, 0];
        led.landed += raw;
        led.dealt += d;
        h[0] += d;
        h[1] += d * nerve;
        hurt.set(id, h);
      }
      for (const s of ["att", "def"]) {
        this.airBank[s].clear();
        this.airNerve[s].clear();
        this.airLeft[s] = 0;
      }
      for (const k of this.strikes) {
        for (const l of k.legs) l.done = Math.max(l.done, 1);
        k.ended = true;
      }
      for (const [id, [d, n]] of hurt) this.suffer(this.unit(id), d, d > 0 ? n / d : 1, []);
    }
    /**
     * What the fighters on call still owe, each side's, block by block: the bank, and what strikes in the air carry for
     * the rest of their runs (an aimed run's, shared as the bank it took was). Read only: payAir pays it, airDue projects it.
     */
    airDues() {
      const out = [];
      for (const s of ["att", "def"]) {
        if (!this.airOn[s]) continue;
        for (const [id, v] of this.airBank[s]) out.push({ s, id, raw: v, nerve: v > 0 ? (this.airNerve[s].get(id) ?? v) / v : 1 });
        for (const k of this.strikes) if (k.side === s && !k.ended) for (const l of k.legs) {
          const left = 1 - l.done;
          if (left <= 0) continue;
          const sum = this.sumOf(l.owed);
          for (const [id, v] of l.owed) out.push({ s, id, raw: (l.aimed ? l.total * (sum > 0 ? v / sum : 0) : v) * left, nerve: v > 0 ? (l.nerve.get(id) ?? v) / v : 1 });
        }
      }
      return out;
    }
    /** The damage each block on the field would take were what is owed paid now (for the staff's chance), or nothing. */
    airDue() {
      if (!this.airOn.att && !this.airOn.def) return void 0;
      const due = /* @__PURE__ */ new Map();
      for (const { id, raw } of this.airDues()) {
        const e = this.unit(id);
        if (e && live2(e)) due.set(id, (due.get(id) ?? 0) + raw * this.groundTake(e));
      }
      return due;
    }
    /**
     * What is owed a block that has left the field (withdrawn, or fled off it): it still pays, in open-ground terms, as
     * the day's count still counts what it took away (lostShares). Never wasted: only an aimed run's empty ground is.
     */
    offField(s, id, raw) {
      const e = this.unit(id), led = this.airLedger[s];
      if (!e) {
        led.wasted += raw;
        return;
      }
      led.landed += raw;
      led.dealt += raw;
      e.str = Math.max(0, e.str - raw / Math.max(1e-9, e.power0) * e.str0);
      e.figures = e.str <= 0 ? 0 : Math.max(1, Math.ceil(e.tray * e.str));
    }
    /** The flights over the field now, for the screens. */
    flightsNow() {
      const out = [];
      const r13 = (v) => Math.round(v * 10) / 10;
      for (const k of this.strikes) for (const l of k.legs) if (!k.ended && !l.away) out.push({ ...l.flight, x: r13(l.flight.x), y: r13(l.flight.y), alt: r13(l.flight.alt), face: Math.round(l.flight.face * 1e3) / 1e3, run: [...l.flight.run] });
      return out;
    }
    /** Each computer general's style and its attack's plan, for the 3D game; nothing where neither side has a style. */
    planView() {
      const out = {};
      for (const s of ["att", "def"]) {
        const st = this.style[s];
        if (!st || !this.ai[s]) continue;
        const P = this.plan[s];
        const walking = P ? [] : this.units.filter((u) => u.side === s && u.reserve === "held" && this.shifted.has(u.id) && live2(u)).map((u) => u.id);
        out[s] = { style: st.id, ...P ? { phase: P.phase, main: [...P.main], startY: P.startY } : walking.length ? { phase: "shift", reserve: walking } : {} };
      }
      return Object.keys(out).length ? { plan: out } : {};
    }
    view(as) {
      const ev = this.events;
      this.events = [];
      return { t: this.t, limit: this.ticket.limit, units: this.units, balance: this.standing(), events: ev, zones: this.wrecks.concat(this.craters).map((z) => ({ ...z })), ai: { ...this.ai }, over: this.over, breaking: { att: this.breaking("att"), def: this.breaking("def") }, breakingBy: { att: this.breakCause("att"), def: this.breakCause("def") }, contact: this.contactAt, ...this.setup ? { setup: this.setup } : {}, ...this.airView(), ...this.planView(), ...this.comingView(), ...this.riderView(), ...this.patrolView(as), ...this.cutView(as), ...this.screenView(as), ...this.phonesView(as), ...this.bearerView(as) };
    }
    /** Air support's part of the view, only where a side has fighters on call. */
    airView() {
      if (!this.airOn.att && !this.airOn.def) return {};
      const air = {};
      for (const s of ["att", "def"]) {
        if (!this.airOn[s]) continue;
        const k = this.strikes.find((x) => x.side === s && !x.ended);
        air[s] = { ready: this.airReady(s) ? 1 : 0, left: this.airLeft[s], nextIn: this.airLeft[s] > 0 && this.airNext[s] !== Infinity && this.t < this.airNext[s] ? Math.round((this.airNext[s] - this.t) * 10) / 10 : null, eta: k && this.t < k.called + LAND.air.eta ? Math.round((k.called + LAND.air.eta - this.t) * 10) / 10 : null };
      }
      return { flights: this.flightsNow(), air };
    }
  };

  // ../../src/battle/land/formation.ts
  var FORMATIONS = ["line", "two", "wedge", "column"];
  var ROW = 110;
  var STEP = 60;
  var widthOf = (us) => us.reduce((a, u) => a + u.w, 0) + GAP * Math.max(0, us.length - 1);
  var byX = (a, b) => a.x - b.x || a.id - b.id;
  function wrap2(us, wide) {
    const rows = [];
    for (const u of us) {
      const r = rows[rows.length - 1];
      if (r && widthOf([...r, u]) <= wide) r.push(u);
      else rows.push([u]);
    }
    return rows;
  }
  function drawnUp(kind, blocks, dir, room) {
    if (!blocks.length) return [];
    const wide = room[2] - room[0] - 0.1, deep = room[3] - room[1] - 0.1;
    const ahead2 = (a, b) => dir * (b.y - a.y) || byX(a, b);
    let front = blocks.filter((u) => !u.rear).sort(byX), rear = blocks.filter((u) => u.rear).sort(byX);
    if (!front.length) {
      front = rear;
      rear = [];
    }
    const maxD = Math.max(...blocks.map((u) => u.d)), close = maxD + LAND.placeClear + 1;
    const rows = [];
    let pitch = ROW;
    if (kind === "column") {
      const file = [...[...front].sort(ahead2), ...[...rear].sort(ahead2)];
      pitch = maxD + GAP;
      const perFile = Math.max(1, Math.floor((deep - maxD) / pitch) + 1), files = Math.ceil(file.length / perFile);
      for (let k = 0; k < file.length; k += files) rows.push({ us: file.slice(k, k + files).sort(byX), back: 0 });
    } else {
      if (kind === "wedge" && widthOf(front) <= wide && front.length >= 3) {
        const mid = Math.floor((front.length - 1) / 2);
        rows.push({ us: front, back: 0, steps: front.map((_, i) => Math.abs(i - mid)) });
      } else if (kind === "two" && front.length >= 2) {
        const order = [...front].sort(ahead2), n = Math.ceil(front.length / 2);
        for (const r of wrap2(order.slice(0, n).sort(byX), wide)) rows.push({ us: r, back: 0 });
        for (const r of wrap2(order.slice(n).sort(byX), wide)) rows.push({ us: r, back: 0 });
      } else {
        for (const r of wrap2(front, wide)) rows.push({ us: r, back: 0 });
      }
      for (const r of wrap2(rear, wide)) rows.push({ us: r, back: 0 });
    }
    const wedge = rows.find((r) => r.steps), most = wedge ? Math.max(...wedge.steps) : 0;
    let step = most > 0 ? STEP : 0;
    const need2 = () => maxD + most * step + pitch * (rows.length - 1);
    if (need2() > deep && most > 0) step = Math.max(0, (deep - maxD - pitch * (rows.length - 1)) / most);
    if (need2() > deep && rows.length > 1) pitch = (deep - maxD - most * step) / (rows.length - 1);
    if (need2() > deep + 1e-6 || rows.length > 1 && pitch < close) return null;
    const slots = [];
    let back = 0;
    for (const r of rows) {
      let x = -widthOf(r.us) / 2;
      r.us.forEach((u, i) => {
        slots.push({ id: u.id, x: x + u.w / 2, y: -dir * (back + (r.steps ? r.steps[i] * step : 0)) });
        x += u.w + GAP;
      });
      back += pitch + (r.steps ? most * step : 0);
    }
    const size = new Map(blocks.map((u) => [u.id, u]));
    const box = () => [
      Math.min(...slots.map((s) => s.x - size.get(s.id).w / 2)),
      Math.min(...slots.map((s) => s.y - size.get(s.id).d / 2)),
      Math.max(...slots.map((s) => s.x + size.get(s.id).w / 2)),
      Math.max(...slots.map((s) => s.y + size.get(s.id).d / 2))
    ];
    const cx = blocks.reduce((a, u) => a + u.x, 0) / blocks.length, cy = blocks.reduce((a, u) => a + u.y, 0) / blocks.length;
    const b0 = box(), mx = (b0[0] + b0[2]) / 2, my = (b0[1] + b0[3]) / 2;
    for (const s of slots) {
      s.x += cx - mx;
      s.y += cy - my;
    }
    const b1 = box(), E = 0.02;
    const sx = b1[0] < room[0] + E ? room[0] + E - b1[0] : b1[2] > room[2] - E ? room[2] - E - b1[2] : 0;
    const sy = b1[1] < room[1] + E ? room[1] + E - b1[1] : b1[3] > room[3] - E ? room[3] - E - b1[3] : 0;
    for (const s of slots) {
      s.x = Math.round((s.x + sx) * 100) / 100;
      s.y = Math.round((s.y + sy) * 100) / 100;
    }
    return slots;
  }

  // ../../src/battle/land/pieces.ts
  var NORMANDY = {
    key: "normandy-1944",
    title: "The hedgerows before Le Mesnil",
    date: "July 1944",
    turn: 59,
    place: { region: -1, name: "Normandy", terrain: "plains", river: null, bridgeDown: false, town: "Le Mesnil", townSize: "village", fort: 0, mines: 0 },
    att: { nat: "USA", adj: "American", color: "#6b7a4a", army: "USA", standIn: "POL", tanks: ["Sherman"], gun: "Howitzer105", general: "General Hale", style: "methodical" },
    def: { nat: "GER", adj: "German", color: "#5f6a6e", army: "GER", tanks: ["Tiger", "PanzerIV"], gun: "leFH18", general: "General Vane", style: "bold" },
    attForces: [
      ["VII Corps", "inf", 3, 0.5, 4],
      ["3rd Armored Group", "arm", 2, 0.32, 3],
      ["Corps Artillery", "art", 1, 0.18, 1]
    ],
    defForces: [
      ["LXXXIV Corps", "inf", 3, 0.6, 352, true],
      ["Heavy Panzer Group", "arm", 2, 0.25, 101],
      ["Artillery Group", "art", 1, 0.15, 1]
    ],
    odds: 1.2,
    seeds: [11, 3, 7, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97, 101, 103, 107],
    str: [0.92, 0.85],
    xp: [0.45, 0.6],
    sky: { cover: 0.45, base: 900, kind: "cumulus" },
    latLon: [49.12, -1.09],
    hour: 9.5
  };
  var PROKHOROVKA = {
    key: "prokhorovka-1943",
    title: "The fields before Prokhorovka",
    date: "July 1943",
    turn: 47,
    place: { region: -1, name: "Kursk salient", terrain: "plains", river: null, bridgeDown: false, town: "Oktyabrsky", townSize: "village", fort: 0, mines: 0 },
    att: { nat: "SOV", adj: "Soviet", color: "#7b7748", army: "SOV", tanks: ["T34", "T34", "LightTank", "T34", "T34", "LightTank"], gun: "ZiS3", general: "General Orlov", style: "mass" },
    def: { nat: "GER", adj: "German", color: "#5f6a6e", army: "GER", tanks: ["PanzerIV", "Tiger", "StuG3", "PanzerIV"], gun: "leFH18", general: "General Vane", style: "methodical" },
    attForces: [
      ["Guards Tank Army", "arm", 6, 0.5, 18],
      ["Guards Rifle Corps", "inf", 4, 0.36, 9],
      ["Army Artillery", "art", 2, 0.14, 1]
    ],
    defForces: [
      ["Panzer Corps", "arm", 4, 0.42, 1],
      ["Panzergrenadier Group", "inf", 3, 0.43, 1, true],
      ["Corps Artillery", "art", 2, 0.15, 1]
    ],
    odds: 1.3,
    seeds: [5, 13, 17, 11, 3, 7, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59],
    str: [0.95, 0.8],
    xp: [0.35, 0.7],
    // (a close, hazy July morning on the steppe after the night's rain, cumulus building; a light westerly; the grade a
    // little warmer than Normandy's for the ripe rye and the dust: the art director's to check)
    sky: { cover: 0.5, base: 1200, kind: "cumulus", wind: [2.2, 0.7], grade: [0.45, 0.5, 1.02, 0.99] },
    latLon: [51.03, 36.72],
    hour: 8.5,
    open: "steppe",
    caps: { att: 10, def: 5, men: 180, len: [180, 420] }
  };
  var PIECES = [NORMANDY, PROKHOROVKA];
  function withTeams(t, share = 0.04) {
    const add = (s, base) => {
      const nat = s.forces[0]?.nat ?? s.nat;
      const team = (kind, i) => ({
        corps: base + i,
        name: kind === "mg" ? "Machine-Gun Battalion" : "Mortar Battalion",
        nat,
        kind,
        divs: 1,
        men: 900,
        str: 0.9,
        org: 0.85,
        xp: 0.3,
        mor: 0.65,
        share,
        first: 1
      });
      return { ...s, forces: [...s.forces.map((f) => ({ ...f, share: f.share * (1 - 2 * share) })), team("mg", 1), team("mor", 2)] };
    };
    return { ...t, att: add(t.att, 9e3), def: add(t.def, 9100) };
  }
  function ticketOf(p, seed, human = null) {
    const side = (a, forces, human2, base, str, xp) => ({
      nat: a.nat,
      adj: a.adj,
      color: a.color,
      human: human2,
      general: { name: a.general, level: 2 },
      style: a.style,
      forces: forces.map(([name, kind, divs, share, first, dug], i) => ({
        corps: base + i,
        name,
        nat: a.nat,
        kind,
        divs,
        men: divs * 15e3 * str,
        str,
        org: 0.85,
        xp,
        mor: 0.7,
        share,
        dug: !!dug,
        first,
        battles: 4
      }))
    });
    const A = 100;
    const t = {
      domain: "land",
      kind: "field",
      seed,
      turn: p.turn,
      date: p.date,
      title: p.title,
      place: p.place,
      att: side(p.att, p.attForces, human === "att", 1, p.str[0], p.xp[0]),
      def: side(p.def, p.defForces, human === "def", 11, p.str[1], p.xp[1]),
      reckoning: { A, D: A / p.odds, odds: p.odds, dice: [0.8, 1.2], even: 1.2 },
      lines: [],
      limit: LAND.limit
    };
    return withTeams(t);
  }
  var r1 = (x) => Math.round(x * 10) / 10;
  var r3 = (x) => Math.round(x * 1e3) / 1e3;
  function rand(seed) {
    let a = seed >>> 0;
    return () => {
      a = a + 1831565813 >>> 0;
      let t = a;
      t = Math.imul(t ^ t >>> 15, t | 1);
      t ^= t + Math.imul(t ^ t >>> 7, t | 61);
      return ((t ^ t >>> 14) >>> 0) / 4294967296;
    };
  }
  var CROPS = ["none", "wheat", "rye", "pasture", "fallow"];
  function openSteppe(f, seed) {
    const rnd = rand(seed ^ 5761438);
    const { cols, rows, cell, w, h } = f;
    const at = (c, r) => r * cols + c;
    const mid = (i) => [(i % cols + 0.5) * cell, (Math.floor(i / cols) + 0.5) * cell];
    const code = (g) => GROUNDS.indexOf(g);
    const OPEN = code("open"), FIELD_ = code("field"), WOODS = code("woods"), ROUGH = code("rough"), WORKS = code("works");
    const made = new Set(["open", "field", "woods", "rough", "marsh", "rock"].map(code));
    const town = f.town;
    const nearTown = (x, y) => !!town && Math.hypot(x - town.x, y - town.y) < town.r * 1.25;
    const crop = new Array(cols * rows).fill(0);
    const base = f.height.slice();
    for (let i = 0; i < base.length; i++) f.height[i] = base[i] * 0.55;
    const turn3 = (rnd() - 0.5) * 0.24, cs = Math.cos(turn3), sn = Math.sin(turn3);
    const edges = (len, lo, hi) => {
      const e = [-6e3];
      while (e[e.length - 1] < len + 6e3) e.push(e[e.length - 1] + lo + rnd() * (hi - lo));
      return e;
    };
    const xs = edges(w, 450, 800), ys = edges(h, 300, 550);
    const rshift = rand(seed ^ 373239);
    const shift = xs.slice(1).map(() => (rshift() - 0.5) * 420);
    const slot = (v, e) => {
      let k = 0;
      while (k < e.length - 2 && e[k + 1] <= v) k++;
      return k;
    };
    const patch = /* @__PURE__ */ new Map();
    const draw = () => {
      const v = rnd();
      return v < 0.38 ? 1 : v < 0.72 ? 2 : v < 0.88 ? 3 : 4;
    };
    const cropOf = (a, b) => {
      const key = `${a},${b}`;
      let k = patch.get(key);
      if (k === void 0) {
        const west = patch.get(`${a - 1},${b}`), south = patch.get(`${a},${b - 1}`);
        k = draw();
        for (let tries = 0; tries < 4 && (k === west || k === south); tries++) k = draw();
        patch.set(key, k);
      }
      return k;
    };
    let patchOut = "";
    for (let b = 0; b + 1 < ys.length; b++) for (let a = 0; a + 1 < xs.length; a++) patchOut += String(cropOf(a, b));
    for (let i = 0; i < crop.length; i++) {
      const [x, y] = mid(i);
      const u = (x - w / 2) * cs + (y - h / 2) * sn + w / 2, v = -(x - w / 2) * sn + (y - h / 2) * cs + h / 2;
      const a = slot(u, xs), k = cropOf(a, slot(v - shift[a], ys));
      if (!made.has(f.ground[i])) continue;
      if (nearTown(x, y)) continue;
      crop[i] = k;
      f.ground[i] = k <= 2 ? FIELD_ : OPEN;
    }
    const hx = town ? town.x - 200 - rnd() * 120 : w * (0.36 + rnd() * 0.14), hy = town ? town.y - 150 - rnd() * 60 : h * (0.7 + rnd() * 0.06);
    const hill = { x: Math.max(w * 0.2, Math.min(w * 0.8, hx)), y: Math.max(h * 0.6, Math.min(h * 0.8, hy)), r: 430, rise: 11 };
    for (let i = 0; i < f.height.length; i++) {
      const [x, y] = mid(i);
      f.height[i] += hill.rise * Math.exp(-((x - hill.x) ** 2 + (y - hill.y) ** 2) / (2 * hill.r * hill.r));
    }
    const line = [];
    let bx = 0, by2 = h * (0.34 + rnd() * 0.08), head = 0.25 + rnd() * 0.25;
    const reach = w * (0.42 + rnd() * 0.08);
    while (bx < reach) {
      line.push([bx, by2]);
      head += (rnd() - 0.5) * 0.5;
      head = Math.max(-0.15, Math.min(0.6, head));
      bx += 60 * Math.cos(head);
      by2 += 60 * Math.sin(head);
      by2 = Math.min(h * 0.55, by2);
    }
    line.push([bx, by2]);
    const deep = 7.5;
    const along = (x, y) => {
      let best = Infinity, s = 0, run = 0, total = 0;
      for (let k = 1; k < line.length; k++) total += Math.hypot(line[k][0] - line[k - 1][0], line[k][1] - line[k - 1][1]);
      for (let k = 1; k < line.length; k++) {
        const [ax, ay] = line[k - 1], [cx, cy] = line[k], dx = cx - ax, dy = cy - ay, L2 = Math.hypot(dx, dy);
        const t = Math.max(0, Math.min(1, ((x - ax) * dx + (y - ay) * dy) / (L2 * L2)));
        const d = Math.hypot(x - (ax + t * dx), y - (ay + t * dy));
        if (d < best) {
          best = d;
          s = (run + t * L2) / total;
        }
        run += L2;
      }
      return { d: best, s };
    };
    for (let i = 0; i < f.height.length; i++) {
      const [x, y] = mid(i);
      const { d, s } = along(x, y);
      const wide = 95 * (1 - 0.55 * s), dig = deep * (1 - s * s);
      if (d >= wide) continue;
      f.height[i] -= dig * (1 - (d / wide) ** 2) ** 1.5;
      if (d < 20 && s < 0.92 && made.has(f.ground[i]) && !nearTown(x, y)) {
        const scrub = Math.sin(x * 0.013 + y * 0.021 + seed) * 0.5 + 0.5 < 0.38 && s < 0.75;
        f.ground[i] = scrub ? WOODS : ROUGH;
        crop[i] = 0;
      }
    }
    const rx0 = w * (0.925 + rnd() * 0.02), rx1 = w * (0.945 + rnd() * 0.02), bank = 3;
    const railX = (y) => {
      const t = y / h;
      return (1 - t) * (1 - t) * rx0 + 2 * t * (1 - t) * ((rx0 + rx1) / 2 + 12) + t * t * rx1;
    };
    const rail = [];
    const onLine = (y) => y < 0 ? railX(0) + y * (railX(50) - railX(0)) / 50 : y > h ? railX(h) + (y - h) * (railX(h) - railX(h - 50)) / 50 : railX(y);
    for (let y = -1e3; y <= h + 1e3; y += 100) rail.push([onLine(y), y]);
    for (let r = 0; r < rows; r++) {
      const y = (r + 0.5) * cell, rx = railX(y);
      for (let c = 0; c < cols; c++) {
        const x = (c + 0.5) * cell, d = Math.abs(x - rx), i = at(c, r);
        if (d < 12.5 && made.has(f.ground[i])) {
          f.ground[i] = ROUGH;
          crop[i] = 0;
        }
      }
    }
    for (let k = 0; k < 2; k++) {
      const cx = rnd() < 0.5 ? w * (0.04 + rnd() * 0.1) : w * (0.74 + rnd() * 0.12), cy = h * (0.15 + rnd() * 0.7), rr = 35 + rnd() * 30;
      for (let i = 0; i < f.ground.length; i++) {
        const [x, y] = mid(i);
        if (Math.hypot(x - cx, y - cy) < rr && made.has(f.ground[i]) && crop[i] !== 0) {
          f.ground[i] = WOODS;
          crop[i] = 0;
        }
      }
    }
    if (f.dugInto) for (const k of Object.keys(f.dugInto)) {
      const i = Number(k);
      if (f.ground[i] === WORKS && made.has(f.dugInto[i])) f.dugInto[i] = crop[i] ? crop[i] <= 2 ? FIELD_ : OPEN : f.dugInto[i];
    }
    return { crops: CROPS, crop: crop.join(""), fields: { turn: r3(turn3), cx: w / 2, cy: h / 2, xs: xs.map(r1), ys: ys.map(r1), shift: shift.map(r1), patch: patchOut }, balka: { line: line.map(([x, y]) => [r1(x), r1(y)]), deep }, rail: { line: rail.map(([x, y]) => [r1(x), r1(y)]), bank }, hill: { ...hill, x: r1(hill.x), y: r1(hill.y) } };
  }

  // ../../src/battle/land/live.ts
  var KEEP_M = 0.1;
  var KEEP_F = 0.05;
  var r12 = (x) => Math.round(x * 10) / 10;
  var r2 = (x) => Math.round(x * 100) / 100;
  var r32 = (x) => Math.round(x * 1e3) / 1e3;
  var turn2 = (a, b) => {
    let d = b - a;
    while (d > Math.PI) d -= 2 * Math.PI;
    while (d < -Math.PI) d += 2 * Math.PI;
    return d;
  };
  var same2 = (a, b) => a[1] === b[1] && a[2] === b[2] && a[3] === b[3];
  var keyOf = (b, m, fo) => `${b}:${fo ? "fo" : ""}${m.k}${m.j !== void 0 ? `.${m.j}` : ""}`;
  var LiveKeeper = class {
    constructor() {
      __publicField(this, "men", /* @__PURE__ */ new Map());
      __publicField(this, "order", []);
    }
    see(t, b, m, fo) {
      const key = keyOf(b, m, fo);
      const p = [r2(t), r12(m.x), r12(m.y), r32(m.f)];
      const ch = [r2(t), m.p, m.c, m.a ? r12(m.a[0]) : null, m.a ? r12(m.a[1]) : null, m.h ? 1 : 0, m.b ? 1 : 0];
      if (m.sv) ch.push(m.sv[0], m.sv[1]);
      let k = this.men.get(key);
      if (!k) {
        k = {
          n: this.order.length,
          who: { b, k: m.k, ...m.j !== void 0 ? { j: m.j } : {}, r: m.r, ...fo ? { fo: 1 } : {}, ...m.ml ? { ml: m.ml } : {}, ...m.sw ? { sw: m.sw } : {} },
          last: p,
          pend: [],
          ch,
          shot: void 0,
          newAt: [p],
          newCh: [ch],
          newS: [],
          sentWho: false,
          sentD: false,
          sentMk: false,
          sentOut: void 0,
          sentTail: null
        };
        this.men.set(key, k);
        this.order.push(k);
      } else {
        if (k.ch.length !== ch.length || k.ch.slice(1).some((v, i) => v !== ch[i + 1])) {
          k.ch = ch;
          k.newCh.push(ch);
        }
        this.keep(k, p);
      }
      if (m.s !== void 0 && k.shot !== m.s) {
        k.shot = m.s;
        k.newS.push(m.s);
      }
      if (m.d !== void 0 && k.d !== m.d) {
        k.d = m.d;
        k.sentD = false;
      }
      if (m.mk) {
        const mk = JSON.stringify(m.mk);
        if (mk !== k.mkKey) {
          k.mk = m.mk;
          k.mkKey = mk;
          k.sentMk = false;
        }
      }
      k.out = void 0;
    }
    /** The recorder's line test: keep the previous point if the line from the last kept point to this one misses any point between. */
    keep(k, p) {
      const a = k.last;
      const off = (q) => {
        const span2 = p[0] - a[0];
        const u = span2 > 0 ? (q[0] - a[0]) / span2 : 0;
        const x = a[1] + (p[1] - a[1]) * u, y = a[2] + (p[2] - a[2]) * u, f = a[3] + turn2(a[3], p[3]) * u;
        return Math.hypot(q[1] - x, q[2] - y) > KEEP_M || Math.abs(turn2(f, q[3])) > KEEP_F;
      };
      if (k.pend.some(off)) {
        const q = k.pend[k.pend.length - 1];
        k.last = q;
        k.newAt.push(q);
        k.pend.length = 0;
      }
      const n = k.pend.length;
      if (n >= 2 && same2(k.pend[n - 1], p) && same2(k.pend[n - 2], p)) k.pend[n - 1] = p;
      else k.pend.push(p);
    }
    /** Men not seen this step: they left with their block (their last point kept, as the recorder does). */
    gone(t, seen) {
      for (const [key, k] of this.men) if (!seen.has(key) && k.out === void 0) {
        if (k.pend.length) {
          const q = k.pend[k.pend.length - 1];
          k.last = q;
          k.newAt.push(q);
          k.pend.length = 0;
        }
        k.out = r2(t);
      }
    }
    /** Ends every track: its last point kept (the recorder's close, at the battle's end). */
    close() {
      for (const k of this.order) if (k.pend.length) {
        const q = k.pend[k.pend.length - 1];
        k.last = q;
        k.newAt.push(q);
        k.pend.length = 0;
      }
    }
    /** What changed since the last call, and marks it sent: the tracks' news, and where the men stand now (tails). */
    drain() {
      const out = [], tails = [];
      for (const k of this.order) {
        const shown = k.sentTail && k.sentTail[0] > k.last[0] ? k.sentTail : k.last;
        const tail = k.pend.length ? k.pend[k.pend.length - 1] : null;
        if (tail && tail[0] > k.last[0] && !same2(tail, shown)) {
          tails.push(k.n, tail[1], tail[2], tail[3]);
          k.sentTail = tail;
        }
        const changed = !k.sentWho || k.newAt.length || k.newCh.length || k.newS.length || !k.sentD || !k.sentMk || k.out !== k.sentOut;
        if (!changed) continue;
        const d = { n: k.n };
        if (!k.sentWho) {
          Object.assign(d, k.who);
          k.sentWho = true;
        }
        if (k.newAt.length) {
          d.at = k.newAt;
          k.newAt = [];
        }
        if (k.newCh.length) {
          d.ch = k.newCh;
          k.newCh = [];
        }
        if (k.newS.length) {
          d.s = k.newS;
          k.newS = [];
        }
        if (k.d !== void 0 && !k.sentD) {
          d.d = k.d;
        }
        k.sentD = true;
        if (k.mk && !k.sentMk) {
          d.mk = k.mk;
        }
        k.sentMk = true;
        if (k.out !== k.sentOut) {
          d.out = k.out ?? -1;
          k.sentOut = k.out;
        }
        out.push(d);
      }
      return { men: out, tails };
    }
    /** Every track as the recorder writes it, from the deltas drained so far (for the tests). */
    static assemble(deltas) {
      const tracks = [];
      for (const dl of deltas) {
        if (dl.reset) tracks.length = 0;
        for (const d of dl.men) {
          let tr = tracks[d.n];
          if (!tr) {
            tr = tracks[d.n] = { b: d.b, k: d.k, ...d.j !== void 0 ? { j: d.j } : {}, r: d.r, at: [], ch: [], s: [], ...d.fo ? { fo: 1 } : {}, ...d.ml ? { ml: d.ml } : {}, ...d.sw ? { sw: d.sw } : {} };
          }
          if (d.at) tr.at.push(...d.at);
          if (d.ch) tr.ch.push(...d.ch);
          if (d.s) tr.s.push(...d.s);
          if (d.d !== void 0) tr.d = d.d;
          if (d.mk) tr.mk = d.mk;
          if (d.out === -1) delete tr.out;
          else if (d.out !== void 0) tr.out = d.out;
        }
      }
      return tracks;
    }
  };
  function landUnitOut(u, t, known = true, mine = false, sim) {
    const motion = MOTION[u.kind];
    const loops = motion && motion !== "fixed" ? LOOPS[motion] : void 0;
    const town = u.act === "fight" && (u.cover === "buildings" || u.cover === "suburb" || u.cover === "industry") && !!loops && motion !== "guns";
    const drops = town ? TOWN.drops : motion && motion !== "fixed" ? DROPS[motion]?.[u.act] : void 0;
    if (!known) {
      const off = u.state === "gone" || u.state === "withdrawn";
      const [w, d] = LAND.size.inf;
      return { id: u.id, side: u.side, known: false, x: r12(u.x), y: r12(u.y), face: r32(u.face), w, d, act: off ? "down" : u.speed > 0.05 ? "march" : "idle", speed: r12(u.speed), cover: u.cover ?? "", state: off ? u.state : "unknown", ...u.hid ? { hid: 1 } : {} };
    }
    return {
      id: u.id,
      side: u.side,
      kind: u.kind,
      name: u.name,
      x: r12(u.x),
      y: r12(u.y),
      face: r32(u.face),
      str: r32(u.str),
      mor: r32(u.mor),
      state: u.state,
      figures0: u.figures0,
      figures: u.figures,
      // (the real men it stands for now, for the general's plate: the historian, 22 November 05:50Z)
      men: Math.round(u.men0 * Math.min(1, u.str / Math.max(0.01, u.str0))),
      w: u.w,
      d: u.d,
      act: u.act,
      speed: r12(u.speed),
      engaged: u.engaged ?? -1,
      cover: u.cover ?? "",
      loop: town ? TOWN.loop : loops?.[u.act] ?? "",
      ...drops ? { drops: { takes: drops.takes, every: drops.every } } : {},
      ...u.short ? { short: u.short, parent: u.parent } : {},
      ...u.crew ? { crew: u.crew } : {},
      ...u.cal ? { cal: u.cal } : {},
      ...u.aloneAt !== void 0 && t - u.aloneAt <= 1 ? { alone: true } : {},
      ...u.aim !== void 0 ? { aim: u.aim } : {},
      ...u.team ? { team: u.team } : {},
      // Direct command (release 2), as the bridge writes it: a block the player has taken in hand, and the seconds till his
      // order on the air reaches it.
      ...u.taken ? { taken: true } : {},
      // Hidden in the woods from the other side (release 2, LAND.hide): his own show the forest mark, the enemy's are not drawn.
      ...u.hid ? { hid: 1 } : {},
      ...u.orderAt !== void 0 ? { orderIn: Math.max(0, r12(u.orderAt - t)) } : {},
      // Hold until relieved (release 2, sim.relief), his own blocks only: waiting for a corps coming up from next door, dug in
      // where it stands while not free; late, help never came and its men break sooner.
      ...mine && u.waits ? { waits: { due: r12(u.waits.due), ...u.waits.late ? { late: true } : {}, ...u.waits.free ? { free: true } : {} } } : {},
      // Hedge cutters (release 2, sim.cutters): a tank block that carries them, with the share of its tanks fitted (the
      // 3D game draws the cutters on that many). Smoke on call (sim.smoke): his own blocks that may lay it, with the
      // screens they have left.
      ...sim?.cutters && sim.cutterOf(u) ? { cutter: LAND.cutters.share } : {},
      ...mine && sim?.smoke ? smokeOut(sim, u) : {},
      order: u.order ? { kind: u.order.kind, x: r12(u.order.x), y: r12(u.order.y), target: u.order.target ?? -1, path: u.path.map(([x, y]) => [r12(x), r12(y)]) } : null
    };
  }
  function smokeOut(sim, u) {
    const m = sim.smokeOf(u);
    return m ? { smoke: u.smokeLeft ?? m.count ?? LAND.smoke.count } : {};
  }
  function yearOf(t) {
    const m = /(19[34]\d)/.exec(t.date ?? "");
    return m ? Number(m[1]) : 1942;
  }
  function standInModel(kind, nat, year, n, own = false) {
    const axis = ["GER", "ITA", "JAP", "HUN", "ROM", "BUL", "FIN"].includes(nat);
    if (own && nat === "JAP") {
      if (kind === "art") return "Gun75";
      if (year >= 1938) return "Type97";
    }
    if (kind === "art") return nat === "SOV" ? "ZiS3" : axis ? "leFH18" : "Howitzer105";
    if (nat === "GER") return year >= 1943 ? ["PanzerIV", "PanzerIV", "Tiger", "StuG3"][n % 4] : year >= 1941 ? ["PanzerIV", "StuG3", "LightTank"][n % 3] : ["LightTank", "PanzerIV"][n % 2];
    if (nat === "SOV") return year >= 1941 ? ["T34", "T34", "LightTank"][n % 3] : "LightTank";
    if (axis) return year >= 1943 && nat !== "JAP" ? ["StuG3", "LightTank"][n % 2] : "LightTank";
    return year >= 1942 ? ["Sherman", "Sherman", "LightTank"][n % 3] : "LightTank";
  }
  function ticketSky(t) {
    const w = (t.weather ?? "").toLowerCase();
    const lean = (t.seed % 21 - 10) / 100;
    const c = (x) => Math.round(Math.min(1, Math.max(0, x + lean)) * 100) / 100;
    if (w === "rain") return { cover: c(0.85), base: 600, kind: "deck", hour: 11 };
    if (w === "snow") return { cover: c(0.9), base: 500, kind: "deck", hour: 11 };
    if (w === "mud") return { cover: c(0.7), base: 800, kind: "deck", hour: 11 };
    if (w === "clear") return { cover: c(0.2), base: 1400, kind: "cumulus", hour: 10 };
    return { cover: c(0.45), base: 1e3, kind: "cumulus", hour: 10 };
  }
  var DRAW = { reach: 400, step: 20 };
  var LiveBattle = class {
    constructor(ticket, piece, opts = {}) {
      __publicField(this, "ticket", ticket);
      __publicField(this, "piece", piece);
      __publicField(this, "sim");
      __publicField(this, "drill");
      __publicField(this, "me");
      __publicField(this, "keeper", new LiveKeeper());
      /** A campaign battle's sky (no set piece): as given, or drawn from its ticket. */
      __publicField(this, "skyOf");
      __publicField(this, "steppe");
      __publicField(this, "blocks");
      /** The player's looks switches the header reads (worn uniforms, mud remembered, scarred ground). */
      __publicField(this, "looks", { worn: true, mud: true, scars: true });
      /** The place's own dress passed on to the 3D game (release 2, LiveOpts.island): a campaign battle's only. */
      __publicField(this, "island", false);
      __publicField(this, "seen", /* @__PURE__ */ new Set());
      __publicField(this, "roundsNew", []);
      __publicField(this, "phases", /* @__PURE__ */ new Map());
      __publicField(this, "phaseNew", /* @__PURE__ */ new Map());
      __publicField(this, "events", []);
      __publicField(this, "views", []);
      /** The hedge cutters' gaps already sent (the rules' cuts, in the order begun). */
      __publicField(this, "cutsSent", 0);
      __publicField(this, "lastView", -1);
      __publicField(this, "placed", false);
      __publicField(this, "sentOver", false);
      this.me = opts.me ?? null;
      this.skyOf = opts.sky ?? ticketSky(ticket);
      const sim = this.sim = new LandBattle(ticket);
      this.steppe = piece?.open === "steppe" ? openSteppe(sim.field, ticket.seed) : null;
      if (this.steppe) for (const u of sim.units) u.cover = groundAt(sim.field, u.x, u.y);
      for (const u of sim.allBlocks()) if (u.kind === "arm") {
        const c = calFor("arm", u.nat, ticket.turn, opts.designYears?.[u.nat]);
        if (c) u.cal = c;
      }
      if (opts.fog) {
        sim.fog = true;
        sim.seen = new Set(opts.seen ?? []);
      }
      for (const s of ["att", "def"]) sim.command({ do: "auto", side: s, on: true }, s);
      if (this.me && opts.staff !== false) sim.perUnit = true;
      if (this.me && opts.radio !== false) sim.radio = true;
      if (opts.mech === true) sim.mech = true;
      if (opts.spg === true && !piece) sim.spg = true;
      if (opts.td === true && !piece) sim.td = true;
      if (opts.cars === true && !piece) sim.cars = true;
      if (opts.aa === true && !piece) sim.aa = true;
      if (opts.light === true && !piece) sim.light = true;
      this.drill = sim.drillOn(opts.platoons === false ? { sections: true } : { sections: true, platoons: true });
      if (opts.setup && this.me) sim.setupOn(this.me, true);
      if (opts.hide !== false) sim.hideOn();
      if (opts.armour !== false) sim.measured = true;
      if (opts.names !== false) sim.namesOn();
      if (this.me && opts.relief !== false) sim.relief = true;
      if (this.me && opts.rider !== false) sim.rider = true;
      if (opts.cutters !== false) sim.cutters = true;
      if (opts.smoke !== false) sim.smoke = true;
      if (opts.bocage) sim.bocage = true;
      if (opts.phones !== false) sim.phones = true;
      if (opts.bearers !== false) sim.bearers = true;
      if (opts.stand === true) sim.standRule = true;
      if (opts.facing === true && opts.stand === true) sim.faceRule = true;
      if (opts.teams === true && opts.facing === true && opts.stand === true) sim.teamsRule = true;
      if (opts.blast === true) sim.blast = true;
      if (opts.escort === true) sim.escort = true;
      if (opts.patrol !== false && opts.hide !== false) sim.patrol = true;
      const tanksSeen = { att: 0, def: 0 };
      this.looks = { worn: opts.worn !== false, mud: opts.mud !== false, scars: opts.scars !== false };
      this.island = !piece && opts.island === true;
      const caught = !piece && opts.captured !== false && ticket.captured ? ticket.captured : null;
      const caughtLeft = { att: caught?.att?.n ?? 0, def: caught?.def?.n ?? 0 };
      this.blocks = sim.allBlocks().map((u) => {
        const a = piece ? u.side === "att" ? piece.att : piece.def : null;
        let model = null;
        if (a && u.kind === "arm") model = a.tanks[Math.min(a.tanks.length - 1, tanksSeen[u.side]++)];
        else if (a && u.kind === "art" && !u.team) model = a.gun;
        if (model) sim.vehicleIs(u.id, model);
        else if (!piece && (u.kind === "arm" || u.kind === "art" && !u.team)) model = standInModel(u.kind, u.nat, opts.designYears?.[u.nat] ?? yearOf(ticket), tanksSeen[u.side]++, opts.japan === true);
        const took = caught && u.kind === "arm" && caughtLeft[u.side] > 0 ? caught[u.side] : void 0;
        if (took) {
          caughtLeft[u.side]--;
          model = standInModel("arm", took.from, opts.designYears?.[took.from] ?? yearOf(ticket), 0, opts.japan === true);
        }
        return {
          id: u.id,
          side: u.side,
          nat: u.nat,
          kind: u.kind,
          name: u.name,
          short: u.short,
          parent: u.parent,
          corps: u.corps,
          figures0: u.figures0,
          men0: u.men0,
          tray: u.tray,
          w: u.w,
          d: u.d,
          ...u.cal ? { cal: u.cal } : {},
          ...u.team ? { team: u.team } : {},
          ...u.crew ? { crew: u.crew } : {},
          model,
          ...a ? { army: a.army, ...a.standIn ? { standIn: a.standIn } : {} } : {},
          ...u.joined !== void 0 ? { joins: true } : {},
          ...took ? { captured: took.from } : {},
          // (release 2, the "japan" row: a Japanese block's vehicles in her own coat where the model is one she shares with
          // other armies and its file has no Japanese paint, the field gun, the lorry, the light tank; the 3D game keeps
          // the coat by this name. Looks only, a campaign battle's only: a set piece is as the historian drew it.)
          ...!piece && opts.japan === true && u.nat === "JAP" ? { coat: "japan" } : {},
          // (release 2: the half-tracks, self-propelled guns and tank destroyers its men are drawn with, for the 3D game to
          // read before the battle opens)
          ...((mls) => mls.length ? { mls } : {})(this.drill.modelsFor(u))
        };
      });
      this.look();
    }
    /** The recording as the scene reads it, with nothing fought yet: what LandFeed reads before the first delta. */
    header() {
      const p = this.piece, f = this.sim.field, t = this.ticket;
      const room = this.sim.setupRoom();
      return {
        what: "LIVE: a land battle fought as the 3D game shows it (src/battle/land/live.ts). The recording's shapes; the men, rounds, events, views and phases come in the deltas.",
        recording: true,
        live: true,
        key: p?.key ?? "field",
        seed: t.seed,
        dt: DT,
        title: t.title,
        date: t.date,
        turn: t.turn,
        // (the campaign's weather word, for the ground under it, and scarred ground: looks only, release 2; the 3D game
        // lays thaw mud from "mud", weathered old craters from the scars, and old ruts from the battles fought in mud)
        ...t.weather ? { weather: t.weather } : {},
        ...t.scars && t.scars.count > 0 && this.looks.scars ? { scars: { count: t.scars.count, months: t.scars.months } } : {},
        ...t.oldRuts && t.oldRuts.count > 0 && this.looks.mud ? { oldRuts: { count: t.oldRuts.count, months: t.oldRuts.months } } : {},
        // (the place's own latitude and longitude, release 2 on its "island" option: only where the sky has none)
        ...p ? { sky: { ...p.sky, latLon: p.latLon, hour: p.hour } } : { sky: this.island && t.latLon && !this.skyOf.latLon ? { ...this.skyOf, latLon: t.latLon } : this.skyOf },
        sides: {
          att: { nat: t.att.nat, adj: t.att.adj, color: t.att.color, ...p ? { army: p.att.army, ...p.att.standIn ? { standIn: p.att.standIn } : {} } : {}, ...t.att.worn !== void 0 && this.looks.worn ? { worn: t.att.worn } : {} },
          def: { nat: t.def.nat, adj: t.def.adj, color: t.def.color, ...p ? { army: p.def.army, ...p.def.standIn ? { standIn: p.def.standIn } : {} } : {}, ...t.def.worn !== void 0 && this.looks.worn ? { worn: t.def.worn } : {} }
        },
        field: {
          w: f.w,
          h: f.h,
          cell: f.cell,
          cols: f.cols,
          rows: f.rows,
          grounds: GROUNDS,
          ground: f.ground,
          height: f.height.map((x) => r12(x)),
          zones: f.zones,
          town: f.town,
          place: t.place,
          ...f.storeys ? { storeys: f.storeys } : {},
          ...f.landmark ? { landmark: f.landmark } : {},
          ...f.stream ? { stream: f.stream } : {},
          ...this.steppe ? { steppe: this.steppe } : {},
          // (the 3D look the campaign names for this ground, release 2 on its "island" option: LandScape reads it first)
          ...this.island && t.look ? { look: t.look } : {}
        },
        limit: LAND.limit,
        contact: null,
        end: 0,
        blocks: this.blocks,
        phases: [],
        views: [],
        events: [],
        rounds: [],
        men: [],
        over: null,
        me: this.me,
        ...room ? { setup: { side: this.me, room } } : {}
      };
    }
    /** Watches one step's men, rounds, phases, events and view, as the recorder's look() does. */
    look() {
      const t = this.sim.t;
      const men = this.drill.view();
      const seen = /* @__PURE__ */ new Set();
      for (const b of men) {
        for (const m of b.men) {
          this.keeper.see(t, b.id, m, false);
          seen.add(keyOf(b.id, m, false));
        }
        for (const m of b.fo ?? []) {
          this.keeper.see(t, b.id, m, true);
          seen.add(keyOf(b.id, m, true));
        }
        const last = this.phases.get(b.id);
        const now = [r2(t), b.ph, b.foWith ?? null, b.foOn ?? null];
        if (!last || last[1] !== now[1] || last[2] !== now[2] || last[3] !== now[3]) {
          this.phases.set(b.id, now);
          const ph = this.phaseNew.get(b.id) ?? [];
          ph.push(now);
          this.phaseNew.set(b.id, ph);
        }
      }
      this.keeper.gone(t, seen);
      for (const r of this.drill.rounds()) if (!this.seen.has(r.n)) {
        this.seen.add(r.n);
        this.roundsNew.push(r);
      }
      const v = this.sim.view();
      for (const e of v.events) this.events.push(e);
      if (Math.floor(t) !== this.lastView || v.over) {
        this.lastView = Math.floor(t);
        this.views.push({
          t: r2(t),
          units: v.units.map((u) => landUnitOut(u, v.t, !this.me || this.sim.known(u), !!this.me && u.side === this.me, this.sim)),
          balance: v.balance,
          zones: v.zones,
          ...v.breaking ? { breaking: { att: r32(v.breaking.att), def: r32(v.breaking.def) } } : {},
          ...v.contact !== void 0 ? { contact: v.contact } : {},
          ...v.plan ? { plan: Object.fromEntries(Object.entries(v.plan).filter(([, p]) => p?.phase).map(([s, p]) => [s, { phase: p.phase, main: p.main ?? [], startY: p.startY ?? null }])) } : {},
          ...v.setup ? { setup: v.setup } : {},
          // (the corps still coming up from next door that the player may know of: where on its edge and when)
          ...v.coming?.length ? { coming: v.coming.map((c) => ({ id: c.id, side: c.side, corps: c.corps, kind: c.kind, at: r12(c.at), edge: c.edge, x: r12(c.x), y: r12(c.y), ...c.arrive ? { arrive: c.arrive } : {} })) } : {},
          // (the rider ahead: his own corps' despatch riders, riding in before it)
          ...v.riders?.length ? { riders: v.riders } : {},
          // (smoke on call: the screens standing now, the enemy's block named only where he knows it; the smoke rounds
          // carry how long each hangs, for the 3D field. The hedge cutters' gaps go once each, in the delta's cuts)
          ...this.sim.screenView(this.me ?? void 0),
          // (field telephones: the wires he may see; stretcher bearers: the light cases back in the line, each shown once
          // by its t and unit; the night patrol: the attack's own route, and where it was caught)
          ...this.sim.phonesView(this.me ?? void 0),
          ...this.sim.bearerView(this.me ?? void 0),
          ...this.sim.patrolView(this.me ?? void 0),
          ...v.over ? { over: v.over } : {}
        });
      }
    }
    /**
     * Fights on for up to `seconds` of battle time (none while the setup step is open) and says what happened since the
     * last delta. At most maxSteps steps a call, so the game hears back often.
     */
    tick(seconds, maxSteps = 50) {
      const steps = Math.min(maxSteps, Math.max(0, Math.round(seconds / DT)));
      for (let i = 0; i < steps && !this.sim.over && !this.sim.setup; i++) {
        this.sim.step();
        this.look();
      }
      return this.delta();
    }
    /** Fights on until battle time `t` (or the end), at most maxSteps steps. */
    to(t, maxSteps = 50) {
      return this.tick(Math.max(0, t - this.sim.t + 1e-6), maxSteps);
    }
    delta() {
      const over = this.sim.over;
      if (over && !this.sentOver) this.keeper.close();
      const last = !!over && !this.sentOver;
      const d = {
        t: r2(this.sim.t),
        ...last ? { end: r2(this.sim.t) } : {},
        ...this.placed ? { reset: true } : {},
        ...this.keeper.drain(),
        rounds: this.roundsNew.sort((a, b) => a.n - b.n),
        events: this.events,
        views: this.views,
        phases: [...this.phaseNew.entries()].map(([b, ph]) => ({ b, ph })),
        contact: this.sim.contactAt
      };
      const cuts = this.sim.cuts.slice(this.cutsSent).filter((c) => !this.me || c.side === this.me || c.known);
      this.cutsSent = this.sim.cuts.length;
      if (cuts.length) d.cuts = cuts.map((c) => ({ t0: r2(c.t0), t1: r2(c.t1), unit: c.unit, x: r12(c.x), y: r12(c.y), yaw: r32(c.yaw), width: r2(c.width) }));
      if (last) {
        d.over = over;
        this.sentOver = true;
      }
      this.events = [];
      this.views = [];
      this.roundsNew = [];
      this.phaseNew.clear();
      this.placed = false;
      return d;
    }
    /** One of the player's orders, in the rules' own shapes. False where the rules refuse it. */
    order(c) {
      if (!this.me) return false;
      return this.sim.command(c, this.me);
    }
    /**
     * Hold until relieved: why one of his blocks may not wait for help (null where it may), and the battle second help
     * could reach it there (null where it may not wait).
     */
    waitWhy(id) {
      const u = this.sim.unit(id);
      if (!u) return { why: "No such block.", due: null };
      if (!this.me || u.side !== this.me) return { why: "Only your own troops take your orders.", due: null };
      const why = this.sim.waitWhy(u);
      return { why, due: why === null ? this.sim.dueAt(u)?.due ?? null : null };
    }
    /**
     * Hedge cutters: why one of his tank blocks may not cut a gap through the hedge at (x, y) (null where it may); with no
     * point, whether it may cut at all.
     */
    cutWhy(id, x, y) {
      const u = this.sim.unit(id);
      if (!u) return "Nothing to order.";
      if (!this.me || u.side !== this.me) return "Only your own troops take your orders.";
      return this.sim.cutWhy(u, x, y);
    }
    /**
     * Smoke on call: why one of his blocks may not lay a screen along that line (x0, y0 to x1, y1, or only its middle; null
     * where it may); with no line, whether it may lay one at all.
     */
    smokeWhy(id, x0, y0, x1, y1) {
      const u = this.sim.unit(id);
      if (!u) return "Nothing to order.";
      if (!this.me || u.side !== this.me) return "Only your own troops take your orders.";
      return this.sim.smokeWhy(u, x0, y0, x1, y1);
    }
    /** Night patrol: why one of his blocks may not go out on patrol tonight, or null where it may. */
    patrolWhy(id) {
      const u = this.sim.unit(id);
      if (!u) return "Nothing to order.";
      if (!this.me || u.side !== this.me) return "Only your own troops take your orders.";
      return this.sim.patrolWhy(u);
    }
    /** Why a block may not stand there in the setup step, or null where it may. */
    canPlace(id, x, y) {
      const u = this.sim.units.find((v) => v.id === id);
      if (!u) return "No such block.";
      return this.sim.placeWhy(u, x, y);
    }
    /** Places one of the player's blocks in the setup step (and, where the rules take it, turns it to face). */
    place(id, x, y, face) {
      if (!this.me || !this.sim.setup) return false;
      const ok2 = this.sim.command({ do: "land:place", units: [id], x, y, ...face !== void 0 ? { face } : {} }, this.me);
      if (ok2) this.replace();
      return ok2;
    }
    /**
     * The 3D field's hedges and their gates, before the clock runs (the rules' sim.hedgesOn; the historian's check, 22
     * November: a block on a hedge line hides as in woods, and the men on foot go through the gates). False once the clock
     * has run. The men stand again (a reset piece), as their hiding may have changed.
     */
    hedges(hedges, gates) {
      const ok2 = this.sim.hedgesOn(hedges, gates);
      if (ok2) this.replace();
      return ok2;
    }
    /**
     * Why several of his blocks, each [id, x, y, face?], may not be placed as one act in the setup step: the block that
     * cannot go (its id and name) and why, or null where they all may.
     */
    canPlaceMany(list) {
      if (!this.me || !this.sim.setup) return { id: -1, who: "", why: "Not now: the battle has begun." };
      if (!list.length) return { id: -1, who: "", why: "Nothing to place." };
      const us = list.map((p) => this.sim.units.find((v) => v.id === Number(p[0])));
      const lost = us.findIndex((u) => !u || u.side !== this.me);
      if (lost >= 0) return { id: Number(list[lost][0]), who: "", why: us[lost] ? "Only your own troops take your orders." : "No such block." };
      const no = this.sim.placeManyWhy(us, list.map((p) => p.slice(1)));
      return no ? { ...no, who: this.whoIs(no.id) } : null;
    }
    /**
     * The ground that bars a block from standing at a place in the setup step, for the screen's sentence when the rules
     * refuse it there (sim.standWhy's water line): a bridge under any of it, else the water (a blown bridge is in the
     * river), else the ground under its middle, which it can hardly cross (a marsh, bare rock or a bunker, to wheels,
     * tracks and guns). Null where there is no such block or place. Words only: no rule reads it.
     */
    barredBy(id, x, y) {
      const u = this.sim.units.find((v) => v.id === id), f = this.sim.field;
      if (!u || !Number.isFinite(x) || !Number.isFinite(y)) return null;
      const hw = u.w / 2, hd = u.d / 2, nx = Math.max(1, Math.ceil(2 * hw / (f.cell / 2))), ny = Math.max(1, Math.ceil(2 * hd / (f.cell / 2)));
      let wet = false;
      for (let i = 0; i <= nx; i++) for (let j = 0; j <= ny; j++) {
        const c = Math.floor((x - hw + 2 * hw * i / nx) / f.cell), r = Math.floor((y - hd + 2 * hd * j / ny) / f.cell);
        if (c < 0 || r < 0 || c >= f.cols || r >= f.rows) continue;
        const k = GROUNDS[f.ground[r * f.cols + c]];
        if (k === "bridge") return "bridge";
        wet = wet || k === "water" || k === "blown";
      }
      const g = groundAt(f, Math.min(Math.max(x, 5), f.w - 5), Math.min(Math.max(y, 5), f.h - 5));
      if (g === "bridge") return "bridge";
      if (wet || g === "water" || g === "blown") return "water";
      return g === "marsh" || g === "rock" || g === "bunker" ? g : null;
    }
    /** Places several of the player's blocks as one act in the setup step, each [id, x, y, face?]: all of them or none. */
    placeMany(list) {
      if (this.canPlaceMany(list) !== null) return false;
      const ok2 = this.sim.command({ do: "land:place", units: list.map((p) => Number(p[0])), places: list.map((p) => p.slice(1)) }, this.me);
      if (ok2) this.replace();
      return ok2;
    }
    /**
     * Draws his blocks up in the setup step (formation.ts; Brian, 26 November 2026): those named, or with none named
     * every block of his that may be placed, in a line, two lines, a wedge or a column about the middle of where they
     * stand, facing the enemy, guns and support teams behind. Where a place of it is barred (water, a block that stays, a
     * fort), the whole formation is tried a little to every side, the nearest first, and stands on the first ground that
     * takes all of it: never outside his own ground. Says where each was placed and how far off the place worked out
     * (off, metres), and those named that stay where they are (left: dug in, each with its name); or why none was:
     * 'begun', 'nothing' (none of his to draw up), 'stay' (every one named stays where it is: id, who and why the
     * first's), 'room' (his ground cannot hold them so).
     */
    drawUp(kind, ids = []) {
      const none2 = (no, why, id) => ({ ok: false, why, none: no, ...id !== void 0 && id >= 0 ? { id, who: this.whoIs(id) } : {}, placed: [], left: [], off: 0 });
      const room = this.sim.setupRoom();
      if (!this.me || !this.sim.setup || !room) return none2("begun", "Not now: the battle has begun.");
      const me = this.me, mine = this.sim.units.filter((u) => u.side === me);
      const named = ids.length ? mine.filter((u) => ids.includes(u.id)) : mine;
      const us = named.filter((u) => this.sim.placeable(u) === null);
      const left = ids.length ? named.filter((u) => !us.includes(u)).map((u) => ({ id: u.id, who: this.sim.named(u), why: this.sim.placeable(u) })) : [];
      if (!us.length) return left.length ? { ...none2("stay", left[0].why, left[0].id), left } : none2("nothing", "Nothing to draw up.");
      const dir = me === "att" ? 1 : -1, face = me === "att" ? 0 : Math.PI;
      const slots = drawnUp(kind, us.map((u) => ({ id: u.id, x: u.x, y: u.y, w: u.w, d: u.d, rear: u.kind === "art" })), dir, room);
      if (!slots) return none2("room", "Your ground has no room to draw them up so.");
      const order = slots.map((s) => us.find((u) => u.id === s.id));
      const shifts = [];
      for (let dx = -DRAW.reach; dx <= DRAW.reach; dx += DRAW.step) for (let dy = -DRAW.reach; dy <= DRAW.reach; dy += DRAW.step) shifts.push([dx, dy]);
      shifts.sort((a, b) => Math.hypot(a[0], a[1]) - Math.hypot(b[0], b[1]) || a[1] - b[1] || a[0] - b[0]);
      for (const [dx, dy] of shifts) {
        const at = slots.map((s) => [s.x + dx, s.y + dy, face]);
        if (this.sim.placeManyWhy(order, at)) continue;
        const list = slots.map((s, i) => [s.id, at[i][0], at[i][1], face]);
        return this.placeMany(list) ? { ok: true, why: "", placed: list, left, off: Math.hypot(dx, dy) } : none2("begun", "Not now: the battle has begun.");
      }
      return none2("room", "Your ground has no room to draw them up so.");
    }
    /** A block's name as the player reads it ('' for none). */
    whoIs(id) {
      const u = this.sim.units.find((v) => v.id === id);
      return u ? this.sim.named(u) : "";
    }
    /** Ends the setup step: the clock may run. */
    begin() {
      if (!this.me || !this.sim.setup) return false;
      const ok2 = this.sim.command({ do: "land:begin" }, this.me);
      if (ok2) this.replace();
      return ok2;
    }
    /** The men stand somewhere new before the clock has run: every track starts again from where they stand now. */
    replace() {
      this.keeper = new LiveKeeper();
      this.phases.clear();
      this.phaseNew.clear();
      this.lastView = -1;
      this.placed = true;
      this.look();
    }
  };
  function liveTicket(ticket, opts = {}, pieceKey) {
    const p = pieceKey ? PIECES.find((x) => x.key === pieceKey) ?? null : null;
    return new LiveBattle(ticket, p, opts);
  }
  function livePiece(key, seed, opts = {}) {
    const p = PIECES.find((x) => x.key === key);
    if (!p) throw new Error(`no set piece ${key}`);
    const s = seed ?? p.seeds[0];
    return new LiveBattle(ticketOf(p, s, opts.me ?? null), p, opts);
  }

  // Rules/land-bridge.ts
  var live3 = null;
  var none = { t: 0, men: [], tails: [], rounds: [], events: [], views: [], phases: [], contact: null };
  function need() {
    if (!live3) throw new Error("No ground battle is being fought: TC.landStart first.");
    return live3;
  }
  var DUG = "It is dug in where it stands.";
  var DRAWN2 = { line: "in a line", two: "in two lines", wedge: "in a wedge", column: "in a column" };
  var COUNTED = ["None", "One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight", "Nine", "Ten", "Eleven", "Twelve"];
  var BARRED = { water: "in the water", bridge: "on a bridge", marsh: "in a marsh", rock: "on bare rock", bunker: "on a bunker" };
  var theName = (who) => /^\d/.test(who) ? `The ${who}` : who;
  function cannotGo(who, why, on) {
    if (!who) return why;
    const name = theName(who);
    if (why === DUG) return `${name} is dug in where it stands.`;
    if (why === "Outside your ground.") return `${name} cannot go there: it would stand outside your ground.`;
    if (why === "Another block stands there.") return `${name} cannot go there: another unit already stands there.`;
    if (why === "Off the field.") return `${name} cannot go there: it would stand off the field.`;
    if (why === "Not on water, a bridge or ground it cannot stand on.") return `${name} cannot go there: it cannot stand ${on ? BARRED[on] : "on water or on a bridge"}.`;
    return `${name} cannot go there. ${why}`;
  }
  globalThis.TC = {
    landPieces() {
      return PIECES.map((p) => ({
        key: p.key,
        title: p.title,
        date: p.date,
        seeds: p.seeds,
        att: { nat: p.att.nat, adj: p.att.adj, army: p.att.army, general: p.att.general },
        def: { nat: p.def.nat, adj: p.def.adj, army: p.def.army, general: p.def.general }
      }));
    },
    landStart(key, seed, opts) {
      live3 = livePiece(key, seed ?? void 0, {
        me: opts?.me ?? null,
        setup: !!opts?.setup,
        platoons: opts?.platoons !== false,
        staff: opts?.staff !== false,
        radio: opts?.radio !== false,
        hide: opts?.hide !== false,
        armour: opts?.armour !== false,
        names: opts?.names !== false,
        relief: opts?.relief !== false,
        rider: opts?.rider !== false,
        cutters: opts?.cutters !== false,
        smoke: opts?.smoke !== false,
        bocage: !!opts?.bocage,
        phones: opts?.phones !== false,
        bearers: opts?.bearers !== false,
        teams: opts?.teams === true,
        facing: opts?.facing === true,
        stand: opts?.stand === true,
        patrol: opts?.patrol !== false,
        captured: opts?.captured !== false,
        worn: opts?.worn !== false,
        mud: opts?.mud !== false,
        scars: opts?.scars !== false,
        blast: opts?.blast === true,
        escort: opts?.escort === true,
        mech: opts?.mech === true,
        spg: opts?.spg === true,
        td: opts?.td === true,
        cars: opts?.cars === true,
        aa: opts?.aa === true,
        light: opts?.light === true,
        japan: opts?.japan === true
      });
      return live3.header();
    },
    /**
     * A campaign battle live (release 2, behind the game's switch): its ticket as the campaign's TC.battleLook gives it,
     * the card's fog (fog, seen: the corps it identified) and each nation's armour design year, with the same switches as
     * landStart. Its result (the last delta's over) goes back to the campaign's TC.battleSettleLand(ids, to, over).
     */
    landStartTicket(ticketOrLook, opts) {
      const sw = opts;
      if (sw?.teams === true && sw?.facing === true && sw?.stand === true) ticketOrLook = ownTeamsLook(ticketOrLook);
      const look = "ticket" in ticketOrLook && ticketOrLook.ticket ? ticketOrLook : null;
      const ticket = look ? look.ticket : ticketOrLook;
      const o = { ...look?.live ?? {}, ...opts ?? {} };
      live3 = liveTicket(ticket, {
        me: o.me ?? null,
        setup: !!o.setup,
        platoons: o.platoons !== false,
        staff: o.staff !== false,
        radio: o.radio !== false,
        hide: o.hide !== false,
        armour: o.armour !== false,
        names: o.names !== false,
        relief: o.relief !== false,
        rider: o.rider !== false,
        cutters: o.cutters !== false,
        smoke: o.smoke !== false,
        bocage: !!o.bocage,
        phones: o.phones !== false,
        bearers: o.bearers !== false,
        teams: o.teams === true,
        facing: o.facing === true,
        stand: o.stand === true,
        patrol: o.patrol !== false,
        captured: o.captured !== false,
        worn: o.worn !== false,
        mud: o.mud !== false,
        scars: o.scars !== false,
        blast: o.blast === true,
        escort: o.escort === true,
        mech: o.mech === true,
        spg: o.spg === true,
        td: o.td === true,
        cars: o.cars === true,
        aa: o.aa === true,
        light: o.light === true,
        japan: o.japan === true,
        island: o.island === true,
        fog: !!o.fog,
        seen: o.seen ?? [],
        designYears: o.designYears ?? {},
        ...o.sky ? { sky: o.sky } : {}
      });
      return live3.header();
    },
    landTick(seconds, maxSteps) {
      return live3 ? live3.tick(seconds, maxSteps ?? 50) : none;
    },
    landTo(t, maxSteps) {
      return live3 ? live3.to(t, maxSteps ?? 50) : none;
    },
    landOrder(c) {
      const ok2 = need().order(c);
      const o = c;
      if (!ok2 && o.do === "land:wait" && o.units?.length) return { ok: ok2, why: need().waitWhy(Number(o.units[0])).why ?? "" };
      const p = c;
      if (!ok2 && o.do === "land:cut" && o.units?.length) return { ok: ok2, why: need().cutWhy(Number(o.units[0]), p.x, p.y) ?? "" };
      if (!ok2 && o.do === "land:smoke" && o.units?.length) return { ok: ok2, why: need().smokeWhy(Number(o.units[0]), p.x0, p.y0, p.x1, p.y1) ?? "" };
      const n = c;
      if (!ok2 && o.do === "land:patrol") return { ok: ok2, why: need().patrolWhy(Number(n.unit ?? o.units?.[0])) ?? "" };
      return { ok: ok2 };
    },
    landWaitWhy(id) {
      return need().waitWhy(Number(id));
    },
    landCutWhy(id, x, y) {
      return { why: need().cutWhy(Number(id), x ?? void 0, y ?? void 0) };
    },
    landSmokeWhy(id, x0, y0, x1, y1) {
      return { why: need().smokeWhy(Number(id), x0 ?? void 0, y0 ?? void 0, x1 ?? void 0, y1 ?? void 0) };
    },
    landCanPlace(id, x, y) {
      const why = need().canPlace(id, x, y);
      return { ok: why === null, why: why ?? "" };
    },
    landPlace(id, x, y, face) {
      const why = need().canPlace(id, x, y);
      const ok2 = need().place(id, x, y, face ?? void 0);
      return { ok: ok2, why: ok2 ? "" : why ?? "Not now: the battle has begun." };
    },
    landPlaceMany(list) {
      const l = need(), rows = Array.isArray(list) ? list.filter((p) => Array.isArray(p)).map((p) => p.map(Number)) : [];
      const no = l.canPlaceMany(rows);
      const ok2 = no === null && l.placeMany(rows);
      const at = no ? rows.find((p) => p[0] === no.id) : void 0;
      return { ok: ok2, why: ok2 ? "" : no ? cannotGo(no.who, no.why, at ? l.barredBy(no.id, at[1], at[2]) : null) : "Not now: the battle has begun.", ...no && no.id >= 0 ? { id: no.id, who: no.who } : {} };
    },
    landDrawUp(kind, ids) {
      const k = FORMATIONS.find((f) => f === kind);
      if (!k) return { ok: false, why: "No such formation.", placed: 0 };
      const named = Array.isArray(ids) ? ids.map(Number) : [];
      const r = need().drawUp(k, named);
      const dug = r.left.filter((b) => b.why === DUG);
      if (!r.ok) {
        const why = r.none === "room" ? `Your ground has no room to draw them up ${DRAWN2[k]}.` : r.none === "stay" && dug.length === r.left.length && dug.length > 1 ? "They are all dug in where they stand." : r.none === "stay" ? cannotGo(r.who ?? "", r.why) : r.why;
        return { ok: false, why, ...r.id !== void 0 ? { id: r.id, who: r.who ?? "" } : {}, placed: 0 };
      }
      const notes = [];
      if (dug.length === 1) notes.push(`${theName(dug[0].who)} is dug in and stays where it stands.`);
      else if (dug.length > 1) notes.push(`${COUNTED[dug.length] ?? dug.length} of them are dug in and stay where they stand.`);
      if (r.off >= 50) notes.push(`Drawn up ${Math.round(r.off / 10) * 10} m off, on the nearest ground that holds them all.`);
      return { ok: true, why: "", placed: r.placed.length, ...notes.length ? { note: notes.join(" ") } : {} };
    },
    landBegin() {
      return { ok: need().begin() };
    },
    landHedges(hedges, gates) {
      const l = need();
      const ok2 = Array.isArray(hedges) && l.hedges(hedges, Array.isArray(gates) ? gates : []);
      return { ok: ok2, hedges: l.sim.hedges.length, gates: l.sim.gates.length };
    },
    landResult() {
      return { over: live3?.sim.over ?? null };
    },
    landEnd() {
      const over = live3?.sim.over ?? null;
      live3 = null;
      return { over };
    }
  };
})();
