#include "Battle/SeaSailors.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace SeaSailors
{
	namespace
	{
		/** 0 to 1 from a man's own number (his clip's phase, so two men side by side are not in step). */
		double Phase01(uint32_t Look, uint32_t Salt)
		{
			uint32_t H = Look * 2654435761u ^ (Salt * 0x9E3779B9u);
			H ^= H >> 15;
			H *= 0x85EBCA6Bu;
			H ^= H >> 13;
			return static_cast<double>(H & 0xFFFFFFu) / 16777216.0;
		}

		float Wrapped(double T, float Long)
		{
			if (Long <= 0.f || !std::isfinite(T))
			{
				return 0.f; // (a time that is no number is the clip's start: the pre-check's probe asks it)
			}
			const double W = std::fmod(T, static_cast<double>(Long));
			return static_cast<float>(W < 0.0 ? W + Long : W);
		}

		bool IsLoop(Act A)
		{
			return A != Act::Enter;
		}

		/** The clips whose root is at the sea's top (the README's `surface`): the man in the water. */
		bool InWater(Act A)
		{
			return A == Act::Enter || A == Act::Tread || A == Act::Wave || A == Act::Swim || A == Act::Dead || A == Act::Cling;
		}

		/** The colours in a flat object of the text (its braces at Open and Close): each "name": "RRGGBB" with its colour
		 * (any other string, an `about`, left out). */
		std::vector<std::pair<std::string, uint32_t>> ColoursIn(const std::string& Text, size_t Open, size_t Close)
		{
			std::vector<std::pair<std::string, uint32_t>> Out;
			if (Open == std::string::npos || Close == std::string::npos || Close < Open)
			{
				return Out;
			}
			size_t P = Open;
			for (;;)
			{
				const size_t A = Text.find('"', P + 1);
				const size_t B = A < Close ? Text.find('"', A + 1) : std::string::npos;
				const size_t C = B < Close ? Text.find('"', B + 1) : std::string::npos;
				const size_t D = C < Close ? Text.find('"', C + 1) : std::string::npos;
				if (D == std::string::npos || D > Close)
				{
					break;
				}
				const std::string Hex = Text.substr(C + 1, D - C - 1);
				char* End = nullptr;
				const unsigned long V = std::strtoul(Hex.c_str(), &End, 16);
				if (Hex.size() == 6 && End && *End == '\0')
				{
					Out.emplace_back(Text.substr(A + 1, B - A - 1), static_cast<uint32_t>(V));
				}
				P = D;
			}
			return Out;
		}

		/** The flat object after Key in the text: its braces. */
		std::pair<size_t, size_t> ObjectAfter(const std::string& Text, const char* Key, size_t From = 0)
		{
			const size_t At = Text.find(Key, From);
			if (At == std::string::npos)
			{
				return { std::string::npos, std::string::npos };
			}
			const size_t Open = Text.find('{', At);
			return { Open, Open == std::string::npos ? Open : Text.find('}', Open) };
		}

		/** The numbers written in the text between From and To. */
		std::vector<double> Numbers(const std::string& Text, size_t From, size_t To)
		{
			std::vector<double> Out;
			const char* S = Text.c_str();
			size_t I = From;
			while (I < To && I < Text.size())
			{
				const char Ch = S[I];
				if ((Ch >= '0' && Ch <= '9') || Ch == '-' || Ch == '.')
				{
					char* End = nullptr;
					const double V = std::strtod(S + I, &End);
					if (End != S + I)
					{
						Out.push_back(V);
						I = static_cast<size_t>(End - S);
						continue;
					}
				}
				++I;
			}
			return Out;
		}

		/** How much of a paint fuel oil blackens, at the most: his skin and his clothes nearly all; his hat less, his kapok
		 * least (its canvas stays lighter). */
		double OilTakes(int Number)
		{
			return Number == Skin ? 0.85 : Number == Hat ? 0.75 : Number == Kapok ? 0.6 : 0.9;
		}
	}

	const char* ArmyOf(const std::string& Nation)
	{
		std::string Up;
		for (const char Ch : Nation)
		{
			Up += static_cast<char>(Ch >= 'a' && Ch <= 'z' ? Ch - 'a' + 'A' : Ch);
		}
		if (Up == "JAP" || Up == "JPN" || Up == "JP") return "JAP";
		if (Up == "UK" || Up == "GBR" || Up == "GB" || Up == "ENG") return "GBR";
		if (Up == "GER" || Up == "DE") return "GER";
		if (Up == "ITA" || Up == "IT") return "ITA";
		if (Up == "SOV" || Up == "USSR" || Up == "SU") return "SOV";
		return "USA";
	}

	const char* ClipName(Act A)
	{
		switch (A)
		{
		case Act::Fall: return "fall_loop";
		case Act::Enter: return "enter_water";
		case Act::Tread: return "tread";
		case Act::Wave: return "wave";
		case Act::Swim: return "swim_breast";
		case Act::Dead: return "float_dead";
		case Act::Cling: return "cling";
		case Act::Climb: return "climb_net";
		case Act::Row: return "row";
		case Act::Sit: return "sit_raft";
		default: return "";
		}
	}

	int PosesOf(Act A)
	{
		// (about one a fifth of a second of a quick loop, fewer for the slow ones: 64 in all)
		switch (A)
		{
		case Act::Fall: return 4;
		case Act::Enter: return 8;
		case Act::Tread: return 8;
		case Act::Wave: return 6;
		case Act::Swim: return 8;
		case Act::Dead: return 4;
		case Act::Cling: return 6;
		case Act::Climb: return 6;
		case Act::Row: return 8;
		case Act::Sit: return 6;
		default: return 1;
		}
	}

	bool Crew::AddFile(const std::string& Army, const std::string& Text, std::string& OutError)
	{
		FileRead F;
		F.Army = Army;
		F.S = std::make_shared<LandMen::Soldier>();
		if (!F.S->Load(Text, OutError))
		{
			return false;
		}
		for (int A = 0; A < Acts; ++A)
		{
			F.Clip[A] = F.S->ClipIndex(ClipName(static_cast<Act>(A)));
			if (F.Clip[A] < 0)
			{
				OutError = std::string("no clip ") + ClipName(static_cast<Act>(A));
				return false;
			}
		}
		F.HandL = F.S->BoneIndex("fingers_l");
		F.HandR = F.S->BoneIndex("fingers_r");
		if (F.HandL < 0 || F.HandR < 0 || F.S->Meshes[LandMen::Near].Points <= 0)
		{
			OutError = "no hands or no mesh";
			return false;
		}
		{
			// (the ends a man in the water may lift alone; a file without one of these bones keeps that end as it is)
			F.EndOf.assign(F.S->Bones.size(), static_cast<int8_t>(-1));
			const char* Joints[4] = { "hand_l", "hand_r", "foot_l", "foot_r" };
			const char* Parts[4][3] = { { "hand_l", "fingers_l", "thumb_l" }, { "hand_r", "fingers_r", "thumb_r" }, { "foot_l", "ball_l", nullptr }, { "foot_r", "ball_r", nullptr } };
			for (int E = 0; E < 4; ++E)
			{
				F.EndJoint[E] = F.S->BoneIndex(Joints[E]);
				for (const char* Name : Parts[E])
				{
					const int B = Name ? F.S->BoneIndex(Name) : -1;
					if (B >= 0 && static_cast<size_t>(B) < F.EndOf.size())
					{
						F.EndOf[static_cast<size_t>(B)] = static_cast<int8_t>(E);
					}
				}
			}
		}
		// (the file's own colours for the army paints it wears: his skin, his boots, his soles)
		const std::vector<std::string>& Names = LandMen::PaintNames();
		const auto [ColoursOpen, ColoursClose] = ObjectAfter(Text, "\"colours\"");
		for (const auto& [Name, Rgb] : ColoursIn(Text, ColoursOpen, ColoursClose))
		{
			for (size_t I = 0; I < Names.size() && I < 16; ++I)
			{
				if (Names[I] == Name)
				{
					F.Own[I] = Rgb;
					F.bOwn[I] = true;
				}
			}
		}
		// (its other dresses: each of its colour_variants that gives colours, by its own paints' names (new_paints) and the
		// army paints', with its name)
		const auto [NewOpen, NewClose] = ObjectAfter(Text, "\"new_paints\"");
		std::vector<std::pair<std::string, int>> NewPaints;
		for (size_t P = NewOpen; NewClose != std::string::npos && P < NewClose;)
		{
			const size_t A = Text.find('"', P + 1);
			const size_t B = A < NewClose ? Text.find('"', A + 1) : std::string::npos;
			if (B == std::string::npos || B > NewClose)
			{
				break;
			}
			const size_t Next = Text.find_first_of(",}", B);
			const std::vector<double> V = Numbers(Text, B + 1, Next);
			if (V.size() == 1)
			{
				NewPaints.emplace_back(Text.substr(A + 1, B - A - 1), static_cast<int>(V[0]));
			}
			P = Next;
		}
		const size_t Variants = Text.find("\"colour_variants\"");
		const size_t VariantsOpen = Variants == std::string::npos ? Variants : Text.find('{', Variants);
		for (size_t P = VariantsOpen; P != std::string::npos;)
		{
			const size_t Open = Text.find('{', P + 1), Close = Open == std::string::npos ? Open : Text.find('}', Open);
			const size_t End = Text.find('}', P + 1);
			if (Open == std::string::npos || Close == std::string::npos || End < Open)
			{
				break;
			}
			const size_t NameEnd = Text.rfind('"', Open), NameFrom = NameEnd == std::string::npos || NameEnd <= P ? std::string::npos : Text.rfind('"', NameEnd - 1);
			const std::string Variant = NameFrom == std::string::npos || NameFrom <= P ? std::string() : Text.substr(NameFrom + 1, NameEnd - NameFrom - 1);
			Dress Other;
			bool bAny = false;
			for (const auto& [Name, Rgb] : ColoursIn(Text, Open, Close))
			{
				for (const auto& [Paint, Number] : NewPaints)
				{
					if (Paint == Name)
					{
						Other.Put(Number, Rgb);
						bAny = true;
					}
				}
				for (size_t I = 0; I < Names.size(); ++I)
				{
					if (Names[I] == Name)
					{
						Other.Put(static_cast<int>(I), Rgb);
						bAny = true;
					}
				}
			}
			if (bAny)
			{
				F.Others.emplace_back(Variant, Other);
			}
			P = Close;
		}
		// (the row clip's oar: its rowlock, then its handle's track)
		const size_t Lock = Text.find("\"rowlock\"");
		if (Lock != std::string::npos)
		{
			const size_t Open = Text.find('[', Lock), Close = Text.find(']', Lock);
			const std::vector<double> L = Open < Close ? Numbers(Text, Open, Close) : std::vector<double>();
			const size_t Track = Text.find("\"track\"", Lock);
			const size_t From = Track == std::string::npos ? Track : Text.find('[', Track);
			const size_t To = From == std::string::npos ? From : Text.find("]]", From);
			const std::vector<double> H = To == std::string::npos ? std::vector<double>() : Numbers(Text, From, To);
			if (L.size() == 3 && H.size() >= 8 && H.size() % 4 == 0)
			{
				for (int C = 0; C < 3; ++C)
				{
					F.Lock[C] = static_cast<float>(L[static_cast<size_t>(C)]);
				}
				F.Handle.assign(H.begin(), H.end());
			}
		}
		Read.push_back(std::move(F));
		return true;
	}

	int Crew::FileFor(const std::string& Navy) const
	{
		const std::string Want = ArmyOf(Navy);
		for (size_t K = 0; K < Read.size(); ++K)
		{
			if (Read[K].Army == Want)
			{
				return static_cast<int>(K);
			}
		}
		// (another navy's men in the American's working rig until their own file is made, in their own colours)
		for (size_t K = 0; K < Read.size(); ++K)
		{
			if (Read[K].Army == "USA")
			{
				return static_cast<int>(K);
			}
		}
		return Read.empty() ? -1 : 0;
	}

	void Crew::DressOf(int K, int Variant, bool bWarm, Dress& Out) const
	{
		Out = Dress();
		const FileRead& F = Read[static_cast<size_t>(K)];
		const auto White = [](const std::string& Name) { return Name.rfind("white", 0) == 0; };
		// (his own dress white where its shirt is pale: each of its colours over three fifths)
		const auto Own = F.S->OwnPaints.find(Shirt);
		const uint32_t Rgb = Own == F.S->OwnPaints.end() ? 0u : Own->second;
		const bool bOwnWhite = ((Rgb >> 16) & 255u) >= 0xB0u && ((Rgb >> 8) & 255u) >= 0xB0u && (Rgb & 255u) >= 0xB0u;
		const bool bNoWhite = !bWarm && bOwnWhite;
		if (Variant == 0 && !bNoWhite)
		{
			return;
		}
		for (const auto& [Name, Other] : F.Others)
		{
			if (bWarm || !White(Name))
			{
				Out = Other;
				return;
			}
		}
	}

	bool SummerWhites(bool bWhen, int Month, double Lat, double Lon)
	{
		if (!bWhen || Month < 4 || Month > 8)
		{
			return false;
		}
		// (the Mediterranean: from Gibraltar to the Levant, the Aegean and the Adriatic, not the Black Sea or the Bay of
		// Biscay; the Indian Ocean north of the Tropic of Capricorn, with the Red Sea and the Gulf, to Sumatra: south of
		// it May to September is its winter; the tropics, the Tropic of Cancer to the Tropic of Capricorn)
		const bool bMed = Lat >= 30.0 && Lat <= 46.0 && Lon >= -5.7 && Lon <= 36.5 && !(Lat > 40.8 && Lon > 26.5) && !(Lon < -0.5 && Lat > 38.5);
		const bool bIndian = Lat >= -23.5 && Lat <= 30.0 && Lon >= 20.0 && Lon <= 120.0 && !(Lat > 10.0 && Lon > 100.0);
		const bool bTropics = Lat >= -23.5 && Lat <= 23.5;
		return bMed || bIndian || bTropics;
	}

	float Crew::Long(int K, Act A) const
	{
		const FileRead& F = Read[static_cast<size_t>(K)];
		return F.S->Clips[static_cast<size_t>(F.Clip[static_cast<int>(A)])].Long;
	}

	Placed Crew::Place(int K, const SeaShow::ManOut& M, const Aboard& Where, double Now, double LandedAt) const
	{
		using SeaShow::Swim;
		Placed P;
		P.File = K;
		P.X = M.At.X;
		P.Y = M.At.Y;
		P.Z = 0.0;
		P.Heading = M.Heading;
		auto Loop = [&](Act A, double T)
		{
			P.What = A;
			P.Time = Wrapped(T, Long(K, A));
		};
		const double Own = Phase01(M.Look, 7);
		const SeaShow::FloatOut* On = Where.On;
		switch (M.Doing)
		{
		case Swim::Jump:
			// (falling feet first, the root under his feet as the show drops him)
			Loop(Act::Fall, M.Since);
			P.Z = M.At.Z;
			break;
		case Swim::Climb:
			// (up the net, his hands and feet keeping time with how far up her side he is)
			Loop(Act::Climb, M.At.Z / 0.45 * Long(K, Act::Climb));
			P.Z = M.At.Z;
			break;
		case Swim::Cling:
			// (holding on where the show puts him: a raft's tube at his hands, a boat's grab line (SeaShow::GrabHold); riding
			// the swell with it, its rise where he is)
			Loop(Act::Cling, Now + Own * Long(K, Act::Cling));
			P.Z = Where.Ride;
			break;
		case Swim::Raft:
			// (on the tube's top, his back to the sea, a little out from where the show sits him)
			Loop(Act::Sit, Now + Own * Long(K, Act::Sit));
			if (On)
			{
				const double DX = M.At.X - On->At.X, DY = M.At.Y - On->At.Y, D = std::hypot(DX, DY);
				if (D > 1e-6)
				{
					P.X += DX / D * 0.13;
					P.Y += DY / D * 0.13;
				}
			}
			P.Z = RaftTop + Where.Ride;
			break;
		case Swim::Boat:
			// (an oarsman on his thwart, the rest on the boards; all face aft)
			if (Where.bRowing)
			{
				P.What = Act::Row;
				P.Time = Wrapped(Where.Stroke, Long(K, Act::Row));
				P.Z = ThwartTop + Where.Ride;
				P.bMirror = Where.bStarboard;
			}
			else
			{
				Loop(Act::Sit, Now + Own * Long(K, Act::Sit));
				P.Z = Boards + Where.Ride;
			}
			break;
		case Swim::Dead:
			Loop(Act::Dead, Now + Own * Long(K, Act::Dead));
			break;
		case Swim::Swim:
			if (LandedAt >= 0.0 && Now - LandedAt < Long(K, Act::Enter))
			{
				// (just come down: under, and up)
				P.What = Act::Enter;
				P.Time = static_cast<float>(std::max(0.0, Now - LandedAt));
			}
			else
			{
				Loop(Act::Swim, Now + Own * Long(K, Act::Swim));
			}
			break;
		case Swim::Tread:
		default:
			if (LandedAt >= 0.0 && Now - LandedAt < Long(K, Act::Enter))
			{
				P.What = Act::Enter;
				P.Time = static_cast<float>(std::max(0.0, Now - LandedAt));
			}
			else
			{
				Loop(M.bWave ? Act::Wave : Act::Tread, Now + Own * 4.0);
			}
			break;
		}
		return P;
	}

	int Crew::PoseOf(int K, Act A, float Time) const
	{
		const int N = PosesOf(A);
		const float L = Long(K, A);
		if (N <= 1 || L <= 0.f)
		{
			return 0;
		}
		if (IsLoop(A))
		{
			// (on the loop's own time first, so a negative or a very late time gives a pose in range)
			return static_cast<int>(std::lround(N * static_cast<double>(Wrapped(Time, L)) / L)) % N;
		}
		return std::clamp(static_cast<int>(std::lround((N - 1) * static_cast<double>(Time) / L)), 0, N - 1);
	}

	float Crew::PoseTime(int K, Act A, int Pose) const
	{
		const int N = PosesOf(A);
		const float L = Long(K, A);
		if (N <= 1)
		{
			return 0.f;
		}
		return IsLoop(A) ? L * static_cast<float>(Pose) / N : L * static_cast<float>(Pose) / (N - 1);
	}

	void Crew::Frames(int K, Act A, float Time, LandMen::Xform* Out) const
	{
		const FileRead& F = Read[static_cast<size_t>(K)];
		LandMen::Pose P;
		LandMen::Sample(F.S->Clips[static_cast<size_t>(F.Clip[static_cast<int>(A)])], Time, P);
		LandMen::BoneFrames(*F.S, P, Out);
	}

	int Crew::Skin(int K, Act A, float Time, LandMen::Lod L, std::vector<float>& OutAt, std::vector<float>& OutFacing) const
	{
		const LandMen::Mesh& M = File(K).Meshes[L];
		LandMen::Xform Fr[LandMen::MaxBones];
		Frames(K, A, Time, Fr);
		OutAt.resize(static_cast<size_t>(M.Points) * 3);
		OutFacing.resize(OutAt.size());
		LandMen::SkinMesh(M, Fr, OutAt.data(), OutFacing.data());
		if (InWater(A))
		{
			// (a hand or boot under the water at its wrist or ankle: all of it under, SinkTo down)
			const FileRead& F = Read[static_cast<size_t>(K)];
			bool bSink[4] = { false, false, false, false };
			bool bAny = false;
			for (int E = 0; E < 4; ++E)
			{
				const int B = F.EndJoint[E];
				if (B >= 0 && B < LandMen::MaxBones && static_cast<size_t>(B) < F.S->Bones.size())
				{
					bSink[E] = Fr[B].Point(F.S->Bones[static_cast<size_t>(B)].Head).Z < SinkFrom;
					bAny = bAny || bSink[E];
				}
			}
			const size_t Points = static_cast<size_t>(M.Points);
			if (bAny && M.Part.size() >= Points)
			{
				const bool bTwo = M.Part2.size() >= Points && M.W2.size() >= Points;
				for (size_t I = 0; I < Points; ++I)
				{
					int E = M.Part[I] < F.EndOf.size() ? F.EndOf[M.Part[I]] : -1;
					if (E < 0 && bTwo && M.W2[I] > 0.f && M.Part2[I] < F.EndOf.size())
					{
						E = F.EndOf[M.Part2[I]];
					}
					if (E >= 0 && bSink[E])
					{
						OutAt[I * 3 + 2] = std::min(OutAt[I * 3 + 2], -SinkTo);
					}
				}
			}
		}
		return M.Points;
	}

	LandMen::Vec3 Crew::Hands(int K, Act A, float Time) const
	{
		const FileRead& F = Read[static_cast<size_t>(K)];
		LandMen::Xform Fr[LandMen::MaxBones];
		Frames(K, A, Time, Fr);
		const LandMen::Vec3 L = Fr[F.HandL].Point(F.S->Bones[static_cast<size_t>(F.HandL)].Head);
		const LandMen::Vec3 R = Fr[F.HandR].Point(F.S->Bones[static_cast<size_t>(F.HandR)].Head);
		return { 0.5f * (L.X + R.X), 0.5f * (L.Y + R.Y), 0.5f * (L.Z + R.Z) };
	}

	bool Crew::Oar(int K, float Time, bool bMirror, LandMen::Vec3& OutHandle, LandMen::Vec3& OutLock) const
	{
		const FileRead& F = Read[static_cast<size_t>(K)];
		const size_t Keys = F.Handle.size() / 4;
		if (Keys < 2)
		{
			return false;
		}
		const float T = Wrapped(Time, Long(K, Act::Row));
		size_t I = 0;
		while (I + 2 < Keys && F.Handle[(I + 1) * 4] <= T)
		{
			++I;
		}
		const float* A = &F.Handle[I * 4];
		const float* B = &F.Handle[(I + 1) * 4];
		const float Span = B[0] - A[0];
		const float U = Span > 1e-6f ? std::clamp((T - A[0]) / Span, 0.f, 1.f) : 0.f;
		const float Hand = bMirror ? -1.f : 1.f;
		OutHandle = { A[1] + (B[1] - A[1]) * U, Hand * (A[2] + (B[2] - A[2]) * U), A[3] + (B[3] - A[3]) * U };
		OutLock = { F.Lock[0], Hand * F.Lock[1], F.Lock[2] };
		return true;
	}

	void Crew::Paint(int K, LandMen::Lod L, const Dress& D, double Oil, std::vector<uint8_t>& OutRgb) const
	{
		const FileRead& F = Read[static_cast<size_t>(K)];
		const LandMen::Soldier& S = File(K);
		const LandMen::Mesh& M = S.Meshes[L];
		const double Black = std::clamp(Oil, 0.0, 1.0);
		static const double OilRgb[3] = { 20.0, 18.0, 16.0 };
		OutRgb.resize(static_cast<size_t>(M.Points) * 3);
		const bool bTint = M.Tint.size() == static_cast<size_t>(M.Points) * 3;
		for (int I = 0; I < M.Points; ++I)
		{
			const size_t Iz = static_cast<size_t>(I);
			const int Number = M.Paint[Iz];
			uint32_t Rgb = 0x707070u;
			if (Number < 64 && D.bSet[Number])
			{
				Rgb = D.Paint[Number];
			}
			else if (Number < 16 && F.bOwn[Number])
			{
				Rgb = F.Own[Number];
			}
			else if (const auto Own = S.OwnPaints.find(Number); Own != S.OwnPaints.end())
			{
				Rgb = Own->second;
			}
			const double Soaked = Black * OilTakes(Number);
			const float Shade = M.Shade.empty() ? 1.f : M.Shade[Iz];
			for (int C = 0; C < 3; ++C)
			{
				const double Clean = static_cast<double>((Rgb >> (16 - 8 * C)) & 0xFFu);
				const float Ch = static_cast<float>(Clean + (OilRgb[C] - Clean) * Soaked);
				const float V = Ch * Shade * (bTint ? M.Tint[Iz * 3 + static_cast<size_t>(C)] : 1.f);
				OutRgb[Iz * 3 + static_cast<size_t>(C)] = static_cast<uint8_t>(std::min(255.f, std::max(0.f, V + 0.5f)));
			}
		}
	}
}
