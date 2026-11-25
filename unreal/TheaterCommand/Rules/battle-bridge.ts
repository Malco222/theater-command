// The doorway between the played battles (src/battle) and the Unreal game.
//
// The battles are the browser's own TypeScript sims, run in the game by the same JavaScript engine as the campaign
// rules (Tools/build_rules.sh bundles this file with campaign-bridge.ts). Nothing here decides anything: it builds the
// battle from the campaign's own attack, runs the shared clock, passes the player's orders, hands plain pictures of
// the field back as JSON, and settles the battle through the campaign's own attack order with the field's R in place
// of the dice. The 3D game draws the pictures: the models, their walk and fire, the sounds the events name.
// It draws every explosion too, by the art director's sheet (land: "Explosions" below).
//
// Wiring (made in campaign-bridge.ts; kept here as the record of what it takes, and what to keep if it is redone):
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
// (campaign-bridge.ts passes both hooks through its playerOrder, so a fought attack or raid is noted as the player's
// own deed, in the notebook and the lessons, as the same order given from the map is.)
//
// The calls, on the same global object TC:
//   TC.battleLook(ids, to)       -> { ticket, live } for the attack the card shows, null where there is no field battle to
//                                   fight, or { ticket: null, why } where the staff settles that kind of place (offered.ts):
//                                   keep FIGHT IT YOURSELF on the card, off, with why beside it. live (release 2, the
//                                   general's battle fought in the 3D game's own land rules, the production lead's (a)):
//                                   { fog, seen, designYears }, the card's fog, the corps it identified and each side's
//                                   armour design year, as TC.battleStart sets them on its own field; the whole answer goes
//                                   as it stands to the land rules' TC.landStartTicket
//   TC.battleSettleLand(ids, to, result[, seed]) -> what TC.battleEnd returns, for a land battle fought in the 3D game's own
//                                   land rules from TC.battleLook's ticket: result is their TC.landResult answer ({ over })
//                                   or the FieldResult itself, settled through the same closure as TC.battleStart's battle
//                                   (the campaign's own attack order with the field's R, its bill, the pursuit line and
//                                   brokeBy). seed, the ticket's, where given: null and nothing settled when the attack the
//                                   campaign would make now is another battle ({ why }). Asked again with the same result,
//                                   the same answer, settled once
//   TC.battleStart(ids, to[, { setup: true }]) -> { field, going, view[, setup] }: the battle opens paused; field is the
//                                   ground, sent once ("The ground" below). With { setup: true } (release 2) it opens
//                                   on the setup step: the player places his guns and support teams before the clock
//                                   runs ("The setup step" below), and setup is { room: [x0, y0, x1, y1] }, where they
//                                   may stand. Two arguments: no setup step, as always
//                                   Direct command (release 2; Brian, 22 November): { setup: true } places every block of his
//                                   side, not only his guns and teams ({ do: 'land:place', units: [id], x, y, face? }); { staff:
//                                   true }: a block he orders is his (view units carry taken: true) and his staff fights the
//                                   rest ({ do: 'land:staff', units } hands blocks back); { radio: true }: his orders reach a
//                                   block a few seconds later, longer when its headquarters is hit (units carry orderIn, the
//                                   seconds left); an order may carry at, the battle time he gave it as shown, and the wait
//                                   counts from there (never before it). A 'move' with run: true goes at the double. Off unless asked
//                                   Hidden in the woods (release 2; Brian, 22 November 04:18Z, ambushes): { hide: true }: a block
//                                   in woods when the clock starts is hidden from the other side until an enemy comes close, it
//                                   opens fire, it is struck or it leaves the woods (LAND.hide). Units carry hid: 1 while hidden
//                                   from the enemy (his: draw the woods mark; the enemy's: draw nothing), in the setup step too
//                                   as he places them; a land:found event (extra.how: fired, seen, heard, out, struck, searched)
//                                   says when one is found. An attack or fire order at a hidden block is refused. The
//                                   historian's check (22 November): a block on a hedge line hides too (TC.battleHedges); one
//                                   found by its own fire carries vague: 1 till it is located (25 s, or an enemy close: draw
//                                   its fire, its men not yet plain); a tank moving in cover is heard at 350 m; fire on cover,
//                                   { do: 'land:fire', units, x, y } (any of his blocks, no target), makes them stand and fire
//                                   at that point (search: [x, y] on the block: draw their fire into the hedge or wood there)
//                                   and flushes out what hides within 60 m of it; the computer's attack does it too once it
//                                   has been ambushed. Orders by radio wait by each army's sets (LAND.radio.nations)
//                                   Measured armour (release 2; Brian, 22 November 04:03Z to 04:16Z): { armour: true }: a tank's
//                                   or gun's blow on a tank weighs what its gun gets through the face it strikes at that range
//                                   (data/battle/armour.json, the modeller's sheet), and the computer's tanks go round to the
//                                   flank of a tank whose front they cannot get through. Tank kill rounds carry fate (out, burn
//                                   or blow), by her armour and the face struck, whether or not it is on
//                                   Army names (release 2; the historian, 22 November): { names: true }: a regiment block's
//                                   short is its army's own word ("33rd Infantry Regiment", "Grenadier-Regiment 33", "32nd
//                                   Infantry Brigade", "Combat Command A") and its parent its division's full name
//                                   Uniforms (release 2, looks only): every land block in a view carries wear, 0.1 (fresh kit) to
//                                   1 (six weeks or more in the line since its corps was last rested or formed), its corps' own
//                                   from the campaign, pulled down by its share of fresh drafts; a practice battle's are fresh.
//                                   With it, fresh: the share of its men who joined this month (0 to 1). wear is the whole
//                                   block's; drawing that share clean (0.1), the rest read (wear - 0.1 x fresh) / (1 - fresh)
//   TC.battleCanPlace(id, x, y)  -> { why }: the setup step, while the pointer carries one of the player's guns or
//                                   support teams: null where it may stand (a brass ghost), else why not, in words (a
//                                   red ghost with the words beside it). Moves nothing (release 2)
//   TC.battlePlace(id, x, y[, face])-> { ok, why, view }: the setup step, on the drop: the gun or team put there (ok), or
//                                   why it may not stand there, and it stays where it was (release 2)
//   TC.battleBegin()             -> { view }: ends the setup step and starts the clock at ×1, the rush still on
//                                   (release 2). A speed chosen while placing (TC.battleSpeed) begins it too
//   TC.battleTick(seconds)       -> view: runs the clock for that much real time at the speed set. Until the armies first
//                                   meet (view.contact null) a land battle runs ×4 whatever the speed below that, then
//                                   drops to the speed set (ruling 226(1)): say so on the field, "×4 until the armies
//                                   meet", then "The armies meet · Running ×1" when view.contact turns to a time
//   TC.battleSpeed(speed)        -> { speed }: 0 (paused), 0.5, 1, 2 or 4
//   TC.battleOrder(command)      -> { ok }: one of the core's commands (src/battle/core/types.ts), for the player's side
//   TC.battleView()              -> view, without running the clock
//   TC.battleHedges(hedges, gates) -> { ok, hedges, gates, view }: hands the land field's hedges to the rules, before the clock
//                                   starts (false once it runs): hedges [{ kind, points: [[x, y], ...] }], gates [{ x, y, yaw,
//                                   width, kind }], metres from the field's south-west corner as the field's own x and y (the
//                                   hedge file's shape; lines wholly off the field are dropped). With { hide: true }, a block
//                                   on a hedge line hides as in woods (the historian's check, 22 November)
//   TC.battleMen()               -> { t, blocks, rounds }: the men inside the land battle's blocks, man by man, tank by tank, gun
//                                   by gun (src/battle/land/drill.ts, the squad brain; "The men" below); null for a sea
//                                   or an air battle. Call it after each battleTick. Looks only: the fight never reads it
//   TC.battleAirRun(x, y[, x1, y1]) -> { run, why } for the player's air support ("Air support" below): run, the strafing
//                                   run [x0, y0, x1, y1] a strike would fly if called where he points (x, y: down the
//                                   frontage of the enemy block he can see nearest the point), or down the run he drew
//                                   (all four); why, null where it can be called now, else the reason in words. Call it
//                                   as the pointer moves to show the run; the call itself is TC.battleOrder({ do:
//                                   'land:air', x0, y0, x1, y1 }) with that run
//   TC.battleChance()            -> { chance, contact }: the staff's chance now that the attack carries the ground, 0 to
//                                   1, and whether the armies have met, for HAND IT TO THE STAFF's question (ruling
//                                   226(5)); null where the battle has no such question (sea and air word their own).
//                                   Where the card did not know the defence (b453) it is { lo, hi, contact } instead:
//                                   the staff's band moved as the fight has moved it, "between about 40% and 65%",
//                                   never the chance at the true odds
//   TC.battleEnd()               -> what TC.attack returns ({ report, siege, note, price, paid, moved, view }), once the
//                                   battle is over; null before that (after an air battle: what TC.act returns for the raid,
//                                   its after card with lost: { att, def: { ftr, bmr } }, the sums of the card's rows).
//                                   Where the army that broke was booked more than the field cost it, also pursuit: one
//                                   line for the after card, under ON THE FIELD straight after the strength line (b464).
//                                   An army pulled out after contact pays the rules' bill for that defeat times its bar
//                                   squared where that is more than the field cost it; pursuit then says what the
//                                   rearguard and the stragglers cost, in the same place: "As the Polish army withdrew
//                                   under fire, it lost 4,644 more men in the rearguard and as stragglers." (b474)
//                                   Where an army broke on the field, also brokeBy: why it broke, as below (b496(2)).
//   TC.airLook(kind, target, ids) -> { ticket } for the raid the raid card shows, or { ticket: null, why } where it
//                                   cannot be fought out (src/battle/air FOUGHT_KINDS: 'bomb', 'superiority'; and the
//                                   kinds TC.airKinds has switched on)
//   TC.airKinds([kinds])         -> { kinds }: release 2's further kinds of air battle, each behind its own switch, all
//                                   off until switched on here (src/battle/air/ticket.ts MORE_KINDS; today 'ground', a
//                                   raid on troops). With a list, those kinds are offered from then on (an empty list
//                                   switches them all off); with none, it only answers which are on. A raid on troops
//                                   carries its troops on the ticket (place.marks) and each view (marks: { fire, cut,
//                                   pieces }, the pieces changed since the view before); its events 'air:mark-hit' and
//                                   'air:strafe' (/mnt/project-files/air-battles/every-kind/CONTRACT.md sections 2 to 4).
//                                   Its own order: TC.battleOrder({ do: 'air:strafe', units }) sends fighters down on
//                                   the troops now, { do: 'air:strafe', units, hold: true } keeps them up with the bombers
//   TC.airStart(kind, target, ids) -> { sky, view }: the air battle opens paused; sky is the board, sent once
//                                   ({ w, h, scale, target, flakR, bands, place, title, date, att, def }: att and def
//                                   { nat, adj, human }). Each view of an air battle also carries sky: { chance, fired,
//                                   winding, near: { att, def: { at, why } }, score, called, me } (AirFeed.cpp ReadView)
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
// A bold defence's reserve walking toward the player's attack (the 'shift' phase below) raises one event, once a battle,
// { what: 'land:reserve', unit, side } at the player's own block it walks toward ("Your 3rd Armoured reports movement
// behind the enemy line, shifting toward it."; that block reports what it sees, so it tells nothing the fog hides, b464;
// words the UI director's).
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
// then smoking to the end, never cut off (the art director's b477 look). With the men drawn (TC.battleMen), draw her
// where her tank fell, her last x, y there (the 3D ground battle scene, 21 November 20:35Z): she falls where she
// stands, at a firing post up to about 20 m from this x, y or held back behind her own foot in woods, and a shell that
// knocked her out lands on her there; this x, y is the rules' own place for the shelter the wreck gives. Men on foot within r metres take light cover
// in the rules already; nothing more to draw for it. The zones also hold its craters (below).
// The ground (Brian, 18 November: "make sure the battle terrian matches the map in the world so it matches. if are
// attacking in hills than have that or a forrest or towns ect"): field is built from the province on the campaign map
// (src/battle/land/field.ts): its terrain, the river between the armies and its bridges, its town and the town's size,
// its fortress works. { w, h, cell, cols, rows, grounds, ground, height, zones, town, place, storeys?, landmark? }:
// ground[r * cols + c] is an index into grounds, row 0 at the attacker's (south) edge, height that cell's metres. A
// capital or a city is streets (the design director, b511; the art director's rings, 17:23Z): a capital's core of
// blocks fills the defender's part of the field with suburbs back to the attacker's start, about 70 per cent built; a
// city's core is about 800 by 600 m round its centre, suburbs to mid-field, about a third built. Its grounds:
// 'buildings' (the core's blocks and the terraces), 'road' (its avenues too), 'suburb' (houses and gardens, open ground
// between), 'industry' (sheds, yards, chimneys), 'park' (squares and parks, open with trees). storeys[i]: the houses'
// storeys on a built cell (3 to 5 in a core, 2 in terraces and industry, 1 or 2 in suburbs, villages and towns), 0
// elsewhere: draw the core tight, tall and shadowed, the suburbs low, green and open (the art director). landmark:
// { x, y, kind } with kind 'spire', 'dome' or 'station', one a city, at its heart; draw it generic, no real building.
// The region's own building style (brick in the north and east, stone and plaster in the south, timber further east)
// is the 3D game's to pick from place and the nations, where it has the sets. Looks only beyond the grounds' cover.
// A capital's or a city's own river through its streets (the design director, b526; the art director's 18:53Z note):
// stream, where the city has one on the hand list (data/battle/city-rivers.json) and the card prices no crossing:
// { name, lie, side?, strip?, great, half, course, join?, front? }. Its water is 'water' and its bridges 'bridge' in ground as on every field,
// its road bridges as many as the city had in the war years (b596, the historian's record), on the avenues nearest the heart,
// each laid straight across the water; every other avenue that meets it ends at its quay. lie 'up': it runs up the field through the
// core, along the attack's line; 'across': through the core's back half behind the defence, the city's far quarter
// beyond it; 'shore': along the city's edge at one side of the field (side 'west' or 'east'), its water out to the field's
// edge, or, where its water lies straight ahead of the attack, along the field's back edge behind the defence (side
// 'back', the Volga at Stalingrad's back; it keeps behind front as a river across does; strip: its city is laid as a strip
// along it, the field's whole width and about 800 m deep, the art director's 19 Nov Stalingrad). course: its
// middle line in field metres from upstream to downstream (the way its water runs), and half the metres either side of
// that line that are water (a little more and less as it goes; ground has the cells); a shore's course is its bank,
// half 0. join: a second river meeting it (the Sava at Belgrade), { name, half, course } from the field's edge to the
// meeting, with its own bridges by the record: up the field, from its real side behind the defence's front; across, from
// its side edge to the heart's corner (b596(2)), beyond the defence's flank and never between the armies, the defence
// drawn up on the heart's side of it. A second river is about half the first one's width. front (lie 'across', and a shore along the back edge): the defence's front line in field metres north that the river keeps behind; the
// rules' own, nothing to draw. Draw a city river as a river in a city: stone quays and embankments where the core meets the water, the
// bridges stone or steel to the region's style, nothing named after a real bridge. A bridge's deck runs across the
// water, square to course at that point. great: wider than about 250 m at the real city (the Danube at Budapest).
//
// Explosions (Brian, 18 November: "very dramatic and life like"; his own look, copying no game's effects): the 3D game
// draws every burst, its flash and light, shock, earth crown, debris, smoke, the camera's jolt and the scar it leaves,
// by the art director's sheet (/mnt/project-files/art-director/explosions/explosions-look.md) and the ground under it.
// The rules give it what it cannot work out for itself (src/battle/land/shells.ts); none of it moves the fight:
//   cal, on a land unit: what its guns throw, as the sheet's size classes: 'mortar' 81 or 82 mm, 'field' 75 or 76 mm,
//     'howitzer' 105 mm, 'heavy' 150 or 155 mm, and 'light' for a 37 to 57 mm tank or anti-tank gun (the sheet has no
//     row for it yet: draw it at its hand grenade row). Guns by the model the unit table draws them with in the
//     battle's year, tanks in the year their nation's designs have reached (DesignYears; armourYear in
//     src/campaign/tech.ts, set at TC.battleStart), so a tank's blast is the gun it is drawn with; foot, mountain and
//     lorried infantry 'mortar', their own battalions' (they land no shells: their blows count as small arms);
//     cavalry, engineers and fortress troops carry no cal. The mix plan's
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
//     buildings, suburb, industry, bunker, road and bridge town and stone; marsh mud; water and blown the splash
//     column; the rest (park too) earth, or sand where place.terrain is desert, and snow or mud as the weather the 3D
//     game shows on the field. reach:
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
//     fights 0 to 5, the nine practice grounds 2.8 a fight, 6 at most, twenty river crossings 5.8 a fight, 24 at
//     most), one block's two never closer than 7 s apart in those fights, so the plan's shots are most of what is
//     seen and heard, and every model that falls to a gun or a tank falls under a shell. Which striker a model falls
//     to is the share rule (the design director, b507): each block keeps what every striker has done to it over the
//     fight, and a model it loses goes to the striker it owes most, so guns firing from far back, whose blows are
//     light, still land their shells in time (a quarter of the practice grounds' shells are the guns'). A big blow
//     could cost a block two models in one tick: then two land in the same second.
//   The browser page draws none of it (Brian: no look work in the browser).
// The men (Brian, 2026-11-20 20:43Z: "lets make this super realistic and meaningful and everyone has high AI iq"; the
// art director, 17 November 17:56Z: "no tray ... the men move, kneel and take cover as men"). TC.battleMen() -> { t,
// blocks, rounds }, each block { id, ph, men, fo?, foWith?, foOn? }: what every man, tank, gun and lorry of the block is doing
// now, from the squad brain (src/battle/land/drill.ts). Looks only (ruling 217): the fight never reads any of it, so it
// never moves a result (tests/battle-land-drill.test.ts proves a battle ends the same with it on or off).
//   A man: { k, r, x, y, f, p, v, c, a?, s?, d?, h?, b?, mk? }. k his model on the block's tray (lorries 100 up, the spotter
//   200 and his signaller 201); r his role: officer, rifle, mg (light machine gun team), at (anti-tank rifle or
//   launcher), sapper, holder (cavalry horse holder), tank, gun, lorry, fo; x, y field metres; f facing (radians, 0
//   north, clockwise, as a block's face; in (-pi, pi]: turn the short way round); p the pose; v the metres a battle
//   second he went over the ground in the last step, 0 in every still pose (play the gait's clip at v over the clip's
//   own speed, capped at 1.6 times: the battle runs faster than life, and blocks march at up to 6 m a battle second); c the cover he is in (none, works, building, woods, rock, rough, field, wreck, crater, crest); a
//   where he fires, while he fires; s the battle time of his last shot (a tank's or a gun's round, an anti-tank round,
//   a grenade): play one shot each time it changes; d the battle time he fell (he lies where he fell to the end of the
//   fight; a tank burns where she fell, never moved to her wreck zone's x, y; the men of a block that runs off the field go with
//   it and never fall: they leave the list with it, the moment its middle crosses the edge, so its trailing lorries and
//   the men in them can still be up to about 130 m inside: carry each on at its last v and f and fade it out over a
//   second rather than drop it); h an anti-tank team lying hidden in ambush (draw it, it is the player's own or a known
//   enemy's); b a tank closed up (hatches shut), or a gun firing over open sights; mk a vehicle's scars (below).
//   Full sections (release 2; Brian, 21 November 18:29Z: "the ground battle needs a lot more soldiers in it"): the bridge
//   runs the drill with { sections: true }, so each model on the tray comes as its whole section. Its key man is the
//   model the rules know (no j); every other man has j, his place in model k's section from 1, so a man is (block, k, j).
//   Roles, by data/battle/drill.json sections: a rifle section is its leader (r leader, the key man), an automatic rifle
//   or light machine gun (mg), his loaders (loader) and riflemen, the US with two scouts out ahead on the move (US 12
//   men, Germany and Britain 10); an officer has three runners (rifle); an mg team two loaders and a rifleman; an
//   anti-tank team a loader; sappers seven more; a horse holder two; a gun seven crew (crew) round it; a machine-gun or
//   mortar team four or five bearers (loader) besides the three drawn with the weapon. Tanks and lorries have none. As
//   the rules wear a model down its section's men fall one by one, each by his own kill round (j the shooter's man, oj
//   the man struck; a shell's falls share one burst, x and y); the key man falls only when the rules take the model, and
//   the section's men still up join the nearest standing section of the block. Halted in a fight, a tank of every army
//   but a mass one fights from alternate posts: two to four shots, back 12 m out of sight (reverse), across behind the
//   line, and up at a post 16 m to one side of her place and then the other. A tank drives no faster than 12 m a battle
//   second (the rules' armour goes at 10 across open ground) unless her block itself goes faster (13 on a road), and on
//   the move she halts to fire only for a shot, and only while she keeps up with her place; from a halt she gathers way
//   at 3 m a battle second each second (to 10 in about 3 s). The battle runs about twice as fast as life: played at half
//   its clock a tank goes 5 m a second across a field and backs at 2, a rifleman rushes at 3. Living men on foot keep
//   at least 1.6 m apart (a gun's crew 1.1 m at their own gun): a man whose place is taken stands at the nearest free
//   spot (creeping, or crawling if he lies low); a man on the move bends his way round a mate ahead of him, and where
//   two men's ways cross the lesser (a loader for his gunner) checks his pace to let the other by. Each battery's
//   spotter party takes a place of its own, 12 m or more from another's. Men of different blocks are not spaced.
//   Platoons (release 2; Brian, 22 November: "I want a lot of troops in the ground battles"): the bridge runs the drill
//   with { platoons: true } too, so each rifle model is its whole platoon by its army (data/battle/drill.json
//   sections.platoon: the US three squads of 12 and five at its head, 41; Germany and Britain three of 10 and seven, 37;
//   the rest by their own). The key man's squad leads; the platoon's other squads form one behind another 9 m apart, a
//   little to either side, its headquarters inside the leading squad's wedge. Each squad and the headquarters has its
//   own leader (r leader, with j), and j runs on through the platoon (a German platoon's key man has j 1 to 9 in his own
//   squad, the next squad 10 to 19, the third 20 to 29, the headquarters 30 to 36). Gun crews, support teams, anti-tank
//   teams, sappers, an officer's party and tanks stay as sections. Lorried, each squad and the headquarters has a lorry.
//   Normandy draws about 2,000 men (690 as sections).
//   Every round (Brian, 21 November 04:04Z: "make the right odds if they actually hit so it not always an automatic hit
//   ... when they do hit ... it shows up on the receiving object"): rounds lists those fired in the last two battle
//   seconds, each { n, t, l, by?, k?, w, on, ok?, x, y, r, fc?, u?, h? }: play each once, those with n above the last
//   you played. Book each the first time you see it: a long flight (up to about 7 s) can leave the list before l.
//   Fired at battle time t by model k of block by (no by or k: a shooter the player has not seen) with w (tank,
//   gun, at, mg, rifle, grenade) at block on; lands at x, y at battle time l (where what it was fired at will be by
//   then, going on as it goes); r what it did: miss (landed there, near what it was fired at, never on a vehicle of
//   any block nor on a wreck: dirt, a splash of earth, a ricochet), glance (struck armour or a gun's
//   shield and bounced: sparks, and on a tank a bright gouge; small arms on a tank only rattle it), worn (went in short
//   of a kill: a tank holed, a lorry shot through) or kill (the round the rules took that model to: a man falls, a tank
//   is knocked out; it lands as it is fired). ok the model it struck (glance, worn, kill), on its face fc (front, left,
//   right, rear, top) at u across that face (-1 its left edge to 1 its right) and h up it (0 the ground to 1 the top;
//   over 0.65 a tank's turret). A round never strikes a man short of a kill: the rules decide who falls. The odds are
//   the drill's (data/battle/drill.json rounds: range, a moving shooter or target, cover and hull-down, the crew), so
//   most rounds miss. A gun firing far lands round its aim (the spotter's), on a tank's top where it falls on one.
//   mk, on a tank, a gun or a lorry: its scars, [face, u, h, kind, from], kind gouge (a bounce), hole (a round that went
//   in), scorch (a shell's blast on armour) or pocks (bullet holes), shown from battle time from to the battle's end
//   (12 at most). With rounds, play the men's shots from them, not the mix plan's timed near misses.
//   A tank's machine-gun round (w mg from a tank block) names its gun in g: coax (beside the main gun), hull (the
//   bow gunner's) or roof (the commander's, up in his hatch: only the armies that carried one, and never buttoned up).
//   cm: 1 where a round struck a tank's commander in his hatch: he drops inside, and she fights buttoned up from then
//   (b on her men). fate, on a round that knocked out a tank (r kill): out (she stops, crew bail out), burn (she
//   catches fire) or blow (she burns and her ammunition goes up), by her measured armour and the face struck (a hit in
//   a German tank's rear burns and blows up more often). Looks only: draw it; where a kill round has none, draw your own.
//   Poses, as the soldier's clips (battle-models/land/soldier/README.md): idle, walk, run, advance, creep, crawl, flee,
//   fire_stand, fire_kneel, fire_prone, lmg_burst, throw_grenade, cower play as named; wait is kneeling, rifle up,
//   watching (fire_kneel's aim held, no shot); hide is lying still in cover (fire_prone's aim held); observe a spotter
//   lying with his glasses up (fire_prone, rifle lowered); mounted sits in a lorry; ride is on a horse (walking or
//   standing by v); horses a holder leading horses; down a fallen man (die_back, die_forward or die_kneel once, by
//   k, then the body). Tanks: drive, halt, fire (a round by s), reverse; guns: towed, deployed, fire; lorries: drive,
//   parked, halt. Blend clip changes over a fifth of a second. The motion is smooth as sent (the 3D pre-check's
//   every-step sweep, 20 November 22:35Z): no man goes more than 0.9 m a step (a tank or a lorry 1.8 m) unless his
//   block itself is pushed faster, and then no further than it went; no one turns more than 0.8 rad a step (a tank
//   0.08: she pivots on the spot, drive with v 0 while f turns, and backs into a place behind her, reverse). Lerp
//   between ticks. Lorried men start mounted and sit in their lorry (x, y their seat in its bed, within 2 m of the
//   lorry's own x, y; f the lorry's) until they jump down; cavalry ride, then get off to fight, a holder leading the
//   horses at the rear of the block's ground. Down, when the rules drive their block on at lorry or horse pace for six
//   seconds, they mount again (the 3D pre-check, 21 November 00:33Z: men on foot never outrun a man): the lorries come
//   up and halt, the holders bring the horses up and halt; a man waits kneeling (wait) while his lorry or holder is
//   still far off and coming, else runs to it and, once it has halted, climbs in (mounted) or swings into the saddle
//   beside it (ride); the block rides on once all are up. A block the rules only jostle in a fight is not going on: the
//   men stay down. A man on foot never goes more than 0.9 m a step; a man mounted no further than his lorry (1.6 m: she
//   slows in a turn). At the field's edge a lorry or a tank halts (parked, halt); nobody marches on the spot.
//   A block's phase ph: column, deploy, bound (fire and movement under fire), startline, fight, hold, rest, pinned,
//   fallback, flee, rally, gone, battery, limbered: for the camera and the sound (pinned and fight are loudest).
//   Fog (b464(2)): an enemy block the player does not know sends no men; draw its plain stand as now. The battery's
//   spotter (fo) goes up with the friendly foot block foWith when its guns first fire at block foOn from further than
//   500 m, and the pair stays out to the battle's end (foOn absent while the guns are quiet): a new foWith or foOn
//   sends the same pair there on foot, so it moves, never jumps.
// Air support (Brian, 2026-11-18 18:58Z: "during ground battles where fighters are in range, you should be able to call
// in air support where the fighters will strafe and drop bombs"; the design director, b533 and b536). A side whose
// fighter groups are on ground support that month and reach the place comes on with fighters on call: the battle card
// counted them in the side's strength, and on the field part of the side's blows is held back for them and paid where
// the strikes land, never more than the card counted (b418). Nothing in the view changes where no side has any: no
// 'flights', no 'air', no events below.
//   view.air: { att?, def? }, one entry per side with fighters on call: { ready, left, nextIn, eta }. ready 1 when a
//     strike can be called now; left, the strikes not yet flown; nextIn, battle seconds until the next is ready (null
//     when ready or none left); eta, seconds until the strike called is over the field (null when none is on the way).
//     None before the armies meet (TC.battleAirRun's why says "After the armies meet"). A strike left ready 20 s is
//     called by the staff, so a player who forgets his planes is never short of them. The enemy's are called by its
//     staff by the same rule, never aimed sharper.
//   The button: "Air support" in the field's top bar where view.air[me] is set, its state beside it ("Ready · 2 left",
//     "Ready in 0:31 · 2 left", "Over the field in 0:07", "All flown", "After the armies meet"); then the player points at the
//     ground and the run shows as a band 60 m wide, brass where it can be flown and red where TC.battleAirRun's why is
//     set ("Too close to your own men", where his men are under too much of it; the field's tag says it); a click calls it. The browser page's
//     battle page does all of this with plain marks: copy its behaviour, not its look. The words are the UI director's
//     (21:18Z review).
//   view.flights: [{ id, side, nat, year, models, x, y, alt, face, phase, run }], each flight two to four fighters drawn
//     as one picture (models), never a unit: nothing on the field fires at it or is hidden by it. x, y field metres,
//     alt metres above the ground, face radians as the units'. phase 'in' (flying to the start of its run), 'run'
//     (strafing down run [x0, y0, x1, y1] at alt LAND.air.low, 60 m) or 'out' (pulling up and away, climbing to 150 m
//     and off the field). Draw nat's own fighter (battle-models/air: every nation has one; 62 fly the plain Fighter)
//     at year's look; fly the fighter's 'fly' clip, 'bank.port' or 'bank.starboard' on the turns and 'peel_off' as
//     the run ends; between view ticks, move it on its own heading at about 120 m/s.
//   Events (all with side, t, and x, y where they happen):
//     'land:air-call' { flights, runs, eta, by: 'you' | 'staff' } when a strike is called; x, y the first run's start.
//       Its flights are in view.flights from then, phase 'in', eta seconds out. Text on the player's side when his
//       staff called it: "The staff called in the fighters."
//     'land:air-in' { flight, run, models, alt, nat, year, heading }, cue 'fighter_pass': the flight is over the start
//       of its run and drops onto it. Its engines are the 'fighter_loop' bed while it is in view.flights.
//     'land:strafe' { flight, from, to, width, secs }, cue 'machine_gun', at the same moment: the guns open down the
//       run from 'from' to 'to', 60 m wide, for secs seconds: the fighter's 'fire' clip and its muzzles gun.1..N, the
//       strike of the rounds walking down the strip as the flight flies it. No words.
//     'land:strafed' { flight, on } as the run ends: 'on' lists the blocks it struck that the player can see. Its text,
//       on the player's side: "Your fighters strafed the 4th Rifle Division." ("strafed and bombed" where the run let
//       bombs go; or "the enemy line", or "found nothing under their run"); on his own men: "Soviet fighters strafed
//       your 1st Panzer Division." No cue.
//     'land:bomb-drop' { flight, from: { x, y, alt }, to: { x, y }, at }: a bomb let go from the flight's 'bombs'
//       socket, two a run, from the year its nation first flew fighter-bombers (the historian's table: Germany, the
//       Soviets and Italy 1941; Britain and America 1942; Japan and France 1943; Poland never; others 1942); before
//       it the run strafes alone. Spawn the 'Bomb' prop (socket 'nose', clip 'fall', 6 s) about 1.2 m either side of
//       the belly at 'from' and fly it down to 'to' by battle second 'at' (LAND.air.toss, 1.5 s), sound
//       'bombs_falling_01' or '_02'.
//     'shell' with cal 'bomb' { extra: { flight } } at 'at': the burst where it lands, the 250 kg row of the
//       sheet (reach 45 m), sound 'bomb_single_01' or '_02'. It leaves a 'crater' zone like a shell's. 'on' and
//       'armour' only for a block the player knows; over one he does not, it bursts as on its ground. A bomb costs no
//       model of its own: it is a picture of the strike's blow, which the field pays with the run.
//     'air:lost' { flight, kind, chutes, over, air: 'ftr', alt }, cue 'plane_down', text on the player's side ("Ground
//       fire brought down one of your fighters." / "Your men brought down a Soviet fighter."): one fighter of the flight
//       falls as the strike pulls away, for each whole plane the campaign's price for a strike comes to (never the dice):
//       kind 'burn', 'spin' or 'explode' (its clips 'smoking', then 'fall_burn', 'fall_spin' or 'fall_explode');
//       chutes, how many 'bail_out' (0 or 1). Brian, 19:33Z: "if a plane gets hit show the bullet holes or flak holes":
//       before 'smoking', lay her <Name>Damage model on her and hold 'damage.flak' at 2 or 3 (ground fire;
//       battle-models/air/damage-handoff.md); over 'own' or 'enemy', the side of the field it comes down on.
//   over.air (and the view's over once the battle is over): { att?, def? } with [{ group, sorties, lost }] per group:
//     every group on call flies every strike, as the campaign books it; handed to the staff, the strikes not yet flown
//     count as flown (the staff fly them in the settlement). The campaign books these as flown (FieldBill.air).
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
// The computer general's plan (the tactics panel, item 1; ruling 239): a land view carries plan: { att?, def? } for
// each side the computer commands with a style, { phase, main, startY }: phase 'guns' while its guns fire on the main
// effort and its foot waits at the start line, startY metres north (the foot down there, the shells on the main
// stretch), 'assault' once it goes in (the fire lifts), 'hook' once its tanks wheel round the main target's flank; main
// lists the enemy blocks of the main effort, the main target first (empty for a mass, which goes in all along the line).
// A defence has one phase, 'shift' (ruling 239, rider 03:20Z): while its line is not yet struck, a bold defence walks its
// held block sideways along its own row, behind its line, toward the stretch the attack is coming at, up to 290 m from
// home; reserve lists those blocks (main empty, startY null). Draw the march behind the line for the blocks the view
// carries (an unseen block is not in it); the phase ends when the line is struck and the block is sent as any reserve.
// For pictures only: never its style's name, which the player is not told (ruling 239(4)). A gun block told to fire on
// one block and stay put carries aim (that block's id): the computer's main effort, or the player's own order
// { do: 'land:fire', units, target } (ruling 239(6); its words are the UI director's). A tank block paying for having
// left its foot on close ground (item 7) carries alone: true while it pays: buttoned up, close-assault teams at it.
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
// A sea view's zones are the ships it has lost (the art director's grit direction, 19 Nov: black oil smoke from
// burning ships, standing to the end of the fight): { id, kind: 'sunk' | 'burning', x, y, r, until, side, at, unit },
// one for each ship lost, at her place in her piece's line, from battle second 'at' to the end. 'sunk' is the oil
// burning on the water where she went down: flame first, then a tall black column. 'burning' is where she left the
// line on fire for port: her smoke trailing off. Lean every column the same way with one wind; r is the oil's reach in
// metres. The rules never read them. The horizon's far columns, the camera, spray, debris and grade are the 3D game's.

import { Clock, SPEEDS, type Speed } from '../../../src/battle/core/clock';
import { DROPS, LOOPS, TOWN } from '../../../src/battle/core/cues';
import type { BattleSim, BattleView, BreakCause, Command, FieldResult, FieldTicket, Side, UnitBase } from '../../../src/battle/core/types';
import { GROUNDS, type LandPlace } from '../../../src/battle/land/field';
import { cardKnows, fieldBill, landTicket, pursuitLine } from '../../../src/battle/land/ticket';
import { LandBattle, type Gate, type Hedge } from '../../../src/battle/land/sim';
import { calFor } from '../../../src/battle/land/shells';
import { armourYear } from '../../../src/campaign/tech';
import { staffOnly } from '../../../src/battle/land/offered';
import { LAND, MOTION, type Kind } from '../../../src/battle/land/rules';
import type { LandUnit } from '../../../src/battle/land/units';
import { STORIES } from '../../../src/campaign/stories';
import { SKY } from '../../../src/battle/air/book';
import { AirSim } from '../../../src/battle/air/sim';
import { airTicket, flyKinds, foughtAir } from '../../../src/battle/air/ticket';
import type { AirOutcome, AirUnit } from '../../../src/battle/air/types';
import { afterLost, type AfterCard, type FoughtAir } from '../../../src/campaign/airfought';
import type { RaidKind } from '../../../src/campaign/airwar';
import { airAct } from '../../../src/campaign/airpages';
import type { BattleReport, Game } from '../../../src/campaign/types';
import { groundWeather, wreckLook, type FieldBill } from '../../../src/campaign/game';
import { seaField } from '../../../src/campaign/navy';
import { attackOutlook, endEve, endHuman, humanUp, onEve, stepAI } from '../../../src/campaign/turn';
import { seaLoop } from '../../../src/battle/sea/cues';
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
/** A block whose corps has no time in the line on record (a practice battle's, or fresh): new uniforms. */
const WEAR_FRESH = STORIES.wear.fresh;

export function unitOut(u: UnitBase, t = 0): Record<string, unknown> {
  const land = u as LandUnit;
  const air = (u as AirUnit).air ? (u as AirUnit) : null;
  const motion = MOTION[u.kind as Kind] as string | undefined;
  const loops = motion && motion !== 'fixed' ? LOOPS[motion as keyof typeof LOOPS] : undefined;
  // A block fighting among buildings (a city's suburbs and industry too) plays the town's street battle in place of the
  // open field's bed, with the town's machine guns dropped in; guns in action drop one far salvo at a time (the approved
  // pack's field table).
  const town = !air && u.act === 'fight' && (u.cover === 'buildings' || u.cover === 'suburb' || u.cover === 'industry') && !!loops && motion !== 'guns';
  const drops = air ? undefined : town ? TOWN.drops : motion && motion !== 'fixed' ? DROPS[motion as keyof typeof LOOPS]?.[u.act] : undefined;
  const extra = air ? { ...(u.extra ?? {}), n: air.n, n0: air.n0, band: air.band, alt: Math.round(air.alt), job: air.job.do, per: air.per } : u.extra;
  return {
    id: u.id, side: u.side, kind: u.kind, name: u.name, x: Math.round(u.x * 10) / 10, y: Math.round(u.y * 10) / 10, face: Math.round(u.face * 1000) / 1000,
    str: Math.round(u.str * 1000) / 1000, mor: Math.round(u.mor * 1000) / 1000, state: u.state, figures0: u.figures0, figures: u.figures,
    w: u.w, d: u.d, act: u.act, speed: Math.round(u.speed * 10) / 10, engaged: u.engaged ?? -1, cover: u.cover ?? '',
    loop: air ? airLoop(air) : (u.extra as { cls?: string } | undefined)?.cls ? seaLoop((u.extra as { cls?: string }).cls, u.act) : town ? TOWN.loop : loops?.[u.act] ?? '',
    ...(drops ? { drops: { takes: drops.takes, every: drops.every } } : {}),
    // A land block's name for a card and what it belongs to under it ("11th Infantry" over "I Corps"), beside its full name.
    ...(land.short ? { short: land.short, parent: land.parent } : {}),
    // A tank block's crews' own marks (b477): names, with their English meaning, or Germany's kill rings, per tank.
    ...(land.crew ? { crew: land.crew } : {}),
    // Direct command (release 2): a block the player has taken in hand, and the seconds till his order on the air reaches it.
    ...(land.taken ? { taken: true } : {}),
    // Hidden in the woods from the enemy (release 2, LAND.hide); found by its fire but not yet located; firing on cover.
    ...(land.hid ? { hid: 1 } : {}),
    ...(land.unloc !== undefined && t < land.unloc ? { vague: 1 } : {}),
    ...(land.search ? { search: [Math.round(land.search[0]), Math.round(land.search[1])] } : {}),
    ...(land.orderAt !== undefined ? { orderIn: Math.max(0, Math.round((land.orderAt - t) * 10) / 10) } : {}),
    // How worn its uniforms and kit look (release 2, looks only): 0.1 fresh to 1 after six weeks in the line; and the
    // share of its men new this month, in new kit (0 none), which 'wear' counts in.
    ...(land.short ? { wear: land.wear ?? WEAR_FRESH, fresh: land.fresh ?? 0 } : {}),
    // What its guns throw, as the art director's explosions sheet sizes it (src/battle/land/shells.ts).
    ...(land.cal ? { cal: land.cal } : {}),
    // A tank block that has left its foot on close ground and is paying for it this second (item 7): buttoned up,
    // close-assault teams at it.
    ...(land.aloneAt !== undefined && t - land.aloneAt <= 1 ? { alone: true } : {}),
    // Guns told to fire on one block and stay put (the computer's main effort, or the player's order): that block.
    ...(land.aim !== undefined ? { aim: land.aim } : {}),
    // A support team (release 2): kind 'art' with its team, 'mg' (tripod machine guns) or 'mor' (mortars). Draw the
    // team's models, never a field gun.
    ...(land.team ? { team: land.team } : {}),
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
    ...((u as LandUnit).hid ? { hid: 1 } : {}),
  };
}

/** Each computer attack's plan as the 3D game shows it: phase, main stretch, start line; no style's name. Exported for the tests. */
export function planOut(plan: NonNullable<BattleView['plan']>): Record<string, unknown> {
  const out: Record<string, unknown> = {};
  for (const [s, p] of Object.entries(plan)) if (p?.phase) out[s] = { phase: p.phase, main: p.main ?? [], startY: p.startY ?? null, ...(p.reserve ? { reserve: p.reserve } : {}) };
  return out;
}

/** Each army's bar and what drives it, with the enemy's cut to its bar: under fog the player sees no more of it. */
function fogged(by: Record<Side, BreakCause>, me: Side): Record<Side, Partial<BreakCause>> {
  const them: Side = me === 'att' ? 'def' : 'att';
  return { [me]: by[me], [them]: { side: them, bar: by[them].bar } } as Record<Side, Partial<BreakCause>>;
}

/**
 * What an air battle offers the screens beyond the shared view, as the 3D game reads it (AirFeed.cpp ReadView's 'sky',
 * the shape the air recorder writes, tools/air-record): the staff's chance now, whether a shot has been fired, whether
 * the fight is winding down, how near each side is to breaking off and why, the field's score, and each group's name as
 * the dispatches call it.
 */
function skyOut(a: AirSim, me: Side): Record<string, unknown> {
  const r3 = (x: number): number => Math.round(x * 1000) / 1000;
  const near = (side: Side) => { const n = a.nearWhy(side); return { at: r3(n.at), why: n.why }; };
  const sc = a.score();
  return {
    chance: r3(a.staffChance()), fired: a.fired, winding: a.winding(), near: { att: near('att'), def: near('def') },
    score: { lost: sc.lost, worth: Math.round(sc.worth), cut: sc.cut, ruined: sc.ruined, ratio: r3(sc.ratio), up: sc.up },
    called: Object.fromEntries(a.units.map((u) => [u.id, a.called(u)])), me,
  };
}

function viewOut(s: Session, v: BattleView): Record<string, unknown> {
  const land = s.sim instanceof LandBattle ? s.sim : null;
  return {
    ...(s.sim instanceof AirSim ? { sky: skyOut(s.sim, s.me) } : {}),
    t: v.t, limit: v.limit, speed: s.clock.speed, units: v.units.map((u) => (land && !land.known(u as LandUnit) ? hiddenOut(u) : unitOut(u, v.t))), balance: v.balance, events: v.events, zones: v.zones, ai: v.ai, over: v.over, me: s.me,
    ...(v.breaking ? { breaking: { att: Math.round(v.breaking.att * 1000) / 1000, def: Math.round(v.breaking.def * 1000) / 1000 } } : {}),
    // What drives each bar (b496(2)): plain numbers, to three places already. Under fog, of the enemy only its bar
    // until the battle is over, as its blocks are hidden (hiddenOut).
    ...(v.breakingBy ? { breakingBy: land?.fog && !v.over ? fogged(v.breakingBy, s.me) : v.breakingBy } : {}),
    ...(v.contact !== undefined ? { contact: v.contact } : {}),
    // The setup step (release 2): the side placing its guns and teams; the clock does not run until it ends.
    ...(v.setup ? { setup: v.setup } : {}),
    // The computer general's attack plan, for the pictures ("The computer general's plan" above): never its style's
    // name, which the player is not told (ruling 239(4)).
    ...(v.plan ? { plan: planOut(v.plan) } : {}),
    // Air support ("Air support" above): only where a side has fighters on call.
    ...(v.flights ? { flights: v.flights } : {}),
    ...(v.air ? { air: v.air } : {}),
    // A strike on ships carries the ships under attack (src/battle/air/ships.ts, ruling 238): each one's place, hits
    // and how it went down. The air thread's; passed as it comes.
    ...((v as BattleView & { ships?: unknown }).ships ? { ships: (v as BattleView & { ships?: unknown }).ships } : {}),
    // A raid on troops carries the troops under it (src/battle/air/troops.ts, release 2): the guns' fire, the cut so far
    // and the pieces changed since the view before. The air thread's; passed as it comes.
    ...((v as BattleView & { marks?: unknown }).marks ? { marks: (v as BattleView & { marks?: unknown }).marks } : {}),
  };
}

/** Fast until the armies meet, then the speed the player set (ruling 226(1)); the browser page runs the same. */
const RUSH = 4;

/**
 * A land battle's options for the 3D game's own land rules (TC.landStartTicket), as battleStart sets them on its field:
 * the card's fog where it did not know the defence (b453), the corps it identified (b476(3)), each side's armour design
 * year (armourYear: looks only, the shells a tank is drawn firing).
 */
export function liveOf(g: Game, t: FieldTicket<LandPlace>, look: { exact: boolean } | null): { fog: boolean; seen: number[]; designYears: Record<string, number> } {
  const fog = !!look && !look.exact;
  const designYears: Record<string, number> = {};
  // (every nation with a force on the field: an ally's corps fights in its own nation's tanks)
  for (const nat of new Set([t.att.nat, t.def.nat, ...t.att.forces.map((f) => f.nat), ...t.def.forces.map((f) => f.nat)])) {
    const y = armourYear(g, nat);
    if (y) designYears[nat] = y;
  }
  return { fog, seen: fog ? [...cardKnows(g, t)] : [], designYears };
}

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

  /**
   * How the campaign takes a land battle's result (battleStart's battle, or one fought in the 3D game's own land rules and
   * handed back by battleSettleLand): the campaign's own attack order with the field's R and its bill, the pursuit line
   * and why the army broke.
   */
  const landSettle = (ids: number[], to: number, t: FieldTicket<LandPlace>) => (r: FieldResult): unknown => {
    const out = h.attack(ids, to, r.R, fieldBill(r));
    // The army that broke, or pulled out under fire, booked more than the field cost it: the after card's line (b464, b474).
    const rep = (out as { report?: BattleReport | null } | null)?.report;
    const pursuit = rep ? pursuitLine(t, r, rep) : null;
    // Why the army broke, for the after card's words where it broke (b496(2)).
    const more = { ...(pursuit ? { pursuit } : {}), ...(r.brokeBy ? { brokeBy: r.brokeBy } : {}) };
    return out && typeof out === 'object' && Object.keys(more).length ? { ...out, ...more } : out;
  };
  /**
   * An air battle opened on the shared clock: the raid's ticket on 'g', fought out shown, settled once through 'raid'
   * with what the battle reports. 'side' flies the raid for 'nat' with his side 'me' (a battle of your own); without it
   * the raid is his on the grand campaign.
   */
  function airBegin(g: Game, kind: string, target: number, ids: number[], side: { nat: string; me: Side } | null, raid: (fought?: FoughtAir) => unknown) {
    const t = side ? airTicket(g, kind as RaidKind, target, ids, side.nat) : airTicket(g, kind as RaidKind, target, ids);
    if (!t.ticket) return null;
    const ticket = side ? { ...t.ticket, att: { ...t.ticket.att, human: side.me === 'att' }, def: { ...t.ticket.def, human: side.me === 'def' } } : t.ticket;
    // Shown, never counted (ruling 237): who shot whom, passes, stragglers, guns, escort roles for the 3D game to act out.
    const sim = new AirSim(ticket, { show: true });
    // The raid is ordered at the end with the groups the player picked, and paid with what the battle reports (the
    // campaign keeps it inside the dice's own run). Handed to the staff partway, the battle reports the whole raid. The
    // after card goes out with its totals, each side's fighters and bombers lost (AirFeed.cpp ReadAfter reads
    // after.lost), summed from its own rows (afterLost), so the totals and the rows never disagree.
    const withLost = (out: unknown): unknown => {
      const after = (out as { after?: AfterCard } | null)?.after;
      return after ? { ...(out as object), after: { ...after, lost: afterLost(after) } } : out;
    };
    const ses = begin(sim, (r) => withLost(raid(r.extra?.air ? foughtAir(r.extra.air as AirOutcome) : undefined)));
    // The title, the date and the two sides as the 3D game reads them (AirFeed.cpp ReadSky).
    const sideOut = (x: typeof ticket.att) => ({ nat: x.nat, adj: x.adj, human: x.human });
    return {
      sky: {
        w: SKY.w, h: SKY.h, scale: SKY.scale, target: SKY.target, flakR: SKY.flakR, bands: SKY.bands.alt, place: ticket.place,
        title: ticket.title, date: ticket.date, att: sideOut(ticket.att), def: sideOut(ticket.def),
      },
      view: viewOut(ses, sim.view(ses.me)),
    };
  }
  /**
   * The land battles handed back (battleSettleLand), so the same result asked again is settled once: kept by the campaign
   * they were settled in (a load or a new campaign is another game, and starts with none), every one of its month, each
   * with the ticket's seed it was settled under (the city glance thread's review, 22 November 19:00Z).
   */
  const landSettled = new WeakMap<Game, Map<string, { seed: number; out: unknown }>>();
  const WRONG_BATTLE = 'The attack the campaign would make now is not the battle that was fought.';

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
      if (why) return { ticket: null, why };
      // Scarred ground (release 2): the battles fought here before this one, and the months since the last, for the
      // field to be laid already torn up; absent where none has been.
      const scar = g.scars?.[to];
      const ticket: typeof t = { ...t };
      if (scar && scar.n > 0) ticket.scars = { count: scar.n, months: Math.max(0, g.turn - scar.last) };
      // Mud memory (release 2): the battles fought here in the mud, and the months since the last of them.
      if (scar?.mudN && scar.mudLast !== undefined) ticket.oldRuts = { count: scar.mudN, months: Math.max(0, g.turn - scar.mudLast) };
      // Old wrecks (release 2): the armoured vehicles the battles here left burnt out on the field, by nation, and the
      // months since the last of them.
      const wrecks = wreckLook(g.wrecks?.[to], g.turn);
      if (wrecks) ticket.oldWrecks = wrecks;
      // The ground's weather this month, in the 3D game's words (thaw mud, snow, rain, clear); looks only.
      const wx = groundWeather(g, g.regions[to]);
      if (wx && !ticket.weather) ticket.weather = wx;
      // Captured kit (release 2): each side's latest capture of the last six months, whose tanks and how many blocks.
      const took = (nat: string) => (g.captured?.[nat] ?? []).filter((k) => g.turn - k.turn < 6).slice(-1)[0];
      const ca = took(t.att.nat), cd = took(t.def.nat);
      if (ca || cd) ticket.captured = { ...(ca ? { att: { from: ca.from, n: ca.n } } : {}), ...(cd ? { def: { from: cd.from, n: cd.n } } : {}) };
      // Uniforms that age (release 2): each side's months in the line without a rest (its corps' weeks, as the stories
      // keep them, weighed by divisions), where the campaign counts them.
      const worn = (cs: typeof corps) => {
        const divs = cs.reduce((s2, c) => s2 + c.divs, 0);
        return divs > 0 && cs.some((c) => c.line !== undefined) ? Math.round(cs.reduce((s2, c) => s2 + (c.line ?? 0) * c.divs, 0) / divs / 4.345) : undefined;
      };
      const inAttack = new Set(corps.map((c) => c.id));
      const wa = worn(corps), wd = worn(g.corps.filter((c) => c.reg === to && !inAttack.has(c.id) && c.kind !== 'fort'));
      if (wa !== undefined) ticket.att = { ...ticket.att, worn: wa };
      if (wd !== undefined) ticket.def = { ...ticket.def, worn: wd };
      return { ticket, live: liveOf(g, t, attackOutlook(g, corps, g.regions[to])) };
    },
    battleSettleLand(ids: number[], to: number, result: FieldResult | { over: FieldResult | null }, seed?: number) {
      const r = result && 'over' in result ? result.over : (result as FieldResult | null);
      if (!r || typeof r.R !== 'number' || !Array.isArray(r.lines)) return null;
      const g = h.game();
      const given = seed !== undefined && seed !== null ? Number(seed) : null;
      // (settled already in this campaign this month: its answer again, nothing booked twice; a seed given must be the one
      // it was settled under, or it is another battle)
      const done = landSettled.get(g) ?? new Map<string, { seed: number; out: unknown }>();
      const key = JSON.stringify([g.turn, ids, to, r]);
      const was = done.get(key);
      if (was) return given !== null && given !== was.seed ? { why: WRONG_BATTLE } : was.out;
      const corps = ids.map((i) => g.corps.find((c) => c.id === i)).filter((c): c is NonNullable<typeof c> => !!c);
      const t = landTicket(g, corps, g.regions[to]);
      if (!t || staffOnly(t)) return null;
      // (the battle fought was the one the campaign would make now: the ticket's own seed, where the 3D game passes it)
      if (given !== null && given !== t.seed) return { why: WRONG_BATTLE };
      const out = landSettle(ids, to, t)(r) ?? null;
      done.set(key, { seed: t.seed, out });
      landSettled.set(g, done);
      return out;
    },
    battleStart(ids: number[], to: number, opts?: { setup?: boolean; staff?: boolean; radio?: boolean; hide?: boolean; armour?: boolean; names?: boolean }) {
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
      const live = liveOf(g, t, look);
      sim.fog = live.fog;
      // All but the corps the card identified, which show as the card shows them (b476(3)).
      if (sim.fog) sim.seen = new Set(live.seen);
      // The men inside the blocks, for the 3D game to draw as men (TC.battleMen): looks only, never read by the fight.
      // Each model as its whole section (release 2: "the ground battle needs a lot more soldiers in it").
      sim.drillOn({ sections: true, platoons: true });
      // The campaign is asked again at the end with the corps the player picked: it decides who goes in, as it did for the
      // card. Those going in are the ticket's own, so a corps spent since it was picked is not shown going (b453).
      const going = t.att.forces.map((f) => f.corps);
      const ses = begin(sim, landSettle(ids, to, t));
      if (look && !look.exact) ses.band = { lo: look.lo, hi: look.hi };
      // The setup step (release 2): only when asked; the 3D game's two-argument call never opens it. Every block of his
      // side is placed in it (the general's controls).
      if (opts?.setup) sim.setupOn(ses.me, true);
      // Direct command (release 2): his staff fights the blocks he has not taken in hand, and his orders go by radio.
      if (opts?.staff) { sim.perUnit = true; sim.ai[ses.me] = true; }
      if (opts?.radio) sim.radio = true;
      // Hidden in the woods (release 2): blocks in woods when the clock starts are hidden from the other side.
      if (opts?.hide) sim.hideOn();
      // Measured armour (release 2): each tank's armour by face and range weighs the blows on her.
      if (opts?.armour) sim.measured = true;
      // Army names (release 2): the regiment blocks named as their armies named a third of a division (the historian).
      if (opts?.names) sim.namesOn();
      const f = sim.field;
      return {
        field: {
          w: f.w, h: f.h, cell: f.cell, cols: f.cols, rows: f.rows, grounds: GROUNDS, ground: f.ground, height: f.height.map((x) => Math.round(x * 10) / 10), zones: f.zones, town: f.town, place: t.place,
          ...(f.storeys ? { storeys: f.storeys } : {}), ...(f.landmark ? { landmark: f.landmark } : {}),
          ...(f.stream ? { stream: f.stream } : {}),
        },
        going, view: viewOut(ses, sim.view(ses.me)),
        ...(sim.setup ? { setup: { room: sim.setupRoom() } } : {}),
      };
    },
    battleCanPlace(id: number, x: number, y: number) {
      const ses = need();
      const land = ses.sim instanceof LandBattle ? ses.sim : null;
      const u = land?.unit(Number(id));
      return { why: !land || !u ? 'Nothing to place.' : u.side !== ses.me ? 'Only your own troops are placed.' : land.placeWhy(u, Number(x), Number(y)) };
    },
    battlePlace(id: number, x: number, y: number, face?: number) {
      const ses = need();
      const land = ses.sim instanceof LandBattle ? ses.sim : null;
      const u = land?.unit(Number(id));
      const why = !land || !u ? 'Nothing to place.' : land.placeWhy(u, Number(x), Number(y));
      const ok = why === null && ses.sim.command({ do: 'land:place', units: [Number(id)], x: Number(x), y: Number(y), ...(face !== undefined ? { face: Number(face) } : {}) }, ses.me);
      return { ok, why: ok ? null : why, view: viewOut(ses, ses.sim.view(ses.me)) };
    },
    battleHedges(hedges: Hedge[], gates: Gate[] = []) {
      const ses = need();
      const land = ses.sim instanceof LandBattle ? ses.sim : null;
      const ok = !!land && Array.isArray(hedges) && land.hedgesOn(hedges, Array.isArray(gates) ? gates : []);
      return { ok, hedges: land?.hedges.length ?? 0, gates: land?.gates.length ?? 0, view: viewOut(ses, ses.sim.view(ses.me)) };
    },
    battleBegin() {
      const ses = need();
      const land = ses.sim instanceof LandBattle ? ses.sim : null;
      if (land?.setup) { land.command({ do: 'land:begin' }, ses.me); ses.clock.set(1); }
      return { view: viewOut(ses, ses.sim.view(ses.me)) };
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
      // A speed chosen while placing ends the setup step first (release 2), as Space does on the page.
      if (sp > 0 && ses.sim instanceof LandBattle && ses.sim.setup) ses.sim.command({ do: 'land:begin' }, ses.me);
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
    battleMen() {
      const ses = need();
      const land = ses.sim instanceof LandBattle ? ses.sim : null;
      if (!land) return null;
      const d = land.drillOn({ sections: true, platoons: true });
      return { t: land.t, blocks: d.view(ses.me), rounds: d.rounds(ses.me) };
    },
    battleAirRun(x: number, y: number, x1?: number, y1?: number) {
      const ses = need();
      const land = ses.sim instanceof LandBattle ? ses.sim : null;
      if (!land) return { run: null, why: 'No fighters on call' };
      // A drawn run as the field will fly it (kept on the field and to its longest), or the one pointed at.
      const run: [number, number, number, number] = x1 !== undefined && y1 !== undefined ? land.airRun([x, y, x1, y1]) : land.airRunAt(ses.me, x, y);
      return { run, why: land.airWhyNot(ses.me, run) };
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
    airKinds(kinds?: string[]) {
      return { kinds: flyKinds(Array.isArray(kinds) ? kinds.map(String) : undefined) };
    },
    airLook(kind: string, target: number, ids: number[]) {
      const t = airTicket(h.game(), kind as RaidKind, target, ids);
      return t.ticket ? { ticket: t.ticket } : { ticket: null, why: t.why };
    },
    airStart(kind: string, target: number, ids: number[]) {
      const raid = h.raid;
      if (!raid) return null;
      return airBegin(h.game(), kind, target, ids, null, (r) => raid(kind, target, ids, r));
    },
    /**
     * A battle of your own in the air (release 2, custom-bridge.ts): the same battle on a war of only the two air forces,
     * the raid flown by 'nat' (the computer's when he defends), his side the one he picked, and the raid paid on that war
     * with its own after card. The grand campaign is not touched.
     */
    airStartOn(g: Game, kind: string, target: number, ids: number[], nat: string, me: Side, settle?: (fought?: FoughtAir) => unknown) {
      return airBegin(g, kind, target, ids, { nat, me }, settle ?? ((r) => airAct(g, `raid:${kind}:${target}:${ids.join('.')}`, r)));
    },
  };
}
