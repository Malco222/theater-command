// The doorway between the played battles (src/battle) and the Unreal game.
//
// The battles are the browser's own TypeScript sims, run in the game by the same JavaScript engine as the campaign
// rules (Tools/build_rules.sh bundles this file with campaign-bridge.ts). Nothing here decides anything: it builds the
// battle from the campaign's own attack, runs the shared clock, passes the player's orders, hands plain pictures of
// the field back as JSON, and settles the battle through the campaign's own attack order with the field's R in place
// of the dice. The 3D game draws the pictures: the models, their walk and fire, the sounds the events name.
// It draws every explosion too, by the art director's sheet (land: "Explosions" below).
//
// Wiring (two lines in campaign-bridge.ts, for the 3D builder):
//   1. its attack(ids, to) takes two more arguments, fought?: number and field?: FieldBill (src/campaign/game.ts), and
//      passes them on: orderAttack(s, sent, s.regions[to], fought, field) (field: what the field cost each corps and
//      each side, so the bill falls on the corps as the field had it and the after card agrees with the field, ruling 226(6))
//   2. import { battleCalls } from './battle-bridge';   and in the TC object:   ...battleCalls({ game, attack }),
// With no battle fought, nothing changes: attack(ids, to) rolls the dice as before.
// Air adds one more line: its act(action, region) takes a third argument, fought?: FoughtAir, passes it to
// airAct(s, action, fought), and puts airAct's after on what it returns (after: air.after, the after card written
// from what the campaign booked: { lines, level, groups }); and the TC object's line becomes
//   ...battleCalls({ game, attack, raid: (k, t, ids, f) => act(`raid:${k}:${t}:${ids.join('.')}`, -1, f) }),
// With no air battle fought, nothing changes there either: the raid is paid with the dice as before.
//
// The calls, on the same global object TC:
//   TC.battleLook(ids, to)       -> { ticket } for the attack the card shows, null where there is no field battle to fight,
//                                   or { ticket: null, why } where the staff settles that kind of place (offered.ts): keep
//                                   FIGHT IT YOURSELF on the card, off, with why beside it
//   TC.battleStart(ids, to)      -> { field, view }: the battle opens paused; field is the ground, sent once
//   TC.battleTick(seconds)       -> view: runs the clock for that much real time at the speed set. Until the armies first
//                                   meet (view.contact null) a land battle runs ×4 whatever the speed below that, then
//                                   drops to the speed set (ruling 226(1)): say so on the field, "×4 until the armies
//                                   meet", then "The armies meet · Running ×1" when view.contact turns to a time
//   TC.battleSpeed(speed)        -> { speed }: 0 (paused), 0.5, 1, 2 or 4
//   TC.battleOrder(command)      -> { ok }: one of the core's commands (src/battle/core/types.ts), for the player's side
//   TC.battleView()              -> view, without running the clock
//   TC.battleChance()            -> { chance, contact }: the staff's chance now that the attack carries the ground, 0 to
//                                   1, and whether the armies have met, for HAND IT TO THE STAFF's question (ruling
//                                   226(5)); null where the battle has no such question (sea and air word their own).
//                                   Where the card did not know the defence (b453) it is { lo, hi, contact } instead:
//                                   the staff's band moved as the fight has moved it, "between about 40% and 65%",
//                                   never the chance at the true odds
//   TC.battleEnd()               -> what TC.attack returns ({ report, siege, note, price, paid, moved, view }), once the
//                                   battle is over; null before that (after an air battle: what TC.act returns for the raid).
//                                   Where the army that broke was booked more than the field cost it, also pursuit: one
//                                   line for the after card, under ON THE FIELD straight after the strength line (b464).
//                                   An army pulled out after contact pays the rules' bill for that defeat times its bar
//                                   squared where that is more than the field cost it; pursuit then says what the
//                                   rearguard and the stragglers cost, in the same place: "As the Polish army withdrew
//                                   under fire, it lost 4,644 more men in the rearguard and as stragglers." (b474)
//                                   Where an army broke on the field, also brokeBy: why it broke, as below (b496(2)).
//   TC.airLook(kind, target, ids) -> { ticket } for the raid the raid card shows, or { ticket: null, why } where it
//                                   cannot be fought out (src/battle/air FOUGHT_KINDS: 'bomb', 'superiority')
//   TC.airStart(kind, target, ids) -> { sky, view }: the air battle opens paused; sky is the board, sent once
//
// A view: { t, limit, speed, units, balance, events, zones, ai, over, me }, and for a land battle breaking: { att, def }
// (how near each army is to breaking, 0 to 1, for the two army bars; an army breaks at 1, ruling 226(3)), breakingBy:
// { att, def } (what drives each bar as it stands, below) and contact (battle seconds of the first contact, null before
// it). Each unit: { id, side, kind, name, x, y, face, str, mor, state, figures0, figures, w, d, act, speed, engaged,
// cover, loop, order }, and cal where a land block has one ("Explosions" below). Field metres, x east, y north,
// the attacker from the south; face in radians, 0 north, clockwise. 'loop' is the sound it plays while it does what it
// does (its engine or its march; a block in a fight carries no fight loop of its own, as its shots by its nation's
// weapons and the shared beds are the fight, in the sound director's plan /mnt/project-files/sound/mix/mix.json, which
// the 3D game reads for the beds, the shots, and the orders and cheers by nation); each event's 'cue' is a one-shot, at
// its x and y (today only the attacker's whistle at first contact). Sound names are the approved battle pack's
// (/mnt/project-files/sound/battle-pack/, played from its own files).
// A unit may also carry drops: { takes, every: [lo, hi] }: play one take at a time, at random and never the one played
// last, then wait a random lo to hi seconds before the next (core/cues.ts nextTake and nextGap say it in code). Guns in
// action carry far_guns_01 to 03 every 6 to 20 s in place of a loop; a block fighting among buildings plays
// street_battle as its loop, with mg_town_01 to 03 dropped in; a lorry idles on lorry_idle_loop and drives on
// lorry_drive_loop. Several units playing the same bed or drops near each other should share one voice, not stack.
// An army of five blocks or fewer that its fresh blocks hold at the brink (its bar stands at 0.99, b480) does not stand
// there to be ground down (b496(1)): each of its blocks no longer fresh gives ground. Its order turns to 'withdraw', as
// a block the player sends back, an event { what: 'land:gives' } says so ("1st Regiment, 4th Motorised gives ground."),
// and it leaves the field as such a block does (state 'withdrawn'). Nothing new to draw.
// Where the attack card did not know the defence (b453), an enemy block none of the player's has fought yet, of a corps
// the card did not identify (a corps it names shows whole from the start, b476(3)), comes as
// { id, side, known: false, x, y, face, w, d, act, speed, cover, state } (b464(2)): draw a plain stand in its side's
// colour, w by d (the plain size every such block shares), with no name, strength, bar, state or arm, and no models of
// its arm; its hover reads "Polish troops, not identified · Right-click to attack" (the last part only while the
// player holds some of his own). act is only 'march', 'idle' or 'down', state only 'unknown', 'gone' or 'withdrawn'.
// It comes whole, without 'known', once one of his has fought it (his guns' target too), once it has struck one of his
// in contact (its guns firing from far back do not count), and once the battle is over. The dispatches never name it:
// where it falls on one of his, the event reads "Polish troops are attacking your 2nd Regiment."; no other event about
// it is sent at all, words or sound (the UI director, b464: a sound with nothing to read looks like a fault).
// A river the attack crosses in part (b464(5)): where some of the card's corps cross and some come on over dry ground
// ("1 of the 2 attacking corps cross the Vistula"), the river lies only before those that cross and bends away to the
// attacker's edge between the two; each force in the ticket that does not cross carries cross: false, and place.dryEnd
// says which end of the field ('west' or 'east') they draw up on. Nothing new to read for the ground: it comes in
// field.ground and field.height as always. The field opens with the UI director's words before the usual line: "XXI
// Corps must cross the Vistula. XIX Corps comes on over dry ground." The title's "across the Vistula" stands while any
// corps crosses.
// A land view's zones are its wrecks (b477, Brian's pick: burning wrecks stay): { id, kind: 'wreck', x, y, r, until,
// side, at, unit }, one for each tank a block has lost, where she stood, from battle second 'at' to the end of the
// fight. Draw her as the model of block 'unit', knocked out (her knocked_out clip, then burn), burning at first and
// then smoking to the end, never cut off (the art director's b477 look). Men on foot within r metres take light cover
// in the rules already; nothing more to draw for it. The zones also hold its craters (below).
// Explosions (Brian, 18 November: "very dramatic and life like"; his own look, copying no game's effects): the 3D game
// draws every burst, its flash and light, shock, earth crown, debris, smoke, the camera's jolt and the scar it leaves,
// by the art director's sheet (/mnt/project-files/art-director/explosions/explosions-look.md) and the ground under it.
// The rules give it what it cannot work out for itself (src/battle/land/shells.ts); none of it moves the fight:
//   cal, on a land unit: what its guns throw, as the sheet's size classes: 'mortar' 81 or 82 mm, 'field' 75 or 76 mm,
//     'howitzer' 105 mm, 'heavy' 150 or 155 mm, and 'light' for a 37 to 57 mm tank or anti-tank gun (the sheet has no
//     row for it yet: draw it at its hand grenade row). Guns by the model the unit table draws them with in the
//     battle's year, tanks in the year their nation's designs have reached (DesignYears; armourYear in
//     src/campaign/tech.ts, set at TC.battleStart), so a tank's blast is the gun it is drawn with; foot, mountain and lorried infantry 'mortar', their own battalions' (they land no shells:
//     their blows count as small arms); cavalry, engineers and fortress troops carry no cal. The mix plan's
//     impacts.bySize (sound/mix/mix.json) reads light as grenade, mortar as mortar, field as field_gun, howitzer as
//     howitzer_105 and heavy as howitzer_150.
//   A 'shell' event each time a gun's or a tank's blow costs a block one of its models (figures falls in the same
//     view): { t, what: 'shell', text: '', x, y, cal, by, on, armour, ground, reach }, with no words and no cue: keep
//     it out of the dispatches strip (the after card's log never has it). x, y: where it landed, the lost model's place
//     within block 'on': take off the model standing nearest it. armour: true where 'on' is tanks: it struck the tank
//     it knocked out, whose wreck zone (unit 'on', from the same second) lies at x, y: sparks and a bright scar on her
//     armour, then her brew-up, no earth crown and no crater. cal: the firing block's. by: the block that fired, left
//     out where the player does not know that block (b464(2)): then it is always guns firing from far back, unseen.
//     ground: the field's ground under it, a name from field.grounds, which picks the sheet's look: woods a tree burst;
//     buildings, bunker, road and bridge town and stone; marsh mud; water and blown the splash column; the rest earth,
//     or sand where place.terrain is desert, and snow or mud as the weather the 3D game shows on the field. reach:
//     metres from x, y within which men drop or are knocked flat and vehicles rock on their springs (the sheet's
//     "Things react"; men a little beyond it flinch and duck). It is the blast's, not the camera shake's radius, which
//     stays the sheet's: light 4, mortar 9, field 11, howitzer 15, heavy 23.
//   A crater for each shell on the ground (none on armour, none in the river), as a zone beside the wrecks: { id, kind:
//     'crater', x, y, r, until, at, cal }, r half the sheet's crater across for its class, ids -1, -2 and on, from
//     battle second 'at' to the end of the fight. Draw the sheet's scar for the ground there, smoking faintly for about
//     8 s from 'at', overlapping where hits overlap, never removed. Looks only: a crater gives no cover and slows no
//     one.
//   How they fit the mix plan's fireModel: the sims fight block against block with no single shots, so the plan times
//     each block's firing (guns every 6 to 20 s, the impact 2 to 5 s later at the block engaged; tanks' guns every 6 to
//     12 s). Those shots stay as they are, the near misses round the block engaged. A 'shell' event is the shot that
//     hit: land it at once at x, y, as fired before by block 'by' (fire no gun again for it), and let it stand in for
//     the next impact the plan has timed for that block, so the plan's pace holds. Its sound is the plan's impacts
//     (near, middle or far by the listener's distance, sized by impacts.bySize), and its incoming the plan's artillery,
//     mortar or tankGun by block 'by' (no 'by': artillery). A fight lands few shells (the six cold-check campaign
//     fights 0 to 5, the nine practice grounds 2.6 a fight, 6 at most, twenty river crossings 6.2 a fight, 27 at
//     most), one block's two seldom closer than 7 s apart (as little as 2.7 s in a river crossing), so the plan's
//     shots are most of what is seen and heard, and every model that falls to a gun or a tank falls under a shell. A
//     big blow could cost a block two models in one tick: then two land in the same second.
//   The browser page draws none of it (Brian: no look work in the browser).
// A tank block may also carry crew (b477, Brian's pick: tank crews name their tanks): { mark: 'name', names } or
// { mark: 'rings', rings }, one entry per tank in the block's model order. names[i] is { name, en } or null (that tank is
// plain); paint it from the model's entry under that name in battle-models/land/names-fit.json (its rows, in Russian
// letters for the Soviet lines, and letter height), on the place its file's name_place gives, and only on a model whose
// index-land.json crew_mark is 'name' (a light tank stays plain; the Type 97 waits until her file says ready). rings[i]
// is 1 to 5 thin off-white rings on that tank's barrel (crew_mark 'rings'); no count is ever shown on screen. The
// names fit the army's line tank already. A tank of another model (an M10, M7 or M8, or a British Sherman under her
// 'also.UK') takes the name only where her own 'fits' lists it; with no entry she is painted plain, never with the
// name shrunk or cut to fit (the modeller, 13:38Z).
// The block's hover may add one line naming them (crewLine in src/battle/land/crews.ts).
// Why an army broke (b496(2)): a large army breaks worn across its whole line, 13 in 100 of its power lost attacking,
// 18 defending, while every block's card may still read Steady, so the field gives the numbers for the words that say
// why. The words are the UI director's, and the field speaks them already: the 'broken' event's text is the break line
// ("Your army breaks: it has lost about 13% of its men along the whole line, as much as an attack will pay before it is
// called off, though every part of it was still steady."), the player's own 'nearBreak' text is the warning by what
// drives his bar, and after a rout over.lines[1] is the break line in the past, so the strength line is lines[2] and
// pursuit goes after it. The bar's hover: "When a bar fills, that army breaks and leaves the field. It fills with the
// men lost along the whole line, or with the share of it running or gone, whichever is nearer, so an army can break
// while every part of it is still steady."
// The fields come in four places: view.breakingBy[side] as each bar stands, the player's own 'nearBreak' event's
// brokeBy at the warning, the 'broken' event's brokeBy at the break, and view.over.brokeBy (and battleEnd's brokeBy)
// once it is over, where over.broke is set. They are BreakCause in src/battle/core/types.ts: { side, bar, by, nearer,
// worn, spent, running, breaks, fresh, fought, small, holding, held, heldFor, blocks, blocksWorn, wornAt, routing,
// fled, steady, stillSteady }. The bar fills when worn reaches spent (shares of the army's power: worn across the whole
// line, against what it will stand) or running reaches breaks (half its power running or fled); by names which ('worn',
// 'running', or 'none' where an army driven from the field reached neither), nearer which stands higher, reached or not
// (the warning goes by it). Worn to its mark, an army holds at the brink while fresh is more than fought: held says the
// hold had kept it there, heldFor for how many battle seconds, small whether its steady blocks counted as fresh (five
// blocks or fewer, not behind works). A small army held there gives ground: its blocks no longer fresh fall back off
// the field, each with a 'land:gives' event ("The 2nd Libyan Division gives ground."), and an enemy that comes on after
// them strikes only their rearguard. blocksWorn counts its blocks below wornAt (0.9) of their strength; steady is the
// share of its power in blocks reading Steady, and stillSteady says it in the UI director's words ('all', 'most',
// 'less').
// Under fog (the card did not know the defence, b453) the enemy's breakingBy is only { side, bar } until the battle is
// over: the rest would count blocks the player has not met.
// An air group also carries extra: { n, n0, band, alt, job, per } (aircraft now and at the start, height band 0 to 2,
// metres up, what it is doing, aircraft per model); /mnt/project-files/air-battles/3d-handoff.md says how to draw it.
//
// Sea and air plug in by the same rule (their threads send the diff): a start call of their own that builds their sim
// and hands it to begin() with how to settle it; tick, speed, order, view and end are shared.
//
// Fleet actions (src/battle/sea): the rules fight them while a month closes, not from a card, so they are found ahead:
//   TC.seaLook()                 -> { actions: [{ key, title, ticket }] }: the player's fleet actions the month's end
//                                   will bring, found on a copy of the game (call it only when the player has asked to
//                                   fight his fleet actions: it plays the computer's months once more on the copy)
//   TC.seaStart(key)             -> { field: { size, edge, zoneName }, view }: that action, opened paused
// then tick, speed, order, view as above (the sea's own orders: { do: 'sea:speed', units, speed: 'slow' | 'cruise' |
// 'flank' }; the carrier strike is the core's ability command with key 'strike'), and TC.battleEnd() -> { key, R,
// level, lines, sunk, burnt } once it is over. Nothing is booked yet: the played result waits until TC.endMonth()
// reaches that action, and stands in for the dice there (navy.ts seaField). An action not fought, or one that comes
// out of the month with other fleets in it, rolls the dice as before. Each sea unit's extra carries its class (cls),
// its ships afloat and sunk, and its fleet; 'loop' comes from src/battle/sea/cues.ts.

import { Clock, SPEEDS, type Speed } from '../../../src/battle/core/clock';
import { DROPS, LOOPS, TOWN } from '../../../src/battle/core/cues';
import type { BattleSim, BattleView, BreakCause, Command, FieldResult, Side, UnitBase } from '../../../src/battle/core/types';
import { GROUNDS } from '../../../src/battle/land/field';
import { cardKnows, fieldBill, landTicket, pursuitLine } from '../../../src/battle/land/ticket';
import { LandBattle } from '../../../src/battle/land/sim';
import { calFor } from '../../../src/battle/land/shells';
import { armourYear } from '../../../src/campaign/tech';
import { staffOnly } from '../../../src/battle/land/offered';
import { LAND, MOTION, type Kind } from '../../../src/battle/land/rules';
import type { LandUnit } from '../../../src/battle/land/units';
import { SKY } from '../../../src/battle/air/book';
import { AirSim } from '../../../src/battle/air/sim';
import { airTicket, foughtAir } from '../../../src/battle/air/ticket';
import type { AirOutcome, AirUnit } from '../../../src/battle/air/types';
import type { FoughtAir } from '../../../src/campaign/airfought';
import type { RaidKind } from '../../../src/campaign/airwar';
import type { BattleReport, Game } from '../../../src/campaign/types';
import type { FieldBill } from '../../../src/campaign/game';
import { seaField } from '../../../src/campaign/navy';
import { attackOutlook, endEve, endHuman, humanUp, onEve, stepAI } from '../../../src/campaign/turn';
import { SEA_LOOPS } from '../../../src/battle/sea/cues';
import { foughtOf, playedHook, seaAhead, type SeaAhead, type SeaPlayed } from '../../../src/battle/sea/handoff';
import { SEA } from '../../../src/battle/sea/numbers';
import { SeaSim, type SeaLosses } from '../../../src/battle/sea/sim';

export interface BattleHooks {
  game: () => Game;
  /** The campaign bridge's own attack call; with 'fought', the field's R stands in for the dice. */
  attack: (ids: number[], to: number, fought?: number, field?: FieldBill) => unknown;
  /** The campaign bridge's own raid order (act 'raid:kind:target:ids'); with 'fought', the air battle's losses stand in for the dice. */
  raid?: (kind: string, target: number, ids: number[], fought?: FoughtAir) => unknown;
}

interface Session {
  sim: BattleSim;
  clock: Clock;
  me: Side;
  /** How the campaign takes the result, once the battle is over. Called once. */
  settle: (r: FieldResult) => unknown;
  settled: unknown;
  /** A speed chosen before the armies met: the rush is over and that speed runs (the UI director, b467, as the page). */
  byHand?: boolean;
  /** The card's band where it did not know the defence (b453): the staff speaks only this, never the true odds. */
  band?: { lo: number; hi: number };
}

/** An air group's loop: its engines while it flies, nothing on the ground or once it is gone. */
function airLoop(a: AirUnit): string {
  return a.act === 'idle' || a.act === 'down' ? '' : SKY.cues.loops[a.air];
}

/** One unit as the 3D game reads it: plain fields only. Exported for the tests. */
export function unitOut(u: UnitBase): Record<string, unknown> {
  const land = u as LandUnit;
  const air = (u as AirUnit).air ? (u as AirUnit) : null;
  const motion = MOTION[u.kind as Kind] as string | undefined;
  const loops = motion && motion !== 'fixed' ? LOOPS[motion as keyof typeof LOOPS] : undefined;
  // A block fighting among buildings plays the town's street battle in place of the open field's bed, with the town's
  // machine guns dropped in; guns in action drop one far salvo at a time (the approved pack's field table).
  const town = !air && u.act === 'fight' && u.cover === 'buildings' && !!loops && motion !== 'guns';
  const drops = air ? undefined : town ? TOWN.drops : motion && motion !== 'fixed' ? DROPS[motion as keyof typeof LOOPS]?.[u.act] : undefined;
  const extra = air ? { ...(u.extra ?? {}), n: air.n, n0: air.n0, band: air.band, alt: Math.round(air.alt), job: air.job.do, per: air.per } : u.extra;
  return {
    id: u.id, side: u.side, kind: u.kind, name: u.name, x: Math.round(u.x * 10) / 10, y: Math.round(u.y * 10) / 10, face: Math.round(u.face * 1000) / 1000,
    str: Math.round(u.str * 1000) / 1000, mor: Math.round(u.mor * 1000) / 1000, state: u.state, figures0: u.figures0, figures: u.figures,
    w: u.w, d: u.d, act: u.act, speed: Math.round(u.speed * 10) / 10, engaged: u.engaged ?? -1, cover: u.cover ?? '',
    loop: air ? airLoop(air) : (u.extra as { cls?: string } | undefined)?.cls ? SEA_LOOPS[u.act] ?? '' : town ? TOWN.loop : loops?.[u.act] ?? '',
    ...(drops ? { drops: { takes: drops.takes, every: drops.every } } : {}),
    // A land block's name for a card and what it belongs to under it ("11th Infantry" over "I Corps"), beside its full name.
    ...(land.short ? { short: land.short, parent: land.parent } : {}),
    // A tank block's crews' own marks (b477): names, with their English meaning, or Germany's kill rings, per tank.
    ...(land.crew ? { crew: land.crew } : {}),
    // What its guns throw, as the art director's explosions sheet sizes it (src/battle/land/shells.ts).
    ...(land.cal ? { cal: land.cal } : {}),
    order: land.order ? { kind: land.order.kind, x: land.order.x, y: land.order.y, target: land.order.target ?? -1, path: land.path } : null,
    ...(extra ? { extra } : {}),
  };
}

/**
 * An enemy block the player has not met, where his card did not know the defence (b464(2)): where it stands and whether
 * it moves, as a commander sees the line he attacks; nothing that names, counts, arms or wears it.
 */
function hiddenOut(u: UnitBase): Record<string, unknown> {
  const off = u.state === 'gone' || u.state === 'withdrawn';
  const [w, d] = LAND.size.inf;
  return {
    id: u.id, side: u.side, known: false, x: Math.round(u.x * 10) / 10, y: Math.round(u.y * 10) / 10, face: Math.round(u.face * 1000) / 1000,
    w, d, act: off ? 'down' : u.speed > 0.05 ? 'march' : 'idle', speed: Math.round(u.speed * 10) / 10, cover: u.cover ?? '', state: off ? u.state : 'unknown',
  };
}

/** Each army's bar and what drives it, with the enemy's cut to its bar: under fog the player sees no more of it. */
function fogged(by: Record<Side, BreakCause>, me: Side): Record<Side, Partial<BreakCause>> {
  const them: Side = me === 'att' ? 'def' : 'att';
  return { [me]: by[me], [them]: { side: them, bar: by[them].bar } } as Record<Side, Partial<BreakCause>>;
}

function viewOut(s: Session, v: BattleView): Record<string, unknown> {
  const land = s.sim instanceof LandBattle ? s.sim : null;
  return {
    t: v.t, limit: v.limit, speed: s.clock.speed, units: v.units.map((u) => (land && !land.known(u as LandUnit) ? hiddenOut(u) : unitOut(u))), balance: v.balance, events: v.events, zones: v.zones, ai: v.ai, over: v.over, me: s.me,
    ...(v.breaking ? { breaking: { att: Math.round(v.breaking.att * 1000) / 1000, def: Math.round(v.breaking.def * 1000) / 1000 } } : {}),
    // What drives each bar (b496(2)): plain numbers, to three places already. Under fog, of the enemy only its bar
    // until the battle is over, as its blocks are hidden (hiddenOut).
    ...(v.breakingBy ? { breakingBy: land?.fog && !v.over ? fogged(v.breakingBy, s.me) : v.breakingBy } : {}),
    ...(v.contact !== undefined ? { contact: v.contact } : {}),
  };
}

/** Fast until the armies meet, then the speed the player set (ruling 226(1)); the browser page runs the same. */
const RUSH = 4;

export function battleCalls(h: BattleHooks) {
  let s: Session | null = null;
  const need = (): Session => { if (!s) throw new Error('No battle is being fought'); return s; };

  // Fleet actions the month's end will bring, and the ones the player has fought, waiting for the close to reach them.
  let seaAhead0: SeaAhead[] = [];
  const seaPlayed: SeaPlayed = new Map();
  // Left in place for the session: with nothing played it answers nothing, so every fleet action rolls the dice as before.
  seaField.fought = playedHook(seaPlayed);
  /** The month's end as the rules run it, on a copy: the player's close, then the computer's months to his next. */
  const closeMonth = (c: Game): void => {
    if (onEve(c)) endEve(c); else endHuman(c);
    for (let guard = 0; guard < 200 && !humanUp(c) && c.human; guard++) stepAI(c);
  };

  /** Any domain's battle on the shared clock: it opens paused, the player's side commanding. */
  function begin(sim: BattleSim, settle: (r: FieldResult) => unknown): Session {
    const clock = new Clock();
    clock.set(1); clock.toggle();
    s = { sim, clock, me: sim.ticket.att.human ? 'att' : 'def', settle, settled: undefined };
    return s;
  }

  return {
    battleLook(ids: number[], to: number) {
      const g = h.game();
      const corps = ids.map((i) => g.corps.find((c) => c.id === i)).filter((c): c is NonNullable<typeof c> => !!c);
      const t = landTicket(g, corps, g.regions[to]);
      if (!t) return null;
      // The safety net (src/battle/land/offered.ts): a kind of place the field still lands more than a tenth off its card
      // is settled by the staff; the card keeps FIGHT IT YOURSELF, off, with this one sentence beside it.
      const why = staffOnly(t);
      return why ? { ticket: null, why } : { ticket: t };
    },
    battleStart(ids: number[], to: number) {
      const g = h.game();
      const corps = ids.map((i) => g.corps.find((c) => c.id === i)).filter((c): c is NonNullable<typeof c> => !!c);
      const target = g.regions[to];
      const t = landTicket(g, corps, target);
      if (!t || staffOnly(t)) return null;
      const sim = new LandBattle(t);
      // A tank's shells by the model the game draws it with: its nation's design year where the campaign keeps one
      // (armourYear, as this game's DesignYears), not the calendar's. Looks only: nothing in the fight reads cal.
      for (const u of sim.units) if (u.kind === 'arm') { const c = calFor('arm', u.nat, t.turn, armourYear(g, u.nat)); if (c) u.cal = c; }
      // A defence the attack card did not know (b453), by the card's own outlook: its blocks show only where they stand
      // until one of the player's has fought them (b464(2)).
      const look = attackOutlook(g, corps, target);
      sim.fog = !!look && !look.exact;
      // All but the corps the card identified, which show as the card shows them (b476(3)).
      if (sim.fog) sim.seen = cardKnows(g, t);
      // The campaign is asked again at the end with the corps the player picked: it decides who goes in, as it did for the
      // card. Those going in are the ticket's own, so a corps spent since it was picked is not shown going (b453).
      const going = t.att.forces.map((f) => f.corps);
      const ses = begin(sim, (r) => {
        const out = h.attack(ids, to, r.R, fieldBill(r));
        // The army that broke, or pulled out under fire, booked more than the field cost it: the after card's line (b464, b474).
        const rep = (out as { report?: BattleReport | null } | null)?.report;
        const pursuit = rep ? pursuitLine(t, r, rep) : null;
        // Why the army broke, for the after card's words where it broke (b496(2)).
        const more = { ...(pursuit ? { pursuit } : {}), ...(r.brokeBy ? { brokeBy: r.brokeBy } : {}) };
        return out && typeof out === 'object' && Object.keys(more).length ? { ...out, ...more } : out;
      });
      if (look && !look.exact) ses.band = { lo: look.lo, hi: look.hi };
      const f = sim.field;
      return {
        field: { w: f.w, h: f.h, cell: f.cell, cols: f.cols, rows: f.rows, grounds: GROUNDS, ground: f.ground, height: f.height.map((x) => Math.round(x * 10) / 10), zones: f.zones, town: f.town, place: t.place },
        going, view: viewOut(ses, sim.view(ses.me)),
      };
    },
    battleTick(seconds: number) {
      const ses = need();
      const met = (ses.sim as Partial<LandBattle>).contactAt;
      const sp = ses.clock.speed;
      ses.clock.rush = met === null && !ses.byHand && sp > 0 && sp < RUSH ? RUSH / sp : 1;
      ses.clock.advance(ses.sim, seconds);
      return viewOut(ses, ses.sim.view(ses.me));
    },
    battleSpeed(speed: number) {
      const ses = need();
      const sp = (SPEEDS as readonly number[]).includes(speed) ? (speed as Speed) : 1;
      // Before the armies meet, a speed chosen ends the rush at once (b467); the pause does not.
      if (sp > 0 && (ses.sim as Partial<LandBattle>).contactAt === null) ses.byHand = true;
      ses.clock.set(sp);
      return { speed: ses.clock.speed };
    },
    battleOrder(c: Command) {
      const ses = need();
      return { ok: ses.sim.command(c, ses.me) };
    },
    battleView() {
      const ses = need();
      return viewOut(ses, ses.sim.view(ses.me));
    },
    battleChance() {
      const ses = need();
      const land = ses.sim instanceof LandBattle ? ses.sim : null;
      if (!land) return null;
      const r3 = (x: number): number => Math.round(x * 1000) / 1000, contact = land.contactAt !== null;
      return ses.band ? { lo: r3(land.chanceAt(ses.band.lo)), hi: r3(land.chanceAt(ses.band.hi)), contact } : { chance: r3(land.chanceNow()), contact };
    },
    battleEnd() {
      const ses = need();
      if (!ses.sim.over) return null;
      if (ses.settled === undefined) ses.settled = ses.settle(ses.sim.over) ?? null;
      const out = ses.settled;
      s = null;
      return out;
    },
    seaLook() {
      const g = h.game();
      const me = g.human;
      seaAhead0 = me ? seaAhead(g, closeMonth).filter((a) => a.ticket.place.fleets.some((f) => f.nat === me)) : [];
      return { actions: seaAhead0.map((a) => ({ key: a.key, title: a.ticket.title, ticket: a.ticket })) };
    },
    seaStart(key: string) {
      const a = seaAhead0.find((x) => x.key === key);
      if (!a) return null;
      const sim = new SeaSim(a.ticket);
      const ses = begin(sim, (r) => {
        seaPlayed.set(a.key, { meeting: a.meeting, fought: foughtOf(r) });
        const x = r.extra as unknown as SeaLosses;
        return { key: a.key, R: r.R, level: r.level, lines: r.lines, sunk: x.sunk, burnt: x.burnt };
      });
      return { field: { size: SEA.field.size, edge: SEA.field.edge, zoneName: a.ticket.place.zoneName }, view: viewOut(ses, sim.view(ses.me)) };
    },
    airLook(kind: string, target: number, ids: number[]) {
      const t = airTicket(h.game(), kind as RaidKind, target, ids);
      return t.ticket ? { ticket: t.ticket } : { ticket: null, why: t.why };
    },
    airStart(kind: string, target: number, ids: number[]) {
      const raid = h.raid;
      if (!raid) return null;
      const t = airTicket(h.game(), kind as RaidKind, target, ids);
      if (!t.ticket) return null;
      const sim = new AirSim(t.ticket);
      // The raid is ordered at the end with the groups the player picked, and paid with what the battle reports
      // (the campaign keeps it inside the dice's own run). Handed to the staff partway, the battle reports the whole raid.
      const ses = begin(sim, (r) => raid(kind, target, ids, r.extra?.air ? foughtAir(r.extra.air as AirOutcome) : undefined));
      return {
        sky: { w: SKY.w, h: SKY.h, scale: SKY.scale, target: SKY.target, flakR: SKY.flakR, bands: SKY.bands.alt, place: t.ticket.place },
        view: viewOut(ses, sim.view(ses.me)),
      };
    },
  };
}
