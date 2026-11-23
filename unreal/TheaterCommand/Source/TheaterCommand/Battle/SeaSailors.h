#pragma once

#include "Battle/LandMen.h"
#include "Battle/SeaShow.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

/**
 * The sea battle's sailors as men of the soldier files (Brian, 22 November 2026: "sailors should jump in the water if the
 * boat is going down", "have them use the models we have for the soldiers for the sailors"). The soldiers thread's sailor
 * files (battle-models/land/soldier/README.md, "The sailors"; Content/Models/Soldiers/soldier_<ARMY>_sailor.json) are
 * soldier.py's man on the same 32 bones with the clips of a man going over the side and in the water; LandMen reads and
 * skins them (a byte copy of the ground thread's Battle/LandMen, claude/project-thread-uhhxrn 982b6300, the same at
 * f8503669: keep it so).
 *
 * The show (SeaShow) decides where every man is and what he is doing; this file says only which clip shows it, how far
 * into it he is, and where its root stands (the README's roots: `surface` on the sea where he is, `feet` under him as he
 * falls, `net` at the foot of the net up a rescuer's side, `seat` on a thwart, a raft's deck or a boat's boards; `deck`
 * under his boots on a ship's deck, a gun's crew, whom the scene places itself).
 *
 * Plain C++, no Unreal types: the scene (ASeaBattle) copies what Skin writes into its own meshes, and Tools/sea_show
 * checks it in the cloud. A man's own axes are the files': x forward, y to his RIGHT, z up, metres. The board is the
 * show's: x east, y north, z up, headings in radians clockwise from north. His own axes go to the scene's world by a
 * plain turn of his heading about the vertical (Unreal's x north and y east, as the scene's World puts the board): no
 * mirror, so the files' triangles face out as they are.
 */
namespace SeaSailors
{
	/** What a man is shown doing: each a clip of the sailor files. */
	enum class Act : int
	{
		Fall,   // fall_loop: dropping from the rail, the root under his feet
		Enter,  // enter_water: from the splash, under and up
		Tread,  // tread
		Wave,   // wave: an arm up for the ship in sight
		Swim,   // swim_breast
		Dead,   // float_dead
		Cling,  // cling: forearms over the thing he holds
		Climb,  // climb_net: up a rescuer's side
		Row,    // row: pulling an oar on a thwart
		Sit,    // sit_raft: on a raft's deck or a boat's boards
		Stand,  // idle: on a deck, standing to a gun (a gun's crew, ConvoyLook::GunCrewOf)
		Duck,   // cower: crouched on a deck as a shell strikes or falls close aboard
		Count
	};
	constexpr int Acts = static_cast<int>(Act::Count);
	const char* ClipName(Act A);
	/** The army of a nation's sailors as the files name it ("USA", "JAP", "GBR", "GER", "ITA", "SOV"), from the show's
	 * nation (USA, UK, GER, ITA, JAP, SOV, as the recordings write them) or the scene's tag (us, gb, de, it, jp, su). */
	const char* ArmyOf(const std::string& Nation);
	/** The bank's poses of an act: its loop (or its one-shot) cut into this many. */
	int PosesOf(Act A);

	/** His dress: a colour (0xRRGGBB) for each paint number the files use (0 to 63; the sailor files use boots 2, skin 7,
	 * soles 12 (leather) and their own kapok 41, shirt 42, trousers 43, hat 44, ties 45). Unset paints take the file's own
	 * colour (its `colours`, by the army paints' names and its new_paints). */
	struct Dress
	{
		uint32_t Paint[64] = {};
		bool bSet[64] = {};
		void Put(int Number, uint32_t Rgb)
		{
			if (Number >= 0 && Number < 64)
			{
				Paint[Number] = Rgb;
				bSet[Number] = true;
			}
		}
	};

	/** The paint numbers of the sailor files' own paints, and of the army paints they wear. */
	constexpr int Boots = 2, Skin = 7, Soles = 12, Kapok = 41, Shirt = 42, Trousers = 43, Hat = 44, Ties = 45;

	/** How a man is shown now: which file, which act at which time in its clip, his root (board metres) and heading;
	 * bMirror, the file's man drawn left for right (an oarsman on her starboard side: the clip pulls an oar on his right,
	 * and facing aft his right is her port side). */
	struct Placed
	{
		int File = -1;
		Act What = Act::Tread;
		float Time = 0.f;
		double X = 0.0, Y = 0.0, Z = 0.0;
		double Heading = 0.0;
		bool bMirror = false;
	};

	/** Where he is on what he is on, for Place: the float's ride on the swell (metres up) and, in a boat, whether he is
	 * one of her oarsmen, how far into his stroke (seconds of the row clip) and whether he sits on her starboard side. */
	struct Aboard
	{
		const SeaShow::FloatOut* On = nullptr;
		double Ride = 0.0;
		bool bRowing = false;
		float Stroke = 0.f;
		bool bStarboard = false;
	};

	/** The heights of the things the men sit on (metres over the float's own waterline): a boat's thwart top, her bottom
	 * boards, a Carley float's tube top (the scene draws them so). A man holding on is drawn where the show puts him: a
	 * raft's holders a third of a metre out from her rim, their hands at her tube's side; a boat's on her grab line
	 * (SeaShow::GrabHold). */
	constexpr double ThwartTop = 0.47, Boards = 0.12, RaftTop = 0.26;
	/** The row clip's rowlock out to his side (metres, the file's `oar`): where his oar crosses her gunwale. */
	constexpr double LockOut = 0.6;
	/** A man in the water (root at the sea's top): a hand or a boot whose joint (wrist, ankle) is under SinkFrom metres
	 * over the sea is drawn SinkTo under it, every point of it, so no hand shows above the water with the arm behind it
	 * under (read as a hand cut off) and no boot tip breaks it alone (read as a fin): the art director, 22 November 12:17Z.
	 * The breaststroke's glide lifts the hands up to 0.13 m and its kick the boot tips 0.05 m over a wrist or ankle under
	 * the water. A hand on a raft's tube, an arm up to wave: its wrist is out of the water, so it shows. */
	constexpr float SinkFrom = 0.03f, SinkTo = 0.03f;

	/** Whether a battle is known to fall in summer in warm water, where a navy's men wore their whites (the art director,
	 * 22 November 13:47Z): its month May to September in the Mediterranean, the Indian Ocean north of the Tropic of
	 * Capricorn or the tropics. False where its month is not known: no whites then. */
	bool SummerWhites(bool bWhen, int Month, double Lat, double Lon);

	class Crew
	{
	public:
		/** A sailor file for a navy's men (Army "USA", "JAP", as the file's name): read, its water clips found. False,
		 * with the reason, where it does not read or lacks a clip. */
		bool AddFile(const std::string& Army, const std::string& Text, std::string& OutError);
		int Files() const { return static_cast<int>(Read.size()); }
		/** The file a navy's men are drawn with: the navy's own where read, else the American's, else the first read (-1
		 * none). Navy as ArmyOf takes it. */
		int FileFor(const std::string& Navy) const;
		const LandMen::Soldier& File(int K) const { return *Read[static_cast<size_t>(K)].S; }
		const std::string& Army(int K) const { return Read[static_cast<size_t>(K)].Army; }
		/** A clip's length (seconds) for an act. */
		float Long(int K, Act A) const;
		/** File K's dress for a man: Variant 0 most men's, 1 the one in seven's (the first of its colour_variants with
		 * colours: the American officer's khaki, the Japanese grey-green, the British cap's white cover, the German and
		 * Italian summer white). Not bWarm (SummerWhites false), no man wears white: the one in seven takes the file's
		 * first other dress that is not white (the German grey, the Italian winter blue, the Soviet pea coat, the British
		 * sea jersey), and a file whose own dress is white (the Japanese) its first other dress for every man. A variant
		 * is white where its name begins "white"; the file's own dress, where its shirt is pale. Out left with no paint
		 * set: the file's own colours. */
		void DressOf(int K, int Variant, bool bWarm, Dress& Out) const;
		/** File K's first other dress (the first of its colour_variants with colours), whatever its colour; false where it
		 * has none. (The pre-check's sailor probe paints it; the scene asks DressOf.) */
		bool AltDress(int K, Dress& Out) const
		{
			const FileRead& F = Read[static_cast<size_t>(K)];
			Out = F.Others.empty() ? Dress() : F.Others.front().second;
			return !F.Others.empty();
		}

		/** How a man is shown now (Now and LandedAt battle seconds; LandedAt -1 when he has not just come down). */
		Placed Place(int K, const SeaShow::ManOut& M, const Aboard& Where, double Now, double LandedAt) const;

		/** The bank's pose for an act at a time in its clip, and the time of a pose. */
		int PoseOf(int K, Act A, float Time) const;
		float PoseTime(int K, Act A, int Pose) const;

		/** Skins file K doing A at Time at level L, in his own axes, metres: Points * 3 floats each into OutAt and
		 * OutFacing (resized). Returns the points; 0 where the clip is missing. In the water's clips (root at the sea's
		 * top) a hand or boot under the water at its joint is sunk whole (SinkFrom). */
		int Skin(int K, Act A, float Time, LandMen::Lod L, std::vector<float>& OutAt, std::vector<float>& OutFacing) const;
		/** The middle of his two hands (his own axes), doing A at Time. */
		LandMen::Vec3 Hands(int K, Act A, float Time) const;
		/** His oar pulling (the row clip's `oar` track, his own axes, metres over the seat): its handle at Time and its
		 * rowlock, on his right (bMirror: his left). False where the file has no track (then Hands and LockOut). */
		bool Oar(int K, float Time, bool bMirror, LandMen::Vec3& OutHandle, LandMen::Vec3& OutLock) const;
		/** File K's triangles at level L, as the files wind them. */
		const std::vector<uint32_t>& Triangles(int K, LandMen::Lod L) const { return File(K).Meshes[L].Triangles; }
		/** File K's colours at level L in a dress: sRGB bytes, three a point (the paint by the dress or the file's own,
		 * times the file's shade and tint). Oil, 0 to 1: how black with fuel oil he is (his kapok and his hat keep a little
		 * of their colour, so he still reads on the oil). */
		void Paint(int K, LandMen::Lod L, const Dress& D, double Oil, std::vector<uint8_t>& OutRgb) const;

	private:
		struct FileRead
		{
			std::string Army;
			std::shared_ptr<LandMen::Soldier> S;
			int Clip[Acts] = {};
			int HandL = -1, HandR = -1;
			// (the four ends a man in the water may lift alone: each one's joint bone (hand_l, hand_r, foot_l, foot_r), and
			// each bone's end, -1 for none: the hand's, its fingers' and thumb's; the foot's and its ball's)
			int EndJoint[4] = { -1, -1, -1, -1 };
			std::vector<int8_t> EndOf;
			// (the file's colours for the army paints, 0 to 14, where it gives them; its oar: the rowlock and the handle's
			// track, t x y z)
			uint32_t Own[16] = {};
			bool bOwn[16] = {};
			float Lock[3] = { 0.f, 0.f, 0.f };
			std::vector<float> Handle;
			// (its colour_variants that give colours, in the file's order, by name)
			std::vector<std::pair<std::string, Dress>> Others;
		};
		std::vector<FileRead> Read;
		void Frames(int K, Act A, float Time, LandMen::Xform* Out) const;
	};
}
