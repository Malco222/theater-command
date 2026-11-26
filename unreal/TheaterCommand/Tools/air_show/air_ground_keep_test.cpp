// The ground kept on disk (Battle/AirGroundKeep.h; release 2, the air battle's hitch, rows flyPaintEarly and
// flyGroundKeep): the key of a ground changes when any one thing the painter reads changes (every field of the place,
// the book of town plans, a picture's frame, the painter's version) and not otherwise; a ground packed and unpacked is
// the same bytes, through a file on disk too; a file cut short, lengthened or altered anywhere (its header, key, sizes,
// texels or check) is refused; the folder's order keeps the six grounds last used; and SamePlace tells every field.
// The pictures are the real frames' sizes, their first rows painted by the real painter on the recording's place (its
// alpha always 255, which the file leaves out).
//
// Usage: air_ground_keep_test <recording.json> <towns.json> <out folder>
#include "Battle/AirFeed.h"
#include "Battle/AirGroundKeep.h"
#include "Battle/AirLand.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	int Failures = 0;
	int Checks = 0;

	void Check(bool bPass, const std::string& What, const std::string& Seen = std::string())
	{
		++Checks;
		std::printf("  %s %-66s %s\n", bPass ? "pass" : "FAIL", What.c_str(), Seen.c_str());
		if (!bPass)
		{
			++Failures;
		}
	}

	bool ReadFile(const std::string& Path, std::string& Out)
	{
		std::ifstream File(Path, std::ios::binary);
		if (!File)
		{
			return false;
		}
		std::ostringstream Text;
		Text << File.rdbuf();
		Out = Text.str();
		return true;
	}

	/** The place as the scene asks the painter for it (AAirBattle::PaintLand, PaintEarly), the sun from the board. */
	AirLand::Place PlaceOf(const AirFeed::Recording& R)
	{
		const AirFeed::Sky& B = R.Board;
		AirLand::Place P;
		P.AimX = B.TargetX;
		P.AimY = B.TargetY;
		P.NorthX = B.NorthX;
		P.NorthY = B.NorthY;
		P.Terrain = B.Terrain.empty() ? std::string("plains") : B.Terrain;
		P.Name = B.PlaceName;
		P.Month = B.Month + 1;
		P.Seed = R.Seed ? R.Seed : 238;
		double X = 0.0, Y = 0.0, Z = 0.0;
		AirLand::SunAt(std::isfinite(B.Latitude) ? B.Latitude : 50.0, B.Month + 1, 12.5, B.NorthX, B.NorthY, X, Y, Z);
		const double L = std::sqrt(X * X + Y * Y + std::max(Z, 0.07) * std::max(Z, 0.07));
		P.SunX = X / L;
		P.SunY = Y / L;
		P.SunZ = std::max(Z, 0.07) / L;
		P.Land = B.Land;
		P.Rivers = B.Rivers;
		return P;
	}

	/** A picture of a frame's size: a pattern of its own, then its first rows painted by the painter. */
	std::vector<uint8_t> Picture(const AirLand::Painter& Painter, const AirLand::Frame& F, int Index, int Painted)
	{
		std::vector<uint8_t> Rgba(static_cast<size_t>(F.W) * F.H * 4);
		uint32_t Seed = 2463534242u + static_cast<uint32_t>(Index) * 977u;
		for (size_t I = 0; I < Rgba.size(); I += 4)
		{
			for (int C = 0; C < 3; ++C)
			{
				Seed ^= Seed << 13;
				Seed ^= Seed >> 17;
				Seed ^= Seed << 5;
				Rgba[I + C] = static_cast<uint8_t>(Seed);
			}
			Rgba[I + 3] = 255;
		}
		std::vector<float> Heights(static_cast<size_t>(F.W) * F.H);
		Painter.PaintRows(F, 0, Painted, Rgba.data(), Heights.data());
		return Rgba;
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 4)
	{
		std::fprintf(stderr, "usage: air_ground_keep_test <recording.json> <towns.json> <out folder>\n");
		return 2;
	}
	std::string Text, Plans, Error;
	AirFeed::Recording R;
	if (!ReadFile(Argv[1], Text) || !AirFeed::ReadRecording(Text, R, Error) || !ReadFile(Argv[2], Plans))
	{
		std::fprintf(stderr, "air_ground_keep_test: could not read %s or %s (%s)\n", Argv[1], Argv[2], Error.c_str());
		return 2;
	}
	const std::string Out = Argv[3];
	std::printf("The ground kept on disk (Battle/AirGroundKeep.h), on %s's place:\n", R.Key.c_str());
	const AirLand::Place P = PlaceOf(R);
	const std::vector<AirLand::Frame> Frames = AirLand::Frames(P);
	Check(!P.Land.empty() && Frames.size() == 3, "the recording has land, and the painter three pictures", std::to_string(P.Land.size()) + " rings, " + std::to_string(Frames.size()) + " pictures");

	// ---- The key: each thing the painter reads, changed alone, changes it; the same asking keeps it. ----
	const uint64_t Base = AirGroundKeep::Key(P, Plans, Frames);
	Check(AirGroundKeep::Key(P, Plans, Frames) == Base && AirGroundKeep::Key(AirLand::Place(P), std::string(Plans), std::vector<AirLand::Frame>(Frames)) == Base,
		"the same place, plans and frames: the same key");
	struct Change
	{
		const char* Name;
		std::function<void(AirLand::Place&, std::string&, std::vector<AirLand::Frame>&)> Do;
	};
	const std::vector<Change> Changes = {
		{ "the aim's x", [](AirLand::Place& Q, std::string&, std::vector<AirLand::Frame>&) { Q.AimX += 1.0; } },
		{ "the aim's y", [](AirLand::Place& Q, std::string&, std::vector<AirLand::Frame>&) { Q.AimY -= 0.001; } },
		{ "true north's x", [](AirLand::Place& Q, std::string&, std::vector<AirLand::Frame>&) { Q.NorthX = std::nextafter(Q.NorthX, 2.0); } },
		{ "true north's y", [](AirLand::Place& Q, std::string&, std::vector<AirLand::Frame>&) { Q.NorthY = std::nextafter(Q.NorthY, -2.0); } },
		{ "the country", [](AirLand::Place& Q, std::string&, std::vector<AirLand::Frame>&) { Q.Terrain = Q.Terrain == "forest" ? "plains" : "forest"; } },
		{ "the target's name", [](AirLand::Place& Q, std::string&, std::vector<AirLand::Frame>&) { Q.Name += "x"; } },
		{ "the month", [](AirLand::Place& Q, std::string&, std::vector<AirLand::Frame>&) { Q.Month = Q.Month % 12 + 1; } },
		{ "the seed", [](AirLand::Place& Q, std::string&, std::vector<AirLand::Frame>&) { Q.Seed += 1; } },
		{ "the sun's x", [](AirLand::Place& Q, std::string&, std::vector<AirLand::Frame>&) { Q.SunX = std::nextafter(Q.SunX, 2.0); } },
		{ "the sun's y", [](AirLand::Place& Q, std::string&, std::vector<AirLand::Frame>&) { Q.SunY = std::nextafter(Q.SunY, 2.0); } },
		{ "the sun's height", [](AirLand::Place& Q, std::string&, std::vector<AirLand::Frame>&) { Q.SunZ = std::nextafter(Q.SunZ, 2.0); } },
		{ "a point of the land moved", [](AirLand::Place& Q, std::string&, std::vector<AirLand::Frame>&) { Q.Land.front().front().first += 0.5; } },
		{ "a point of the land dropped", [](AirLand::Place& Q, std::string&, std::vector<AirLand::Frame>&) { Q.Land.front().pop_back(); } },
		{ "a ring of land more", [](AirLand::Place& Q, std::string&, std::vector<AirLand::Frame>&) { Q.Land.push_back({ { 0.0, 0.0 }, { 10.0, 0.0 }, { 0.0, 10.0 } }); } },
		{ "a river moved, or one more", [](AirLand::Place& Q, std::string&, std::vector<AirLand::Frame>&) {
			if (Q.Rivers.empty()) { Q.Rivers.push_back({ { 0.0, 0.0 }, { 5.0, 5.0 } }); } else { Q.Rivers.front().front().second += 0.5; } } },
		{ "an empty river line more", [](AirLand::Place& Q, std::string&, std::vector<AirLand::Frame>&) {
			Q.Rivers.push_back({}); } },
		{ "one letter of the town plans", [](AirLand::Place&, std::string& Book, std::vector<AirLand::Frame>&) { Book[Book.size() / 2] = Book[Book.size() / 2] == 'a' ? 'b' : 'a'; } },
		{ "a space more in the town plans", [](AirLand::Place&, std::string& Book, std::vector<AirLand::Frame>&) { Book += " "; } },
		{ "a picture's width", [](AirLand::Place&, std::string&, std::vector<AirLand::Frame>& F) { F[1].W += 1; } },
		{ "a picture's height", [](AirLand::Place&, std::string&, std::vector<AirLand::Frame>& F) { F[2].H -= 1; } },
		{ "a picture's step", [](AirLand::Place&, std::string&, std::vector<AirLand::Frame>& F) { F[0].Step *= 2.0; } },
		{ "a picture's corner (x)", [](AirLand::Place&, std::string&, std::vector<AirLand::Frame>& F) { F[0].X0 += 8.0; } },
		{ "a picture's corner (y)", [](AirLand::Place&, std::string&, std::vector<AirLand::Frame>& F) { F[1].Y0 += 24.0; } },
		{ "a picture fewer", [](AirLand::Place&, std::string&, std::vector<AirLand::Frame>& F) { F.pop_back(); } },
	};
	for (const Change& C : Changes)
	{
		AirLand::Place Q = P;
		std::string Book = Plans;
		std::vector<AirLand::Frame> F = Frames;
		C.Do(Q, Book, F);
		const uint64_t Changed = AirGroundKeep::Key(Q, Book, F);
		Check(Changed != Base, std::string("the key changes with ") + C.Name);
		if (Book == Plans && F.size() == Frames.size())
		{
			const bool bSameFrames = F[0].W == Frames[0].W && F[1].W == Frames[1].W && F[2].H == Frames[2].H && F[0].Step == Frames[0].Step && F[0].X0 == Frames[0].X0 && F[1].Y0 == Frames[1].Y0;
			if (bSameFrames)
			{
				Check(!AirGroundKeep::SamePlace(P, Q), std::string("SamePlace tells ") + C.Name);
			}
		}
	}
	Check(AirGroundKeep::Key(P, Plans, Frames, AirGroundKeep::PainterVersion + 1) != Base, "the key changes with the painter's version");
	Check(AirGroundKeep::SamePlace(P, AirLand::Place(P)), "SamePlace: a place is the same as its copy");
	Check(AirGroundKeep::FileName(Base).size() == 23 && AirGroundKeep::IsGroundName(AirGroundKeep::FileName(Base)), "the file's name is its key", AirGroundKeep::FileName(Base));

	// ---- Pack and unpack: the same bytes, here and through a file. ----
	const AirLand::Painter Painter(P, Plans);
	std::vector<std::vector<uint8_t>> Pictures;
	bool bAlpha = true;
	for (size_t K = 0; K < Frames.size(); ++K)
	{
		Pictures.push_back(Picture(Painter, Frames[K], static_cast<int>(K), 64));
		for (size_t I = 3; I < static_cast<size_t>(Frames[K].W) * 64 * 4; I += 4)
		{
			bAlpha = bAlpha && Pictures[K][I] == 255;
		}
	}
	Check(bAlpha, "the painter's texels all alpha 255 (what the file leaves out)", "the first 64 rows of each picture");
	const std::vector<uint8_t> Packed = AirGroundKeep::Pack(Base, Frames, Pictures);
	size_t Texels = 0;
	for (const AirLand::Frame& F : Frames)
	{
		Texels += static_cast<size_t>(F.W) * F.H;
	}
	Check(Packed.size() == 4 + 4 + 8 + 4 + Frames.size() * 8 + Texels * 3 + 8, "a ground's file: its header, three bytes a texel, its check", std::to_string(Packed.size() / (1024 * 1024)) + " MB");
	std::vector<std::vector<uint8_t>> Back;
	std::string Why;
	Check(AirGroundKeep::Unpack(Packed.data(), Packed.size(), Base, Frames, Back, Why) && Back == Pictures, "packed and unpacked: the same bytes", Why);
	{
		std::vector<std::vector<uint8_t>> Wrong = Pictures;
		Wrong[1].pop_back();
		Check(AirGroundKeep::Pack(Base, Frames, Wrong).empty(), "a picture not its frame's size is not packed");
	}
	const std::string Path = Out + "/" + AirGroundKeep::FileName(Base);
	const std::string Beside = Path + ".writing";
	std::string FromDisk;
	const bool bWritten = AirGroundKeep::WriteBytes(Beside, Packed) && std::rename(Beside.c_str(), Path.c_str()) == 0;
	Check(bWritten && ReadFile(Path, FromDisk) && FromDisk.size() == Packed.size()
		&& AirGroundKeep::Unpack(reinterpret_cast<const uint8_t*>(FromDisk.data()), FromDisk.size(), Base, Frames, Back, Why) && Back == Pictures,
		"written beside its place, moved in, read back: the same bytes", Why);
	std::remove(Path.c_str());

	// ---- Refused: cut short, lengthened, altered anywhere, another key, other frames. ----
	auto Refused = [&](const std::vector<uint8_t>& Bytes, uint64_t KeyOf, const std::vector<AirLand::Frame>& F, std::string& Said)
	{
		std::vector<std::vector<uint8_t>> Got;
		const bool bTaken = AirGroundKeep::Unpack(Bytes.data(), Bytes.size(), KeyOf, F, Got, Said);
		return !bTaken && Got.empty();
	};
	const size_t Head = 4 + 4 + 8 + 4 + Frames.size() * 8;
	for (const size_t Cut : { size_t(0), size_t(3), Head - 1, Head, Head + 8, Packed.size() / 2, Packed.size() - 9, Packed.size() - 8, Packed.size() - 1 })
	{
		const std::vector<uint8_t> Short(Packed.begin(), Packed.begin() + static_cast<std::ptrdiff_t>(Cut));
		Check(Refused(Short, Base, Frames, Why), "refused: the file cut short at byte " + std::to_string(Cut), Why);
	}
	{
		std::vector<uint8_t> Long = Packed;
		Long.push_back(0);
		Check(Refused(Long, Base, Frames, Why), "refused: a byte more at its end", Why);
	}
	const struct { const char* Name; size_t At; } Alters[] = {
		{ "its mark", 1 }, { "its format", 4 }, { "its key", 12 }, { "its count of pictures", 16 }, { "a picture's width", 20 }, { "a picture's height", 24 + 8 },
		{ "the first texel", Head }, { "a texel in the middle", Head + Texels * 3 / 2 }, { "the last texel", Head + Texels * 3 - 1 }, { "its check", Packed.size() - 3 } };
	for (const auto& Alter : Alters)
	{
		std::vector<uint8_t> Altered = Packed;
		Altered[Alter.At] ^= 0x10;
		Check(Refused(Altered, Base, Frames, Why), std::string("refused: ") + Alter.Name + " altered", Why);
	}
	Check(Refused(Packed, Base ^ 1, Frames, Why), "refused: asked for another ground's key", Why);
	{
		std::vector<AirLand::Frame> Other = Frames;
		Other[2].W = 1024;
		Check(Refused(Packed, Base, Other, Why), "refused: asked for other picture sizes", Why);
		Other = Frames;
		Other.pop_back();
		Check(Refused(Packed, Base, Other, Why), "refused: asked for two pictures, not three", Why);
	}

	// ---- The folder: the six grounds last used, the one used now first. ----
	{
		std::vector<std::string> Drop;
		std::string Order;
		std::vector<std::string> Names;
		for (int K = 0; K < 8; ++K)
		{
			Names.push_back(AirGroundKeep::FileName(0x1000u + static_cast<uint64_t>(K)));
			Order = AirGroundKeep::Remember(Order, Names.back(), Drop);
			Check(Drop.size() == (K >= 6 ? 1u : 0u) && (K < 6 || Drop.front() == Names[static_cast<size_t>(K) - 6]),
				"the folder's order after ground " + std::to_string(K + 1) + ": the oldest dropped past six", Drop.empty() ? std::string("none dropped") : Drop.front());
		}
		Order = AirGroundKeep::Remember(Order, Names[3], Drop);
		Check(Drop.empty() && Order.rfind(Names[3] + "\n", 0) == 0, "a ground used again goes first, nothing dropped");
		Order = AirGroundKeep::Remember(Order, Names[0], Drop);
		Check(Drop.size() == 1 && Drop.front() == Names[2] && Order.rfind(Names[0] + "\n", 0) == 0, "a ground dropped and painted again comes back, the oldest drops", Drop.empty() ? std::string() : Drop.front());
		size_t Lines = 0;
		for (const char C : Order)
		{
			Lines += C == '\n' ? 1 : 0;
		}
		Check(Lines == AirGroundKeep::Keep, "the order holds six names", std::to_string(Lines));
		Order = AirGroundKeep::Remember("  \r\n" + Names[1] + "\r\n" + Names[1] + "\n\n", Names[2], Drop);
		Check(Order == Names[2] + "\n" + Names[1] + "\n" && Drop.empty(), "an order with blank lines, Windows ends and a name twice reads as its names once");
		Check(!AirGroundKeep::IsGroundName("order.txt") && !AirGroundKeep::IsGroundName("0123456789ABCDEF.ground") && !AirGroundKeep::IsGroundName("0123456789abcdef.ground.writing"),
			"only a ground's own name may be dropped");
	}

	std::printf("%s: %d checks, %d failed\n", Failures == 0 ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
