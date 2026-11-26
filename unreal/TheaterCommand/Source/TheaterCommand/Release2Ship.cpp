#include "Release2Ship.h"

#include "TheaterCommand.h"
#include "CoreGlobals.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"

namespace
{
	using Release2Ship::EKind;
	using Release2Ship::FFeature;

	/** Where his own choices on OPTIONS are kept (the Battle rules card keeps its own, in [TheaterCommand.LandRules]). */
	const TCHAR* const ShipSection = TEXT("TheaterCommand.Release2");

	// The list, in OPTIONS' order. bShips true: ON in the build Brian plays. false: OFF, and Note says why.
	// (To hold a feature back after the Mac pass or the art director: its bShips to false and the reason in Note. Nothing
	// else changes: its row leaves OPTIONS, its words stay out, its own switch reads as before the list.)
	const FFeature ShipList[] = {
		// ---- The ground battle's looks (the ground, and the grass, trees and sky; the soldiers' detail) ----
		{ TEXT("wind"), EKind::Launch, TEXT("Battle looks: wind in the grass and trees"), TEXT("Grass, corn and trees sway in the day's gusts, with a light sheen. Off: the field stands still."), TEXT("-TheaterLandWind"), true, TEXT("AD PASS 23 Nov 15:00Z; the Mac's stills and 45 fps in the final pass"), false },
		{ TEXT("cloudshadows"), EKind::Launch, TEXT("Battle looks: cloud shadows"), TEXT("The shadows of the day's clouds drift across the fields. Off: the light lies even."), TEXT("-TheaterLandCloudShadows"), true, TEXT("AD PASS 23 Nov 15:00Z; Mac in the final pass"), false },
		{ TEXT("sunshafts"), EKind::Launch, TEXT("Battle looks: shafts of a low sun"), TEXT("A low sun throws shafts of light through smoke and trees. Off: none."), TEXT("-TheaterLandSunShafts"), true, TEXT("AD PASS 23 Nov 15:00Z; Mac in the final pass"), false },
		{ TEXT("darkread"), EKind::Launch, TEXT("Battle camera: the dark reading"), TEXT("The camera reads how dark the picture is and steps back from a black shot. Off: it never reads the picture."), TEXT("-TheaterLandDarkRead"), false, TEXT("pass 2 (1.5.2 final 8571f14d6, Brian 20:48Z \"ship 1.5.2 as tested\"): 1.5.2 ships the dark reading OFF unless -TheaterLandDarkRead; the Mac's m2 B3 Normandy reading: 55/44/44 fps on against 55/45/46 off over 541 reads, 0 dark shots by day; -TheaterLandNoDarkRead still keeps it off; ON only when the camera thread's cheaper reading (read only in low light) passes the Mac within 1 fps"), false },
		{ TEXT("detailmen"), EKind::Launch, TEXT("Battle looks: soldiers in full detail"), TEXT("Every soldier is drawn to the detail of the tanks beside him: his face, his kit, his own army's uniform. Off: the plainer soldiers."), TEXT("-TheaterLandDetailMen"), true, TEXT("AD PASS (German rifleman, 24 Nov 17:01Z); Mac 45 fps in the final pass"), false },
		{ TEXT("marks"), EKind::Launch, TEXT("Battle looks: tank markings"), TEXT("Tanks carry their army's national markings and numbers. Off: plain hulls."), TEXT("-TheaterLandMarks"), true, TEXT("AD PASS 24 Nov 05:29Z; Mac in the final pass"), false },
		{ TEXT("smoothtanks"), EKind::Launch, TEXT("Battle looks: tanks drive smoothly"), TEXT("Tanks roll along smooth curves instead of turning in jerks. Off: as they drove before."), TEXT("-TheaterLandSmoothTanks"), true, TEXT("built 85a482e0; Mac look in the final pass"), false },
		{ TEXT("falling"), EKind::Launch, TEXT("Battle looks: rain you can see"), TEXT("When the campaign's weather brings rain to a battle, you see it falling round you under a grey sky, the air thickens and the light dulls, wet ground darkens, shines and holds water in its ruts, and you hear the rain. Off: nothing falls, the day as drawn."), TEXT("-TheaterLandFalling"), false, TEXT("built 25 Nov (look bible G10); AD round 2 PASS TO THE MAC 12:21Z; waits the Mac's shots 1-5 (ratios, units at 300/600 m, corn from above, a pan) and 45 fps in a downpour"), false },
		{ TEXT("fallingsnow"), EKind::Launch, TEXT("Battle looks: snow you can see falling"), TEXT("When the campaign's weather brings snow down on a battle, you see the flakes drifting round you under a grey sky, and the far field fades into it. Works with rain you can see. Off: the snow lies, none falls."), TEXT("-TheaterLandFallingSnow"), false, TEXT("built 25 Nov with \"falling\" (G10); AD round 2 PASS TO THE MAC on a short lead; waits the Mac's shots 6-8 (the lid 15 under the snow, units at 300/600 m, flakes); may ship off, or capped by pick.snow.most, while rain ships"), false },
		{ TEXT("houses"), EKind::TrialWord, TEXT("Battle looks: farms and villages in full detail"), TEXT("Farmhouses, barns, byres, village houses, the izba and khata of the east and burnt-out houses, drawn in full. Off: the plainer houses."), TEXT("houses"), true, TEXT("AD PASS (Normandy, izba, khata, burnt house); Mac stills in the final pass"), false },
		{ TEXT("church"), EKind::TrialWord, TEXT("Battle looks: village churches"), TEXT("A village has its church, with its tower over the roofs. Off: none."), TEXT("church"), true, TEXT("AD PASS 22 Nov 23:42Z; Mac in the final pass"), false },
		{ TEXT("walltop"), EKind::TrialWord, TEXT("Battle looks: broken wall tops"), TEXT("Old stone walls in the south are broken along their tops, as in Italy. Off: level walls."), TEXT("walltop"), true, TEXT("AD PASS 22 Nov 23:42Z; Mac in the final pass"), false },
		{ TEXT("spruce"), EKind::TrialWord, TEXT("Battle looks: dark winter spruce"), TEXT("Spruce woods stand dark against the snow, as they look from above. Off: the plainer woods."), TEXT("spruce"), true, TEXT("the AD's gate 25 Nov 14:02Z: NOT YET, small mend, stays ON (snow-laden boughs as hard shards under about 15 m, B4 shot 3). Pass 2 carries the mend (nu6w43 f71c0a7e: each bough's snow soft-edged and frosted, near models only); waits B4 shot 3 again on the Mac and the AD"), false },
		{ TEXT("edge"), EKind::TrialWord, TEXT("Battle looks: wood edges that wander"), TEXT("The edge of a wood runs along a wandering line, with saplings and scrub in front of it. Off: straighter edges."), TEXT("edge"), true, TEXT("AD PASS 22 Nov 23:42Z; Mac in the final pass"), false },
		{ TEXT("kunai"), EKind::TrialWord, TEXT("Battle looks: kunai grass"), TEXT("The tall kunai grass of the Pacific islands, with its plumes. Off: plain grass."), TEXT("kunai"), false, TEXT("the AD's gate 25 Nov 14:02Z: OFF (B11 33 to 50 a second). Pass 1b's pair on Panama City: without 59/49/52 (worst 48), with 58/45/52 (worst 44), 4 apart at 40 s. The AD 15:01Z: thin first (a fifth fewer plants in the far half of its reach, the near 30 m as thick), then ON if the pair's gap at 40 s is 3 or less and its worst second 45 or better; if thinning buys nothing, ON as it stands"), false },
		{ TEXT("rice"), EKind::TrialWord, TEXT("Battle looks: rice country"), TEXT("Fields in rice country are paddies behind their low banks. Off: ordinary fields."), TEXT("rice"), false, TEXT("the AD's gate 25 Nov 14:02Z: OFF. No Mac field drew it and its frame rate was never measured. One rice ticket (China or Burma) with a still and its rates"), false },
		{ TEXT("plain"), EKind::TrialWord, TEXT("Battle looks: dry plains"), TEXT("Dry plains look like dry plains: thin grass, bare earth and scrub. Off: the greener ground."), TEXT("plain"), true, TEXT("AD PASS 23 Nov (some months' stills owed); Mac fps in the final pass"), false },
		{ TEXT("america"), EKind::TrialWord, TEXT("Battle looks: North American farmland"), TEXT("Fields in North America are laid out in its own big square farms. Off: the European pattern."), TEXT("america"), true, TEXT("AD PASS 23 Nov (some months' stills owed); Mac fps in the final pass"), false },
		{ TEXT("runs"), EKind::TrialWord, TEXT("Battle looks: men run in their own strides"), TEXT("Men in a squad run each in his own stride instead of in step. Off: in step."), TEXT("runs"), true, TEXT("AD and Mac in the final pass"), false },
		{ TEXT("deserthouses"), EKind::TrialWord, TEXT("Battle looks: desert towns' houses in full detail"), TEXT(""), TEXT("deserthouses"), false, TEXT("the AD's first review NOT YET (six mends, one more round); no Mac look yet"), false },
		{ TEXT("desertland"), EKind::TrialWord, TEXT("Battle looks: desert wadis and ridges"), TEXT(""), TEXT("desertland"), false, TEXT("pass 2 (nu6w43 12681e5a + round 2 972039b1 + tune 3ae03fbc, the AD's B5 ask: a wadi cut with a dark foot and a stony bed across every desert field, camel thorn thick on its banks, a low ridge 300 to 700 m out; the AD passed round 2 to the Mac 15:11Z); off, the desert is byte for byte as before; Mac B5 with -TheaterLandTrial=desertland, then the AD"), false },
		{ TEXT("islandsand"), EKind::TrialWord, TEXT("Battle looks: island sand and surf line"), TEXT(""), TEXT("islandsand"), false, TEXT("pass 2 (environments' patch on 12681e5a, LandTropic.cpp: warm sand on the islands, a darker, wider wet band at the water); off, the island is byte for byte as before; the AD passed it to the Mac 15:04Z with the shade tune (nu6w43 38258d63); Mac B11 island coast with -TheaterLandTrial=islandsand, then the AD's read"), false },
		{ TEXT("birchbark"), EKind::TrialWord, TEXT("Battle looks: birch bark"), TEXT(""), TEXT("birchbark"), false, TEXT("pass 2 (nu6w43 007d52f7 + d8c40e53 tune: half as many marks again on the lowest 3 m, one in five an eyebrow where a branch was; near birches get off-white bark with short dark marks and a dark foot in place of black bands; near birch 4840 triangles against 4272, middle and far gain none); off, the birches are byte for byte as before; at the AD 15:25Z; Mac B2 and B7 (the gate field's birches) without and with -TheaterLandTrial=birchbark, fps at 20/40/60 s both ways"), false },
		{ TEXT("sheds"), EKind::TrialWord, TEXT("Battle looks: the steppe farm's long sheds"), TEXT(""), TEXT("sheds"), false, TEXT("pass 2 (nu6w43 925f4918 + ad268b23, then v3 7ba051e7 + f3e81f47 + a34ad90e: the steppe barn and cow shed in clay under grey thatch, the historian's, replacing the boards on the same word; the AD passed v3 on the cloud pictures 19:37Z; Brian's 17:13Z steppe screenshot); off, Normandy and Prokhorovka digests unchanged; on, Prokhorovka tiles 9c87d309be6e0e39, the village tile 7,864 -> 35,749 built triangles; Mac (run sheet P22): Prokhorovka, the village (pass 1's B view of the khata and the long shed's gable) without and with -TheaterLandTrial=sheds, fps at 20/40/60 s, one still each from the same eye; the shell-hit gable's stub 12 L or more over the inside; no sparkle between thatch panels on the long shed from 15 m"), false },
		{ TEXT("feet"), EKind::TrialWord, TEXT("Battle looks: clear footing round bushes and trunks"), TEXT(""), TEXT("feet"), false, TEXT("pass 2 (nu6w43 64ef4643; Brian 16:35Z, \"the grass is blowing through tree\"): every bush, hedge shrub, trunk and stack keeps a clear footing, no tall grass, flowers or ferns planted inside it; dial word feetwide widens it (=all turns both on); never dearer (Normandy's 120 m square 14,635 pieces off, 14,382 feet, 14,359 feet,feetwide); digests unchanged; the AD passed it on the cloud pictures 19:37Z; Mac (run sheet P31): 10 s at a Normandy hedge and 10 s at a tree trunk in the stiffest breeze, with -TheaterLandTrial=feet and with feet,feetwide; passes if no blade tip inside a bush's leaves or across a trunk, no bare ring wider than 0.3 m from 10 m, frame rate not lower; then ON"), false },
		{ TEXT("teamguns"), EKind::TrialWord, TEXT("Battle looks: machine gun and mortar teams"), TEXT(""), TEXT("teamguns"), false, TEXT("pass 2 (ground uhhxrn f8974f4f merge): each support team's key man drawn at his machine gun on its mount or his mortar (f8974f4f, Brian 25 Nov); new engine code, at the pre-check; Mac first build, stills and a paired run with -TheaterLandNoTeamGuns, then the AD"), false },
		{ TEXT("gunners"), EKind::TrialWord, TEXT("Battle looks: the section's machine gunner"), TEXT(""), TEXT("gunners"), false, TEXT("pass 2 (ground uhhxrn 47da7978 + soldiers labtrr 854cb3d0): each section's gunner with his own gun, the German MG 42, his MG 34 before July 1942; a halted section gunner lies behind his gun (ground 2684c74a); at the pre-check; Mac stills, then the AD"), false },
		{ TEXT("glasses"), EKind::TrialWord, TEXT("Battle looks: tank commanders' field glasses"), TEXT(""), TEXT("glasses"), false, TEXT("pass 2 (ground uhhxrn 2684c74a; Brian 16:35Z, a Sherman commander holding a weapon where his glasses should be): the commander's field glasses drawn where his clip holds them (Binoculars.json, prop key binoculars), put away when the clip has none; -TheaterLandNoGlasses takes them out again; at the pre-check (LandBattle.cpp, not compiled in the cloud); Mac: Normandy, a Sherman with its commander up, close eye, without and with -TheaterLandTrial=glasses, one still each; then the AD"), false },
		{ TEXT("leave"), EKind::TrialWord, TEXT("Battle looks: men leave the field on their feet"), TEXT(""), TEXT("leave"), false, TEXT("pass 2 (ground uhhxrn 1e37cf3c; Brian 17:11Z, \"some soldiers freeze and then disappear\"): a man whose block falls back off the field walks or runs on and is hidden once out of the picture, not stopped dead and cut; a body left standing where the camera last saw it is drawn again; -TheaterLandNoLeave off whatever the word says; Mac: Normandy custom battle, All fall back at about 60 s, 20 s of the men at the edge from a soldier's height and from the general's, paired with -TheaterLandNoLeave, passes if no man stops mid-stride and none blinks out in the picture (log: men who left the place they were last drawn); the overfull lorry's last men seated in her bed ride on this word too (ground 04e8ea03); then ON"), false },
		{ TEXT("burntcoat"), EKind::TrialWord, TEXT("Battle looks: burnt wrecks in soot, rust and ash"), TEXT(""), TEXT("burntcoat"), false, TEXT("pass 2 (ground uhhxrn f8974f4f merge): a burnt-out wreck in soot, rust and ash, each point its own (1e550bf4; pre-check PASS 15:41Z); Mac needs TC_REBUILD=M_LandSteel, then the AD"), false },
		{ TEXT("desertstop"), EKind::TrialWord, TEXT("Battle looks: desert sand stopped down"), TEXT(""), TEXT("desertstop"), false, TEXT("pass 2 (ground uhhxrn f8974f4f merge): the desert's fields half a stop down for sand that washed pale on the Mac (faabad93, the AD's ask); before and after still"), false },
		{ TEXT("islandstop"), EKind::TrialWord, TEXT("Battle looks: island sand stopped down"), TEXT(""), TEXT("islandstop"), false, TEXT("pass 2 (ground uhhxrn f8974f4f merge): an island's fields a third of a stop down (faabad93, the AD's ask); before and after still"), false },
		{ TEXT("eastgold"), EKind::TrialWord, TEXT("Battle looks: the east's gold summer"), TEXT(""), TEXT("eastgold"), false, TEXT("pass 2 (ground uhhxrn f8974f4f merge): the east's summer graded as its own theatre, steppe haze and the T2 grade (environments c6121f54 + 8832b0f7 + 1a050591); Mac stills, then the AD"), false },
		{ TEXT("medwall"), EKind::TrialWord, TEXT("Battle looks: Italy's dry stone walls"), TEXT(""), TEXT("medwall"), false, TEXT("pass 2 (ground uhhxrn f8974f4f merge): Italy's field walls as warm dry stone with thickets (environments d41f379d + 4fd7319e); Mac stills, then the AD"), false },
		{ TEXT("sprucedepth"), EKind::TrialWord, TEXT("Battle looks: deep spruce woods"), TEXT(""), TEXT("sprucedepth"), false, TEXT("pass 2 (ground uhhxrn f8974f4f merge): a deep, ragged spruce wood, not a comb (environments f9f74678 + 2 mends); Mac stills, then the AD"), false },
		{ TEXT("farmgrass"), EKind::TrialWord, TEXT("Battle looks: eastern grassland in tussocks"), TEXT(""), TEXT("farmgrass"), false, TEXT("pass 2 (ground uhhxrn f8974f4f merge): eastern grassland as sward and tussocks in clumps, judged with heads (environments aef04817); Mac stills, then the AD"), false },
		{ TEXT("wintergrass"), EKind::TrialWord, TEXT("Battle looks: dry stalks in the snow"), TEXT(""), TEXT("wintergrass"), false, TEXT("pass 2 (ground uhhxrn f8974f4f merge): the farmland's dry stalks weathered and bent in the snow (environments 54973aec + a471fa57); Mac B4 stills, then the AD"), false },
		{ TEXT("fieldtone"), EKind::TrialWord, TEXT("Battle looks: each field its own tone"), TEXT(""), TEXT("fieldtone"), false, TEXT("pass 2 (ground uhhxrn f8974f4f merge): no two pastures of one crop one green (environments 91eab2a6 + 526ab2fe); Mac stills, then the AD"), false },
		{ TEXT("heads"), EKind::TrialWord, TEXT("Battle looks: seed heads and meadow flowers"), TEXT(""), TEXT("heads"), false, TEXT("pass 2 (ground uhhxrn f8974f4f merge): grass seed heads and meadow flowers (environments 63dd16d1); Mac stills, then the AD"), false },
		{ TEXT("city"), EKind::TrialWord, TEXT("Battle looks: ruined cities"), TEXT(""), TEXT("city"), false, TEXT("pass 2 (q82imy 1d615923, the AD's three ride-along mends on 38b6ab24): waits the Mac's stills at the capital-east field (avenue, core street, suburb, barricade) and 45 fps in the core, then the AD"), false },
		{ TEXT("scorch"), EKind::TrialWord, TEXT("Battle looks: scorched ground round burnt-out vehicles"), TEXT(""), TEXT("scorch"), false, TEXT("pass 2 (nu6w43 50c648ff): the crater sheet passes; waits the effects timing and a Mac A/B, then the AD"), false },
		{ TEXT("trigh"), EKind::TrialWord, TEXT("Battle looks: desert wheel tracks"), TEXT(""), TEXT("trigh"), false, TEXT("pass 2 (nu6w43 3e5946b2 + 3b0b72bb): waits the 3D pre-check's compile and its A/B shots (the desert at 31.5N 25.0E)"), false },
		{ TEXT("wetlook"), EKind::TrialWord, TEXT(""), TEXT(""), TEXT("wet"), true, TEXT("the look of the Battle rules card's Wet mud row (its switch); the AD's gate 25 Nov 14:02Z: NOT YET, small mend, stays ON (the wet row's note)"), false },
		{ TEXT("wornlook"), EKind::TrialWord, TEXT(""), TEXT(""), TEXT("worn"), true, TEXT("the look of the Battle rules card's Worn uniforms row (its switch)"), false },
		{ TEXT("capturedlook"), EKind::TrialWord, TEXT(""), TEXT(""), TEXT("captured"), true, TEXT("the look of the Battle rules card's Captured tanks row (its switch)"), false },
		// ---- The ground battle's fight (the effects) ----
		{ TEXT("bailout"), EKind::Launch, TEXT("Battle effects: crews bail out"), TEXT("The crew of a knocked-out tank climbs out and runs for cover before she brews up. Off: no one gets out."), TEXT("-TheaterLandBailOut"), true, TEXT("AD PASS 23 Nov 19:33Z; Mac in the final pass"), false },
		{ TEXT("bailsmoke"), EKind::Launch, TEXT("Battle effects: smoke covers a bail-out"), TEXT("With the enemy close, a tank's commander throws smoke to screen his crew getting out. Works with crews bail out. Off: no smoke."), TEXT("-TheaterLandBailSmoke"), true, TEXT("AD PASS 24 Nov 12:51Z; Mac (4+ screens) in the final pass"), false },
		{ TEXT("airsupport"), EKind::Launch, TEXT("Battle effects: aircraft over the field"), TEXT("Fighters and ground-attack aircraft of the battle's air war fly over the field. Off: an empty sky."), TEXT("-TheaterLandAirSupport"), true, TEXT("AD PASS 23 Nov 19:16Z; Mac in the final pass"), false },
		{ TEXT("airsound"), EKind::Launch, TEXT("Battle sound: the planes over the field"), TEXT(""), TEXT("-TheaterLandAirSound"), false, TEXT("pass 2 (sound 5qkrdl 6dd9526): the fighters on call over a ground battle heard (engines, pass-bys, dives, wing guns, the ground's MGs firing up, losses); works only beside airsupport; waits for Mac + Brian's ear (a land battle with air support with and without the word: the log says \"the fighters on call heard\")"), false },
		{ TEXT("chutes"), EKind::Launch, TEXT("Battle effects: pilots under parachutes"), TEXT("A pilot shot down over the field comes down under his canopy. Works with aircraft over the field. Off: none."), TEXT("-TheaterLandChutes"), true, TEXT("AD PASS 24 Nov 14:34Z; Mac (a still from above) in the final pass"), false },
		{ TEXT("gore"), EKind::Launch, TEXT("Battle effects: men thrown by blasts"), TEXT("Men near a shell burst or a tank's hit are thrown, and some are maimed. No blood. Off: men fall where they stand."), TEXT("-TheaterLandGore"), true, TEXT("AD PASS 23 Nov 18:34Z; Mac in the final pass"), false },
		{ TEXT("gunflash"), EKind::Launch, TEXT("Battle effects: each gun's own flash"), TEXT("Every kind of gun has its own muzzle flash and smoke. Off: one flash for all."), TEXT("-TheaterLandGunFlash"), true, TEXT("AD PASS; Mac in the final pass"), false },
		{ TEXT("craters"), EKind::Launch, TEXT("Battle effects: craters of every kind"), TEXT("Each shell hole has its own size, slant and scorch, by the gun and the ground. Off: one kind of hole."), TEXT("-TheaterLandCraterVariety"), true, TEXT("AD PASS; Mac in the final pass"), false },
		{ TEXT("filmfire"), EKind::Launch, TEXT("Battle effects: burning wrecks as on film"), TEXT("A burning vehicle burns as war film shows it: dark smoke, low flame, sparks. Off: the plainer fire."), TEXT("-TheaterLandFilmFire"), true, TEXT("AD PASS; Mac (8+ fires) in the final pass"), false },
		// ---- The ground battle's camera ----
		{ TEXT("focus"), EKind::Launch, TEXT("Battle camera: long-lens focus"), TEXT("Close shots that follow a man or a tank hold him sharp and soften what is behind, as a long lens does. Off: everything sharp."), TEXT("-TheaterLandFocusLive=1"), false, TEXT("OFF for Normandy's 45 (the AD's drop order, first in line): the Mac's B3 on 0531fa9b, fps at 20/40/60 s (worst second), ran 56/44/46 (34) as shipped and 56/46/49 (46) without focus, the only run at 45 or better at all three marks (without detailmen 55/46/44, without wind, cloud shadows and shafts 57/48/49). Its look passed the AD as no harm; back ON when it holds 45 in B3"), false },
		{ TEXT("whistle"), EKind::Launch, TEXT("Battle camera: the whistle start"), TEXT("An attack opens on its officer blowing his whistle as the first men step off. Off: it opens on the wide view."), TEXT("-TheaterLandWhistle"), true, TEXT("camera specials s1-s3 in the final pass"), false },
		{ TEXT("laststand"), EKind::Launch, TEXT("Battle camera: the last stand"), TEXT("A cut-off handful of men still firing get one held shot. Off: no such shot."), TEXT("-TheaterLandLastStand"), true, TEXT("camera specials s3 in the final pass"), false },
		{ TEXT("nickcard"), EKind::Launch, TEXT("Battle camera: the earned name's card"), TEXT(""), TEXT("-TheaterLandNickCard"), false, TEXT("nothing fills a campaign battle's earned names yet (camera-director/specials-r2/specials-campaign-names-on-sgfei1.diff)"), false },
		{ TEXT("spotter"), EKind::Launch, TEXT("Battle camera: spotter's rings"), TEXT(""), TEXT("-TheaterLandSpotter"), false, TEXT("mended (the camera thread zmn7zg, show-flags fault mended in 5d40c8f8, joined edf39bf6; its head 85979349 via ground f926f6a9): waits the Mac at 45 fps (Normandy B3, with and without the word, fps at 20/40/60 s) and the art and UI directors' stills; then ON"), false },
		{ TEXT("corner"), EKind::Launch, TEXT("Battle camera: film in a corner"), TEXT(""), TEXT("-TheaterLandCorner"), false, TEXT("mended (the camera thread zmn7zg, show-flags fault mended in 5d40c8f8, joined edf39bf6; its head 85979349 via ground f926f6a9): waits the Mac at 45 fps (Normandy B3, with and without the word, fps at 20/40/60 s) and the art and UI directors' stills; then ON"), false },
		{ TEXT("platoons"), EKind::Launch, TEXT("Battle looks: platoon-sized crowds"), TEXT(""), TEXT("-TheaterLandPlatoons"), false, TEXT("held for the Mac's frame rate (8-20 fps in Normandy until the crowd is drawn on the card)"), false },
		{ TEXT("bocage"), EKind::Launch, TEXT("Battle rules: bocage hedges"), TEXT(""), TEXT("-TheaterLandBocage"), false, TEXT("held while the tactics thread finishes it (ruling 242)"), false },
		{ TEXT("everylook"), EKind::Launch, TEXT("FIGHT IT YOURSELF everywhere"), TEXT(""), TEXT("-TheaterFightEveryLook"), false, TEXT("standing rule: FIGHT IT YOURSELF only on passed battlefields"), false },
		// ---- The ground's Battle rules card (each row's own switch is on the card) ----
		{ TEXT("mech"), EKind::LandRule, TEXT(""), TEXT(""), TEXT("mech"), true, TEXT("Mac 45 fps + AD in the final pass (B7-B9)"), false },
		{ TEXT("spg"), EKind::LandRule, TEXT(""), TEXT(""), TEXT("spg"), true, TEXT("Mac 45 fps + AD in the final pass"), false },
		{ TEXT("td"), EKind::LandRule, TEXT(""), TEXT(""), TEXT("td"), true, TEXT("Mac 45 fps + AD in the final pass"), false },
		{ TEXT("cars"), EKind::LandRule, TEXT(""), TEXT(""), TEXT("cars"), true, TEXT("Mac 45 fps + AD in the final pass"), false },
		{ TEXT("aa"), EKind::LandRule, TEXT(""), TEXT(""), TEXT("aa"), true, TEXT("Mac 45 fps + AD in the final pass"), false },
		{ TEXT("light"), EKind::LandRule, TEXT(""), TEXT(""), TEXT("light"), true, TEXT("models AD PASS 23 Nov; Mac 45 fps + AD in battle in the final pass"), false },
		{ TEXT("blast"), EKind::LandRule, TEXT(""), TEXT(""), TEXT("blast"), true, TEXT("Mac off/on line + AD in the final pass"), false },
		{ TEXT("escort"), EKind::LandRule, TEXT(""), TEXT(""), TEXT("escort"), true, TEXT("overrun passed the DD's measure 23 Nov; Mac off/on + AD in the final pass"), false },
		{ TEXT("worn"), EKind::LandRule, TEXT(""), TEXT(""), TEXT("worn"), true, TEXT("look AD PASS 23 Nov; Mac 45 fps in the final pass"), false },
		{ TEXT("wet"), EKind::LandRule, TEXT(""), TEXT(""), TEXT("wet"), true, TEXT("the AD's gate 25 Nov 14:02Z: NOT YET, small mend, stays ON: on snow every wheeled vehicle stands in a near-black patch twice its size (B4 shot 0, L 42 on snow L 158). Mend: no darker than L 100, feathered, 1.2 hull lengths at most"), false },
		{ TEXT("captured"), EKind::LandRule, TEXT(""), TEXT(""), TEXT("captured"), true, TEXT("look AD PASS 23 Nov; Mac 45 fps in the final pass"), false },
		{ TEXT("wrecks"), EKind::LandRule, TEXT(""), TEXT(""), TEXT("wrecks"), true, TEXT("AD PASS 24 Nov 21:01Z; Mac in the final pass"), false },
		{ TEXT("life"), EKind::LandRule, TEXT(""), TEXT(""), TEXT("life"), false, TEXT("pass 2 (5wfo8w c25bad76, Brian 13:40Z: no squad all doing the same thing at once): each soldier his own part of his squad's drill, looks only; off is today's drill; Mac P11 Normandy same seed -TheaterLandNoSquadLife vs -TheaterLandRuleSquadLife, fps both ways + two 20 s clip pairs, then the AD"), false },
		{ TEXT("hatches"), EKind::LandRule, TEXT(""), TEXT(""), TEXT("hatches"), false, TEXT("pass 2 (5wfo8w 0fc6ba25, Brian 15:11Z): each tank commander closes his hatch in his own time, green early, veterans late; looks only; off: a platoon shuts as one at 120 m; Mac P11b Prokhorovka same seed -TheaterLandNoHatches vs -TheaterLandRuleHatches, fps both ways + a 20 s clip pair on a platoon under fire, then the AD"), false },
		{ TEXT("exits"), EKind::LandRule, TEXT(""), TEXT(""), TEXT("exits"), false, TEXT("pass 2 (5wfo8w 4ed05f8a, 76c4d51d; Brian 17:11Z, \"some soldiers freeze and then disappear\"): the drill half of that bug, ground's trial word leave is the 3D half and the two stack: routing men run on past the field's edge instead of halting on it, and the men of a block that has left the fight stay in the feed, marked lv, running or driving to their own edge until each is past it; looks only, a battle ends the same on or off (tests/land-exits 8 of 8); -TheaterLandRuleExits / -TheaterLandNoExits; Mac P36: Normandy custom battle, All fall back at about 60 s, one off/on pair with leave ON in both, 20 s of the men at the edge, fps both ways, then the AD; then ON. The same thread's seasoned, leaders, tankmind and tankmg rows are wired with nothing behind them yet: no ship row and no Mac pass until their builds land"), false },
		{ TEXT("pierce"), EKind::LandRule, TEXT(""), TEXT(""), TEXT("pierce"), false, TEXT("pass 2 (5wfo8w 55645e1c, dab26f1d; Brian 15:58Z, \"cannon fire or bazookas should be the only think taking them out\"): only anti-tank weapons take a tank: a blow on a tank block is weighed by what the striker carries that can pierce armour, the rest falls on the men with her; a REAL rule, it changes results; DO NOT SHIP ON: the design director's paired measure (5,185 battles) passes 47 of 51 marks, the 4 failures all the shipping set with the tanks defending (attack wins 59.6% off, 48.8% on, bar 8 points); his card is with Brian (keep off, offer as a choice, or ship anyway); off identical (89 of 89 digests, tests/battle-land-pierce); joined with sniper teams (LAND.pierce.teams snp 0, design director 22:19Z) and ground's teams rule (split fire lands through tally); -TheaterLandRulePierce / -TheaterLandNoPierce; no Mac pass until Brian rules"), false },
		{ TEXT("teamwork"), EKind::LandRule, TEXT(""), TEXT(""), TEXT("teamwork"), false, TEXT("pass 2 (pjgb8e 9a6ba447 + 2462d1da): \"Gun crews at work\" (the UI director's name 17:55Z), machine gun and mortar teams at work, each team's guns and crews as men with jobs, looks only; 63a3a2a7 adds three drawn drills (last-ditch final fire, ammunition runs with bearers, jams and misfires); Mac Normandy same seed -TheaterLandNoTeamwork vs -TheaterLandRuleTeamwork for fps, two 20 s clips (an MG team, a mortar team firing), then the AD"), false },
		{ TEXT("snipers"), EKind::LandRule, TEXT(""), TEXT(""), TEXT("snipers"), false, TEXT("pass 2 (s0jerm ff412061 on 5wfo8w c25bad76; Brian 16:07Z \"yes\"): each army's foot gives its share of power to a few sniper pairs hidden in houses, woods and hedges, few kills, much nerve and tempo; NOT looks only, but the design director's full fairness gate passes on build A (pin cap 20 s, bite 0.6; rulings 16:30Z, 17:48Z, 18:55Z); off identical to c25bad76 on 60 land battles; needs Forest ambushes (hide) on, else no teams; the whole-save identity check (id2) running; the 3D pre-check CLEAR 19:38Z; -TheaterLandRuleSnipers / -TheaterLandNoSnipers; ON only once the 3D draws a hidden shot as hidden (the design director's guard 3): sound's reader of the shot's sn/hd is in (5qkrdl fd27106d via aed17ed4, LandPlay/LandSound), still owed the effects thread's no flash or tracer for a hidden shot and the soldiers' scoped rifle and binoculars (MenOut.sn); then a Mac battle with snipers on, fps both ways, and the AD's clip of a hidden shot"), false },
		{ TEXT("japan"), EKind::LandRule, TEXT(""), TEXT(""), TEXT("japan"), false, TEXT("pass 2 (ground uhhxrn f8974f4f merge): Japan's own Type 97 tank and 75 mm gun in campaign battles, looks only (13faf36a + mends); -TheaterLandRuleJapan; Mac stills, then the AD"), false },
		{ TEXT("island"), EKind::LandRule, TEXT(""), TEXT(""), TEXT("island"), false, TEXT("pass 2 (ground uhhxrn f8974f4f merge): a campaign battle on an island drawn as that island (24ac6ac0); needs the Japan and Pacific thread's ticket.look/latLon; -TheaterLandRuleIsland; Mac still, then the AD"), false },
		{ TEXT("teams"), EKind::LandRule, TEXT(""), TEXT(""), TEXT("teams"), false, TEXT("pass 2 (ground uhhxrn 77c69bb5; card label \"Teams with the infantry\" from 4252a1c7; the UI director's name 17:55Z \"Support teams in battle\"): every infantry force with its own machine gun and mortar teams in a campaign battle; NOT looks only (team blocks on the campaign's ticket), so it waits the design director's ruling, then the Mac and the AD; -TheaterLandRuleTeams / -TheaterLandNoTeams; since ground fee09458 (design director 21:54Z) it does nothing unless facing is on too (and, since ground 88c40891, stand), and with both on the campaign card prices the teams (teamfire.ts, its own key teamfire, never the brigades' teams flag; DD 22:11Z: one row, two readers; the campaign does not yet learn the row: ground writes that wiring)"), false },
		{ TEXT("facing"), EKind::LandRule, TEXT(""), TEXT(""), TEXT("facing"), false, TEXT("pass 2 (ground uhhxrn fee09458, 40056789; the design director's ruling 21:54Z on the teams gate's trace; the UI director's name 22:14Z \"Units face the fire that hurts most\"): a block is held only by the enemy whose blows cost it most after its cover, and turns to meet one that costs it more; off, any blow on its front holds it (ruled a fault); NOT looks only, it moves every battle, so it has its own fairness gate (being fought) and stays off until that gate and the design director pass it, then the Mac; teams needs it; since ground 88c40891 (the design director 01:02Z) it does nothing unless stand is on too; the Mac clip P37 is ON HOLD (the AD wants it three ways on one seed: today, stand, stand with facing); -TheaterLandRuleFacing / -TheaterLandNoFacing"), false },
		{ TEXT("stand"), EKind::LandRule, TEXT(""), TEXT(""), TEXT("stand"), false, TEXT("pass 2 (ground uhhxrn 88c40891, b151db06; the AD's number 00:50Z and the design director's ruling 01:02Z: the rocking mend on its own switch and gate; \"Units keep their stand\", ground's name until the UI director names it): a block on an attack order that has stood to fight its target stands while the target is within its hold and closes again only beyond it; off, it asks afresh every tick and rocks on the spot (ruled a fault); NOT looks only, it moves every battle, so its own fairness gate, then the Mac (P37 three ways) and the AD; facing needs it, teams needs both; -TheaterLandRuleStand / -TheaterLandNoStand"), false },
		{ TEXT("scars"), EKind::LandRule, TEXT(""), TEXT(""), TEXT("scars"), false, TEXT("the AD's last word NOT YET (aged too far, mended, not judged again); an open drill fault (ground's)"), false },
		{ TEXT("ruts"), EKind::LandRule, TEXT(""), TEXT(""), TEXT("mud"), false, TEXT("the AD's last word NOT YET; needs a field fought over in the mud to judge"), false },
		{ TEXT("bearers"), EKind::LandRule, TEXT(""), TEXT(""), TEXT("bearers"), false, TEXT("standing rule: off (the get-up bug)"), false },
		{ TEXT("smoke"), EKind::LandRule, TEXT(""), TEXT(""), TEXT("smoke"), false, TEXT("standing rule: off until the tactics mend"), false },
		{ TEXT("noseArt"), EKind::LandRule, TEXT(""), TEXT(""), TEXT("noseArt"), false, TEXT("pass 2 (air wave 1, 0byihx 805b71bc): the paintings passed the AD; ON once the Mac's C13/C13b shows them placed on the bombers"), false },
		{ TEXT("unitColours"), EKind::LandRule, TEXT(""), TEXT(""), TEXT("unitColours"), false, TEXT("pass 2 (air 0byihx 30899121, earned colours): the Battle rules card's Earned colours row; waits the AD's read of the palette on the Mac"), false },
		{ TEXT("flyTroops"), EKind::LandRule, TEXT(""), TEXT(""), TEXT("flyTroops"), false, TEXT("off: no raid on troops has a battle in the air until the air rules thread's side joins, so on it would show nothing"), false },
		{ TEXT("flyDefend"), EKind::LandRule, TEXT(""), TEXT(""), TEXT("flyDefend"), false, TEXT("off: the month stop it needs is not built yet (the stand-in offer is -TheaterAirScrambleDemo)"), false },
		// ---- The campaign map ----
		{ TEXT("monthcard"), EKind::Launch, TEXT("Map: the month card"), TEXT("The month is shown on a leather and brass card with your flag across its head, in place of the desk calendar. Off: the calendar."), TEXT("-TheaterMonthCard"), true, TEXT("mock-up AD PASS 24 Nov 18:56Z; Mac stills in the final pass"), true },
		{ TEXT("towncard"), EKind::Launch, TEXT("Map: the town card"), TEXT("A province's panel opens on its town's street, with its figures as a card. Off: the plain panel."), TEXT("-TheaterTownCard"), true, TEXT("design AD PASS 24 Nov 17:53Z; Mac stills in the final pass"), true },
		{ TEXT("seawaits"), EKind::Launch, TEXT("Map: the month at sea waits for you"), TEXT("In a month that opens with the enemy's moves near you, the month's scene at sea waits behind THE MONTH AT SEA, WATCH. Off: it plays straight after the moves."), TEXT("-TheaterSeaWaits"), false, TEXT("pass 2 (UI director 22e207df, Brian's card 25 Nov 15:12Z: the sea scene waits behind a button after the moves near you; also tc.UI.SeaWaits); off is today's scene exactly; pre-check PASS 15:35Z; Mac P17 stills, then the AD and the UI director"), true },
		{ TEXT("far"), EKind::Launch, TEXT("Map: the far side of the world"), TEXT(""), TEXT("-TheaterFar"), false, TEXT("the Japan and Pacific rules are in (pass 2); waits the front door (42h725 76a3d50d) and the Mac; 1125r a4f8ca42: true reach (88342311) comes on with this switch (a besieged port fed only by ships that truly reach it)"), true },
		{ TEXT("monthaside"), EKind::Variable, TEXT("Map: the table moves while the world plays its month"), TEXT("When you end a month, the other countries play theirs while the table goes on moving, instead of holding still until they are done. Off: it holds still. The same month either way."), TEXT("tc.Speed.MonthAside"), true, TEXT("map-speed lane; Mac month ends in the final pass"), false },
		{ TEXT("wakes"), EKind::Option, TEXT(""), TEXT(""), TEXT("wakes"), true, TEXT("Mac look + AD in the final pass (OPTIONS row Fading wakes)"), false },
		{ TEXT("nightraid"), EKind::Variable, TEXT("Map: towns under a night raid"), TEXT(""), TEXT("tc.Look.NightRaid"), false, TEXT("pass 2 (city glance f688ee1e): ON after the Mac's stills and the AD"), false },
		{ TEXT("flakring"), EKind::Variable, TEXT("Map: flak rings round towns"), TEXT(""), TEXT("tc.Look.FlakRing"), false, TEXT("held: the Mac's A11/A12 wait city glance's flak ring mend"), false },
		{ TEXT("shipway"), EKind::Variable, TEXT("Map: fleets sail a ship's course"), TEXT(""), TEXT("tc.Map.ShipWay"), false, TEXT("pass 2 (city glance 1125o 6e7b71cb): fleets sail a ship's course on the campaign map; also -TheaterShipWay; Mac clip, then the AD"), false },
		{ TEXT("shipride"), EKind::Variable, TEXT("Map: ships heel and ride the swell"), TEXT(""), TEXT("tc.Map.ShipRide"), false, TEXT("pass 2 (city glance 1125o 6e7b71cb): her ships heel and ride the swell; also -TheaterShipRide; Mac clip, then the AD"), false },
		{ TEXT("ease"), EKind::Variable, TEXT("Map: screens ease in and out"), TEXT(""), TEXT("tc.UI.Ease"), false, TEXT("pass 2 (city glance 1125o 6e7b71cb): screens ease in and out, Esc one screen back; also -TheaterEase; Mac stills, then the UI director"), false },
		{ TEXT("nextall"), EKind::Variable, TEXT("Map: Tab round the month's list"), TEXT(""), TEXT("tc.UI.NextAll"), false, TEXT("pass 2 (city glance 1125o 6e7b71cb): Tab goes round the month's list once every army has orders; also -TheaterNextAll; Mac steps, then the UI director"), false },
		{ TEXT("armyweight"), EKind::Variable, TEXT("Map: armies with weight"), TEXT(""), TEXT("tc.Map.ArmyWeight"), false, TEXT("pass 2 (city glance 1125q 364d4bc5; art director 80a55823, 3D pre-check CLEAR 16:43Z): a piece on the campaign map is carried, hangs, lands and rocks; also -TheaterArmyWeight; Mac clip, then the AD"), false },
		{ TEXT("shipsmoke"), EKind::Variable, TEXT("Map: funnel smoke"), TEXT(""), TEXT("tc.Map.ShipSmoke"), false, TEXT("pass 2 (city glance 1125q 364d4bc5; art director e23f13fd, 3D pre-check CLEAR 17:33Z): ships under way trail funnel smoke on the campaign map (measures tc.Map.ShipSmokeMuch, tc.Map.ShipSmokeLong); also -TheaterShipSmoke; Mac clip, then the AD; 1125r a4f8ca42 adds the art director's funnel ribbon (5c1be70c) and dun smoke (c30701f2, ShipSmoke.h)"), false },
		{ TEXT("surf"), EKind::Variable, TEXT("Map: surf on the coasts"), TEXT(""), TEXT("tc.Map.Surf"), false, TEXT("pass 2 (city glance 1125r a4f8ca42; art director ae777dc9, 3D pre-check CLEAR 19:18Z): breakers along the campaign map's coasts; also -TheaterSurf, strength tc.Map.SurfMuch 0 to 2; NEW for a map row: Tools/build_content.sh once on the Mac with the editor closed makes M_SeaSurf, committed as the other materials are, M_Sea.uasset unchanged; without it the switch does nothing and the log says so; frame rate on and off (481 nodes for 307 on every sea pixel); Mac film steps in joins/lvu9ek-surf-onto-c30701f2-NOTES.md; then the AD"), false },
		{ TEXT("fleetheading"), EKind::Variable, TEXT("Map: fleets keep the heading they came in on"), TEXT("Fleets at sea keep pointing the way they sailed in, with their wake behind them. Only the flag turns to face you."), TEXT("tc.Map.FleetHeading"), true, TEXT("pass 2 (art director lvu9ek 726a543d on f75f144e; Brian's 03:50Z bug, the wakes all point one way): a fleet at sea keeps the heading she arrived on, her wake astern, only her flag and plate turn to the eye; also -TheaterFleetHeading; ON: the Mac's P42 (at rest and speed 13:12Z, the Channel arrival p42b 17:30Z, rates at rest no worse) and the art director's PASS of all three parts 17:30Z; its switch stays in Options"), false },
		{ TEXT("onelook"), EKind::Variable, TEXT("Map: one look for the whole war"), TEXT("Paint and national markings stay the same all war. A country's planes and tanks keep one look from the first month to the last."), TEXT("tc.Map.OneLook"), true, TEXT("pass 2 (art director lvu9ek 7ff7eceec on 726a543d; Brian 03:55Z \"have the paint be on it regardless of the year\", his standing rule 03:56Z): the dominions' own roundels all war, the Mustang in olive drab, the Fw 190 in the Bf 109's greens, the code-built German tanks grey; on the Play head also no aircraft scheme by the month (the air thread's dated paints); also -TheaterOneLook; ON: the Mac's P43 (the look, 0 errors, \"2 coats and 1 marks\") and P43b (London 0.34 rested, OFF 57.3 low 50.5, ON 57.6 low 50.5) and the art director's PASS of both 17:36Z and 17:59Z; its switch stays in Options; off still brings the dated marks back"), false },
		{ TEXT("teamsrule"), EKind::CampaignRule, TEXT("Rules: machine gun and mortar brigades"), TEXT(""), TEXT("teamsrule"), false, TEXT("pass 2 (\"Machine gun and mortar brigades\", the UI director's name 17:57Z; city glance 1125q 364d4bc5; balance sweep 4c6e8b5b, 3D pre-check CLEAR 17:29Z): buildable machine-gun and mortar brigades; its own Options row \"Support teams\" stays; ON only after the design director's ruling on its sweep, the 96 gate, a Mac pass and the AD"), false },
		{ TEXT("townqueuerule"), EKind::CampaignRule, TEXT("Rules: building queues for towns"), TEXT(""), TEXT("townqueuerule"), false, TEXT("pass 2 (city glance 1125q 364d4bc5; UI director 3f175773, pre-check PASS 17:33Z): a queue for a town and building from the map; its own Options row \"Building queues\" stays; Mac stills 61a-61d (run sheet P23), then the AD"), false },
		{ TEXT("citydefencesrule"), EKind::CampaignRule, TEXT("Rules: city defences"), TEXT(""), TEXT("citydefencesrule"), false, TEXT("pass 2 (joiner 1125s c0fa5070, city defences 95044c04; 3D pre-check CLEAR): searchlights beside the anti-aircraft guns, the guns by night by what they see, a garrison's town works; its own Options row \"City defences\" stays; OFF until the Mac's raid card and town stills, the UI director's words and the AD"), false },
		// ---- The sea battle's looks (storms and night at sea; Brian 25 Nov 04:20Z) ----
		{ TEXT("seaspray"), EKind::Launch, TEXT("Sea looks: spray over the bows"), TEXT("From a strong breeze up, a ship heading into the sea throws sheets of spray over her bow. Off: no spray."), TEXT("-TheaterSeaSpray=1"), false, TEXT("pass 2 (d2zd89 eb5f13eb; compile PASS 16:03Z): waits for its Mac stills and the AD (S6)"), false },
		{ TEXT("seaice"), EKind::Launch, TEXT("Sea looks: ice on hulls"), TEXT("In Arctic and winter seas, ships carry ice on their hulls and upperworks. Off: bare hulls."), TEXT("-TheaterSeaIce=1"), false, TEXT("pass 2 (d2zd89 eb5f13eb; compile PASS 16:03Z): waits for its Mac stills and the AD (S6)"), false },
		{ TEXT("seanight"), EKind::Launch, TEXT("Sea looks: searchlights and star shells"), TEXT("In a night action, ships sweep the sea with searchlights and fire star shells that light it. Off: none."), TEXT("-TheaterSeaNightFight=1"), false, TEXT("pass 2 (d2zd89 eb5f13eb; compile PASS 16:03Z): Mac (TC_REBUILD=M_SkyDome,M_NightSky) and the AD (S7)"), false },
		{ TEXT("seaclouds"), EKind::Launch, TEXT("Sea looks: storm clouds and far rain"), TEXT("The day's clouds fill the sea battle's sky, their shade drifts over the sea and the ships, and rain hangs in the distance. Off: the plain sky."), TEXT("-TheaterSeaClouds=dome -TheaterSeaCloudShade=1 -TheaterSeaRain=1"), false, TEXT("pass 2 (d2zd89 eb5f13eb; compile PASS 16:03Z): Mac (frame cost, no seam; a 0.5 day and a deck,0.7 day for the AD, HANDOFF-R2 check 5) and the AD (S6)"), false },
		// ---- The whole picture (every scene: Look/WholeLook.h; Brian, 25 Nov 03:58Z "better overall graphics") ----
		{ TEXT("crisp"), EKind::Variable, TEXT("Picture: crisp"), TEXT("The picture is sharpened, and in battle the ground stays clear where it lies at a slant. Off: the softer picture."), TEXT("tc.Look.Crisp"), false, TEXT("the AD 25 Nov 19:29Z on the look reel (5ddffbaa, 31 runs): harmless on the map (R4c); in battle not judged, the reel held no matching pair; off until one full-size pair of one battle scene, off and on, and the reel 2 timing (R7); graphics thread wmdk4s"), false },
		{ TEXT("depth"), EKind::Variable, TEXT("Battle looks: shade where things meet, near and far"), TEXT("Hedges, woods, tanks and houses sit in the field with the soft shade under and between them, as far as the battle is seen. Off: the shade fades out 80 m from the camera."), TEXT("tc.Look.Depth"), false, TEXT("the AD 25 Nov 19:29Z on the look reel (5ddffbaa): passes on looks at 0.65; speed unproven (R1b missed at 120 s, the reel cannot say which look cost it); ON when the graphics thread's switching run holds 45 fps (Mac reel 2, R7, timed in battle with -TheaterLookTime=, wmdk4s 722e5934); graphics thread wmdk4s"), false },
		{ TEXT("bounce"), EKind::Variable, TEXT("Battle looks: light given back by the ground"), TEXT("Every field gives back its own colour into the shade, as snow does: sand warms a tank's shaded side, grass gives a little green. From the next battle. Off: the shaded sides take a dark grey from below on every ground but snow."), TEXT("tc.Look.Bounce"), false, TEXT("the AD 25 Nov 19:29Z on the look reel (5ddffbaa): passes on looks, no frame cost, snow unchanged; one mend owed: a cap on its brightness so pale fields keep their shade (at full strength a steppe barn's shaded wall is 0.91 of its lit one); ON when the cap is in and the AD has two stills: the Prokhorovka village at the cap, a desert still with men or a vehicle near (from a recorded battle's named stills); the cap is in (wmdk4s 0f0a8567, held to 0.17), so it waits only the AD's capped pair (Mac reel 2, R10); graphics thread wmdk4s"), false },
		{ TEXT("wornpaint"), EKind::Variable, TEXT("Battle looks: worn paint"), TEXT("Tanks, guns and lorries carry weathered paint, patchy and flat, not one smooth even coat. From the next battle. Off: the even coat."), TEXT("tc.Look.Worn"), false, TEXT("the AD 25 Nov 19:29Z on the look reel (5ddffbaa): the men's boots and the running gear's ground colour pass; the paint patches do not show (a sunlit panel varies 9 of 255, 10 asked; the hull flatter than off); the mend is in (wmdk4s 0f0a8567: the patches' own depth, 0.78 to 1.22 of the paint; the Mac needs TC_REBUILD=M_LandWorn once), so it waits its mended pair (Mac reel 2, R9: stills 03 and 07 at full size); graphics thread wmdk4s"), false },
		// ---- The grand campaign's rules (each row's own switch is on OPTIONS, in a grand campaign; the war keeps it) ----
		{ TEXT("nicknames"), EKind::CampaignRule, TEXT(""), TEXT(""), TEXT("nicknames"), true, TEXT("Mac stills in the final pass"), false },
		{ TEXT("marked"), EKind::CampaignRule, TEXT(""), TEXT(""), TEXT("marked"), true, TEXT("AD PASS 24 Nov 18:55Z; Mac still 60f in the final pass"), false },
		{ TEXT("taskforces"), EKind::CampaignRule, TEXT(""), TEXT(""), TEXT("taskforcerule"), true, TEXT("Mac look in the final pass"), false },
		{ TEXT("rearguards"), EKind::CampaignRule, TEXT(""), TEXT(""), TEXT("rearguardrule"), true, TEXT("the AD 25 Nov 14:11Z: nothing is drawn on the map, so no art gate; ON. What it shows is words (the army card's HOLD AS REARGUARD row): the UI director reads the Mac's A14 stills"), false },
		{ TEXT("rearbridges"), EKind::CampaignRule, TEXT(""), TEXT(""), TEXT("rearbridgesrule"), true, TEXT("ON with rearguards, its parent (rearBridgesOn needs rearguardOn); Mac look (hand check 3)"), false },
		{ TEXT("shadowing"), EKind::CampaignRule, TEXT(""), TEXT(""), TEXT("shadowrule"), true, TEXT("Mac look in the final pass (hand check 3)"), false },
		{ TEXT("voyagelog"), EKind::CampaignRule, TEXT(""), TEXT(""), TEXT("voyagelogrule"), true, TEXT("Mac look in the final pass (hand checks 2, 3)"), false },
		{ TEXT("seamines"), EKind::CampaignRule, TEXT(""), TEXT(""), TEXT("seaminesrule"), true, TEXT("the AD 25 Nov 14:11Z: nothing is drawn on the map, so no art gate; ON. What it shows is words (a fleet's page: SEAS TO MINE, LAY MINES): the UI director reads the Mac's A14 stills"), false },
		{ TEXT("heavyseas"), EKind::CampaignRule, TEXT(""), TEXT(""), TEXT("heavyseasrule"), true, TEXT("the AD 25 Nov 14:11Z: nothing is drawn on the map, so no art gate; ON. What it shows is words (a fleet's card: the gale line): the UI director reads the Mac's A14 stills"), false },
		{ TEXT("shadowrelay"), EKind::CampaignRule, TEXT(""), TEXT(""), TEXT("shadowrelayrule"), true, TEXT("Mac look in the final pass"), false },
		{ TEXT("fleetwhy"), EKind::CampaignRule, TEXT(""), TEXT(""), TEXT("fleetwhyrule"), true, TEXT("Mac look in the final pass"), false },
		{ TEXT("settledtowns"), EKind::CampaignRule, TEXT(""), TEXT(""), TEXT("settledtownsrule"), true, TEXT("Mac look in the final pass"), false },
		{ TEXT("columns"), EKind::CampaignRule, TEXT(""), TEXT(""), TEXT("columns"), true, TEXT("Mac stills + 45 fps on the map in the final pass (a new war's first month only)"), false },
		{ TEXT("lorryshift"), EKind::CampaignRule, TEXT(""), TEXT(""), TEXT("logistics:lorryShift"), true, TEXT("Mac stills in the final pass (a new war's first month only)"), false },
		{ TEXT("wagons"), EKind::CampaignRule, TEXT(""), TEXT(""), TEXT("logistics:wagons"), true, TEXT("Mac stills in the final pass (a new war's first month only)"), false },
		{ TEXT("dumps"), EKind::CampaignRule, TEXT(""), TEXT(""), TEXT("logistics:dumps"), true, TEXT("Mac stills in the final pass (a new war's first month only)"), false },
		{ TEXT("seekonce"), EKind::CampaignRule, TEXT(""), TEXT(""), TEXT("seekoncerule"), true, TEXT("pass 2 (navy moves): its gate PASSED; Mac look in pass 2"), false },
		{ TEXT("beatreach"), EKind::CampaignRule, TEXT(""), TEXT(""), TEXT("beatreachrule"), false, TEXT("pass 2 (navy etbb99 4d58da41): a fleet whose base moves patrols a sea in its new harbour's reach; its own Options row \"A moved fleet's beat in reach\"; the design director PASSED it 26 Nov 02:14Z on both charts; nothing new is drawn, so no art gate; OFF until the Mac's look (a fleet's page after a withdrawal) and the UI director's words"), false },
		{ TEXT("ownwater"), EKind::CampaignRule, TEXT(""), TEXT(""), TEXT("ownwaterrule"), false, TEXT("pass 2 (navy L1Pd via city glance 1125n): ships keep clear of their own side's torpedoes, in fleet actions fought live only; off until the Mac's destroyer clip and the AD; on a pass R2_SHIP gains ownwaterrule: setOwnWater(s, true); 1125r a4f8ca42 adds its countdown and words (310a1434)"), false },
		{ TEXT("atom"), EKind::CampaignRule, TEXT(""), TEXT(""), TEXT("atomrule"), false, TEXT("pass 2: its gate PASSED; waits the Mac's stills and the AD and UI director's read"), false },
		{ TEXT("airnames"), EKind::CampaignRule, TEXT(""), TEXT(""), TEXT("airnames"), false, TEXT("pass 2 (air lanes): the air thread adds it after its Mac look"), false },
		{ TEXT("carrierstrike"), EKind::CampaignRule, TEXT(""), TEXT(""), TEXT("carrierstrikerule"), false, TEXT("pass 2 (air lanes): off until its Mac look"), false },
		{ TEXT("wingbydate"), EKind::CampaignRule, TEXT(""), TEXT(""), TEXT("wingbydaterule"), false, TEXT("pass 2 (air lanes): off until its Mac look"), false },
	};

	/** -TheaterR2Off holds the whole list back for this run; -TheaterR2Without=Key,Key the named features. */
	bool HeldForRun(const TCHAR* Key)
	{
		const TCHAR* Line = FCommandLine::Get();
		if (FParse::Param(Line, TEXT("TheaterR2Off")))
		{
			return true;
		}
		FString Without;
		if (!FParse::Value(Line, TEXT("TheaterR2Without="), Without, false))
		{
			return false;
		}
		TArray<FString> Named;
		Without.ParseIntoArray(Named, TEXT(","), true);
		for (const FString& One : Named)
		{
			if (One.TrimStartAndEnd().Equals(Key, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	/** Each word of a feature's launch words ("-TheaterLandEpic -TheaterLandWind"). */
	TArray<FString> WordsOf(const TCHAR* Words)
	{
		TArray<FString> Each;
		FString(Words).ParseIntoArrayWS(Each);
		return Each;
	}

	/** The word's name as it starts a token: "-TheaterLandEpic", or "-TheaterLandOldWrecks=" for a word with a value. */
	FString StemOf(const FString& Word)
	{
		int32 Equals = INDEX_NONE;
		return Word.FindChar(TEXT('='), Equals) ? Word.Left(Equals + 1) : Word;
	}

	/** Where in Line a token starting with Stem begins (a bare word must end there; one with "=" runs on), or INDEX_NONE. */
	int32 FindToken(const FString& Line, const FString& Stem, int32 From = 0)
	{
		const bool bValued = Stem.EndsWith(TEXT("="));
		for (int32 At = Line.Find(Stem, ESearchCase::IgnoreCase, ESearchDir::FromStart, From); At != INDEX_NONE;
			At = Line.Find(Stem, ESearchCase::IgnoreCase, ESearchDir::FromStart, At + 1))
		{
			const bool bStarts = At == 0 || FChar::IsWhitespace(Line[At - 1]);
			const int32 After = At + Stem.Len();
			const bool bEnds = bValued || After >= Line.Len() || FChar::IsWhitespace(Line[After]);
			if (bStarts && bEnds)
			{
				return At;
			}
		}
		return INDEX_NONE;
	}

	/** Puts a feature's words on the command line, each one not there already. */
	void PutWords(const TCHAR* Words)
	{
		for (const FString& Word : WordsOf(Words))
		{
			if (FindToken(FCommandLine::Get(), StemOf(Word)) == INDEX_NONE)
			{
				FCommandLine::Append(*(TEXT(" ") + Word));
			}
		}
	}

	/** Takes a feature's words off the command line (every token of each, whatever value it was given). */
	void TakeWords(const TCHAR* Words)
	{
		FString Line = FCommandLine::Get();
		for (const FString& Word : WordsOf(Words))
		{
			const FString Stem = StemOf(Word);
			for (int32 At = FindToken(Line, Stem); At != INDEX_NONE; At = FindToken(Line, Stem, At))
			{
				int32 End = At + Stem.Len();
				while (End < Line.Len() && !FChar::IsWhitespace(Line[End]))
				{
					++End;
				}
				Line.RemoveAt(At, End - At);
			}
		}
		FCommandLine::Set(*Line);
	}

	/** -TheaterLandTrial= as typed on the launch, before the list laid its own: then the list leaves it as typed. */
	bool bTrialTyped = false;

	const TCHAR* const TrialStem = TEXT("-TheaterLandTrial=");

	/** The list's own -TheaterLandTrial=: every trial word that is on, put back as one word ("houses,church,..."). */
	void LayTrial()
	{
		if (bTrialTyped)
		{
			return;
		}
		TakeWords(TrialStem);
		TArray<FString> Words;
		for (int32 Index = 0; Index < Release2Ship::Count(); ++Index)
		{
			if (Release2Ship::At(Index).Kind == EKind::TrialWord && Release2Ship::IsOn(Index))
			{
				Words.Add(Release2Ship::At(Index).Words);
			}
		}
		if (Words.Num() > 0)
		{
			FCommandLine::Append(*FString::Printf(TEXT(" %s%s"), TrialStem, *FString::Join(Words, TEXT(","))));
		}
	}

	void SetVariable(const TCHAR* Name, bool bOn)
	{
		if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Variable->Set(bOn ? 1 : 0, ECVF_SetByCode);
		}
		else
		{
			UE_LOG(LogTheater, Warning, TEXT("Release 2 ship list: no console variable called %s"), Name);
		}
	}
}

int32 Release2Ship::Count()
{
	return UE_ARRAY_COUNT(ShipList);
}

const Release2Ship::FFeature& Release2Ship::At(int32 Index)
{
	return ShipList[FMath::Clamp(Index, 0, Count() - 1)];
}

bool Release2Ship::IsOn(int32 Index)
{
	if (Index < 0 || Index >= Count())
	{
		return false;
	}
	const FFeature& Feature = ShipList[Index];
	if (!Feature.bShips || HeldForRun(Feature.Key))
	{
		return false;
	}
	bool bOn = true;
	if (GConfig && (Feature.Kind == EKind::Launch || Feature.Kind == EKind::TrialWord || Feature.Kind == EKind::Variable || Feature.Kind == EKind::Option))
	{
		GConfig->GetBool(ShipSection, Feature.Key, bOn, GGameUserSettingsIni);
	}
	return bOn;
}

void Release2Ship::Apply()
{
	if (IsRunningCommandlet())
	{
		return; // (the content and sound builds run the editor as a commandlet: the list is the game's, not theirs)
	}
	bTrialTyped = FindToken(FCommandLine::Get(), TrialStem) != INDEX_NONE;
	TArray<FString> On, Off;
	for (int32 Index = 0; Index < Count(); ++Index)
	{
		const FFeature& Feature = ShipList[Index];
		if (!IsOn(Index))
		{
			Off.Add(FString::Printf(TEXT("%s (%s)"), Feature.Key, !Feature.bShips ? Feature.Note : HeldForRun(Feature.Key) ? TEXT("held for this run") : TEXT("switched off on OPTIONS")));
			continue;
		}
		On.Add(Feature.Key);
		if (Feature.Kind == EKind::Launch)
		{
			PutWords(Feature.Words);
		}
		else if (Feature.Kind == EKind::Variable)
		{
			SetVariable(Feature.Words, true);
		}
	}
	LayTrial();
	UE_LOG(LogTheater, Display, TEXT("Release 2 ship list: %d of %d features on: %s"), On.Num(), Count(), *FString::Join(On, TEXT(", ")));
	UE_LOG(LogTheater, Display, TEXT("Release 2 ship list: the launch words now: %s"), FCommandLine::Get());
	if (Off.Num() > 0)
	{
		UE_LOG(LogTheater, Display, TEXT("Release 2 ship list: off: %s"), *FString::Join(Off, TEXT("; ")));
	}
}

void Release2Ship::Flip(int32 Index)
{
	if (Index < 0 || Index >= Count() || !GConfig)
	{
		return;
	}
	const FFeature& Feature = ShipList[Index];
	if (!Feature.bShips || (Feature.Kind != EKind::Launch && Feature.Kind != EKind::TrialWord && Feature.Kind != EKind::Variable))
	{
		return;
	}
	if (HeldForRun(Feature.Key))
	{
		// (a test run's -TheaterR2Off or -TheaterR2Without: the row reads OFF whatever he chose, so a press here would save
		// the opposite of what he sees; nothing is saved, and the feature stays held for this run)
		UE_LOG(LogTheater, Display, TEXT("Release 2 ship list: %s is held back for this run (-TheaterR2Off or -TheaterR2Without): nothing saved"), Feature.Label);
		return;
	}
	bool bWas = true;
	GConfig->GetBool(ShipSection, Feature.Key, bWas, GGameUserSettingsIni);
	GConfig->SetBool(ShipSection, Feature.Key, !bWas, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	const bool bNow = IsOn(Index); // (his choice, unless a test word holds the feature back for this run)
	if (Feature.Kind == EKind::Launch)
	{
		bNow ? PutWords(Feature.Words) : TakeWords(Feature.Words);
	}
	else if (Feature.Kind == EKind::TrialWord)
	{
		LayTrial();
	}
	else
	{
		SetVariable(Feature.Words, bNow);
	}
	UE_LOG(LogTheater, Display, TEXT("Release 2 ship list: %s switched %s on OPTIONS (saved)%s"), Feature.Label, !bWas ? TEXT("ON") : TEXT("OFF"),
		Feature.bOnceARun ? TEXT("; it counts from the next start") : TEXT(""));
}

bool Release2Ship::Ships(EKind Kind, const TCHAR* Words)
{
	for (int32 Index = 0; Index < Count(); ++Index)
	{
		if (ShipList[Index].Kind == Kind && FCString::Stricmp(ShipList[Index].Words, Words) == 0)
		{
			return IsOn(Index);
		}
	}
	return false;
}

void Release2Ship::Keep(EKind Kind, const TCHAR* Words, bool bOn)
{
	for (int32 Index = 0; Index < Count(); ++Index)
	{
		const FFeature& Feature = ShipList[Index];
		if (Feature.Kind != Kind || FCString::Stricmp(Feature.Words, Words) != 0)
		{
			continue;
		}
		if (!GConfig || HeldForRun(Feature.Key))
		{
			return; // (held back for this run: his press holds for this run only)
		}
		GConfig->SetBool(ShipSection, Feature.Key, bOn, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
		return;
	}
}

FString Release2Ship::WarSwitchesJson()
{
	TArray<FString> Ids;
	for (int32 Index = 0; Index < Count(); ++Index)
	{
		if (ShipList[Index].Kind == EKind::CampaignRule && IsOn(Index))
		{
			Ids.Add(FString::Printf(TEXT("\"%s\""), ShipList[Index].Words));
		}
	}
	return FString::Printf(TEXT("[%s]"), *FString::Join(Ids, TEXT(",")));
}

TArray<Release2Ship::FRow> Release2Ship::OptionRows()
{
	TArray<FRow> Rows;
	for (int32 Index = 0; Index < Count(); ++Index)
	{
		const FFeature& Feature = ShipList[Index];
		if (!Feature.bShips || (Feature.Kind != EKind::Launch && Feature.Kind != EKind::TrialWord && Feature.Kind != EKind::Variable) || !*Feature.Label)
		{
			continue;
		}
		FRow& Row = Rows.AddDefaulted_GetRef();
		Row.Id = FString::Printf(TEXT("r2:%s"), Feature.Key);
		Row.Label = Feature.Label;
		Row.Tip = FString::Printf(TEXT("%s%s"), Feature.Tip, Feature.bOnceARun ? TEXT(" A change counts from the next time the game starts.") : TEXT(""));
		Row.bOn = IsOn(Index);
	}
	return Rows;
}

bool Release2Ship::Press(const FString& Id)
{
	if (!Id.StartsWith(TEXT("r2:")))
	{
		return false;
	}
	const FString Key = Id.Mid(3);
	for (int32 Index = 0; Index < Count(); ++Index)
	{
		if (Key.Equals(ShipList[Index].Key, ESearchCase::IgnoreCase))
		{
			Flip(Index);
			return true;
		}
	}
	return false;
}
