// The earned colours' test (Battle/AirUnitColours; release 2, Brian 25 November 2026 03:27Z: "unique colorings or
// markings like the tuskegee airmen"). The palette as the game carries it (Content/Data/Air/unit-colours.json, the art
// director's, third issue): 112 schemes, each one idea whose spinner is its tail's first colour and never its nose
// band. Every aircraft model in Content/Models (the near shape and the far): its airscrews and fins found, the spinner
// only at its nose, the band right behind it and never wider than the spinner is long, the fin's points only aft and
// above the fuselage, a bomber's fin tip its top sixth; Britain's colours only on the rudder, France's only on the fin
// (by the regions, and by the file's own part names where it names them); nothing that is not the machine's own coat or
// spinner recoloured (glass, crew, tyres, blades, rivets and seams, lamps, nose art, tally); a plain patch only round a
// marking on a part its nation colours, on its own face, a little larger than the marking (measured here from the
// marking as the miniatures draw it); each step adding to the one before; a night finish dulled by 0.55, its cream the
// palette's grey, the tail's pattern on the top half only; the same look the same coat, every time; with the row off,
// or a group the board gives no row, nothing at all. The patterns measured on a frame; the parted triangles covering
// what they were cut from, no more and no less, facing as it faced. Then the hand-made raid's board
// (unit_colours_raid.json): its place.corpsMarks read, and its groups' corps (extra.corps) matching them.
// Usage: air_unit_colours_test <Content/Data/Air/unit-colours.json> <Content/Models> <unit_colours_raid.json>
#include "Battle/AirFeed.h"
#include "Battle/AirShow.h"
#include "Battle/AirUnitColours.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

using namespace AirUnitColours;

namespace
{
	int Bad = 0;

	void Check(bool bOk, const char* What, const std::string& Detail = std::string())
	{
		std::printf("  %s  %s%s%s\n", bOk ? "ok  " : "FAIL", What, Detail.empty() ? "" : ": ", Detail.c_str());
		if (!bOk) ++Bad;
	}

	bool ReadText(const std::string& Path, std::string& Out)
	{
		std::ifstream In(Path, std::ios::binary);
		if (!In) return false;
		std::stringstream S;
		S << In.rdbuf();
		Out = S.str();
		return true;
	}

	/** What each model is, as the game's raids fly it: a fighter, its engines, its fins. */
	struct Expect
	{
		bool bFighter;
		int Engines;
		int Tails;
	};
	const std::map<std::string, Expect>& Expected()
	{
		static const std::map<std::string, Expect> E = {
			{ "Bf109", { true, 1, 1 } }, { "Fw190", { true, 1, 1 } }, { "Me262", { true, 1, 1 } }, { "Spitfire", { true, 1, 1 } }, { "Hurricane", { true, 1, 1 } },
			{ "Mustang", { true, 1, 1 } }, { "Warhawk", { true, 1, 1 } }, { "Wildcat", { true, 1, 1 } }, { "Hellcat", { true, 1, 1 } }, { "Zero", { true, 1, 1 } },
			{ "Yak", { true, 1, 1 } }, { "C202", { true, 1, 1 } }, { "D520", { true, 1, 1 } }, { "CarrierFighter", { true, 1, 1 } }, { "Fighter", { true, 1, 1 } },
			{ "B17", { false, 4, 1 } }, { "Lancaster", { false, 4, 2 } }, { "He111", { false, 2, 1 } }, { "Wellington", { false, 2, 1 } }, { "Betty", { false, 2, 1 } },
			{ "Nell", { false, 2, 2 } }, { "Pe2", { false, 2, 2 } }, { "LeO451", { false, 2, 2 } }, { "SM79", { false, 3, 1 } }, { "Stuka", { false, 1, 1 } },
			{ "Sturmovik", { false, 1, 1 } }, { "Val", { false, 1, 1 } }, { "Kate", { false, 1, 1 } }, { "Dauntless", { false, 1, 1 } }, { "Devastator", { false, 1, 1 } },
			{ "Bomber", { false, 2, 1 } },
			// (the finer B-17, Content/Models/B17Fine.json, the Battle rules card's "Finer B-17": four engines, one fin)
			{ "B17Fine", { false, 4, 1 } },
			// (the Silver Mustang, Content/Models/MustangBare.json, the Battle rules card's "Silver Mustang": her bare metal
			// file, drawn for every Mustang with the row on; one engine, one fin)
			{ "MustangBare", { true, 1, 1 } },
		};
		return E;
	}

	PaintIs ClassOf(const Found& F, int Paint) { return Paint >= 0 && static_cast<size_t>(Paint) < F.Paints.size() ? F.Paints[static_cast<size_t>(Paint)] : PaintIs::Never; }
	/** A rivet or seam line that takes the coat's colour (the finer B-17's: AirUnitColours::LinesTakeCoat) is the coat where
	 * it lies; every other model has none, and its classes are as they were. */
	PaintIs RegionOf(const Found& F, int Paint) { const PaintIs Is = ClassOf(F, Paint); return Is == PaintIs::Line ? PaintIs::Coat : Is; }
	/** The palette's colour as a line wears it: each channel times the line's shade against its coat, rounded. */
	Rgb LineShade(const Found& F, int Paint, Rgb C)
	{
		const double K = Paint >= 0 && static_cast<size_t>(Paint) < F.Shade.size() ? F.Shade[static_cast<size_t>(Paint)] : 1.0;
		if (K == 1.0) return C;
		auto One = [K](uint8_t V) { return static_cast<uint8_t>(std::clamp(std::lround(V * K), 0L, 255L)); };
		return Rgb{ One(C.R), One(C.G), One(C.B) };
	}

	double Area(const double A[3], const double B[3], const double C[3], double Out[3])
	{
		const double U[3] = { B[0] - A[0], B[1] - A[1], B[2] - A[2] }, V[3] = { C[0] - A[0], C[1] - A[1], C[2] - A[2] };
		Out[0] = U[1] * V[2] - U[2] * V[1];
		Out[1] = U[2] * V[0] - U[0] * V[2];
		Out[2] = U[0] * V[1] - U[1] * V[0];
		return 0.5 * std::sqrt(Out[0] * Out[0] + Out[1] * Out[1] + Out[2] * Out[2]);
	}

	/** A model's checks with one look on one of its meshes. */
	struct Result
	{
		int Earned = 0, Never = 0, OffPart = 0, TipLow = 0, NightLow = 0, Wrong = 0, Pieces = 0, AreaOff = 0, Turned = 0, Weights = 0;
		int PartName = 0, PatchFar = 0, PatchPlain = 0, Patched = 0;
	};

	using Outline = std::vector<std::pair<double, double>>;
	/** A tail marking's own footprint (x and z) as the miniatures draw it, measured here apart from the module: a disc, a
	 * square, its four-cornered place, or Savoy's cross's two arms and shield; the face it is on (the sign of a facing
	 * across the span that sees it; 0 either), and how far a patch may reach beyond it ("a little larger"). */
	struct Footprint
	{
		std::string Tag;
		double Y = 0.0, Side = 0.0;
		bool bDisc = false;
		double X = 0.0, Z = 0.0, R = 0.0;
		std::vector<Outline> Shapes;
		double Top = 0.0, Margin = 0.0;
		double Distance(double PX, double PZ) const
		{
			if (bDisc) return std::max(0.0, std::hypot(PX - X, PZ - Z) - R);
			double Best = 1e18;
			for (const Outline& O : Shapes)
			{
				bool bPos = false, bNeg = false;
				double Edge = 1e18;
				for (size_t K = 0; K < O.size(); ++K)
				{
					const auto& A = O[K];
					const auto& B = O[(K + 1) % O.size()];
					const double DX = B.first - A.first, DZ = B.second - A.second;
					const double Cross = DX * (PZ - A.second) - DZ * (PX - A.first);
					bPos = bPos || Cross > 0.0;
					bNeg = bNeg || Cross < 0.0;
					const double Len2 = DX * DX + DZ * DZ;
					const double T = Len2 > 1e-18 ? std::clamp(((PX - A.first) * DX + (PZ - A.second) * DZ) / Len2, 0.0, 1.0) : 0.0;
					Edge = std::min(Edge, std::hypot(PX - (A.first + T * DX), PZ - (A.second + T * DZ)));
				}
				Best = std::min(Best, bPos && bNeg ? Edge : 0.0);
			}
			return Best;
		}
	};
	std::vector<Footprint> FootprintsOf(const Model& M, const Look& L)
	{
		std::vector<Footprint> Out;
		for (const MarkPlace& Mk : M.Marks)
		{
			if (Mk.Tag == "*FIN" && !L.bFinStripes) continue; // (not drawn: the nation has no rudder stripes)
			for (const double Hand : { 1.0, -1.0 })
			{
				if (Hand < 0.0 && !Mk.bBoth) continue;
				Footprint Fp;
				Fp.Tag = Mk.Tag;
				Fp.Y = Hand * Mk.At[1];
				Fp.Side = std::fabs(Mk.Facing[1]) < 0.3 ? 0.0 : Hand * Mk.Facing[1] > 0.0 ? 1.0 : -1.0;
				if (Mk.Quad.size() == 4)
				{
					const auto LL = Mk.Quad[0], LT = Mk.Quad[1], HT = Mk.Quad[2], HL = Mk.Quad[3];
					auto Place = [&](double U, double V) {
						const double AX = LL.first + (LT.first - LL.first) * U, AZ = LL.second + (LT.second - LL.second) * U;
						const double BX = HL.first + (HT.first - HL.first) * U, BZ = HL.second + (HT.second - HL.second) * U;
						return std::make_pair(AX + (BX - AX) * V, AZ + (BZ - AZ) * V);
					};
					auto Box = [&](double U0, double V0, double U1, double V1) { Fp.Shapes.push_back({ Place(U0, V0), Place(U1, V0), Place(U1, V1), Place(U0, V1) }); };
					if (Mk.Tag == "ITA-CROSS")
					{
						// (CampaignMiniatures' Savoy cross: arms 0.24 of the chord wide over the whole height and chord; a
						// shield 0.34 of the chord across, 1.24 times that high)
						const auto Mid0 = Place(0.0, 0.5), Mid1 = Place(1.0, 0.5), Up0 = Place(0.5, 0.0), Up1 = Place(0.5, 1.0);
						const double Chord = std::max(0.05, std::hypot(Mid1.first - Mid0.first, Mid1.second - Mid0.second));
						const double Tall = std::max(0.05, std::hypot(Up1.first - Up0.first, Up1.second - Up0.second));
						Box(0.38, 0.0, 0.62, 1.0);
						Box(0.0, 0.5 - 0.12 * Chord / Tall, 1.0, 0.5 + 0.12 * Chord / Tall);
						Box(0.33, 0.5 - 0.2108 * Chord / Tall, 0.67, 0.5 + 0.2108 * Chord / Tall);
					}
					else
					{
						Box(0.0, 0.0, 1.0, 1.0);
					}
					Fp.Margin = 0.1;
					Fp.Top = -1e18;
					for (const Outline& O : Fp.Shapes)
						for (const auto& C : O) Fp.Top = std::max(Fp.Top, C.second);
				}
				else if (Mk.Tag == "#" || Mk.Tag.find("FIN") != std::string::npos)
				{
					const double X = Mk.At[0], Z = Mk.At[2], R = Mk.Radius;
					Fp.Shapes.push_back({ { X - R, Z - R }, { X + R, Z - R }, { X + R, Z + R }, { X - R, Z + R } });
					Fp.Margin = 0.25 * R;
					Fp.Top = Z + R;
				}
				else
				{
					Fp.bDisc = true;
					Fp.X = Mk.At[0];
					Fp.Z = Mk.At[2];
					Fp.R = Mk.Radius;
					Fp.Margin = 0.3 * Mk.Radius;
					Fp.Top = Fp.Z + Fp.R;
				}
				Out.push_back(Fp);
			}
		}
		return Out;
	}

	/** Whether a part's name is one the earned tail never reaches (a newer file's moving surfaces, wings, airscrews, bay,
	 * guns and load), and whether it is a rudder. */
	bool OffTail(const std::string& Name)
	{
		for (const char* Prefix : { "aileron", "elevator", "flap", "wing", "prop", "bay", "turret", "flexgun", "bombs" })
		{
			if (Name.rfind(Prefix, 0) == 0) return true;
		}
		return false;
	}
	bool IsRudder(const std::string& Name) { return Name.rfind("rudder", 0) == 0; }

	Result Examine(const Model& M, const Found& Regions, bool bFar, const Palette& P, const Look& L, const Coat& C)
	{
		// (the regions as laid on the mesh examined: on a far shape made without spinners, the finer B-17's, its own
		// stand-ins, AirUnitColours::OnShape; on every other mesh the regions themselves)
		const Found F = OnShape(Regions, bFar);
		Result R;
		const Mesh& Me = bFar ? M.Far : M.Near;
		std::vector<Where> Pl;
		std::vector<int> TailOf;
		Places(M, F, bFar, Pl, &TailOf);
		const TailPart Area_ = PartFor(L.Nation);
		const Scheme* S = P.Of(L.Nation, L.Scheme);
		const bool bNight = NightFinish(M, L);
		std::map<int, Rgb> Laid;
		for (const auto& Each : C.Points) Laid[Each.first] = Each.second;
		// (by part name, on a file that names its parts: Britain's tail only on a rudder, France's never; none on a part
		// the tail never reaches)
		bool bRudders = false;
		for (const Part& Pt : M.Parts) bRudders = bRudders || IsRudder(Pt.Name);
		auto PartWrong = [&](size_t Point) {
			if (Me.Parts.empty() || M.Parts.empty()) return false;
			const int Index = Me.Parts[Point];
			if (Index < 0 || static_cast<size_t>(Index) >= M.Parts.size()) return true;
			const std::string& Name = M.Parts[static_cast<size_t>(Index)].Name;
			if (OffTail(Name)) return true;
			if (bRudders && Area_ == TailPart::Rudder && !IsRudder(Name)) return true;
			if (bRudders && Area_ == TailPart::Fin && IsRudder(Name)) return true;
			return false;
		};
		// (each plain patch only round a marking on its own face, a little larger than it; by night only round one that
		// reaches the pattern's half)
		const std::vector<Footprint> Prints = FootprintsOf(M, L);
		auto PatchNear = [&](double X, double Y, double Z, double FaceY) {
			double HalfAt = -1e18, Across = 1e18;
			for (const Tail& T : F.Tails)
			{
				const Frame& Fr = T.Frames[static_cast<int>(Area_)];
				if (Fr.bAny && std::fabs(Y - T.Y) < Across)
				{
					Across = std::fabs(Y - T.Y);
					HalfAt = Fr.ZLow + 0.5 * Fr.High();
				}
			}
			for (const Footprint& Fp : Prints)
			{
				if (std::fabs(Y - Fp.Y) > 0.6 || (Fp.Side != 0.0 && FaceY * Fp.Side < 0.0)) continue;
				if (bNight && Fp.Top + Fp.Margin < HalfAt) continue;
				if (Fp.Distance(X, Z) <= Fp.Margin + 1e-6) return true;
			}
			return false;
		};
		auto FacingY = [&Me](size_t K) { return Me.Facings[K * 3 + 1]; };
		for (const PatchLaid& Pl_ : C.Patches)
		{
			const std::string& Tag = M.Marks[static_cast<size_t>(Pl_.Mark)].Tag;
			++R.Patched;
			// (Britain's fin flash stands on the fin, the rudder stripes (France's own, or a plain machine's) on the rudder:
			// the parts Britain and France leave plain)
			if ((Area_ == TailPart::Rudder && Tag == "UK-FIN") || (Area_ == TailPart::Fin && (Tag == "FRA-FIN" || Tag == "*FIN"))) ++R.PatchPlain;
		}
		for (size_t K = 0; K < Me.Count(); ++K)
		{
			const Takes T = C.Took[K];
			if (T == Takes::Keep) continue;
			++R.Earned;
			const PaintIs Is = ClassOf(F, Me.Paints[K]);
			if (Is == PaintIs::Never) ++R.Never;
			// (each earned point where its step says)
			const bool bTail = T == Takes::First || T == Takes::Second || T == Takes::Patch || T == Takes::Tip;
			if (bTail)
			{
				const bool bOnPart = Area_ == TailPart::Both ? (Pl[K] == Where::Fin || Pl[K] == Where::Rudder) : Area_ == TailPart::Rudder ? Pl[K] == Where::Rudder : Pl[K] == Where::Fin;
				if (!bOnPart) ++R.OffPart;
				if (PartWrong(K)) ++R.PartName;
				if (TailOf[K] >= 0)
				{
					const Frame& Fr = F.Tails[static_cast<size_t>(TailOf[K])].Frames[static_cast<int>(Area_)];
					const double H = (Me.Z(K) - Fr.ZLow) / std::max(1e-9, Fr.High());
					if (T == Takes::Tip && H < 5.0 / 6.0 - 1e-6) ++R.TipLow;
					// (by night the pattern on the top half; a marking's patch is not the pattern, and is checked below)
					if ((T == Takes::First || T == Takes::Second) && bNight && H < 0.5 - 1e-6) ++R.NightLow;
				}
				if (T == Takes::Patch && !PatchNear(Me.X(K), Me.Y(K), Me.Z(K), FacingY(K))) ++R.PatchFar;
			}
			if (T == Takes::Spinner && Pl[K] != Where::Spinner) ++R.OffPart;
			if (T == Takes::Ring && Pl[K] != Where::Ring) ++R.OffPart;
			// (its colour the palette's, by day or night; soot laid over as the game lays it: unsooted points only)
			if (S && (Me.Soot.empty() || Me.Soot[K] == 0))
			{
				const std::string& Name = T == Takes::Spinner || T == Takes::Tip ? S->Spinner : T == Takes::Ring ? S->Ring : T == Takes::Second ? S->Second : S->First;
				const Rgb Want = LineShade(F, Me.Paints[K], P.Paint(Name, bNight));
				const auto Got = Laid.find(static_cast<int>(K));
				if (Got == Laid.end() || !(Got->second == Want)) ++R.Wrong;
			}
		}
		// The parted triangles: one class of paint, never "never"; their pieces covering them exactly, facing as they faced.
		for (const Cut& Ct : C.Cuts)
		{
			const PaintIs A = RegionOf(F, Me.Paints[static_cast<size_t>(Ct.Triangle[0])]);
			if (A == PaintIs::Never || RegionOf(F, Me.Paints[static_cast<size_t>(Ct.Triangle[1])]) != A || RegionOf(F, Me.Paints[static_cast<size_t>(Ct.Triangle[2])]) != A) ++R.Never;
			// (each earned piece's part and patch, as each earned point's)
			for (size_t Q = 0; Q + 2 < Ct.Pieces.size(); Q += 3)
			{
				double W[3] = { 0.0, 0.0, 0.0 };
				const AirUnitColours::Corner& First = Ct.Corners[static_cast<size_t>(Ct.Pieces[Q])];
				for (size_t I = 0; I < 3; ++I)
				{
					const AirUnitColours::Corner& Co = Ct.Corners[static_cast<size_t>(Ct.Pieces[Q + I])];
					for (int J = 0; J < 3; ++J) W[J] += Co.Weight[J] / 3.0;
				}
				double PX = 0.0, PY = 0.0, PZ = 0.0, PF = 0.0;
				for (int J = 0; J < 3; ++J)
				{
					const size_t V = static_cast<size_t>(Ct.Triangle[J]);
					PX += W[J] * Me.X(V);
					PY += W[J] * Me.Y(V);
					PZ += W[J] * Me.Z(V);
					PF += W[J] * FacingY(V);
				}
				const Takes What = First.What;
				if (What == Takes::First || What == Takes::Second || What == Takes::Patch || What == Takes::Tip)
				{
					if (PartWrong(static_cast<size_t>(Ct.Triangle[0]))) ++R.PartName;
					if (What == Takes::Patch && !PatchNear(PX, PY, PZ, PF)) ++R.PatchFar;
				}
			}
			double Corner[3][3];
			for (int I = 0; I < 3; ++I)
				for (int X = 0; X < 3; ++X) Corner[I][X] = Me.Points[static_cast<size_t>(Ct.Triangle[I]) * 3 + static_cast<size_t>(X)];
			double N0[3];
			const double Whole = Area(Corner[0], Corner[1], Corner[2], N0);
			double Sum = 0.0;
			for (size_t Q = 0; Q + 2 < Ct.Pieces.size(); Q += 3)
			{
				double At[3][3];
				for (int I = 0; I < 3; ++I)
				{
					const AirUnitColours::Corner& Co = Ct.Corners[static_cast<size_t>(Ct.Pieces[Q + static_cast<size_t>(I)])];
					const double WSum = Co.Weight[0] + Co.Weight[1] + Co.Weight[2];
					if (std::fabs(WSum - 1.0) > 1e-4 || Co.Weight[0] < -1e-5 || Co.Weight[1] < -1e-5 || Co.Weight[2] < -1e-5) ++R.Weights;
					for (int X = 0; X < 3; ++X) At[I][X] = Co.Weight[0] * Corner[0][X] + Co.Weight[1] * Corner[1][X] + Co.Weight[2] * Corner[2][X];
				}
				double N[3];
				const double A2 = Area(At[0], At[1], At[2], N);
				Sum += A2;
				if (A2 > 1e-9 && N[0] * N0[0] + N[1] * N0[1] + N[2] * N0[2] < 0.0) ++R.Turned;
				++R.Pieces;
			}
			if (std::fabs(Sum - Whole) > 1e-4 * std::max(Whole, 1e-6) + 1e-9) ++R.AreaOff;
		}
		return R;
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 4)
	{
		std::fprintf(stderr, "usage: air_unit_colours_test <unit-colours.json> <Content/Models> <unit_colours_raid.json>\n");
		return 2;
	}
	char Line[512];
	std::string Text, Why;

	// ---- The palette.
	std::printf("The palette (%s)\n", Argv[1]);
	Palette P;
	const bool bPalette = ReadText(Argv[1], Text) && ReadPalette(Text, P, Why);
	Check(bPalette, "read", Why);
	if (!bPalette) return 1;
	{
		size_t Schemes = 0;
		int SpinnerFirst = 0, SpinnerRing = 0, Numbers = 0, Twice = 0, Crossed = 0, NationTails = 0;
		std::set<std::tuple<std::string, std::string, std::string, std::string, std::string>> Seen;
		std::map<std::tuple<std::string, std::string, std::string>, std::set<int>> TailSides;
		const std::set<std::string> Axis = { "GER", "ITA", "JAP" };
		for (const auto& [Nation, List] : P.Nations)
		{
			std::set<int> Got;
			std::set<std::tuple<std::string, std::string, std::string>> Tails;
			for (const Scheme& S : List)
			{
				++Schemes;
				Got.insert(S.Number);
				SpinnerFirst += S.Spinner == S.First ? 1 : 0;
				SpinnerRing += S.Spinner != S.Ring ? 1 : 0;
				Twice += Seen.insert({ S.Spinner, S.Ring, S.Pattern, S.First, S.Second }).second ? 0 : 1;
				NationTails += Tails.insert({ S.Pattern, S.First, S.Second }).second ? 0 : 1;
				TailSides[{ S.Pattern, S.First, S.Second }].insert(Axis.count(Nation) ? 0 : 1);
			}
			Numbers += Got.size() == 16 && *Got.begin() == 0 && *Got.rbegin() == 15 ? 1 : 0;
		}
		for (const auto& Each : TailSides) Crossed += Each.second.size() > 1 ? 1 : 0;
		std::snprintf(Line, sizeof Line, "%zu nations, %zu schemes, %zu colours; spinner the tail's first %d, never the band %d", P.Nations.size(), Schemes, P.Colours.size(), SpinnerFirst, SpinnerRing);
		Check(P.Nations.size() == 7 && Schemes == 112 && Numbers == 7 && P.Colours.size() == 14 && SpinnerFirst == 112 && SpinnerRing == 112, "seven nations of sixteen schemes, each one idea", Line);
		std::snprintf(Line, sizeof Line, "%d the same as another, %d tails twice in a nation, %d tails on both sides", Twice, NationTails, Crossed);
		Check(Twice == 0 && NationTails == 0 && Crossed == 0, "no two schemes alike, no tail twice in a nation or on both sides", Line);
		const Rgb Cream = P.Paint("bone", true), Orange = P.Paint("orange", false), Dull = P.Paint("orange", true);
		std::snprintf(Line, sizeof Line, "cream at night #%02X%02X%02X; orange #%02X%02X%02X by night #%02X%02X%02X", Cream.R, Cream.G, Cream.B, Orange.R, Orange.G, Orange.B, Dull.R, Dull.G, Dull.B);
		Check(std::fabs(P.NightMultiply - 0.55) < 1e-9 && Cream == Rgb{ 0x7E, 0x7C, 0x72 } && Dull == Rgb{ static_cast<uint8_t>(std::lround(Orange.R * 0.55)), static_cast<uint8_t>(std::lround(Orange.G * 0.55)), static_cast<uint8_t>(std::lround(Orange.B * 0.55)) },
			"a night finish: every colour times 0.55, the cream its own grey", Line);
		Check(P.Patterns.size() == 5 && P.Patterns.count("tipped") && P.Patterns.count("bands") && P.Patterns.count("checks") && P.Patterns.count("stripes") && P.Patterns.count("halved"), "the five patterns");
	}

	// ---- The patterns on a frame: a tail 2 m high from z 1 to 3, its top front corner at x 0, its bottom rear at x -2.
	std::printf("The patterns\n");
	{
		Frame Fr;
		Fr.bAny = true;
		Fr.ZLow = 1.0;
		Fr.ZHigh = 3.0;
		Fr.XLeadTop = 0.0;
		Fr.XTrailLow = -2.0;
		Fr.SFore = 0.3;
		Fr.SAft = -2.1;
		const double Upright[2] = { 1.0, 0.0 };
		int Ok = 0;
		// tipped: its top fifth the second colour
		Ok += FirstAt("tipped", Fr, Upright, -1.0, 2.55) && !FirstAt("tipped", Fr, Upright, -1.0, 2.65) ? 1 : 0;
		// bands: five of 0.4 m, first, second, first, second, first
		Ok += FirstAt("bands", Fr, Upright, -1.0, 1.1) && !FirstAt("bands", Fr, Upright, -1.0, 1.5) && FirstAt("bands", Fr, Upright, -1.0, 1.9) && !FirstAt("bands", Fr, Upright, -1.0, 2.3) && FirstAt("bands", Fr, Upright, -1.0, 2.9) ? 1 : 0;
		// checks: squares of 0.4 m (five to the height), the first at the top front corner
		Ok += FirstAt("checks", Fr, Upright, -0.2, 2.8) && !FirstAt("checks", Fr, Upright, -0.6, 2.8) && !FirstAt("checks", Fr, Upright, -0.2, 2.4) && FirstAt("checks", Fr, Upright, -0.6, 2.4) ? 1 : 0;
		// stripes: six across the chord (2.4 m: 0.4 m each), the first foremost
		Ok += FirstAt("stripes", Fr, Upright, 0.1, 2.0) && !FirstAt("stripes", Fr, Upright, -0.3, 2.0) && FirstAt("stripes", Fr, Upright, -0.6, 2.0) && !FirstAt("stripes", Fr, Upright, -1.9, 2.0) ? 1 : 0;
		// halved: first above and behind the slant from (0, 3) to (-2, 1), second below and before
		Ok += FirstAt("halved", Fr, Upright, -1.5, 2.8) && !FirstAt("halved", Fr, Upright, -0.2, 1.3) ? 1 : 0;
		Check(Ok == 5, "tipped, bands, checks, stripes and halved where the brief puts them", std::to_string(Ok) + " of 5");
	}

	// ---- The marks and the looks.
	std::printf("The board's marks and the looks\n");
	{
		AirFeed::Json J;
		const std::string Board = "{\"corpsMarks\":[{\"corps\":4,\"nat\":\"GER\",\"scheme\":3,\"tier\":2,\"name\":\"A\"},{\"corps\":5,\"nat\":\"JPN\",\"scheme\":15,\"tier\":3,\"name\":\"B\"},"
			"{\"corps\":6,\"nat\":\"USA\",\"scheme\":2,\"tier\":0,\"name\":\"none yet\"},{\"corps\":7,\"nat\":\"UK\",\"scheme\":16,\"tier\":1},{\"nat\":\"UK\",\"scheme\":1,\"tier\":1}]}";
		std::map<int, CorpsMark> Marks;
		const bool bRead = AirFeed::Parse(Board, J, Why);
		ReadMarks(J["corpsMarks"], Marks);
		Check(bRead && Marks.size() == 2 && Marks.count(4) && Marks.count(5) && Marks.at(5).Nation == "JAP", "rows read; tier 0, scheme 16 and no corps left out; JPN read as JAP", std::to_string(Marks.size()) + " rows");
		Look L;
		const bool bOff = LookOf(false, Marks, 4, P, false, 6, L);
		const bool bNone = LookOf(true, Marks, 6, P, false, 6, L) || LookOf(true, Marks, -1, P, false, 6, L) || LookOf(true, Marks, 99, P, false, 6, L);
		Check(!bOff && !bNone, "the row off, a tier 0 group, no corps, a corps with no row: no look");
		Look A, B, Winter;
		const bool bA = LookOf(true, Marks, 4, P, false, 6, A), bB = LookOf(true, Marks, 4, P, false, 6, B), bW = LookOf(true, Marks, 4, P, false, 11, Winter);
		Check(bA && bB && A.Nation == B.Nation && A.Scheme == B.Scheme && A.Tier == B.Tier && !A.bWinter && bW && Winter.bWinter, "the same corps, the same look; December a winter month");
		Check(PartFor("UK") == TailPart::Rudder && PartFor("FRA") == TailPart::Fin && PartFor("GER") == TailPart::Both && PartFor("ITA") == TailPart::Both && PartFor("SOV") == TailPart::Both
				&& PartFor("JPN") == TailPart::Both && PartFor("USA") == TailPart::Both,
			"Britain the rudder, France the fin, the rest fin and rudder");
	}

	// ---- Every aircraft model.
	std::printf("Every aircraft model (%s)\n", Argv[2]);
	std::vector<std::string> Kinds;
	for (const auto& Entry : std::filesystem::directory_iterator(Argv[2]))
	{
		if (Entry.path().extension() != ".json") continue;
		std::string Head;
		{
			std::ifstream In(Entry.path(), std::ios::binary);
			char Buffer[4096] = {};
			In.read(Buffer, sizeof Buffer - 1);
			Head = Buffer;
		}
		if (Head.find("\"kind\": \"aircraft\"") == std::string::npos && Head.find("\"kind\":\"aircraft\"") == std::string::npos) continue;
		Kinds.push_back(Entry.path().stem().string());
	}
	std::sort(Kinds.begin(), Kinds.end());
	std::set<std::string> Missing;
	for (const auto& Each : Expected()) Missing.insert(Each.first);
	const std::vector<std::string> Nations = { "GER", "UK", "USA", "SOV", "JAP", "ITA", "FRA" };
	double Slowest = 0.0;
	std::string SlowestKind;
	int Models = 0;
	for (const std::string& Kind : Kinds)
	{
		Model M;
		const auto Began = std::chrono::steady_clock::now();
		if (!ReadText(std::string(Argv[2]) + "/" + Kind + ".json", Text) || !ReadModel(Text, Kind, M, Why))
		{
			Check(false, "a model read", Kind + ": " + Why);
			continue;
		}
		const Found F = Find(M);
		++Models;
		Missing.erase(Kind);
		const auto Want = Expected().find(Kind);
		// Its regions.
		std::vector<Where> Pl;
		std::vector<int> TailOf;
		Places(M, F, false, Pl, &TailOf);
		int Spinner = 0, Ring = 0, Fin = 0, Rudder = 0, CoatPoints = 0, SpinnerAft = 0, RingOut = 0, TailLow = 0;
		double XLo = 1e9, XHi = -1e9;
		std::vector<double> CoatZ;
		for (size_t K = 0; K < M.Near.Count(); ++K)
		{
			XLo = std::min(XLo, M.Near.X(K));
			XHi = std::max(XHi, M.Near.X(K));
			if (ClassOf(F, M.Near.Paints[K]) == PaintIs::Coat)
			{
				++CoatPoints;
				CoatZ.push_back(M.Near.Z(K));
			}
		}
		std::nth_element(CoatZ.begin(), CoatZ.begin() + static_cast<std::ptrdiff_t>(CoatZ.size() / 2), CoatZ.end());
		const double ZMid = CoatZ.empty() ? 0.0 : CoatZ[CoatZ.size() / 2];
		const double Long = XHi - XLo;
		for (size_t K = 0; K < M.Near.Count(); ++K)
		{
			const double X = M.Near.X(K), Y = M.Near.Y(K), Z = M.Near.Z(K);
			switch (Pl[K])
			{
			case Where::Spinner:
			{
				++Spinner;
				// (at an engine's front: within its spinner's length and a little of its axis)
				bool bAt = false;
				for (const Engine& E : F.Engines) bAt = bAt || (X >= E.SpinnerBack - 0.021 && X <= E.SpinnerFront + 0.021 && std::hypot(Y - E.Hub[1], Z - E.Hub[2]) <= 0.61);
				// (a fighter's only at its nose: the front fifth of the machine)
				if (!bAt || (F.bFighter && X < XHi - 0.2 * Long)) ++SpinnerAft;
				break;
			}
			case Where::Ring:
			{
				++Ring;
				bool bIn = false;
				for (const Engine& E : F.Engines) bIn = bIn || (X >= E.CowlFront - E.Cap - E.Band - 1e-9 && X <= E.CowlFront + 0.021 && std::hypot(Y - E.Hub[1], Z - E.Hub[2]) <= E.Reach + 1e-9);
				if (!bIn) ++RingOut;
				break;
			}
			case Where::Fin:
			case Where::Rudder:
			{
				(Pl[K] == Where::Fin ? Fin : Rudder) += 1;
				// (aft, and above the fuselage: the aft 40% of the machine, above its middle and above the fin's root; or the
				// model's own rudder part where it hangs below that, aft of every point of the fuselage at its height and side)
				bool bAbove = false;
				for (const Tail& T : F.Tails) bAbove = bAbove || (std::fabs(Y - T.Y) <= T.Slab + 1e-9 && Z >= T.RootA + T.RootB * X - 1e-9);
				const int Own = TailOf[K] >= 0 ? F.Tails[static_cast<size_t>(TailOf[K])].RudderPart : -1;
				bool bAftOfFuselage = Own >= 0 && !M.Near.Parts.empty() && M.Near.Parts[K] == Own;
				for (size_t J = 0; bAftOfFuselage && J < M.Near.Count(); ++J)
				{
					const bool bFuselage = Pl[J] == Where::None && ClassOf(F, M.Near.Paints[J]) == PaintIs::Coat && M.Parts[static_cast<size_t>(M.Near.Parts[J])].Name == "plane";
					if (bFuselage && std::fabs(M.Near.Y(J) - Y) < 0.3 && std::fabs(M.Near.Z(J) - Z) < 0.1 && M.Near.X(J) <= X) bAftOfFuselage = false;
				}
				if (X > XLo + 0.4 * Long || ((Z < ZMid || !bAbove) && !bAftOfFuselage)) ++TailLow;
				break;
			}
			default:
				break;
			}
		}
		bool bBands = !F.Engines.empty();
		for (const Engine& E : F.Engines) bBands = bBands && E.Band > 0.0 && E.Band <= E.SpinnerLong() + 1e-9;
		std::snprintf(Line, sizeof Line, "%s: %zu engines, %zu fins; spinner %d, band %d, fin %d, rudder %d of %d coat points%s%s", Kind.c_str(), F.Engines.size(), F.Tails.size(), Spinner, Ring, Fin, Rudder,
			CoatPoints, F.Note.empty() ? "" : "; ", F.Note.c_str());
		const bool bKnown = Want != Expected().end();
		Check(bKnown && F.bFighter == Want->second.bFighter && static_cast<int>(F.Engines.size()) == Want->second.Engines && static_cast<int>(F.Tails.size()) == Want->second.Tails,
			"its airscrews and fins found, fighter or bomber as it flies", Line);
		std::snprintf(Line, sizeof Line, "%s: %d spinner points off its nose, %d band points off the band, %d fin points low or forward", Kind.c_str(), SpinnerAft, RingOut, TailLow);
		Check(Spinner > 0 && Ring > 0 && Fin + Rudder >= 30 && (Fin + Rudder) * 4 < CoatPoints && SpinnerAft == 0 && RingOut == 0 && TailLow == 0 && bBands,
			"the spinner at the nose, the band behind it no wider than the spinner is long, the fin aft and above", Line);
		// (a far shape made without spinners, the finer B-17's alone, laid by its own stand-ins: its "engine" grey for the
		// spinner and its own cowling lips; every other model's far shape exactly as before)
		{
			int Moved = 0;
			std::string Which;
			for (size_t K = 0; K < F.FarPaints.size() && K < F.Paints.size(); ++K)
			{
				if (F.FarPaints[K] == F.Paints[K]) continue;
				++Moved;
				Which += " " + (K < M.PaintNames.size() ? M.PaintNames[K] : std::to_string(K));
			}
			std::string Lips;
			for (size_t K = 0; K < F.FarCowlFront.size() && K < F.Engines.size(); ++K)
			{
				char Each[48];
				std::snprintf(Each, sizeof Each, " %.3f/%.3f", F.FarCowlFront[K], F.Engines[K].CowlFront);
				Lips += Each;
			}
			const bool bStandIns = !F.FarPaints.empty() || !F.FarCowlFront.empty();
			std::snprintf(Line, sizeof Line, "%s: far stand-ins %s;%s far/near lips%s", Kind.c_str(), bStandIns ? "yes" : "none", Which.c_str(), Lips.c_str());
			Check(Kind == "B17Fine" ? Moved == 1 && Which == " engine" && F.FarCowlFront.size() == F.Engines.size() : !bStandIns,
				"a far shape without spinners (the finer B-17's alone) laid by its own stand-ins; every other far shape as before", Line);
		}

		// Its looks: every nation's, every step, day and night, near and far.
		int Earned[4] = { 0, 0, 0, 0 };
		Result Sum;
		int Steps = 0, Same = 0, Plain = 0, Tips = 0, FighterTips = 0, NightPoints = 0;
		for (const std::string& Nation : Nations)
		{
			for (const int Scheme_ : { 1, 3, 6, 15 })
			{
				for (const bool bNightBattle : { false, true })
				{
					Look L;
					L.Nation = Nation;
					L.Scheme = Scheme_;
					L.bFinStripes = Scheme_ != 6; // (a plain machine's rudder stripes drawn, and for one scheme not)
					for (int Tier = 0; Tier <= 3; ++Tier)
					{
						L.Tier = Tier;
						L.bNightBattle = bNightBattle;
						for (const bool bFar : { false, true })
						{
							if (bFar && M.Far.Count() == 0) continue;
							const Coat C = Paint(M, F, bFar, P, L);
							if (Tier == 0)
							{
								Plain += C.Points.empty() && C.Cuts.empty() ? 0 : 1;
								continue;
							}
							const Result R = Examine(M, F, bFar, P, L, C);
							Sum.Never += R.Never;
							Sum.OffPart += R.OffPart;
							Sum.TipLow += R.TipLow;
							Sum.NightLow += R.NightLow;
							Sum.Wrong += R.Wrong;
							Sum.Pieces += R.Pieces;
							Sum.AreaOff += R.AreaOff;
							Sum.Turned += R.Turned;
							Sum.Weights += R.Weights;
							Sum.PartName += R.PartName;
							Sum.PatchFar += R.PatchFar;
							Sum.PatchPlain += R.PatchPlain;
							Sum.Patched += R.Patched;
							if (!bFar) Earned[Tier] = R.Earned;
							Tips += C.Count(Takes::Tip);
							if (F.bFighter) FighterTips += C.Count(Takes::Tip);
							if (NightFinish(M, L)) NightPoints += C.Count(Takes::First) + C.Count(Takes::Second);
							// (each step adds: 1 the spinner (and a bomber's fin tip), 2 the band, 3 the tail)
							const int Sp = C.Count(Takes::Spinner), Rg = C.Count(Takes::Ring), Tl = C.Count(Takes::First) + C.Count(Takes::Second) + C.Count(Takes::Patch), Tp = C.Count(Takes::Tip);
							const bool bStep = Tier == 1 ? Sp > 0 && Rg == 0 && Tl == 0 && (F.bFighter ? Tp == 0 : Tp > 0)
								: Tier == 2 ? Sp > 0 && Rg > 0 && Tl == 0 && (F.bFighter ? Tp == 0 : Tp > 0)
											: Sp > 0 && Rg > 0 && Tl > 0 && Tp == 0;
							Steps += bStep ? 0 : 1;
							// (the same look again: the same coat)
							const Coat Again = Paint(M, F, bFar, P, L);
							Same += Again.Points == C.Points && Again.Cuts.size() == C.Cuts.size() && KeyOf(M, L) == KeyOf(M, L) ? 0 : 1;
						}
					}
				}
			}
		}
		std::snprintf(Line, sizeof Line, "%s: %d earned points off their part, %d recoloured that are not its coat or spinner, %d of the wrong colour", Kind.c_str(), Sum.OffPart, Sum.Never, Sum.Wrong);
		Check(Sum.OffPart == 0 && Sum.Never == 0 && Sum.Wrong == 0, "each nation's colours on its own part (Britain the rudder, France the fin), nothing else recoloured", Line);
		std::snprintf(Line, sizeof Line, "%s: %d earned tail points or pieces on the wrong part by its name%s", Kind.c_str(), Sum.PartName, M.Parts.empty() ? " (a file with no parts)" : "");
		Check(Sum.PartName == 0, "by the file's own part names: Britain's tail only on a rudder, France's never, none on a wing, an elevator, an aileron or a flap", Line);
		std::snprintf(Line, sizeof Line, "%s: %d patches laid, %d under a marking on a part its nation leaves plain, %d patch points or pieces not round a marking on their own face", Kind.c_str(), Sum.Patched, Sum.PatchPlain, Sum.PatchFar);
		Check(Sum.PatchPlain == 0 && Sum.PatchFar == 0, "a plain patch only round a marking on a part its nation colours, on its own face, a little larger than it", Line);
		std::snprintf(Line, sizeof Line, "%s: %d out of step, %d tip points below the top sixth, %d night pattern points below the half, %d at tier 0, %d not the same twice", Kind.c_str(), Steps, Sum.TipLow, Sum.NightLow, Plain, Same);
		Check(Steps == 0 && Sum.TipLow == 0 && Sum.NightLow == 0 && Plain == 0 && Same == 0 && FighterTips == 0 && (F.bFighter || Tips > 0), "each step adds its own; a bomber's tip its top sixth; by night the pattern on the top half; tier 0 nothing", Line);
		std::snprintf(Line, sizeof Line, "%s: %d pieces, %d not covering their triangle, %d turned over, %d corners off it", Kind.c_str(), Sum.Pieces, Sum.AreaOff, Sum.Turned, Sum.Weights);
		Check(Sum.Pieces > 0 && Sum.AreaOff == 0 && Sum.Turned == 0 && Sum.Weights == 0, "the parted triangles cover what they were cut from, facing as it faced", Line);
		// The coat's rivet and seam lines (paints rivet, rivet_under, seam, seam_under): the finer B-17's take the colour the coat
		// beside them takes, a shade of it (AirUnitColours::LinesTakeCoat; Examine has checked each one's colour above), so
		// that a recoloured tail keeps no olive lines; every other model's are never recoloured, as before.
		{
			Look L;
			L.Nation = "USA";
			L.Scheme = 1;
			L.bFinStripes = true;
			L.Tier = 3;
			const Coat C = Paint(M, F, false, P, L);
			std::vector<Where> OnTail;
			Places(M, F, false, OnTail);
			int Lines = 0, LinesEarned = 0, LinesTail = 0, LinesTailEarned = 0;
			for (size_t K = 0; K < M.Near.Count(); ++K)
			{
				const int Paint_ = M.Near.Paints[K];
				const std::string Name = Paint_ >= 0 && static_cast<size_t>(Paint_) < M.PaintNames.size() ? M.PaintNames[static_cast<size_t>(Paint_)] : std::string();
				if (Name != "rivet" && Name != "rivet_under" && Name != "seam" && Name != "seam_under") continue;
				++Lines;
				const bool bEarned = C.Took[K] != Takes::Keep;
				LinesEarned += bEarned ? 1 : 0;
				if (OnTail[K] == Where::Fin || OnTail[K] == Where::Rudder)
				{
					++LinesTail;
					LinesTailEarned += bEarned ? 1 : 0;
				}
			}
			const bool bFollow = LinesTakeCoat(Kind);
			std::snprintf(Line, sizeof Line, "%s: %d line points, %d earned at step 3 (the United States' first scheme); on the fin and rudder %d of %d", Kind.c_str(), Lines, LinesEarned, LinesTailEarned, LinesTail);
			Check(bFollow ? LinesTail > 0 && LinesTailEarned == LinesTail : LinesEarned == 0,
				bFollow ? "her rivet and seam lines take the coat's colour where it does (all of them on the fin and rudder)" : "its rivet and seam lines never recoloured", Line);
		}
		(void)Earned;
		(void)NightPoints;
		const double Took = std::chrono::duration<double>(std::chrono::steady_clock::now() - Began).count();
		if (Took > Slowest)
		{
			Slowest = Took;
			SlowestKind = Kind;
		}
	}
	std::string Left;
	for (const std::string& Each : Missing) Left += Each + " ";
	std::snprintf(Line, sizeof Line, "%d models; slowest %s, %.2f s for its 448 coats", Models, SlowestKind.c_str(), Slowest);
	Check(Missing.empty() && Models == static_cast<int>(Expected().size()), "every aircraft model the game flies examined", Missing.empty() ? std::string(Line) : "missing: " + Left);

	// ---- A coat laid on a shape: its triangles as the game keeps them (here one turned the other way about).
	std::printf("A coat laid on a shape\n");
	{
		Model M;
		if (ReadText(std::string(Argv[2]) + "/Bf109.json", Text) && ReadModel(Text, "Bf109", M, Why))
		{
			const Found F = Find(M);
			Look L;
			L.Nation = "GER";
			L.Scheme = 1;
			L.Tier = 3;
			const Coat C = Paint(M, F, false, P, L);
			std::vector<int> Shape = M.Near.Triangles, Out, Out2;
			std::vector<std::pair<int, int>> New, New2;
			const int Count = static_cast<int>(M.Near.Count());
			LayTriangles(C, Shape, Count, Count + 10, Out, New);
			// (the shape's own turn of each triangle: B, C, A is the same; A, C, B the other way about)
			std::vector<int> Turned = Shape;
			for (size_t T = 0; T + 2 < Turned.size(); T += 3) std::swap(Turned[T + 1], Turned[T + 2]);
			LayTriangles(C, Turned, Count, Count + 10, Out2, New2);
			size_t CutCorners = 0;
			for (const Cut& Ct : C.Cuts) CutCorners += Ct.Corners.size();
			bool bFlipped = Out.size() == Out2.size();
			for (size_t T = 0; bFlipped && T + 2 < Out.size(); T += 3)
			{
				const bool bNew = Out[T] >= Count + 10;
				if (bNew) bFlipped = Out[T] == Out2[T] && Out[T + 1] == Out2[T + 2] && Out[T + 2] == Out2[T + 1];
			}
			std::snprintf(Line, sizeof Line, "%zu triangles became %zu; %zu new corners (the cuts hold %zu)", Shape.size() / 3, Out.size() / 3, New.size(), CutCorners);
			Check(New.size() == CutCorners && Out.size() > Shape.size() && Out.back() < Count + 10 + static_cast<int>(New.size()), "each parted triangle's pieces in its place, their corners numbered after the shape's", Line);
			Check(bFlipped, "a triangle the shape keeps turned the other way about: its pieces turned with it");
			std::vector<int> NoCut;
			std::vector<std::pair<int, int>> NoNew;
			LayTriangles(Coat(), Shape, Count, Count, NoCut, NoNew);
			Check(NoCut == Shape && NoNew.empty(), "no coat: the shape's triangles as they were");
		}
		else
		{
			Check(false, "the Bf 109 read", Why);
		}
	}

	// ---- The hand-made raid.
	std::printf("The hand-made raid (%s)\n", Argv[3]);
	{
		AirFeed::Json J;
		AirFeed::Sky Board;
		AirFeed::Frame First;
		Why.clear();
		const bool bRaid = ReadText(Argv[3], Text) && AirFeed::Parse(Text, J, Why) && AirFeed::ReadSky(J["sky"], Board) && AirFeed::ReadView(J["view"], Board, First);
		Check(bRaid && J["what"].Str("").rfind("HAND-MADE", 0) == 0, "its board and first view read; it says it is hand-made (its \"what\", as the Mac's ticket says it)", Why);
		if (bRaid)
		{
			std::set<int> Tiers;
			for (const auto& Each : Board.CorpsMarks) Tiers.insert(Each.second.Tier);
			int Matched = 0, Fighters = 0, Bombers = 0;
			std::set<int> Sides;
			for (const AirShow::Group& G : First.Groups)
			{
				Look L;
				if (!LookOf(true, Board.CorpsMarks, G.Corps, P, Board.bNight, Board.Month, L)) continue;
				++Matched;
				(G.bFighter ? Fighters : Bombers) += 1;
				if (G.bFighter) Sides.insert(G.Side);
				Look Off;
				Check(!LookOf(false, Board.CorpsMarks, G.Corps, P, Board.bNight, Board.Month, Off), "with the row off, no look for it", G.Name);
			}
			std::snprintf(Line, sizeof Line, "%zu rows (tiers %zu), %zu groups, %d wear colours: %d fighter groups of %zu sides, %d bomber group", Board.CorpsMarks.size(), Tiers.size(), First.Groups.size(), Matched, Fighters, Sides.size(), Bombers);
			Check(Board.CorpsMarks.size() == 3 && Tiers == std::set<int>{ 1, 2, 3 } && Matched == 3 && Fighters == 2 && Sides.size() == 2 && Bombers == 1,
				"place.corpsMarks read, every group's corps matching its row", Line);
			AirFeed::Sky None;
			AirFeed::Json Empty;
			AirFeed::Parse("{\"place\":{}}", Empty, Why);
			AirFeed::ReadSky(Empty, None);
			Check(None.CorpsMarks.empty(), "a board that names none: no group wears colours");
		}
	}

	std::printf("%s: %d failed\n", Bad == 0 ? "PASS" : "FAIL", Bad);
	return Bad == 0 ? 0 : 1;
}
