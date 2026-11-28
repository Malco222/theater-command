#pragma once

#include "Battle/LandPlay.h"

#include <cstdint>
#include <functional>
#include <array>
#include <map>
#include <string>
#include <vector>

/**
 * The ground battle's sound, decided in plain C++ from the sound director's mix (Content/Data/Sound/mix.json, read as
 * written: no number of it is copied here) so that the cloud test hears what the game hears and can count it.
 *
 * The director's rules for the field (21 November, by the land thread): drive the shots from the rounds; a shot on
 * the screen is always heard (the limits drop only what is off the screen or far); the same take never starts twice
 * within 0.15 s; a hit on a man is silent (his fall and, sometimes, his cry carry it); no incoming whistle; a moving
 * shooter's take moves with him (the scene's part); the distance low-pass's first point is near; log the shots heard
 * against the takes started.
 */
namespace LandSound
{
	using LandPlay::Vec;

	/** Where the ear is: the ground point the camera looks at, raised by the mix's heightFactor times the camera's
	 * height over it (the scene works it out), the camera's yaw, and the camera's height over the ground. */
	struct Listener
	{
		Vec At;
		double Yaw = 0.0;
		double CameraHeight = 2.0;
	};

	/** A one-shot to start: Delay real seconds from now, at At (or about the ear when not bSpatial), in the mix's
	 * distance family, on its bus, at Db over the file as made and Pitch; From seconds into the take, cut at Cut
	 * seconds (0: to its end) with CutFade. */
	struct Take
	{
		double Delay = 0.0;
		std::string Name;
		Vec At;
		std::string Family, Bus = "close";
		double Db = 0.0, Pitch = 1.0;
		double From = 0.0, Cut = 0.0, CutFade = 0.0, FadeIn = 0.0;
		bool bSpatial = true;
		bool bBig = false;
		/** The shooter's track whose take moves with him (-1 none). */
		int Track = -1;
		/** Release 2's battle acoustics (land.acoustics): an echo thrown back by a wood's edge or a house is the same
		 * recording dulled and thinned as that face does it, a low-pass and a high-pass over the whole take (Hz; 0: none),
		 * and bEcho says it is one (the scene's count; never a shot of its own). Every other take: 0, 0, false. */
		double LpfHz = 0.0, HpfHz = 0.0;
		bool bEcho = false;
	};

	/** A loop under the battle and its level now (a bed or the place's air), dB; Silent when it should not play. */
	struct Bed
	{
		std::string Name;
		double Db = 0.0;
		bool bSilent = true;
	};

	/** A distance family of the mix. */
	struct Family
	{
		double Full = 25.0, Silent = 700.0, FarFrom = 1e9;
	};

	/** A piece of a recording: the take, From seconds into it, Cut seconds long (0: to its end). */
	struct Piece
	{
		std::string Take;
		double From = 0.0, Cut = 0.0;
	};

	/** A table of (x, y) rows read linearly between rows and held at the ends (the mix's "...ByCameraHeight"). */
	struct Table
	{
		std::vector<std::pair<double, double>> Rows;
		double At(double X, double Default) const;
	};

	class Mixer
	{
	public:
		/** Reads the mix; false (and why) if it cannot. Seed makes the picks repeat run to run. */
		bool Begin(const std::string& MixText, uint64_t Seed, std::string& OutError);
		/** How long a take plays, for the limits (the scene gives the files' own; unset, about 1.5 s). */
		std::function<double(const std::string& Name)> Length;
		/** Whether a world point is on the screen (the scene's camera); unset, all is on it. */
		std::function<bool(const Vec& At)> OnScreen;
		/** The falls' thuds come from the soldiers' own clips (the body_fall event as the body lands): then a Fall noise
		 * gives only the cry. Off (the cloud test without the soldiers' core), the thud follows the fall by 0.7 s. */
		bool bThudFromClips = false;
		/** Life seconds a battle second: the rules' clock runs about twice as fast as life, so the scene plays the fight
		 * at half the battle clock (FLandBattleState::Life, 2). Hear's Speed is battle seconds a real second; what life
		 * times (a sound crossing the ground, Far / 343 s) reaches the ear Far / 343 / (Speed x Life) real seconds
		 * after its flash. 1: the battle clock is life's. */
		double Life = 1.0;
		/** Rain falling over the battle (release 2, the look bible's G10: LandWeather's Sound, 0 to 1): the mix's
		 * beds.place.rain played over the place's own air, heard through the fight, softer the lighter it falls. 0:
		 * none, as before. */
		double RainShare = 0.0;
		/** Release 2's battle acoustics (the mix's land.acoustics; Brian, 27 November 2026: "lets make the gunfire look
		 * more realistic", and its sound with it), three parts, each off until its own word is given (the scene's
		 * -TheaterLandSoundTravel, -TheaterLandSoundTails, -TheaterLandSoundEcho). All three off: every take is started
		 * as before, to the roll of the dice.
		 * bTravel: a rifle's or a machine gun's report crosses the ground at the speed of sound as the big guns' does, so
		 * a section firing 300 m off is seen to fire and heard most of a second later, and a round passing the ear cracks
		 * before the thump of the gun that fired it.
		 * bTails: a machine-gun burst cut to its rounds ends in its own recording's dying echo (the stretch after its
		 * firing), not in a fade of four hundredths of a second.
		 * bEcho: a shot, a gun or a blast is thrown back by the woods' edges and the houses round the ear: the same
		 * recording again from that face, later by the longer way round, softer, dulled and thinned. */
		bool bTravel = false, bTails = false, bEcho = false;
		/** The cover at a place of the field, for the echoes: 0 open ground, 1 woods, 2 a house, 3 water, 4 a hedgerow on
		 * its bank, and the ground's height there (the scene's field, asked once a cell). Unset: nothing throws a sound
		 * back. */
		std::function<int(double X, double Y, double& OutGroundZ)> CoverOf;
		/** The acoustics' log: small arms' reports held for their crossing and the longest held (real seconds); rounds
		 * whose crack was started ahead of their own report; bursts ended in their recording's tail; echoes started, off
		 * woods, off houses and off hedgerows; echoes left out with every echo voice sounding, and as too faint to hear;
		 * the most echoes started in any one real second; the faces found round the ear at the last look, the looks
		 * taken, and the cells of the field asked for their cover in all. */
		int Travelled = 0, CracksBeforeThump = 0, CracksAfterThump = 0, Tails = 0, TailsFaint = 0, TailsShared = 0, Echoes = 0, EchoesWoods = 0, EchoesWalls = 0, EchoesHedges = 0, EchoesFull = 0, EchoesUnder = 0, EchoesFaint = 0, EchoesMostASecond = 0, EchoFaces = 0, EchoLooks = 0, EchoCellsAsked = 0;
		double MostTravel = 0.0;
		/** The family's loss over distance as the scene's players have it (ALandBattle::FamilyOf: none to `full`, then
		 * falling evenly in decibels to -60 at `silent`), dB at Far metres: the echoes' own level is worked from it. */
		double LossDb(const std::string& FamilyName, double Far) const;

		/** A source heard: what to start. Now is the real clock (seconds), Speed the battle clock's against it (Life says
		 * how many life seconds a battle second is), Battle
		 * the battle time now (a noise's own time may lie a little behind it within the frame). */
		void Hear(const LandPlay::Noise& N, double Now, double Battle, double Speed, const Listener& Ear, std::vector<Take>& Out);
		/** A soldier's clip event heard (a footstep, his knee down, his kit, his bolt, a body landing): Cue the take's
		 * stem as the clip names it (footstep, knee_down, kit_rattle, bolt, body_fall, grenade_throw...). */
		/** The take a vehicle's own cue plays (land.bodyCues renames or silences it: empty for none), else the cue. */
		std::string CueTake(const std::string& Cue) const
		{
			auto Named = BodyCue.find(Cue);
			return Named == BodyCue.end() ? Cue : Named->second;
		}
		/** The loop a vehicle's engine plays: her clip's cue (tank_idle_loop, tank_loop ...), or the take land.engines
		 * names for that cue on her model (by the model's name: "Panzer IV", "Tiger"). */
		std::string EngineTake(const std::string& ModelName, const std::string& Cue) const
		{
			auto Model = EngineByModel.find(ModelName);
			if (Model == EngineByModel.end()) return Cue;
			auto Named = Model->second.find(Cue);
			return Named == Model->second.end() ? Cue : Named->second;
		}
		void HearBody(const std::string& Cue, const std::string& Army, const Vec& At, double Now, const Listener& Ear, std::vector<Take>& Out);
		/** Each frame: the meter and the beds. Fighting is how many blocks are in act 'fight' within the mix's
		 * blockRadius of the ear, FightingShare their share of all blocks (for a high camera); bContact once the armies
		 * have met; Cover the ground round the ear (open, woods, buildings, water). */
		void Tick(double Now, double RealDt, const Listener& Ear, int Fighting, double FightingShare, bool bContact, const std::string& Cover, std::vector<Bed>& OutBeds);
		double Meter() const { return MeterNow; }
		/** The meter's raw heat this frame, before it is scaled by fullAt and clamped (for setting fullAt). */
		double Heat() const { return HeatNow; }
		/** The mix's listener.heightFactor: the ear stands over the looked-at ground by this times the camera's
		 * height over it. */
		double EarHeightFactor() const { return HeightFactor; }
		/** A blast close enough to dull the ears (the mix's shellShock), taken once: its low-pass, and how long down,
		 * held and back (seconds). False when none is due. */
		bool TakeShellShock(double& OutLpfHz, double& OutDown, double& OutHold, double& OutBack, double& OutDb);
		/** The whole battle's lift down among the men (the mix's land.closeView), dB, for a camera this high over the
		 * ground: the scene adds it to the beds and the engines, the mixer to every take it starts. */
		double CloseDb(double CameraHeight) const { return CloseView.At(CameraHeight, 0.0); }

		// The log (the director's: shots heard against takes started).
		int ShotsHeard = 0, ShotTakes = 0, ShotsOutOfHearing = 0, ShotsDropped = 0, ShotsOnScreenDropped = 0, ShotsMerged = 0, Cracks = 0, Landings = 0, Falls = 0, Delayed = 0;
		/** The ground battle's own (land): bursts heard cut to their rounds, runs of cracks, rounds striking the ground
		 * near the ear (in the earth, whining off), Garand clips and reloads, shouts, shells heard coming in. */
		int ShotsByClip = 0, SmgBursts = 0, BurstsCut = 0, CrackRuns = 0, GroundStrikes = 0, Whines = 0, Clips = 0, Reloads = 0, Shouts = 0, Incoming = 0;
		/** Rifle and machine-gun rounds heard ringing off a vehicle's steel (land.armourStrikes), for the log. */
		int ArmourRings = 0;
		/** Effects (R2), small arms off armour: how many steel pings a burst of Rounds rattles (RattleOff: burstShare of
		 * them, at least 1, at most burstMost), and the two numbers as the mix gave them (the checks'). */
		int PingsOf(int Rounds) const;
		int ArmourPingsMost() const { return ArmourBurstMost; }
		double ArmourPingsShare() const { return ArmourBurstShare; }
		/** The same-take rule's last step, when no pitch was free: left out off the screen, held to a clear moment on it,
		 * a close rifle shot moved to a far recording, or (none of those) started at the farthest pitch. */
		int SameLeftOut = 0, SameHeldClear = 0, SameToFar = 0, SameToOtherFar = 0, SameCrowded = 0, SameMerged = 0;
		/** Close rifle shots kept on their own recording at a half step of pitch the whole steps skipped. */
		int SameCloseHalf = 0;
		/** Other takes nearer than their family's far takes (a Maxim's burst) kept on their recording at a half step. */
		int SameNearHalf = 0;
		/** Of those heard in a copy, the ones nearer than their family's far takes (a shot heard on its own, which a merge
		 * would hide: the test faults on any); and the machine-gun bursts crowded out of one recording that gave way to
		 * another on their list instead. */
		int SameMergedNear = 0, MgToOther = 0, MgToOtherPart = 0;
		/** Near MG bursts that, every stretch long enough crowded, played the longest stretch they fit, cut at its end. */
		int MgShortPart = 0;
		std::map<std::string, int> MergedNearBy;
		/** The longest a take was held back by the 0.15 s rule (real seconds), and the takes started on time at another
		 * pitch instead of held longer (land.sameTake), for the log. */
		double MostDelay = 0.0, MostBurstOver = -1.0;
		int Repitched = 0;
		std::map<std::string, int> TakesByName;
		/** A tank's close machine-gun bursts by her gun, army and the take picked ("roof_mg USA gunner_burst"), for the log. */
		std::map<std::string, int> TankMgTakes;
		/** Those whose take fires for less than the burst, so it played whole (its row holds a take too short). */
		int TankMgWhole = 0;
		/** A support team's close bursts by her gun's file and the take picked ("HmgMG34 mg42_burst_01"), for the log and the
		 * test; and those whose take fires for less than her burst, so it played whole. */
		std::map<std::string, int> TeamMgTakes;
		int TeamMgWhole = 0;
		/** Snipers' rounds heard: from his spot (found), and from where the round arrived (hidden), for the log and test. */
		int SniperSeen = 0, SniperUnseen = 0;
		/** Whether a take is on a list the mix names (byNation.mg.GER, tankMg.roof.USA ...), for the test. */
		bool InList(const std::string& ListName, const std::string& Name) const;
		bool HasList(const std::string& ListName) const { return Lists.count(ListName) > 0; }
		/** The heat each weight key has added over the run (weight x closeness), for the log. */
		std::map<std::string, double> HeatBy;
		/** The one-shot takes' players (land.voices): pooled, how many made before the battle, at most how many. */
		bool bVoicesPooled = true;
		int VoicesWarm = 96, VoicesMost = 256;

	private:
		std::map<std::string, Family> Families;
		std::map<std::string, int> Limits;
		int WorldTotal = 64;
		double FtbFrom = 150.0, FtbSpeed = 343.0, FtbMost = 3.0;
		double HeightFactor = 0.6;
		// The meter.
		double Window = 6.0, Radius = 800.0, BlockRadius = 600.0, PerBlock = 3.0, FullAt = 60.0, Rise = 1.5, Fall = 10.0;
		std::map<std::string, double> Weights;
		double MeterNow = 0.0, HeatNow = 0.0;
		struct Heard { double At; double Weight; };
		std::vector<Heard> HeardList;
		// The beds.
		struct BedRow { std::string Name; double In, Full, Hold, Out, Db; };
		std::vector<BedRow> Beds;
		std::string PlaceOpen = "wind_loop", PlaceWoods = "woods_loop";
		double PlaceDb = 0.0, PlaceFullDb = -10.0, PlaceTownDb = -6.0;
		std::string RainTake = "rain_loop";
		double RainDb = 0.0, RainFullDb = -4.0;
		double RumbleDb = -6.0, RiflesFarBeforeDb = -14.0;
		// Takes.
		std::map<std::string, std::vector<std::string>> Lists;
		std::map<std::string, double> SizeDb, SizeBig;
		double NearWithin = 60.0, MiddleWithin = 150.0;
		double DebrisFrom = 2.3, DebrisTo = 4.9, DebrisDb = 12.0, DebrisFadeIn = 0.15, Debris0 = 0.6, Debris1 = 0.9;
		double NearTwoCut = 2.3, NearTwoCutFade = 0.3;
		double CryChance = 0.25, CryBelow = 60.0;
		double BoltWithin = 30.0, BoltChance = 0.3;
		double CaseWithin = 40.0;
		double LevelDb = 2.0;
		std::map<std::string, double> PitchPercent;
		double ShockWithin = 25.0, ShockBelow = 80.0, ShockLpf = 600.0, ShockDown = 0.1, ShockHold = 1.2, ShockBack = 2.5, ShockDb = -6.0, ShockEvery = 90.0;
		double ShockAt = -1e9;
		bool bShockDue = false;
		// The ground battle's own rows (the mix's land block; each has its default here).
		Table CloseView{ { { 0.0, 3.0 }, { 4.0, 3.0 }, { 20.0, 1.5 }, { 80.0, 0.0 } } };
		Table CrackWithin{ { { 0.0, 25.0 }, { 4.0, 20.0 }, { 20.0, 12.0 }, { 80.0, 12.0 } } };
		Table CrackGap{ { { 0.0, 0.08 }, { 4.0, 0.1 }, { 20.0, 0.25 } } };
		double CrackFromShooter = 25.0, CrackDb0 = -2.0, CrackDbPerMetre = -0.3, RunFade = 0.04, RunGap = 0.6;
		std::vector<Piece> CrackRunParts;
		double RunAt = -1e9;
		double StrikeWithin = 12.0, StrikeBelow = 20.0, WhineChance = 0.3, StrikeDb = -4.0, WhineAt = -1e9, WhineGap = 0.35;
		std::vector<Piece> WhinePieces;
		double ArmourDb = -4.0, ArmourWhineChance = 0.3;
		/** A burst off armour (RattleOff): at most burstMost pings, burstShare of its rounds, burstPitchPercent and
		 * burstDbSpread (under db) of each its own; every ping but the last cut to burstRingS (a sharp clang, the steel
		 * ringing out only at the end); burstWhineChance its whine's chance (a single round's whineChance). */
		int ArmourBurstMost = 8;
		double ArmourBurstShare = 0.8, ArmourBurstPitch = 8.0, ArmourBurstDbSpread = 4.0, ArmourBurstRing = 0.25, ArmourBurstWhine = 0.6;
		std::map<std::string, std::pair<double, double>> FiringPart;
		// (a take that fires in several bursts with silences between: its other stretches of firing, besides its part, a
		// crowded machine gun's next places to play from)
		std::map<std::string, std::vector<std::pair<double, double>>> MoreParts;
		double BurstEndFade = 0.04;
		int ClipEvery = 8;
		double PingAfter = 0.12, ReloadAfterPing = 0.45, GarandWithin = 60.0;
		std::string PingTake = "garand_clip_ping", ReloadTake = "garand_reload";
		// The GI's reload clip calls "garand_reload" as the clip goes in (0.95 s) and the operating rod slams home
		// 0.33 s later; the recording has 2.5 s between the two, so two pieces of it: (from, cut, delay after the cue).
		// (true: once the first "garand_reload" cue is heard, the soldier files carry the reload and the shot-counted
		// one stops; until then the interim GI, who has no reload clip, still reloads by the count)
		bool bReloadFromClips = true, bClipReloadSeen = false;
		std::vector<std::array<double, 3>> ReloadPieces{ { { 0.14, 0.3, 0.0 } }, { { 2.66, 0.45, 0.246 } } };
		// The squad leader's Thompson ("thompson_burst" from his fire clip): SmgBurst rounds from the first, cut by the
		// measured round starts of each submachine-gun take (stand-ins until a real Thompson is recorded).
		std::string SmgCue = "thompson_burst";
		// (once his first burst is heard, a round fired by one of these roles of that army plays no rifle take: his
		// fire clip's burst carries it, so it is not heard twice)
		std::vector<std::string> SmgRoles{ "officer", "leader" };
		std::map<std::string, bool> SmgArmySeen;
		int SmgBurst = 4;
		std::map<std::string, std::vector<double>> SmgOnsets{ { "mp40_burst_01", { 0.006, 0.09, 0.193, 0.292, 0.382, 0.464 } }, { "mp40_burst_02", { 0.006, 0.104, 0.196, 0.315 } } };
		// Shouts in the thick of it: per side, when the next may start, and the last shooter of that side heard near.
		double ShoutWithin = 100.0, ShoutMeterFrom = 0.35, ShoutGapLow0 = 10.0, ShoutGapLow1 = 18.0, ShoutGapFull0 = 4.0, ShoutGapFull1 = 8.0;
		double ShoutFewGap = 25.0, ShoutOnFallWithin = 40.0, ShoutOnFallChance = 0.4, ShoutDb = 0.0;
		int ShoutFewLines = 4;
		std::vector<std::string> ShoutExclude;
		struct Shooter { double At = -1e9; Vec Where; std::string Army; };
		std::map<int, Shooter> LastShooter;
		std::map<int, double> ShoutFree, ShoutNext;
		double IncomingWithin = 150.0;
		// Release 2's battle acoustics (land.acoustics), each row at its default where the mix has none.
		// (travel: the families whose reports cross the ground, past how many metres, and never held longer than this)
		std::vector<std::string> TravelFamilies{ "small_arms", "mg", "sniper" };
		double TravelFrom = 0.0, TravelMost = 3.0, BulletSpeed = 760.0;
		// (tails: a take's own dying echo after its firing: from, how long (0: to its end), dB over the burst's level)
		struct TailPart { double From = 0.0, Long = 0.0, Db = 0.0; };
		std::map<std::string, TailPart> TailOf;
		double TailFadeIn = 0.03, TailFloorDb = -24.0;
		std::map<std::string, std::vector<std::pair<double, double>>> TailsLaid;
		// (echoes: the faces are looked for along EchoRays bearings round the ear out to EchoReach, a step at a time,
		// again when the ear has moved EchoLookMoved or after EchoLookEvery seconds; none with the camera over EchoBelow;
		// an echo comes no sooner than EchoLagLeast real seconds behind its own sound (the same-take rule's gap: the
		// one copy of a recording the mix makes on purpose never falls inside it) and no later than EchoLagMost life
		// seconds, at least EchoUnder dB under that sound at the ear, is left out under EchoFloorDb at the ear, and at most EchoMost sound at once: EchoMostSmall of them the rifles' and machine guns', the rest the
		// guns' and blasts', so a barrage's long echoes never take the small arms' share. The busier a share, the
		// louder an echo must be to take a place in it, rising to EchoFloorBusyDb with every place taken: a lone shot
		// in a lull is thrown back however thin, and in the thick of a fight only what would be heard through it. A
		// rifle's or a machine gun's no closer together than EchoGap)
		struct Face { double Db = -15.0, LpfHz = 0.0, HpfHz = 0.0; };
		Face EchoWoods{ -15.0, 4000.0, 250.0 }, EchoWalls{ -9.0, 8000.0, 0.0 }, EchoHedges{ -20.0, 4000.0, 300.0 };
		const Face* FaceOf(int Kind) const { return Kind == 1 ? &EchoWoods : Kind == 2 ? &EchoWalls : Kind == 4 ? &EchoHedges : nullptr; }
		double EchoReach = 300.0, EchoStep = 6.0, EchoBelow = 150.0, EchoLagLeast = 0.15, EchoUnder = 6.0, EchoLagMost = 1.5, EchoFloorDb = -40.0, EchoFloorBusyDb = -26.0, EchoGap = 0.06;
		double EchoLookMoved = 12.0, EchoLookEvery = 2.0, EchoApart = 60.0;
		int EchoRays = 24, EchoMost = 8, EchoMostSmall = 4, EchoOfBig = 2;
		struct Wall { Vec At; int Kind = 0; double Bearing = 0.0; };
		std::vector<Wall> Walls;
		Vec WallsFrom;
		double WallsAt = -1e9, EchoLast = -1e9;
		bool bWallsLooked = false;
		std::vector<double> EchoEnds, EchoEndsSmall;
		std::vector<double> EchoStarts;
		struct CoverCell { int What = 0; double Z = 0.0; };
		std::map<std::pair<int, int>, CoverCell> CoverSeen;
		/** The acoustics' own dice: nothing of theirs moves the picks of the takes themselves. */
		uint64_t EchoDice = 0xC2B2AE3D27D4EB4Full;
		std::map<std::string, std::string> BodyCue;
		std::map<std::string, std::map<std::string, std::string>> EngineByModel;
		/** The ear's camera height at the last noise heard (for the lift down among the men). */
		double EarHeight = 100.0;
		// What is playing, for the limits: by family, each take's end (real seconds) and distance.
		struct Playing { double Ends; double Far; };
		std::map<std::string, std::vector<Playing>> PlayingNow;
		std::map<std::string, std::vector<std::pair<double, double>>> Copies; // piece -> (starts, pitch) of its recent copies
		double CopiesSweptAt = 0.0;
		double MostHold = 0.05, Repitch = 0.04;
		/** When no pitch is free of the copies near it (a crowd of one army's rifles), how long an on-screen take may wait
		 * for a moment clear of them all (land.sameTake.clearWithin): within the tenth of a second the eye forgives. */
		double ClearWithin = 0.1;
		/** A take named in one of the armies' close rifle lists (byNation.rifle.*). */
		bool IsCloseRifle(const std::string& Name) const;
		std::map<std::string, std::string> LastOfList;
		std::map<int, int> RifleCount;
		double CrackAt = -1e9;
		/** The last Start found its take past its family's silent distance. */
		bool bLastTooFar = false;
		bool bLastMerged = false;  // the last Start heard its shot in a copy already sounding (a volley)
		uint64_t Dice = 0x9E3779B97F4A7C15ull;

		double Roll();
		const std::string& Pick(const std::string& ListName);
		std::vector<std::string> ListOf(const std::string& Name) const;
		const Family& FamilyOf(const std::string& Name) const;
		/** Starts a take if the limits allow (always when on the screen): false if it was dropped. */
		bool Start(Take T, double Now, double Far, bool bOnScreen, std::vector<Take>& Out);
		double FlashToBang(double Far, double Speed) const;
		/** Release 2 (bTravel): how long a rifle's or a machine gun's report is in crossing Far metres to the ear, real
		 * seconds at the battle clock's Speed; 0 with the part off or for a family that is not on its list. */
		double Travel(const std::string& FamilyName, double Far, double Speed) const;
		/** Release 2 (bTails): a burst just started as Cut (its last take in Out) ends in its recording's own tail. */
		void TailAfter(const Take& Cut, double Far, double Now, std::vector<Take>& Out);
		/** Release 2 (bEcho): the sound just started (Sounded, as Start left it, from Far metres) thrown back by up to Most
		 * of the faces round the ear. bSmall: a rifle's or a machine gun's (no closer together than the gap). */
		void EchoOf(const Take& Sounded, double Far, double Now, double Speed, const Listener& Ear, int Most, bool bSmall, std::vector<Take>& Out);
		/** The faces round the ear, looked for again when it has moved or the look is old. */
		void LookForWalls(const Listener& Ear, double Now);
		int CoverAt(double X, double Y, double& OutGroundZ);
		double EchoRoll();
		void Count(double Now, const std::string& WeightKey, double Far);
		void Crack(const LandPlay::Noise& N, double Now, const Listener& Ear, std::vector<Take>& Out, double Speed = 1.0, double ReportDelay = -1.0);
		/** A round that missed and struck the ground near the ear: the strike in the earth, sometimes a whine off. */
		void Strike(const LandPlay::Noise& N, double Now, const Listener& Ear, std::vector<Take>& Out);
		/** A ricochet's whine off At (a piece of groundStrikes.whines), Delay real seconds from now, if the gap allows and
		 * the roll falls under Chance: false if none. */
		bool WhineOff(const Vec& At, double Far, double Now, double Delay, double Db, double Chance, std::vector<Take>& Out);
		/** Effects (R2): a small arm's burst off armour (Glance, Rounds over 1): a rattle of steel pings over the burst
		 * (land.armourStrikes' burst keys), Base's delay and place, and now and then a whine off it. */
		void RattleOff(const LandPlay::Noise& N, const Take& Base, double Far, double Now, bool bOnScreen, std::vector<Take>& Out);
		/** A burst cut to its rounds: From a random point in the take's firing part, Cut to rounds / rate + tail. */
		void CutBurst(Take& T, const LandPlay::Noise& N);
		void CutBurstAt(Take& T, const LandPlay::Noise& N, double A, double B);
		/** A shout in this army's tongue at Where (a man of its side near the ear), Delay real seconds from now, if its side
		 * is free to speak. */
		bool Shout(int Side, const std::string& Army, const Vec& Where, double Now, double Delay, const Listener& Ear, std::vector<Take>& Out);
		/** A shell or mortar bomb heard coming in, ending as it lands (only where a real recording is listed). */
		void HearIncoming(const LandPlay::Noise& N, double Now, double Battle, double Speed, const Listener& Ear, std::vector<Take>& Out);
	};
	/** The one-shot takes' players, pooled (the production study of 22 Nov, section 5, item 9): made once and reused,
	 * not one spawned and destroyed for every take, and a cut take's fade-out kept here, not a timer for each. The game
	 * holds an audio player for each voice; the cloud test plays a whole battle's takes through it. Engine-free. */
	class Voices
	{
	public:
		struct Voice
		{
			/** Takes it has played: a take's handle is its voice and this (a take long done no longer holds its voice). */
			int Use = 0;
			bool bBusy = false;
			double Began = 0.0;
			/** When its take is done by its length (the game frees it when its player stops, Free). */
			double Ends = 0.0;
			/** When its cut begins to fade (real seconds; -1 none, or done), and over how long. */
			double FadeAt = -1.0, Fade = 0.0;
		};
		/** Warm voices to make before the battle; at most MostVoices in all. */
		void Size(int Warm, int MostVoices);
		int Warm() const { return WarmCount; }
		/** A voice for a take starting at Now: Plays seconds of its recording at Pitch, or cut Cut seconds after it
		 * starts (0: whole) and faded over Fade. bMade: the pool made a new voice for it (the game makes its player).
		 * With every voice sounding and the pool at its most, the one nearest its end gives way (Stolen). */
		int Take(double Now, double Plays, double Pitch, double Cut, double Fade, bool& bMade);
		/** Its player has stopped: the voice is free. */
		void Free(int Slot);
		/** The voices whose cut is due by Now, each once: fade them out (FadeOf). */
		void Due(double Now, std::vector<int>& OutFade);
		/** Whether the take handed Use on Slot is still the one in it. */
		bool Holds(int Slot, int Use) const { return Slot >= 0 && Slot < static_cast<int>(All.size()) && All[static_cast<size_t>(Slot)].Use == Use && All[static_cast<size_t>(Slot)].bBusy; }
		int UseOf(int Slot) const { return All[static_cast<size_t>(Slot)].Use; }
		double FadeOf(int Slot) const { return All[static_cast<size_t>(Slot)].Fade; }
		const Voice& At(int Slot) const { return All[static_cast<size_t>(Slot)]; }
		int Count() const { return static_cast<int>(All.size()); }
		// The log: takes played, the most sounding at once, voices made past the warm ones, given way (and of those, cut
		// takes whose fade was still due), cuts faded.
		int Taken = 0, BusyNow = 0, MostBusy = 0, MadeLate = 0, Stolen = 0, StolenCuts = 0, Faded = 0;

	private:
		std::vector<Voice> All;
		/** The free voices, last freed on top (it plays next). */
		std::vector<int> Idle;
		int Most = 0, WarmCount = 0;
	};

	/** Each vehicle's engine loop's name (Mixer::EngineTake), worked out only when her cue or model changes, not every
	 * frame; the game keeps its own name (an FName) for each index of Names(). */
	class EngineNames
	{
	public:
		/** The index in Names() of the loop vehicle Vehicle plays for Cue on ModelName, -1 for none (no cue). */
		int Of(const Mixer& Mix, int Vehicle, const std::string& ModelName, const std::string& Cue);
		const std::vector<std::string>& Names() const { return Known; }
		/** Times a name was worked out, and asked for (the log: worked out only on a change). */
		int Built = 0, Asked = 0;

	private:
		struct Was
		{
			std::string Model, Cue;
			int Name = -1;
			bool bSet = false;
		};
		std::vector<Was> ByVehicle;
		std::vector<std::string> Known;
		std::map<std::string, int> Index;
	};
}
