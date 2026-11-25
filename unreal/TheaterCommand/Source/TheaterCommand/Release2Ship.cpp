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
		{ TEXT("desertland"), EKind::TrialWord, TEXT("Battle looks: desert wadis and ridges"), TEXT(""), TEXT("desertland"), false, TEXT("pass 2 (nu6w43 12681e5a + round 2 972039b1, the AD's B5 ask: a wadi with thorn scrub across every desert field, a low ridge 300 to 700 m out); off, the desert is byte for byte as before; Mac B5 with -TheaterLandTrial=desertland, then the AD"), false },
		{ TEXT("islandsand"), EKind::TrialWord, TEXT("Battle looks: island sand and surf line"), TEXT(""), TEXT("islandsand"), false, TEXT("pass 2 (environments' patch on 12681e5a, LandTropic.cpp: warm sand on the islands, a darker, wider wet band at the water); off, the island is byte for byte as before; the AD passed it to the Mac 15:04Z with the shade tune (nu6w43 38258d63); Mac B11 island coast with -TheaterLandTrial=islandsand, then the AD's read"), false },
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
		{ TEXT("spotter"), EKind::Launch, TEXT("Battle camera: spotter's rings"), TEXT(""), TEXT("-TheaterLandSpotter"), false, TEXT("held: the camera thread is mending it"), false },
		{ TEXT("corner"), EKind::Launch, TEXT("Battle camera: film in a corner"), TEXT(""), TEXT("-TheaterLandCorner"), false, TEXT("held: the camera thread is mending it"), false },
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
		{ TEXT("far"), EKind::Launch, TEXT("Map: the far side of the world"), TEXT(""), TEXT("-TheaterFar"), false, TEXT("the Japan and Pacific rules are in (pass 2); waits the front door (42h725 76a3d50d) and the Mac"), true },
		{ TEXT("monthaside"), EKind::Variable, TEXT("Map: the table moves while the world plays its month"), TEXT("When you end a month, the other countries play theirs while the table goes on moving, instead of holding still until they are done. Off: it holds still. The same month either way."), TEXT("tc.Speed.MonthAside"), true, TEXT("map-speed lane; Mac month ends in the final pass"), false },
		{ TEXT("wakes"), EKind::Option, TEXT(""), TEXT(""), TEXT("wakes"), true, TEXT("Mac look + AD in the final pass (OPTIONS row Fading wakes)"), false },
		{ TEXT("nightraid"), EKind::Variable, TEXT("Map: towns under a night raid"), TEXT(""), TEXT("tc.Look.NightRaid"), false, TEXT("pass 2 (city glance f688ee1e): ON after the Mac's stills and the AD"), false },
		{ TEXT("flakring"), EKind::Variable, TEXT("Map: flak rings round towns"), TEXT(""), TEXT("tc.Look.FlakRing"), false, TEXT("held: the Mac's A11/A12 wait city glance's flak ring mend"), false },
		// ---- The whole picture (every scene: Look/WholeLook.h; Brian, 25 Nov 03:58Z "better overall graphics") ----
		{ TEXT("crisp"), EKind::Variable, TEXT("Picture: crisp"), TEXT("The picture is sharpened, and in battle the ground stays clear where it lies at a slant. Off: the softer picture."), TEXT("tc.Look.Crisp"), false, TEXT("held until the AD's read of the cloud mock-ups and the Mac A/B (stills + 45 fps); graphics thread wmdk4s"), false },
		{ TEXT("depth"), EKind::Variable, TEXT("Battle looks: shade where things meet, near and far"), TEXT("Hedges, woods, tanks and houses sit in the field with the soft shade under and between them, as far as the battle is seen. Off: the shade fades out 80 m from the camera."), TEXT("tc.Look.Depth"), false, TEXT("held until the Mac A/B on a full field (stills + 45 fps) and the AD; graphics thread wmdk4s"), false },
		{ TEXT("bounce"), EKind::Variable, TEXT("Battle looks: light given back by the ground"), TEXT("Every field gives back its own colour into the shade, as snow does: sand warms a tank's shaded side, grass gives a little green. From the next battle. Off: the shaded sides take a dark grey from below on every ground but snow."), TEXT("tc.Look.Bounce"), false, TEXT("held until the Mac A/B (strength 0.6 / 1.0 / 1.6 on desert, steppe, bocage) and the AD; graphics thread wmdk4s, agreed with ground + environments 25 Nov 04:26Z"), false },
		{ TEXT("wornpaint"), EKind::Variable, TEXT("Battle looks: worn paint"), TEXT("Tanks, guns and lorries carry weathered paint, patchy and flat, not one smooth even coat. From the next battle. Off: the even coat."), TEXT("tc.Look.Worn"), false, TEXT("held until the Mac A/B (stills, Tobruk desert + Normandy) and the AD (a sunlit panel varies 10 of 255 or more); graphics thread wmdk4s"), false },
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
