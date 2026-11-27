// The far airscrews' blur test (the Battle rules card's "Far airscrews a faint blur", row flyPropBlur; Brian, 27 November
// 2026 01:23:16Z: "yeah the far propellers don't move and dont look good"). His picture of the Overview showed each far
// B-17's airscrew a big solid disc with a yellow rim: past 300 m every aircraft is drawn from its far shape, on the
// aircraft's own solid ink, and the newer models' far meshes carry the modeller's blur disc (paints blur and blur_tip),
// drawn solid there with the blades standing still behind it, the row "flyPropSpin" off (as in every build played) or
// on without its haze's ink. With the row on the far shape loses the blades and the blur disc (Battle/AirScrew's
// StripScrews, as "flyPropSpin" takes them) and the copies draw a faint grey disc in their place (MakeBlurDisc); close
// to, the blades' painted marks go with the blades (BladeDecals) and the tips' paint is a faint arc behind each blade
// (MakeTipArcs). Usage:
//   air_prop_blur_test <Source/TheaterCommand> <model.json ...>
// a model that is not an aircraft is passed over. For each aircraft:
//   B1 the near mesh's blade marks inside its airscrews' discs found (the finer B-17's 84 a screw, the Wellington's the
//      same; no other aircraft carries any), and with the blades, the blur disc and the marks taken nothing painted
//      blade, blur or decal is left standing in a disc;
//   B2 its far mesh has no mark in a disc (the far strip needs no pass of its own for them);
//   B3 the tips' paint read from the file (blade_tip, else blur_tip, where the near mesh paints it; none, no arc);
// and once:
//   B4 a picture of the far disc at the battle's real scale (the 55 degree lens across the 1,050 or so pixels the scene
//      is drawn at, 1,008 pixels a radian), at the Overview's 434 and 832 m: the finer B-17's own blur disc drawn solid
//      as today (what Brian saw) beside the row's disc, which stands 3 to 10 levels of 255 off the sky (a shimmer at
//      most, fainter than "flyPropSpin"'s haze) and is grey (no yellow: red and blue within a level);
//   B5 close to (50, 150, 280 m) the tips' arc is seen (the tips' band 5 levels or more toward the tips' paint, 3 at
//      280 m where the band is under a pixel wide) and faint (no pixel moved more than 40 levels by it), on the old disc
//      and on "flyPropSpin"'s smear, and it turns with the disc;
//   B6 the row off draws what it drew before: the Unreal side read from its source (it cannot be built here), every new
//      branch behind the row, the row off unless asked, never sent to the land rules.
#include "Battle/AirScrew.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	int Bad = 0;
	const double Pi = 3.14159265358979323846;

	void Check(bool bOk, const std::string& What, const std::string& Got = "")
	{
		std::printf("  %s %s%s%s\n", bOk ? "ok  " : "FAIL", What.c_str(), Got.empty() ? "" : ": ", Got.c_str());
		if (!bOk) ++Bad;
	}

	bool ReadText(const std::string& Path, std::string& Out)
	{
		std::ifstream In(Path, std::ios::binary);
		if (!In) return false;
		std::stringstream Buf;
		Buf << In.rdbuf();
		Out = Buf.str();
		return true;
	}

	int Count(const std::string& Text, const std::string& What)
	{
		int N = 0;
		for (size_t At = Text.find(What); At != std::string::npos; At = Text.find(What, At + 1)) ++N;
		return N;
	}

	/** The numbers of the array Key in a model file's part ("near" or "far": its key with its object after it). */
	std::vector<double> PartNumbers(const std::string& Text, const char* Part, const char* Key)
	{
		std::vector<double> Out;
		size_t At = Text.find(std::string("\"") + Part + "\":{");
		if (At == std::string::npos) At = Text.find(std::string("\"") + Part + "\": {");
		const size_t Found = At == std::string::npos ? std::string::npos : Text.find(std::string("\"") + Key + "\"", At);
		const size_t Open = Found == std::string::npos ? std::string::npos : Text.find('[', Found);
		const size_t Close = Open == std::string::npos ? std::string::npos : Text.find(']', Open);
		if (Close == std::string::npos) return Out;
		const char* P = Text.c_str() + Open + 1;
		const char* End = Text.c_str() + Close;
		while (P < End)
		{
			char* Next = nullptr;
			const double V = std::strtod(P, &Next);
			if (Next == P) { ++P; continue; }
			Out.push_back(V);
			P = Next;
		}
		return Out;
	}

	int TopNumber(const std::string& Text, const std::string& Key)
	{
		int Out = -1;
		for (size_t At = Text.find(Key); At != std::string::npos; At = Text.find(Key, At + 1))
		{
			size_t Next = At + Key.size();
			while (Next < Text.size() && (Text[Next] == ' ' || Text[Next] == '\n')) ++Next;
			if (Next < Text.size() && Text[Next] == ':') Out = std::atoi(Text.c_str() + Next + 1);
		}
		return Out;
	}

	/** The file's paint colours ("colours", sRGB hex by paint number). */
	std::vector<std::array<int, 3>> Colours(const std::string& Text)
	{
		std::vector<std::array<int, 3>> Out;
		const size_t Key = Text.find("\"colours\"");
		const size_t Open = Key == std::string::npos ? Key : Text.find('[', Key);
		const size_t Close = Open == std::string::npos ? Open : Text.find(']', Open);
		for (size_t At = Open == std::string::npos ? Close : Open + 1; Close != std::string::npos && At < Close;)
		{
			const size_t Begin = Text.find('"', At);
			if (Begin == std::string::npos || Begin >= Close) break;
			const size_t End = Text.find('"', Begin + 1);
			const unsigned long V = std::strtoul(Text.substr(Begin + 1, End - Begin - 1).c_str(), nullptr, 16);
			Out.push_back({ static_cast<int>((V >> 16) & 0xFF), static_cast<int>((V >> 8) & 0xFF), static_cast<int>(V & 0xFF) });
			At = End + 1;
		}
		return Out;
	}

	struct Mesh
	{
		std::vector<AirScrew::Vec3> Points;
		std::vector<int> Paints, Triangles;
	};

	Mesh Part(const std::string& Text, const char* Name, const std::vector<int>& Paints)
	{
		Mesh M;
		const std::vector<double> Flat = PartNumbers(Text, Name, "points");
		for (size_t K = 0; K + 2 < Flat.size(); K += 3) M.Points.push_back({ Flat[K], Flat[K + 1], Flat[K + 2] });
		for (const double V : PartNumbers(Text, Name, "triangles")) M.Triangles.push_back(static_cast<int>(V));
		M.Paints = Paints;
		return M;
	}

	int PaintOf(const Mesh& M, int Point) { return Point >= 0 && static_cast<size_t>(Point) < M.Paints.size() ? M.Paints[static_cast<size_t>(Point)] : -1; }

	bool InDisc(const AirScrew::Vec3& P, const AirScrew::Screw& S)
	{
		const double DX = P.X - S.Hub.X, DY = P.Y - S.Hub.Y, DZ = P.Z - S.Hub.Z;
		const double Along = DX * S.Axis.X + DY * S.Axis.Y + DZ * S.Axis.Z;
		const double RX = DX - Along * S.Axis.X, RY = DY - Along * S.Axis.Y, RZ = DZ - Along * S.Axis.Z;
		return std::fabs(Along) <= 0.25 * S.Radius && std::sqrt(RX * RX + RY * RY + RZ * RZ) <= 1.05 * S.Radius;
	}

	/** Each triangle of M with every corner in one of Screws' discs. */
	std::vector<bool> InDiscs(const Mesh& M, const std::vector<AirScrew::Screw>& Screws)
	{
		std::vector<bool> Out(M.Triangles.size() / 3, false);
		for (size_t T = 0; T < Out.size(); ++T)
			for (const AirScrew::Screw& S : Screws)
			{
				bool bAll = true;
				for (int C = 0; C < 3 && bAll; ++C)
				{
					const int P = M.Triangles[T * 3 + static_cast<size_t>(C)];
					bAll = P >= 0 && static_cast<size_t>(P) < M.Points.size() && InDisc(M.Points[static_cast<size_t>(P)], S);
				}
				if (bAll) { Out[T] = true; break; }
			}
		return Out;
	}

	// ---- The picture (B4, B5): a disc against the sky, seen 30 degrees off its shaft, in colour. The airscrews' inks
	// (M_Highlight close to, M_ScrewHaze far off) take a point's colour bytes as linear light as they are and lay it over
	// what is behind by its alpha (build_content.py); the aircraft's own ink (M_MapSurface) makes its paint linear first
	// (Power 2.2) and is solid: today's far blur disc is drawn so, at a light of 1.
	double Lin(double S) { return S <= 0.04045 ? S / 12.92 : std::pow((S + 0.055) / 1.055, 2.4); }
	double Srgb(double L) { L = std::clamp(L, 0.0, 1.0); return 255.0 * (L <= 0.0031308 ? L * 12.92 : 1.055 * std::pow(L, 1.0 / 2.4) - 0.055); }

	/** One layer of the picture: points in the disc's plane, each its alpha and its light (linear, by channel), and its
	 * front faces; laid over the layers before it. Turning: the layer turns with the disc (the near disc's sections). */
	struct Layer
	{
		std::vector<double> Y, Z, Alpha;
		std::vector<std::array<double, 3>> Light;
		std::vector<int> Triangles;
		bool bTurning = true;
	};

	Layer LayerOf(const AirScrew::DiscMade& D)
	{
		Layer L;
		for (size_t K = 0; K < D.Points.size(); ++K)
		{
			L.Y.push_back(D.Points[K].Y);
			L.Z.push_back(D.Points[K].Z);
			L.Alpha.push_back(D.Alpha[K] / 255.0);
			L.Light.push_back({ D.Red[K] / 255.0, D.Green[K] / 255.0, D.Blue[K] / 255.0 });
		}
		L.Triangles.assign(D.Triangles.begin(), D.Triangles.begin() + static_cast<std::ptrdiff_t>(D.Front));
		return L;
	}

	/** The old near disc (AirBattle.cpp's DiscOf, both rows off), as it lays its points. */
	Layer OldDisc(double Radius, int Blades)
	{
		AirScrew::DiscMade D = AirScrew::MakeDisc(Radius, Blades, false);
		const int Round = AirScrew::NearRound;
		for (int Ring = 0; Ring < 4; ++Ring)
			for (int K = 0; K < Round; ++K)
			{
				const double A = 2.0 * Pi * K / Round;
				const double Swept = std::pow(0.5 + 0.5 * std::cos(Blades * A), 5.0);
				const double Alpha = Ring == 0 || Ring == 3 ? 0.0 : (Ring == 1 ? 46.0 + 80.0 * Swept : 34.0 + 64.0 * Swept);
				const size_t I = static_cast<size_t>(Ring * Round + K);
				D.Alpha[I] = static_cast<unsigned char>(std::lround(Alpha));
				D.Red[I] = 30;
				D.Green[I] = 30;
				D.Blue[I] = 32;
			}
		return LayerOf(D);
	}

	/** A far mesh's triangles (Which) about one screw, brought into its disc's plane, solid, each corner its paint made
	 * linear: the far shape's blur disc as the copies draw it today. */
	Layer SolidOf(const Mesh& M, const std::vector<bool>& Which, const AirScrew::Screw& S, const std::vector<std::array<int, 3>>& Paint)
	{
		Layer L;
		L.bTurning = false;
		// (two ways square to the shaft)
		const AirScrew::Vec3 Ax = S.Axis;
		const AirScrew::Vec3 Pick = std::fabs(Ax.Z) < 0.9 ? AirScrew::Vec3{ 0.0, 0.0, 1.0 } : AirScrew::Vec3{ 0.0, 1.0, 0.0 };
		AirScrew::Vec3 U = { Ax.Y * Pick.Z - Ax.Z * Pick.Y, Ax.Z * Pick.X - Ax.X * Pick.Z, Ax.X * Pick.Y - Ax.Y * Pick.X };
		const double UL = std::sqrt(U.X * U.X + U.Y * U.Y + U.Z * U.Z);
		U = { U.X / UL, U.Y / UL, U.Z / UL };
		const AirScrew::Vec3 V = { Ax.Y * U.Z - Ax.Z * U.Y, Ax.Z * U.X - Ax.X * U.Z, Ax.X * U.Y - Ax.Y * U.X };
		for (size_t T = 0; T < Which.size(); ++T)
		{
			if (!Which[T]) continue;
			for (int C = 0; C < 3; ++C)
			{
				const int P = M.Triangles[T * 3 + static_cast<size_t>(C)];
				const AirScrew::Vec3& At = M.Points[static_cast<size_t>(P)];
				const AirScrew::Vec3 D = { At.X - S.Hub.X, At.Y - S.Hub.Y, At.Z - S.Hub.Z };
				L.Y.push_back(D.X * U.X + D.Y * U.Y + D.Z * U.Z);
				L.Z.push_back(D.X * V.X + D.Y * V.Y + D.Z * V.Z);
				L.Alpha.push_back(1.0);
				const int K = PaintOf(M, P);
				const std::array<int, 3> Rgb = K >= 0 && static_cast<size_t>(K) < Paint.size() ? Paint[static_cast<size_t>(K)] : std::array<int, 3>{ 0, 0, 0 };
				L.Light.push_back({ std::pow(Rgb[0] / 255.0, 2.2), std::pow(Rgb[1] / 255.0, 2.2), std::pow(Rgb[2] / 255.0, 2.2) });
				L.Triangles.push_back(static_cast<int>(L.Y.size()) - 1);
			}
		}
		return L;
	}

	using Rgb = std::array<double, 3>;

	/** The picture (sRGB levels, 0 to 255, by channel), W by W pixels about the hub, 4 by 4 looks a pixel; the turning
	 * layers turned by Turn degrees toward the growing angle. Each layer's first front face under a look is laid over. */
	std::vector<Rgb> Picture(const std::vector<const Layer*>& Layers, double Metres, double Turn, int W, double Sky, double PerRadian)
	{
		const double Off = 30.0 * Pi / 180.0;
		std::vector<Rgb> Out(static_cast<size_t>(W * W), Rgb{ 0.0, 0.0, 0.0 });
		const int SS = 4;
		for (int PY = 0; PY < W; ++PY)
			for (int PX = 0; PX < W; ++PX)
			{
				Rgb Sum = { 0.0, 0.0, 0.0 };
				for (int J = 0; J < SS; ++J)
					for (int I = 0; I < SS; ++I)
					{
						const double SX = ((PX + (I + 0.5) / SS) - W * 0.5) / PerRadian * Metres;
						const double SY = ((PY + (J + 0.5) / SS) - W * 0.5) / PerRadian * Metres;
						const double Y0 = SX / std::cos(Off), Z0 = SY;
						Rgb Colour = { Lin(Sky / 255.0), Lin(Sky / 255.0), Lin(Sky / 255.0) };
						for (const Layer* L : Layers)
						{
							double Y = Y0, Z = Z0;
							if (L->bTurning)
							{
								const double T = -Turn * Pi / 180.0;
								const double A = std::atan2(Y0, Z0) + T, R = std::sqrt(Y0 * Y0 + Z0 * Z0);
								Y = R * std::sin(A);
								Z = R * std::cos(A);
							}
							for (size_t K = 0; K + 2 < L->Triangles.size(); K += 3)
							{
								const int P0 = L->Triangles[K], P1 = L->Triangles[K + 1], P2 = L->Triangles[K + 2];
								const double X0 = L->Y[static_cast<size_t>(P0)], Y0p = L->Z[static_cast<size_t>(P0)];
								const double X1 = L->Y[static_cast<size_t>(P1)], Y1 = L->Z[static_cast<size_t>(P1)];
								const double X2 = L->Y[static_cast<size_t>(P2)], Y2 = L->Z[static_cast<size_t>(P2)];
								if (Y < std::min({ X0, X1, X2 }) || Y > std::max({ X0, X1, X2 }) || Z < std::min({ Y0p, Y1, Y2 }) || Z > std::max({ Y0p, Y1, Y2 })) continue;
								const double Area = (X1 - X0) * (Y2 - Y0p) - (X2 - X0) * (Y1 - Y0p);
								if (std::fabs(Area) < 1e-15) continue;
								const double W0 = ((X1 - Y) * (Y2 - Z) - (X2 - Y) * (Y1 - Z)) / Area;
								const double W1 = ((X2 - Y) * (Y0p - Z) - (X0 - Y) * (Y2 - Z)) / Area;
								const double W2 = 1.0 - W0 - W1;
								if (W0 < -1e-12 || W1 < -1e-12 || W2 < -1e-12) continue;
								const double Al = W0 * L->Alpha[static_cast<size_t>(P0)] + W1 * L->Alpha[static_cast<size_t>(P1)] + W2 * L->Alpha[static_cast<size_t>(P2)];
								for (int C = 0; C < 3; ++C)
								{
									const double Tn = W0 * L->Light[static_cast<size_t>(P0)][static_cast<size_t>(C)] + W1 * L->Light[static_cast<size_t>(P1)][static_cast<size_t>(C)]
										+ W2 * L->Light[static_cast<size_t>(P2)][static_cast<size_t>(C)];
									Colour[static_cast<size_t>(C)] = Colour[static_cast<size_t>(C)] * (1.0 - Al) + Tn * Al;
								}
								break;
							}
						}
						for (int C = 0; C < 3; ++C) Sum[static_cast<size_t>(C)] += Colour[static_cast<size_t>(C)];
					}
				for (int C = 0; C < 3; ++C) Out[static_cast<size_t>(PY * W + PX)][static_cast<size_t>(C)] = Srgb(Sum[static_cast<size_t>(C)] / (SS * SS));
			}
		return Out;
	}

	int Width(double Radius, double Metres, double PerRadian) { return std::max(4, static_cast<int>(std::ceil(2.4 * Radius / Metres * PerRadian)) + 2); }

	/** Each pixel's share inside the ring From to To of the radius (its 4 by 4 looks), the disc seen 30 degrees off. */
	std::vector<double> Share(double Radius, double From, double To, double Metres, int W, double PerRadian)
	{
		const double Off = 30.0 * Pi / 180.0;
		std::vector<double> Out(static_cast<size_t>(W * W), 0.0);
		for (int PY = 0; PY < W; ++PY)
			for (int PX = 0; PX < W; ++PX)
			{
				int In = 0;
				for (int J = 0; J < 4; ++J)
					for (int I = 0; I < 4; ++I)
					{
						const double Y = ((PX + (I + 0.5) / 4) - W * 0.5) / PerRadian * Metres / std::cos(Off);
						const double Z = ((PY + (J + 0.5) / 4) - W * 0.5) / PerRadian * Metres;
						const double R = std::sqrt(Y * Y + Z * Z);
						In += R >= From * Radius && R <= To * Radius ? 1 : 0;
					}
				Out[static_cast<size_t>(PY * W + PX)] = In / 16.0;
			}
		return Out;
	}

	/** On the mean over the ring (each pixel weighed by its share in it): how far the picture stands off the sky (darker
	 * positive, by the channels' mean), and how far its red stands over its blue (yellow and red positive). */
	struct Seen
	{
		double Off = 0.0, Yellow = 0.0;
	};
	Seen SeenOf(const std::vector<Rgb>& A, const std::vector<double>& In, double Sky)
	{
		Seen S;
		double Weight = 0.0;
		for (size_t K = 0; K < A.size(); ++K)
		{
			S.Off += In[K] * (Sky - (A[K][0] + A[K][1] + A[K][2]) / 3.0);
			S.Yellow += In[K] * (A[K][0] - A[K][2]);
			Weight += In[K];
		}
		if (Weight > 0.0) { S.Off /= Weight; S.Yellow /= Weight; }
		return S;
	}

	std::string Fmt(const char* F, double A, double B = 0.0, double C = 0.0, double D = 0.0)
	{
		char Got[256];
		std::snprintf(Got, sizeof Got, F, A, B, C, D);
		return Got;
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 3)
	{
		std::fprintf(stderr, "usage: air_prop_blur_test <Source/TheaterCommand> <model.json ...>\n");
		return 2;
	}
	const std::string Source = Argv[1];
	// (the modeller's files as filed on 27 November: the blade marks in each near mesh's discs, and the tips' paint)
	const std::map<std::string, int> MarksKnown = { { "B17Fine", 336 }, { "Wellington", 168 } };
	const std::map<std::string, std::string> TipsKnown = { { "B17Fine", "C9A030" }, { "Wellington", "C9A030" }, { "Mustang", "D9B224" }, { "Zero", "B4281F" },
		{ "Bf109", "" }, { "Fw190", "" }, { "B17", "" } };
	int Models = 0, Screwed = 0, MarksAll = 0, Tipped = 0, FarMarks = 0;
	std::string FineText;
	std::printf("The near shapes' blade marks and the tips' paint (B1 to B3)\n");
	for (int A = 2; A < Argc; ++A)
	{
		std::string Text;
		if (!ReadText(Argv[A], Text)) { Check(false, std::string(Argv[A]) + " read"); continue; }
		if (Text.find("\"kind\": \"aircraft\"") == std::string::npos && Text.find("\"kind\":\"aircraft\"") == std::string::npos) continue;
		++Models;
		std::string Name = Argv[A];
		const bool bOld = Name.find("/Old/") != std::string::npos;
		Name = Name.substr(Name.rfind('/') + 1);
		Name = Name.substr(0, Name.size() - 5);
		const std::string Label = (bOld ? "Old/" : "") + Name;
		if (Name == "B17Fine" && !bOld) FineText = Text;
		const AirScrew::PaintsAre Are = AirScrew::PaintKinds(Text);
		const Mesh Near = Part(Text, "near", AirScrew::NearPaints(Text));
		const Mesh Far = Part(Text, "far", AirScrew::FarPaints(Text));
		std::vector<bool> Blade(Near.Paints.size());
		for (size_t K = 0; K < Blade.size(); ++K) Blade[K] = Are.IsBlade(Near.Paints[K]);
		const AirScrew::Found Found = AirScrew::Find(Near.Points, Blade, Near.Triangles);
		const int Jets = TopNumber(Text, "\"jets\"");
		const bool bJet = Jets > 0 && Jets == TopNumber(Text, "\"engines\"");
		const std::vector<bool> Marks = AirScrew::BladeDecals(Near.Points, Near.Paints, Are, Near.Triangles, Found.Screws);
		const int Taken = static_cast<int>(std::count(Marks.begin(), Marks.end(), true));
		MarksAll += Taken;
		// (what the near shape keeps with the row on, as TakeScrews takes: the blades, every all-blur triangle, the marks)
		const std::vector<bool> In = InDiscs(Near, Found.Screws);
		int Left = 0, MarkTris = 0;
		for (size_t T = 0; T < In.size(); ++T)
		{
			bool bAllBlur = true, bAnyMark = false, bAnyOdd = false;
			for (int C = 0; C < 3; ++C)
			{
				const int P = PaintOf(Near, Near.Triangles[T * 3 + static_cast<size_t>(C)]);
				bAllBlur = bAllBlur && Are.IsBlur(P);
				bAnyMark = bAnyMark || Are.IsDecal(P);
				bAnyOdd = bAnyOdd || Are.IsDecal(P) || Are.IsBlade(P) || Are.IsBlur(P);
			}
			MarkTris += bAnyMark ? 1 : 0;
			const bool bGone = Found.BladeTriangle[T] || bAllBlur || Marks[T];
			Left += In[T] && bAnyOdd && !bGone ? 1 : 0;
		}
		unsigned char Tip[3] = { 0, 0, 0 };
		const bool bTip = AirScrew::TipColour(Text, Near.Paints, Tip);
		char TipHex[8] = "";
		if (bTip) std::snprintf(TipHex, sizeof TipHex, "%02X%02X%02X", Tip[0], Tip[1], Tip[2]);
		Tipped += bTip ? 1 : 0;
		// (the far mesh: any mark in a disc)
		int FarIn = 0;
		{
			std::vector<bool> FarBlade(Far.Paints.size());
			for (size_t K = 0; K < FarBlade.size(); ++K) FarBlade[K] = Are.IsBlade(Far.Paints[K]);
			const AirScrew::Found FarFound = AirScrew::Find(Far.Points, FarBlade, Far.Triangles);
			const std::vector<bool> FarInDisc = InDiscs(Far, FarFound.Screws);
			for (size_t T = 0; T < FarInDisc.size(); ++T)
			{
				bool bAnyMark = false;
				for (int C = 0; C < 3; ++C) bAnyMark = bAnyMark || Are.IsDecal(PaintOf(Far, Far.Triangles[T * 3 + static_cast<size_t>(C)]));
				FarIn += FarInDisc[T] && bAnyMark ? 1 : 0;
			}
		}
		FarMarks += FarIn;
		if (bJet)
		{
			Check(Found.Screws.empty() && Taken == 0 && !bTip, Label + ": a jet, no airscrew, no mark taken, no arc");
			continue;
		}
		++Screwed;
		std::printf("%s: %zu screws; %d blade marks taken (%d triangles marked in all), %d painted blade, blur or mark left in a disc; tips %s; far marks in a disc %d\n",
		            Label.c_str(), Found.Screws.size(), Taken, MarkTris, Left, bTip ? TipHex : "none", FarIn);
		Check(Left == 0, Label + ": B1 nothing painted blade, blur or mark left standing in a turning disc", std::to_string(Left));
		if (const auto It = MarksKnown.find(Name); It != MarksKnown.end() && !bOld)
		{
			Check(Taken == It->second && Taken == MarkTris && Taken == 84 * static_cast<int>(Found.Screws.size()),
			      Label + ": B1 its " + std::to_string(It->second) + " blade marks taken, 84 a screw, every mark it carries",
			      std::to_string(Taken) + " of " + std::to_string(MarkTris) + ", " + std::to_string(Found.Screws.size()) + " screws");
		}
		else if (!bOld)
		{
			Check(Taken == 0 && MarkTris == 0, Label + ": B1 no blade marks to take", std::to_string(Taken));
		}
		Check(FarIn == 0, Label + ": B2 no mark in its far mesh's discs", std::to_string(FarIn));
		if (const auto It = TipsKnown.find(Name); It != TipsKnown.end() && !bOld)
		{
			Check(It->second == (bTip ? std::string(TipHex) : std::string()), Label + ": B3 the tips' paint as filed", bTip ? TipHex : "none");
		}
	}
	Check(Models >= 40 && Screwed >= 38, "every aircraft read", std::to_string(Models) + " aircraft, " + std::to_string(Screwed) + " with airscrews");
	Check(MarksAll >= 504 && FarMarks == 0, "the finer B-17's and the Wellington's marks found at least (504), none far off", std::to_string(MarksAll) + " taken, " + std::to_string(FarMarks) + " far");
	Check(Tipped >= 12, "the aircraft with painted tips read (12 or more)", std::to_string(Tipped));

	// ---- B4: the far disc at the battle's real scale, the finer B-17's 1.76 m airscrew, 3 blades.
	const double PerRadian = 525.0 / std::tan(27.5 * Pi / 180.0);
	const double Sky = 190.0;
	std::printf("The far disc's picture (B4), the finer B-17's airscrew, at %.0f pixels a radian, against a sky of %.0f\n", PerRadian, Sky);
	{
		const AirScrew::PaintsAre Are = AirScrew::PaintKinds(FineText);
		const Mesh Far = Part(FineText, "far", AirScrew::FarPaints(FineText));
		const AirScrew::Stripped Cut = AirScrew::StripScrews(Far.Points, Far.Paints, Are, Far.Triangles);
		Check(!Cut.Screws.empty(), "the finer B-17's far airscrews found", std::to_string(Cut.Screws.size()));
		if (!Cut.Screws.empty())
		{
			const AirScrew::Screw& S = Cut.Screws[0];
			// (its blur disc as the far shape has it today: every all-blur triangle in this screw's disc, solid)
			std::vector<bool> Which(Far.Triangles.size() / 3, false);
			int Solid = 0;
			for (size_t T = 0; T < Which.size(); ++T)
			{
				bool bAll = true;
				for (int C = 0; C < 3 && bAll; ++C)
				{
					const int P = Far.Triangles[T * 3 + static_cast<size_t>(C)];
					bAll = Are.IsBlur(PaintOf(Far, P)) && InDisc(Far.Points[static_cast<size_t>(P)], S);
				}
				Which[T] = bAll;
				Solid += bAll ? 1 : 0;
			}
			const Layer Today = SolidOf(Far, Which, S, Colours(FineText));
			const Layer Blur = LayerOf(AirScrew::MakeBlurDisc(S.Radius, S.Blades)), Haze = LayerOf(AirScrew::MakeDisc(S.Radius, S.Blades, true));
			std::printf("  the screw: radius %.2f m, %d blades; its blur disc today %d solid triangles\n", S.Radius, S.Blades, Solid);
			bool bGrey = true;
			const AirScrew::DiscMade Made = AirScrew::MakeBlurDisc(S.Radius, S.Blades);
			for (size_t K = 0; K < Made.Points.size(); ++K) bGrey = bGrey && std::abs(Made.Red[K] - Made.Blue[K]) <= 2 && std::abs(Made.Red[K] - Made.Green[K]) <= 2;
			Check(bGrey && Made.Triangles.size() == AirScrew::MakeDisc(S.Radius, S.Blades, true).Triangles.size(), "the row's far disc grey at every point, as many triangles as the haze (288)",
			      std::to_string(Made.Triangles.size() / 3));
			for (const double M : { 434.0, 832.0 })
			{
				const int W = Width(S.Radius, M, PerRadian);
				const std::vector<double> InDiscShare = Share(S.Radius, 0.0, 1.0, M, W, PerRadian);
				const Seen Was = SeenOf(Picture({ &Today }, M, 0.0, W, Sky, PerRadian), InDiscShare, Sky);
				const Seen Is = SeenOf(Picture({ &Blur }, M, 0.0, W, Sky, PerRadian), InDiscShare, Sky);
				const Seen Spin = SeenOf(Picture({ &Haze }, M, 0.0, W, Sky, PerRadian), InDiscShare, Sky);
				const std::string At = std::to_string(static_cast<int>(M)) + " m (the disc " + Fmt("%.1f", 2.0 * S.Radius / M * PerRadian) + " px across)";
				std::printf("  at %s: today's solid disc %.1f levels off the sky, red over blue %.1f; the row's %.1f and %.1f; flyPropSpin's haze %.1f\n",
				            At.c_str(), Was.Off, Was.Yellow, Is.Off, Is.Yellow, Spin.Off);
				Check(Was.Off >= 30.0 && Was.Yellow >= 10.0, "today's far disc at " + At + " solid and yellow-rimmed (what the row mends)",
				      Fmt("%.1f levels off, red over blue %.1f", Was.Off, Was.Yellow));
				Check(Is.Off >= 3.0 && Is.Off <= 10.0 && Is.Off < Spin.Off, "the row's far disc at " + At + " a faint shimmer (3 to 10 levels off the sky, fainter than flyPropSpin's haze)",
				      Fmt("%.1f levels (the haze %.1f)", Is.Off, Spin.Off));
				Check(std::fabs(Is.Yellow) <= 1.0, "the row's far disc at " + At + " grey, no yellow (red and blue within a level)", Fmt("%.2f", Is.Yellow));
			}
		}
	}

	// ---- B5: the tips' arc close to, the finer B-17's tips (C9A030).
	std::printf("The tips' arc close to (B5), the 1.76 m airscrew, 3 blades\n");
	{
		const double R = 1.76;
		const unsigned char Tip[3] = { 0xC9, 0xA0, 0x30 };
		const AirScrew::DiscMade Arcs = AirScrew::MakeTipArcs(R, 3, Tip);
		int Most = 0;
		for (const unsigned char A : Arcs.Alpha) Most = std::max<int>(Most, A);
		Check(Most <= AirScrew::TipMost && Arcs.Triangles.size() == 384 * 3, "the arc no stronger than TipMost (100 of 255), 384 triangles", std::to_string(Most) + ", " + std::to_string(Arcs.Triangles.size() / 3));
		const Layer Old = OldDisc(R, 3), Smear = LayerOf(AirScrew::MakeDisc(R, 3, false)), Arc = LayerOf(Arcs);
		for (const Layer* Disc : { &Old, &Smear })
		{
			const std::string Which = Disc == &Old ? "the old disc" : "flyPropSpin's smear";
			for (const double M : { 50.0, 150.0, 280.0 })
			{
				const int W = Width(R, M, PerRadian);
				const std::vector<double> Band = Share(R, AirScrew::TipRings[0], AirScrew::TipRings[2], M, W, PerRadian);
				const std::vector<Rgb> Without = Picture({ Disc }, M, 0.0, W, Sky, PerRadian), With = Picture({ Disc, &Arc }, M, 0.0, W, Sky, PerRadian);
				const Seen Bare = SeenOf(Without, Band, Sky), Tipped2 = SeenOf(With, Band, Sky);
				double Moved = 0.0;
				for (size_t K = 0; K < With.size(); ++K)
					for (int C = 0; C < 3; ++C) Moved = std::max(Moved, std::fabs(With[K][static_cast<size_t>(C)] - Without[K][static_cast<size_t>(C)]));
				// (and it turns: a frame later at 60 fps, 1.9 turns a second, the band's picture changes)
				const std::vector<Rgb> Later = Picture({ Disc, &Arc }, M, 1.9 * 360.0 / 60.0, W, Sky, PerRadian);
				double Turned = 0.0;
				for (size_t K = 0; K < With.size(); ++K) Turned = std::max(Turned, Band[K] * std::fabs(With[K][0] - Later[K][0]));
				const std::string At = Which + " at " + std::to_string(static_cast<int>(M)) + " m";
				std::printf("  %s: the tips' band red over blue %.1f without the arc, %.1f with it; the most a pixel moves by it %.1f; a frame later the band changes %.1f\n",
				            At.c_str(), Bare.Yellow, Tipped2.Yellow, Moved, Turned);
				// (at 280 m the tips' band is under a pixel wide, the sky in most of each of its pixels)
				const double Want = M < 200.0 ? 5.0 : 3.0;
				Check(Tipped2.Yellow - Bare.Yellow >= Want && Moved <= 40.0,
				      "the tips' arc on " + At + " seen and faint (" + std::to_string(static_cast<int>(Want)) + " levels or more toward the paint, no pixel moved over 40)",
				      Fmt("%.1f toward the paint, %.1f the most", Tipped2.Yellow - Bare.Yellow, Moved));
				Check(Turned >= 1.0, "the tips' arc on " + At + " turns with the disc", Fmt("%.1f levels a frame", Turned));
			}
		}
	}

	// ---- B6: the Unreal side from its source. These count lines of text: they show the lines are there, not that the
	// code behaves, and a harmless reformat breaks them (then mend the text here). The proof for the Unreal side is the
	// Mac's run with -TheaterAirNoPropBlur against the base.
	std::printf("The air battle's row, from its source (%s)\n", Source.c_str());
	std::string Battle, Rules, State, Header;
	const bool bSource = ReadText(Source + "/Battle/AirBattle.cpp", Battle) && ReadText(Source + "/Battle/LandLive.cpp", Rules)
		&& ReadText(Source + "/Battle/AirBattleState.h", State) && ReadText(Source + "/Battle/AirBattle.h", Header);
	Check(bSource, "the source read");
	if (bSource)
	{
		Check(Count(Rules, "TEXT(\"flyPropBlur\"), TEXT(\"Far airscrews a faint blur\")") == 1 && Count(Rules, "TEXT(\"TheaterAirNoPropBlur\"), false, TEXT(\"TheaterAirPropBlur\")") == 1
				&& Count(Rules, "RuleKey.StartsWith(TEXT(\"fly\")") == 1,
			"the row on the card, off unless asked (-TheaterAirPropBlur), never sent to the land rules (its key begins \"fly\")");
		Check(Count(Battle, "FLandLive::RuleNow(TEXT(\"flyPropBlur\"))") == 1 && Count(State, "int PropBlurOn = -1;") == 1 && Count(Battle, "State->PropBlurOn = ") == 1
				&& Count(Header, "bool PropBlurOn();") == 1,
			"read once a battle, into the battle's own state");
		Check(Count(Battle, "const bool bBlur = PropBlurOn();") == 1 && Count(Battle, "+ (bBlur ? TEXT(\"|blur\") : TEXT(\"\"))") == 1
				&& Count(Battle, "if ((bScrews || bBlur) && Near.Screws.Num() > 0 && ScrewHazeInk())") == 1,
			"the row on, every shape under a key of its own and only then its far shape stripped (off: the keys and shapes of before)");
		Check(Count(Battle, "AirParts::TakeScrews(Near, Made, Kind, Middle, Fit, UnitsPerMetre, bBlur);") == 1
				&& Count(Battle, "if (bPropBlur)") == 1 && Count(Battle, "AirScrew::BladeDecals(") == 1 && Count(Battle, "AirScrew::TipColour(") == 1,
			"the near shape's marks and tips' paint taken only with the row on (off: the near shape of before)");
		Check(Count(Battle, "Away.bBlurDisc = bBlur;") == 1 && Count(Battle, "AirScrew::MakeBlurDisc(") == 2 && Count(Battle, "Shape.bBlurDisc ? AirScrew::MakeBlurDisc(") == 1
				&& Count(Battle, "Far->bBlurDisc ? AirScrew::MakeBlurDisc(") == 1,
			"the faint far disc only on a far shape the row stripped, on the copies and on an aircraft's own mesh");
		Check(Count(Battle, "AirScrew::MakeTipArcs(") == 1 && Count(Battle, "const bool bTips = State && State->PropBlurOn == 1 && Screw.Tip.A > 0;") == 1,
			"the tips' arc only on the near disc, only with the row on (the far copies never)");
		Check(Count(Battle, "return State && (State->PropSpinOn == 1 || State->PropBlurOn == 1) ? ScrewHaze.Get() : nullptr;") == 1
				&& Count(Battle, "const bool bFeatheredFar = (S.PropSpinOn == 1 || S.PropBlurOn == 1) && P.bFeathered && !P.DeadEngines.empty() && ScrewHazeInk();") == 1
				&& Count(Battle, "if (S.PropSpinOn == 1 || S.PropBlurOn == 1)") == 1 && Count(Battle, "if (State && (State->PropSpinOn == 1 || State->PropBlurOn == 1))") == 1,
			"the far machinery of \"flyPropSpin\" (its ink, a feathered bomber's own mesh, its far airscrews, the sort) with either row, and with neither as before");
		Check(Count(Battle, "past 300 m the airscrews stay as before (no faint disc to put in their place)") == 1,
			"no haze ink (M_ScrewHaze not built), no far shape stripped: never an aircraft with no airscrew at all");
	}
	std::printf(Bad ? "prop blur: %d failed\n" : "prop blur: all checks pass\n", Bad);
	return Bad ? 1 : 0;
}
