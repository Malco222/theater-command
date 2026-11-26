// The turning airscrews' test (the Battle rules card's "Turning airscrews", row flyPropSpin; Brian, 26 November 2026
// 04:00Z: "the b-17 propellers didnt look like they were spinning"). Past 300 m from the eye, the whole of the Overview
// (434 to 832 m from its crowd), every aircraft is drawn from its far shape, and that shape kept every blade standing
// still, the newer models' own painted blur disc drawn solid over them. With the row on the far shape loses both
// (Battle/AirScrew's StripScrews) and the copies draw an even haze at each hub; close to, the turning disc trails a
// stronger smear (MakeDisc). Usage:
//   air_prop_spin_test <Source/TheaterCommand> <model.json ...>
// a model that is not an aircraft is passed over. For each aircraft:
//   P1 its far mesh's airscrews are its near mesh's (as many, each hub within a twentieth of its radius): one haze a
//      screw, where its turning disc is close to; a jet none;
//   P2 its far shape keeps no triangle painted blade or blur, and loses nothing else (the boss, hub, cowl and spinner
//      stay); the old far shape's still blades are counted beside it ("before");
//   P3 each screw's own blades are kept apart, every one inside its own disc, all of them together the blades taken
//      (an engine stopped and feathered shows its own blades standing still);
// and once:
//   P4 a picture of the disc at the battle's real scale (the 55 degree lens across the 1,050 or so pixels the scene is
//      drawn at before it is filled out to the screen; 1,008 pixels a radian): close to (50, 150, 280 m) the near disc
//      changes the picture by 30 levels of 255 or more from one frame to the next at 60 fps (the old disc is measured
//      beside it); at the Overview's 434 and 832 m the far haze stands 6 to 30 levels off the sky (seen, not a blot);
//   P5 the row off draws what it drew before: the Unreal side read from its source (it cannot be built here), every new
//      branch behind the row, the row off unless asked, never sent to the land rules.
#include "Battle/AirScrew.h"

#include <algorithm>
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

	bool All(const Mesh& M, size_t T, bool bBlade, const AirScrew::PaintsAre& Are)
	{
		for (int C = 0; C < 3; ++C)
		{
			const int P = M.Triangles[T * 3 + static_cast<size_t>(C)];
			const int Paint = P >= 0 && static_cast<size_t>(P) < M.Paints.size() ? M.Paints[static_cast<size_t>(P)] : -1;
			if (bBlade ? !Are.IsBlade(Paint) : !Are.IsBlur(Paint)) return false;
		}
		return true;
	}

	bool InDisc(const AirScrew::Vec3& P, const AirScrew::Screw& S)
	{
		const double DX = P.X - S.Hub.X, DY = P.Y - S.Hub.Y, DZ = P.Z - S.Hub.Z;
		const double Along = DX * S.Axis.X + DY * S.Axis.Y + DZ * S.Axis.Z;
		const double RX = DX - Along * S.Axis.X, RY = DY - Along * S.Axis.Y, RZ = DZ - Along * S.Axis.Z;
		return std::fabs(Along) <= 0.25 * S.Radius && std::sqrt(RX * RX + RY * RY + RZ * RZ) <= 1.05 * S.Radius;
	}

	// ---- The picture (P4): the disc alone against the sky, as M_Highlight and M_ScrewHaze draw it (light of its own, the
	// point's colour made linear, laid over what is behind by the point's alpha), seen 30 degrees off its shaft.
	double Lin(double S) { return S <= 0.04045 ? S / 12.92 : std::pow((S + 0.055) / 1.055, 2.4); }
	double Srgb(double L) { L = std::clamp(L, 0.0, 1.0); return 255.0 * (L <= 0.0031308 ? L * 12.92 : 1.055 * std::pow(L, 1.0 / 2.4) - 0.055); }

	struct Flat
	{
		std::vector<double> Y, Z, Alpha, Tone; // (each point in the disc's plane; its alpha 0 to 1 and its colour, linear)
		std::vector<int> Triangles;             // (front faces only: the back ones are the same, culled from this side)
		int Round = 0;                          // (points about each ring: the front faces lie ring by ring, two a step)
	};

	Flat FlatOf(const AirScrew::DiscMade& D)
	{
		Flat F;
		for (size_t K = 0; K < D.Points.size(); ++K)
		{
			F.Y.push_back(D.Points[K].Y);
			F.Z.push_back(D.Points[K].Z);
			F.Alpha.push_back(D.Alpha[K] / 255.0);
			F.Tone.push_back(Lin(D.Green[K] / 255.0));
		}
		F.Triangles.assign(D.Triangles.begin(), D.Triangles.begin() + static_cast<std::ptrdiff_t>(D.Front));
		F.Round = static_cast<int>(D.Points.size() / 4);
		return F;
	}

	/** The old near disc (AirBattle.cpp's DiscOf with the row off), point by point as it lays them. */
	Flat OldDisc(double Radius, int Blades)
	{
		AirScrew::DiscMade D = AirScrew::MakeDisc(Radius, Blades, false);
		const int Round = AirScrew::NearRound;
		for (int Ring = 0; Ring < 4; ++Ring)
			for (int K = 0; K < Round; ++K)
			{
				const double A = 6.28318530717958647692 * K / Round;
				const double Swept = std::pow(0.5 + 0.5 * std::cos(Blades * A), 5.0);
				const double Alpha = Ring == 0 || Ring == 3 ? 0.0 : (Ring == 1 ? 46.0 + 80.0 * Swept : 34.0 + 64.0 * Swept);
				const size_t I = static_cast<size_t>(Ring * Round + K);
				D.Alpha[I] = static_cast<unsigned char>(std::lround(Alpha));
				D.Red[I] = 30;
				D.Green[I] = 30;
				D.Blue[I] = 32;
			}
		return FlatOf(D);
	}

	/** The picture's grey levels (sRGB, 0 to 255, one channel), W by W pixels about the hub, 4 by 4 looks a pixel. */
	std::vector<double> Picture(const Flat& F, double Metres, double Turn, int W, double Sky, double PerRadian)
	{
		const double Off = 30.0 * 3.14159265358979323846 / 180.0;
		std::vector<double> Out(static_cast<size_t>(W * W), 0.0);
		const int SS = 4;
		for (int PY = 0; PY < W; ++PY)
			for (int PX = 0; PX < W; ++PX)
			{
				double Sum = 0.0;
				for (int J = 0; J < SS; ++J)
					for (int I = 0; I < SS; ++I)
					{
						// (the screen's place in metres at the disc; its plane leans Off from square to the eye)
						const double SX = ((PX + (I + 0.5) / SS) - W * 0.5) / PerRadian * Metres;
						const double SYs = ((PY + (J + 0.5) / SS) - W * 0.5) / PerRadian * Metres;
						const double Y = SX / std::cos(Off), Z = SYs;
						// (turned by Turn degrees toward the growing angle: what stands here now stood Turn behind)
						const double T = -Turn * 3.14159265358979323846 / 180.0;
						const double A = std::atan2(Y, Z) + T, R = std::sqrt(Y * Y + Z * Z);
						const double PYd = R * std::sin(A), PZd = R * std::cos(A);
						double Colour = Lin(Sky / 255.0);
						// (only the steps about where it lies: the ring it is in, its step round, and their neighbours)
						const double Outer = std::sqrt(F.Y[static_cast<size_t>(F.Round * 3)] * F.Y[static_cast<size_t>(F.Round * 3)] + F.Z[static_cast<size_t>(F.Round * 3)] * F.Z[static_cast<size_t>(F.Round * 3)]);
						if (R > Outer * 1.001) { Sum += Colour; continue; }
						const double Around = std::atan2(PYd, PZd);
						const int Step = static_cast<int>(std::floor((Around < 0 ? Around + 6.28318530717958647692 : Around) / 6.28318530717958647692 * F.Round));
						std::vector<size_t> Near;
						for (int Ring = 0; Ring < 3; ++Ring)
							for (int D = -1; D <= 1; ++D)
							{
								const size_t Quad = static_cast<size_t>(Ring * F.Round + ((Step + D) % F.Round + F.Round) % F.Round);
								for (size_t Tri = 0; Tri < 2; ++Tri) Near.push_back((Quad * 2 + Tri) * 3);
							}
						for (const size_t K : Near)
						{
							const int P0 = F.Triangles[K], P1 = F.Triangles[K + 1], P2 = F.Triangles[K + 2];
							const double X0 = F.Y[P0], Y0 = F.Z[P0], X1 = F.Y[P1], Y1 = F.Z[P1], X2 = F.Y[P2], Y2 = F.Z[P2];
							const double Area = (X1 - X0) * (Y2 - Y0) - (X2 - X0) * (Y1 - Y0);
							if (std::fabs(Area) < 1e-15) continue;
							const double W0 = ((X1 - PYd) * (Y2 - PZd) - (X2 - PYd) * (Y1 - PZd)) / Area;
							const double W1 = ((X2 - PYd) * (Y0 - PZd) - (X0 - PYd) * (Y2 - PZd)) / Area;
							const double W2 = 1.0 - W0 - W1;
							if (W0 < -1e-12 || W1 < -1e-12 || W2 < -1e-12) continue;
							const double Al = W0 * F.Alpha[P0] + W1 * F.Alpha[P1] + W2 * F.Alpha[P2];
							const double Tn = W0 * F.Tone[P0] + W1 * F.Tone[P1] + W2 * F.Tone[P2];
							Colour = Colour * (1.0 - Al) + Tn * Al;
							break;
						}
						Sum += Colour;
					}
				Out[static_cast<size_t>(PY * W + PX)] = Srgb(Sum / (SS * SS));
			}
		return Out;
	}

	/** The most one pixel changes from one frame to the next at 60 frames a second, the disc at 1.9 turns a second (the
	 * game's ScrewTurns), over a turn's worth of starting places. */
	double MostChange(const Flat& F, double Metres, double Radius, double PerRadian)
	{
		const int W = std::max(4, static_cast<int>(std::ceil(2.4 * Radius / Metres * PerRadian)));
		const double Step = 1.9 * 360.0 / 60.0;
		double Most = 0.0;
		for (int Start = 0; Start < 6; ++Start)
		{
			const double From = Start * 7.0;
			const std::vector<double> A = Picture(F, Metres, From, W, 190.0, PerRadian), B = Picture(F, Metres, From + Step, W, 190.0, PerRadian);
			for (size_t K = 0; K < A.size(); ++K) Most = std::max(Most, std::fabs(A[K] - B[K]));
		}
		return Most;
	}

	/** How far the disc's pixels stand off the sky, on the mean (each pixel weighed by how much of it the disc covers). */
	double MeanOff(const Flat& F, double Metres, double Radius, double PerRadian)
	{
		const int W = std::max(4, static_cast<int>(std::ceil(2.4 * Radius / Metres * PerRadian)) + 2);
		const std::vector<double> A = Picture(F, Metres, 0.0, W, 190.0, PerRadian);
		double Sum = 0.0, Weight = 0.0;
		const double Off = 30.0 * 3.14159265358979323846 / 180.0;
		for (int PY = 0; PY < W; ++PY)
			for (int PX = 0; PX < W; ++PX)
			{
				// (the share of the pixel inside the disc's outline, by its 4 by 4 looks)
				int In = 0;
				for (int J = 0; J < 4; ++J)
					for (int I = 0; I < 4; ++I)
					{
						const double Y = ((PX + (I + 0.5) / 4) - W * 0.5) / PerRadian * Metres / std::cos(Off);
						const double Z = ((PY + (J + 0.5) / 4) - W * 0.5) / PerRadian * Metres;
						In += Y * Y + Z * Z <= Radius * Radius ? 1 : 0;
					}
				Sum += In / 16.0 * (190.0 - A[static_cast<size_t>(PY * W + PX)]);
				Weight += In / 16.0;
			}
		return Weight > 0.0 ? Sum / Weight : 0.0;
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 3)
	{
		std::fprintf(stderr, "usage: air_prop_spin_test <Source/TheaterCommand> <model.json ...>\n");
		return 2;
	}
	const std::string Source = Argv[1];
	// (what the modeller's files gave on 26 November, measured with the far probe: blade and blur triangles in each far mesh)
	const std::map<std::string, std::pair<int, int>> Known = { { "B17", { 1152, 0 } }, { "B17Fine", { 1752, 560 } }, { "Bf109", { 288, 28 } },
		{ "Fw190", { 288, 28 } }, { "Zero", { 528, 112 } }, { "Lancaster", { 1152, 0 } }, { "Betty", { 1056, 224 } }, { "Wellington", { 876, 280 } } };
	const std::map<std::string, int> EachScrew = { { "B17", 288 }, { "B17Fine", 438 } };
	int Models = 0, Screwed = 0, StillBefore = 0;
	std::printf("The far shapes' airscrews (P1 to P3)\n");
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
		const AirScrew::PaintsAre Are = AirScrew::PaintKinds(Text);
		const Mesh Near = Part(Text, "near", AirScrew::NearPaints(Text));
		const Mesh Far = Part(Text, "far", AirScrew::FarPaints(Text));
		std::vector<bool> Blade(Near.Paints.size());
		for (size_t K = 0; K < Blade.size(); ++K) Blade[K] = Are.IsBlade(Near.Paints[K]);
		const AirScrew::Found NearFound = AirScrew::Find(Near.Points, Blade, Near.Triangles);
		const int Jets = TopNumber(Text, "\"jets\"");
		const bool bJet = Jets > 0 && Jets == TopNumber(Text, "\"engines\"");
		const std::string Label = (bOld ? "Old/" : "") + Name;
		if (Far.Points.empty() || Far.Points.size() != Far.Paints.size())
		{
			Check(false, Label + ": its far mesh read", std::to_string(Far.Points.size()) + " points, " + std::to_string(Far.Paints.size()) + " paints");
			continue;
		}
		const AirScrew::Stripped Cut = AirScrew::StripScrews(Far.Points, Far.Paints, Are, Far.Triangles);
		// (the old far shape: every blade and blur triangle it drew)
		int BladeAll = 0, BlurAll = 0, BladeLeft = 0, BlurLeft = 0, OtherDropped = 0, OtherInDiscs = 0;
		for (size_t T = 0; T < Far.Triangles.size() / 3; ++T)
		{
			const bool bBlade = All(Far, T, true, Are), bBlur = All(Far, T, false, Are);
			BladeAll += bBlade ? 1 : 0;
			BlurAll += bBlur ? 1 : 0;
			if (!Cut.Drop[T]) { BladeLeft += bBlade ? 1 : 0; BlurLeft += bBlur ? 1 : 0; }
			if (Cut.Drop[T] && !bBlade && !bBlur) ++OtherDropped;
			if (!Cut.Drop[T] && !bBlade && !bBlur)
			{
				bool bIn = true;
				for (int C = 0; C < 3 && bIn; ++C)
				{
					bool bAny = false;
					for (const AirScrew::Screw& S : Cut.Screws) bAny = bAny || InDisc(Far.Points[static_cast<size_t>(Far.Triangles[T * 3 + static_cast<size_t>(C)])], S);
					bIn = bAny;
				}
				OtherInDiscs += bIn && !Cut.Screws.empty() ? 1 : 0;
			}
		}
		StillBefore += BladeAll;
		const std::vector<int> Which = AirScrew::SameScrews(NearFound.Screws, Cut.Screws);
		std::printf("%s: near %zu screws, far %zu; far %zu triangles, %d blade and %d blur taken out (before: %d and %d drawn standing), %d of the rest in the discs kept\n",
		            Label.c_str(), NearFound.Screws.size(), Cut.Screws.size(), Far.Triangles.size() / 3, Cut.BladeTriangles, Cut.BlurTriangles, BladeAll, BlurAll, OtherInDiscs);
		if (bJet)
		{
			Check(NearFound.Screws.empty() && Cut.Screws.empty() && Cut.BladeTriangles == 0 && Cut.BlurTriangles == 0, Label + ": a jet, no airscrew near or far, nothing taken");
			continue;
		}
		++Screwed;
		Check(!NearFound.Screws.empty() && Which.size() == NearFound.Screws.size(), Label + ": P1 each far airscrew is a near one (one haze a screw)",
		      std::to_string(Cut.Screws.size()) + " of " + std::to_string(NearFound.Screws.size()));
		Check(BladeLeft == 0 && BlurLeft == 0 && OtherDropped == 0 && Cut.BladeTriangles == BladeAll && Cut.BlurTriangles == BlurAll,
		      Label + ": P2 no blade or blur triangle left, nothing else taken", std::to_string(BladeLeft) + " blade, " + std::to_string(BlurLeft) + " blur left, " + std::to_string(OtherDropped) + " other taken");
		if (const auto It = Known.find(Name); It != Known.end() && !bOld)
		{
			Check(Cut.BladeTriangles == It->second.first && Cut.BlurTriangles == It->second.second, Label + ": P2 as the far probe counted",
			      std::to_string(Cut.BladeTriangles) + "+" + std::to_string(Cut.BlurTriangles) + " against " + std::to_string(It->second.first) + "+" + std::to_string(It->second.second));
		}
		int Kept = 0;
		bool bOwn = Cut.Blades.size() == Cut.Screws.size();
		std::string Each;
		for (size_t S = 0; S < Cut.Blades.size() && bOwn; ++S)
		{
			Kept += static_cast<int>(Cut.Blades[S].size());
			Each += (S ? "/" : "") + std::to_string(Cut.Blades[S].size());
			for (const int T : Cut.Blades[S])
				for (int C = 0; C < 3; ++C) bOwn = bOwn && InDisc(Far.Points[static_cast<size_t>(Far.Triangles[static_cast<size_t>(T) * 3 + static_cast<size_t>(C)])], Cut.Screws[S]);
			bOwn = bOwn && !Cut.Blades[S].empty();
		}
		Check(bOwn && Kept == Cut.BladeTriangles, Label + ": P3 each screw's blades kept apart, inside its own disc", Each);
		if (const auto It = EachScrew.find(Name); It != EachScrew.end() && !bOld)
		{
			bool bAll = true;
			for (const std::vector<int>& B : Cut.Blades) bAll = bAll && static_cast<int>(B.size()) == It->second;
			Check(bAll, Label + ": P3 " + std::to_string(It->second) + " blade triangles a screw", Each);
		}
	}
	Check(Models >= 40 && Screwed >= 38, "every aircraft read", std::to_string(Models) + " aircraft, " + std::to_string(Screwed) + " with airscrews");
	Check(StillBefore > 0, "before the row, the far shapes drew still blades (what the row takes out)", std::to_string(StillBefore) + " triangles");

	// ---- P4: the disc at the battle's real scale.
	std::printf("The discs' pictures (P4), the B-17's 1.76 m airscrew, 3 blades, at 1,008 pixels a radian\n");
	{
		const double PerRadian = 525.0 / std::tan(27.5 * 3.14159265358979323846 / 180.0);
		const double R = 1.76;
		const Flat Now = FlatOf(AirScrew::MakeDisc(R, 3, false)), Old = OldDisc(R, 3), Haze = FlatOf(AirScrew::MakeDisc(R, 3, true));
		Check(AirScrew::MakeDisc(R, 3, false).Triangles.size() == 576 * 3 && AirScrew::MakeDisc(R, 3, true).Triangles.size() == 288 * 3,
		      "the near disc as many triangles as the old one (576), the far haze 288");
		for (const double M : { 50.0, 150.0, 280.0 })
		{
			const double Was = MostChange(Old, M, R, PerRadian), Is = MostChange(Now, M, R, PerRadian);
			Check(Is >= 30.0, "near disc at " + std::to_string(static_cast<int>(M)) + " m changes 30 levels or more a frame at 60 fps",
			      std::to_string(static_cast<int>(std::lround(Is))) + " levels (the old disc " + std::to_string(static_cast<int>(std::lround(Was))) + ")");
		}
		for (const double M : { 434.0, 832.0 })
		{
			const double Off = MeanOff(Haze, M, R, PerRadian);
			char Got[96];
			std::snprintf(Got, sizeof Got, "%.1f levels darker than the sky, the disc %.1f px across", Off, 2.0 * R / M * PerRadian);
			Check(Off >= 6.0 && Off <= 30.0, "far haze at " + std::to_string(static_cast<int>(M)) + " m seen and faint (6 to 30 levels off the sky)", Got);
		}
	}

	// ---- P5: the Unreal side from its source. These count lines of text: they show the lines are there, not that the
	// code behaves, and a harmless reformat breaks them (then mend the text here). The proof for the Unreal side is the
	// Mac's run with -TheaterAirNoPropSpin against the base.
	std::printf("The air battle's row, from its source (%s)\n", Source.c_str());
	std::string Battle, Rules, State, Content;
	const bool bSource = ReadText(Source + "/Battle/AirBattle.cpp", Battle) && ReadText(Source + "/Battle/LandLive.cpp", Rules)
		&& ReadText(Source + "/Battle/AirBattleState.h", State) && ReadText(Source + "/../../Tools/build_content.py", Content);
	Check(bSource, "the source read");
	if (bSource)
	{
		Check(Count(Rules, "TEXT(\"flyPropSpin\"), TEXT(\"Turning airscrews\")") == 1 && Count(Rules, "TEXT(\"TheaterAirNoPropSpin\"), false, TEXT(\"TheaterAirPropSpin\")") == 1
				&& Count(Rules, "RuleKey.StartsWith(TEXT(\"fly\")") == 1,
			"the row on the card, off unless asked (-TheaterAirPropSpin), never sent to the land rules (its key begins \"fly\")");
		Check(Count(Battle, "FLandLive::RuleNow(TEXT(\"flyPropSpin\"))") == 1 && Count(State, "int PropSpinOn = -1;") == 1 && Count(Battle, "State->PropSpinOn = ") == 1,
			"read once a battle, into the battle's own state");
		Check(Count(Battle, "const bool bScrews = PropSpinOn();") == 1 && Count(Battle, "+ (bScrews ? TEXT(\"|screws\") : TEXT(\"\"));") == 1
				&& Count(Battle, "if (bScrews && Near.Screws.Num() > 0)") == 1,
			"the row on, every shape under a key of its own and only then its far shape stripped (off: the keys and shapes of before)");
		Check(Count(Battle, "bLight ? nullptr : ScrewHazeInk()") == 1 && Count(Battle, "return State && State->PropSpinOn == 1 ? ScrewHaze.Get() : nullptr;") == 1
				&& Count(Battle, "const bool bHaze = Haze && Shape.Screws.Num() > 0;") == 1 && Count(Battle, "if (Mesh->GetStaticMaterials().Num() > 1)") == 1,
			"the copies' haze only with the row on (off: one section, one material, as before)");
		Check(Count(Battle, "const bool bSmear = State && State->PropSpinOn == 1;") == 1 && Count(Battle, "AirParts::DiscOf(Screw.Radius, Screw.Blades, Points, Triangles, Normals, Colours, bSmear);") == 1,
			"the near disc's smear only with the row on (off: the old disc, point for point)");
		Check(Count(Battle, "const bool bFeatheredFar = S.PropSpinOn == 1 && P.bFeathered && !P.DeadEngines.empty();") == 1 && Count(Battle, "if (S.PropSpinOn == 1)") == 1
				&& Count(Battle, "if (State && State->PropSpinOn == 1)") == 1,
			"a feathered bomber's own mesh, its far airscrews and the discs' sort only with the row on");
		Check(Count(Content, "m = new_material(\"M_ScrewHaze\")") == 1 && Count(Content, "(\"M_ScrewHaze\", build_screw_haze_or_tell)") == 1,
			"M_ScrewHaze built with the content, once, when it is missing");
		const size_t Haze = Content.find("def build_screw_haze():");
		const size_t Next = Haze == std::string::npos ? Haze : Content.find("\ndef ", Haze + 1);
		Check(Haze != std::string::npos && Content.substr(Haze, Next - Haze).find("used_with_instanced_static_meshes\", True") != std::string::npos,
			"M_ScrewHaze usable on the copies (instanced)");
	}
	std::printf(Bad ? "prop spin: %d failed\n" : "prop spin: all checks pass\n", Bad);
	return Bad ? 1 : 0;
}
