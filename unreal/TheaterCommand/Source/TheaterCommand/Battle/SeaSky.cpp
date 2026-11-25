#include "Battle/SeaSky.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <mutex>
#include <utility>

namespace SeaSky
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;

		double Clamp(double V, double Least, double Most) { return V < Least ? Least : V > Most ? Most : V; }

		/** 0 under 0, 1 over 1, smooth between. */
		double Smooth(double T)
		{
			const double X = Clamp(T, 0.0, 1.0);
			return X * X * (3.0 - 2.0 * X);
		}
		/** The same between two bounds (the engine's SmoothStep). */
		double Smooth(double From, double To, double X) { return To > From ? Smooth((X - From) / (To - From)) : (X >= To ? 1.0 : 0.0); }

		/** A string with the spaces at its ends taken off. */
		std::string Trimmed(const std::string& Text)
		{
			size_t First = 0, Last = Text.size();
			while (First < Last && std::isspace(static_cast<unsigned char>(Text[First]))) { ++First; }
			while (Last > First && std::isspace(static_cast<unsigned char>(Text[Last - 1]))) { --Last; }
			return Text.substr(First, Last - First);
		}

		/** A number as a value is written on the command line ("0.80"): two places, so the "Sea look:" line reads alike. */
		std::string Spelled(double V)
		{
			char Buffer[32];
			std::snprintf(Buffer, sizeof(Buffer), "%.2f", V);
			return std::string(Buffer);
		}

		/** The switch of a name as it was asked for, or none; its value may be written back. */
		SeaLookSwitches::Asked* AskedFor(SeaLookSwitches::Look& InForce, const char* Name)
		{
			for (SeaLookSwitches::Asked& One : InForce.Given)
			{
				if (One.Index >= 0 && One.Index < SeaLookSwitches::Count && std::string(SeaLookSwitches::All[One.Index].Name) == Name)
				{
					return &One;
				}
			}
			return nullptr;
		}

		/** The sky's word for how much of it is cloud, by eighths (the rules' own wording, src/battle/air/sky.ts). */
		std::string WordOf(double Cover)
		{
			const int Eighths = static_cast<int>(std::lround(Clamp(Cover, 0.0, 1.0) * 8.0));
			return Eighths <= 0 ? "clear" : Eighths <= 2 ? "fair" : Eighths <= 4 ? "scattered" : Eighths <= 7 ? "broken" : "overcast";
		}

		/** The chosen day's cover as it reads, and a cumulus day given a cover laid at what paints it (CumulusLaidFor);
		 * the log says so. A deck, and a cover not given (air's stand-in, the art director's passed cumulus), as given. */
		void LaidToRead(Sky& Out)
		{
			Out.Cover = Out.Day.CoverLaid();
			if (Out.Day.bDeck || !(Out.Day.Cover >= 0.0))
			{
				return;
			}
			const double Laid = CumulusLaidFor(Out.Cover);
			if (Laid != Out.Day.Cover)
			{
				Out.Day.Cover = Laid;
				Out.Why += ", laid at " + Spelled(Laid) + " so that it reads so from the sea";
			}
		}
	}

	static_assert(AirSky::CumulusMost > 0.549 && AirSky::CumulusMost < 0.551,
		"SeaSky::CumulusPainted is measured at 0, 0.05, ... 0.55: measure it again (Tools/sea_waves/cover_table) for air's new CumulusMost");

	// ---- The switches ---------------------------------------------------------------------------------------------

	Asked Take(SeaLookSwitches::Look& InForce)
	{
		Asked Out;
		if (SeaLookSwitches::Asked* Draw = AskedFor(InForce, "TheaterSeaClouds="))
		{
			const std::string Want = Trimmed(Draw->Value);
			// (puffs mode is the plan's step 13(e), which has not landed: asked for, the dome is drawn, and the line says
			// so, since it is what was applied)
			Out.Draw = Want == "dome" || Want == "puffs" ? Clouds::Dome : Clouds::Off;
			Draw->Value = Out.Draw == Clouds::Dome ? "dome" : "0";
		}
		Out.bShade = SeaLookSwitches::TakeOnOff(InForce, "TheaterSeaCloudShade=");
		Out.bRain = SeaLookSwitches::TakeOnOff(InForce, "TheaterSeaRain=");
		Out.bStandIn = SeaLookSwitches::TakeOnOff(InForce, "TheaterSeaSkyStandIn=");
		if (SeaLookSwitches::Asked* Weather = AskedFor(InForce, "TheaterSeaWeather="))
		{
			// "deck,0.8", "cumulus,0.3", "0.3" (cumulus), with the rain after the cover: "deck,0.95,0.6".
			std::vector<std::string> Parts;
			std::string One;
			for (const char Ch : Weather->Value)
			{
				if (Ch == ',') { Parts.push_back(Trimmed(One)); One.clear(); }
				else { One.push_back(Ch); }
			}
			Parts.push_back(Trimmed(One));
			size_t At = 0;
			bool bDeck = false;
			if (At < Parts.size() && !Parts[At].empty() && (std::isalpha(static_cast<unsigned char>(Parts[At][0])) != 0))
			{
				bDeck = Parts[At] == "deck";
				const bool bKnown = Parts[At] == "deck" || Parts[At] == "cumulus";
				At = bKnown ? At + 1 : Parts.size();
			}
			double Cover = -1.0, Rain = -1.0;
			if (At < Parts.size() && !Parts[At].empty())
			{
				Cover = std::atof(Parts[At].c_str());
				++At;
				if (At < Parts.size() && !Parts[At].empty())
				{
					Rain = Clamp(std::atof(Parts[At].c_str()), 0.0, 1.0);
				}
			}
			if (Cover >= 0.0 && Cover <= 1.0)
			{
				Out.bWeather = true;
				Out.bWeatherDeck = bDeck;
				Out.WeatherCover = Cover;
				Out.WeatherRain = Rain;
				Weather->Value = (bDeck ? "deck," : "cumulus,") + Spelled(Cover) + (Rain >= 0.0 ? "," + Spelled(Rain) : "");
			}
			else
			{
				Weather->Value = "none";
			}
		}
		return Out;
	}

	// ---- The day --------------------------------------------------------------------------------------------------

	Given ReadField(const AirFeed::Json& Field)
	{
		Given Out;
		const AirFeed::Json& Sky = Field["sky"];
		if (!Sky.IsObject())
		{
			return Out;
		}
		// (read as AirFeed reads air's sky.place.cloud, the same fallbacks: cumulus, base 950, top 2,500, the cover not
		// given below 0)
		Out.bGiven = true;
		Out.Day.bGiven = true;
		Out.Day.bDeck = Sky["kind"].Str("cumulus") == "deck";
		Out.Day.Base = Sky["base"].Num(Out.Day.Base);
		Out.Day.Top = Sky["top"].Num(Out.Day.Top);
		Out.Day.Chance = Sky["chance"].Num(0.0);
		Out.Day.Zone = Sky["zone"].Str("");
		const AirFeed::Json& Cover = Sky["cover"];
		Out.Day.Cover = Cover.IsNumber() && std::isfinite(Cover.Num(0.0)) ? Clamp(Cover.Num(0.0), 0.0, 1.0) : -1.0;
		// (its word as given: the rules write "sky", a recording of the sea's field "word" or "sky")
		Out.Day.Word = Sky["word"].IsString() ? Sky["word"].Str("") : Sky["sky"].Str("");
		const AirFeed::Json& Rain = Sky["rain"];
		Out.Rain = Rain.IsNumber() && std::isfinite(Rain.Num(0.0)) ? Clamp(Rain.Num(0.0), 0.0, 1.0) : -1.0;
		return Out;
	}

	Given ReadRecording(const std::string& Text, std::string* OutError)
	{
		Given Out;
		AirFeed::Json Whole;
		std::string Error;
		if (!AirFeed::Parse(Text, Whole, Error))
		{
			if (OutError) { *OutError = Error; }
			return Out;
		}
		const AirFeed::Json& Field = Whole["field"];
		if (!Field.IsObject())
		{
			if (OutError) { *OutError = "no field"; }
			return Out;
		}
		Out = ReadField(Field);
		if (!Out.bGiven && OutError)
		{
			*OutError = "the field sends no sky";
		}
		return Out;
	}

	AirSky::Day StandIn()
	{
		// (air's own: the sky AirFeed leaves for a bundle that gives none, AirSky::Day as made, so the stand-in lays the
		// art director's passed cumulus at DefaultCumulus)
		return AirSky::Day();
	}

	double RainOf(const AirSky::Day& D, double GivenRain)
	{
		if (GivenRain >= 0.0)
		{
			return Clamp(GivenRain, 0.0, 1.0);
		}
		const double Cover = D.CoverLaid();
		// (a heavy cumulus day's showers as the air battle has them, none at 0.45 of cover and three of the six curtains
		// from 0.55; a closed deck's rain none at 0.85, three curtains at 0.97 and over [EST: a sky of nine tenths or
		// more under nimbostratus is mostly a raining one])
		return D.bDeck ? 0.6 * Smooth(0.85, 0.97, Cover) : 0.5 * Smooth(0.45, 0.55, Cover);
	}

	Sky Choose(const Given& FromRules, const Asked& Switches)
	{
		Sky Out;
		if (Switches.bWeather)
		{
			Out.bLaid = true;
			Out.bAsked = true;
			Out.Day = FromRules.bGiven ? FromRules.Day : AirSky::Day();
			Out.Day.bGiven = true;
			Out.Day.bDeck = Switches.bWeatherDeck;
			Out.Day.Cover = Switches.WeatherCover;
			Out.Day.Word = WordOf(Out.Day.CoverLaid());
			Out.Rain = RainOf(Out.Day, Switches.WeatherRain);
			Out.Why = std::string("a day asked for review pictures: ") + (Out.Day.bDeck ? "a deck" : "cumulus") + " of "
				+ Spelled(Out.Day.CoverLaid()) + " (" + Out.Day.Word + ")";
			LaidToRead(Out);
			return Out;
		}
		if (FromRules.bGiven)
		{
			Out.bLaid = true;
			Out.Day = FromRules.Day;
			Out.Rain = RainOf(Out.Day, FromRules.Rain);
			Out.Why = std::string("the rules' sky: ") + (Out.Day.bDeck ? "a deck" : "cumulus") + " of "
				+ Spelled(Out.Day.CoverLaid()) + (Out.Day.Word.empty() ? "" : " (" + Out.Day.Word + ")");
			LaidToRead(Out);
			return Out;
		}
		if (Switches.bStandIn)
		{
			Out.bLaid = true;
			Out.bStandIn = true;
			Out.Day = StandIn();
			Out.Rain = RainOf(Out.Day, -1.0);
			Out.Why = "stand-in sky: the rules sent none";
			LaidToRead(Out);
			return Out;
		}
		Out.Why = "no clouds: the rules sent no sky";
		return Out;
	}

	double CumulusLaidFor(double Cover)
	{
		if (!(Cover > 0.0))
		{
			return 0.0;
		}
		if (Cover >= CumulusReach)
		{
			return AirSky::CumulusMost;
		}
		constexpr int Last = static_cast<int>(std::size(CumulusPainted)) - 1;
		for (int Step = 1; Step <= Last; ++Step)
		{
			if (Cover <= CumulusPainted[Step])
			{
				const double Low = CumulusPainted[Step - 1], High = CumulusPainted[Step];
				const double Along = High > Low ? (Cover - Low) / (High - Low) : 1.0;
				return std::min(AirSky::CumulusMost, AirSky::CumulusMost * (Step - 1 + Along) / Last);
			}
		}
		return AirSky::CumulusMost;
	}

	AirSky::Day ReadAs(const Sky& Chosen)
	{
		AirSky::Day Read = Chosen.Day;
		if (Chosen.bLaid && !Read.bDeck && Read.Cover >= 0.0)
		{
			Read.Cover = Chosen.Cover;
		}
		return Read;
	}

	AirSky::Placing PlacingFor(uint64_t Seed, double MiddleX, double MiddleY, const Vec& Sun, const Vec& Wind)
	{
		AirSky::Placing P;
		P.Seed = Seed;
		P.TargetX = MiddleX;
		P.TargetY = MiddleY;
		// (no bombers: a deck stays at the day's own base and top)
		P.Band = 0.0;
		P.LowBand = 0.0;
		// (no hole: a fleet is seen from the sea, not from a bomber)
		P.bShips = false;
		P.Sun = Sun;
		// (the sea under it all: nothing is lifted, and no ground is looked for)
		P.Ground = nullptr;
		P.GroundTop = 0.0;
		P.MostPuffs = MostPuffs;
		P.Wind = Wind;
		return P;
	}

	AirSky::Field Lay(const Sky& Chosen, const AirSky::Placing& P)
	{
		if (!Chosen.bLaid)
		{
			return AirSky::Field();
		}
		return AirSky::Lay(Chosen.Day, P);
	}

	// ---- The dome -------------------------------------------------------------------------------------------------

	void PlaceOf(const Vec& Way, int W, int H, double& OutColumn, double& OutRow)
	{
		const Vec Unit = Way.Unit(Vec(0.0, 0.0, 1.0));
		// (the bearing east of north, over a turn)
		double Round = std::atan2(Unit.X, Unit.Y) / (2.0 * Pi);
		Round -= std::floor(Round);
		OutColumn = Round * W;
		OutRow = (1.0 - Unit.Z) * H;
	}

	Vec WayOf(double Column, double Row, int W, int H)
	{
		const double Round = 2.0 * Pi * Column / std::max(1, W);
		const double Up = Clamp(1.0 - Row / std::max(1, H), -1.0, 1.0);
		const double Flat = std::sqrt(std::max(0.0, 1.0 - Up * Up));
		return Vec(Flat * std::sin(Round), Flat * std::cos(Round), Up);
	}

	void DomeUv(double WorldX, double WorldY, double WorldZ, double& OutU, double& OutV)
	{
		const double Length = std::sqrt(WorldX * WorldX + WorldY * WorldY + WorldZ * WorldZ);
		const double Scale = Length > 1e-12 ? 1.0 / Length : 1.0;
		double Round = std::atan2(WorldY * Scale, WorldX * Scale) / (2.0 * Pi);
		Round -= std::floor(Round);
		OutU = Round;
		OutV = 1.0 - WorldZ * Scale;
	}

	double Dome::AlphaAt(const Vec& Way) const
	{
		if (Rgba.size() != static_cast<size_t>(W) * static_cast<size_t>(H) * 4 || W <= 0 || H <= 0)
		{
			return 0.0;
		}
		double Column = 0.0, Row = 0.0;
		PlaceOf(Way, W, H, Column, Row);
		int X = static_cast<int>(std::floor(Column));
		int Y = static_cast<int>(std::floor(Row));
		X = ((X % W) + W) % W;
		Y = std::max(0, std::min(H - 1, Y));
		return Rgba[(static_cast<size_t>(Y) * W + X) * 4 + 3] / 255.0;
	}

	double Dome::Cover() const
	{
		if (Rgba.size() != static_cast<size_t>(W) * static_cast<size_t>(H) * 4 || W <= 0 || H <= 0)
		{
			return 0.0;
		}
		// (every pixel the same piece of sky: the columns even in bearing, the rows even in the sine of the height)
		double Sum = 0.0;
		for (size_t K = 0; K < Rgba.size(); K += 4)
		{
			Sum += Rgba[K + 3];
		}
		return Sum / 255.0 / static_cast<double>(Rgba.size() / 4);
	}

	namespace
	{
		/** The sheet of shapes with its levels, each half the last, as the chip reads it (the air show's own picture
		 * tool has the same). Kept from one paint to the next: the scene hands the same sheet every time. */
		struct Shapes
		{
			std::vector<std::vector<float>> Levels;
			std::vector<int> Sides;

			void Make(const std::vector<uint8_t>& Rgba)
			{
				Levels.clear();
				Sides.clear();
				int Side = AirSky::FragmentCells * AirSky::FragmentSize;
				if (Rgba.size() != static_cast<size_t>(Side) * static_cast<size_t>(Side) * 4)
				{
					return;
				}
				Levels.emplace_back(Rgba.size());
				for (size_t K = 0; K < Rgba.size(); ++K)
				{
					Levels[0][K] = static_cast<float>(Rgba[K] / 255.0);
				}
				Sides.push_back(Side);
				while (Side > 1)
				{
					const int Half = Side / 2;
					const std::vector<float>& From = Levels.back();
					std::vector<float> To(static_cast<size_t>(Half) * Half * 4);
					for (int Y = 0; Y < Half; ++Y)
					{
						for (int X = 0; X < Half; ++X)
						{
							for (int K = 0; K < 4; ++K)
							{
								auto At = [&](int AX, int AY) { return From[(static_cast<size_t>(AY) * Side + AX) * 4 + K]; };
								To[(static_cast<size_t>(Y) * Half + X) * 4 + K] = 0.25f * (At(2 * X, 2 * Y) + At(2 * X + 1, 2 * Y) + At(2 * X, 2 * Y + 1) + At(2 * X + 1, 2 * Y + 1));
							}
						}
					}
					Levels.push_back(std::move(To));
					Sides.push_back(Half);
					Side = Half;
				}
			}

			bool Ready() const { return !Levels.empty(); }

			void Read(double U, double V, int Level, double Out[4]) const
			{
				const int Pick = std::max(0, std::min(static_cast<int>(Levels.size()) - 1, Level));
				const int Side = Sides[static_cast<size_t>(Pick)];
				const std::vector<float>& P = Levels[static_cast<size_t>(Pick)];
				const double X = U * Side - 0.5, Y = V * Side - 0.5;
				const int X0 = static_cast<int>(std::floor(X)), Y0 = static_cast<int>(std::floor(Y));
				const double FX = X - X0, FY = Y - Y0;
				auto At = [&](int AX, int AY, int K)
				{
					const int CX = std::max(0, std::min(Side - 1, AX)), CY = std::max(0, std::min(Side - 1, AY));
					return static_cast<double>(P[(static_cast<size_t>(CY) * Side + CX) * 4 + K]);
				};
				for (int K = 0; K < 4; ++K)
				{
					Out[K] = (At(X0, Y0, K) * (1.0 - FX) + At(X0 + 1, Y0, K) * FX) * (1.0 - FY) + (At(X0, Y0 + 1, K) * (1.0 - FX) + At(X0 + 1, Y0 + 1, K) * FX) * FY;
				}
			}
		};

		/** The sheet last made, by the bytes it was made from: the scene's sheet is one and the same all battle. */
		const Shapes* SheetFor(const std::vector<uint8_t>& Rgba)
		{
			static std::mutex Lock;
			static const std::vector<uint8_t>* From = nullptr;
			static Shapes Made;
			std::lock_guard<std::mutex> Held(Lock);
			if (From != &Rgba || !Made.Ready())
			{
				Made.Make(Rgba);
				From = &Rgba;
			}
			return Made.Ready() ? &Made : nullptr;
		}

		/** One corner of a triangle as the dome is painted: its way from the eye, its colour, and its place on the sheet. */
		struct Corner
		{
			Vec Way;
			double R = 0.0, G = 0.0, B = 0.0, A = 0.0;
			double U = 0.0, V = 0.0, Behind = 0.0, Ahead = 0.0;
		};
	}

	double DeckClosing(const AirSky::Field& F, const Vec& Eye)
	{
		if (!F.Sky.bDeck || !(Eye.Z < F.Base))
		{
			return 0.0;
		}
		return Smooth(DeckOpenUnder, 1.0, F.Sky.CoverLaid());
	}

	double OverviewHeight(bool bDeckDrawn, double BaseMetres)
	{
		if (!bDeckDrawn || !(BaseMetres > 0.0))
		{
			return OverviewMost;
		}
		return std::max(OverviewLeast, std::min(OverviewMost, BaseMetres - OverviewUnderBase));
	}

	void PaintDome(const AirSky::Field& F, const Vec& Eye, const Vec& Drift, const Vec& Sun, const Rgb& Light, AirSky::Painter& Painter, Dome& Out, const DomeHow& How)
	{
		const int W = std::max(2, How.W), H = std::max(1, How.H);
		const int Rows = How.Rows > 0 ? std::min(H, How.Rows) : H;
		Out.W = W;
		Out.H = H;
		Out.Eye = Eye;
		Out.Drift = Drift;
		Out.Puffs = 0;
		Out.Dropped = 0;
		Out.Triangles = 0;
		Out.Tested = 0;
		Out.Rgba.assign(static_cast<size_t>(W) * static_cast<size_t>(H) * 4, 0);
		if (F.Clouds.empty() || F.Puffs.empty())
		{
			return;
		}
		const Shapes* Sheet = How.Sheet ? SheetFor(*How.Sheet) : nullptr;
		Painter.SetInk(How.Ink);
		Painter.SetSprites(Sheet != nullptr);
		std::vector<Vec> Points, Shadows;
		std::vector<uint8_t> Colours, ShadowColours;
		std::vector<float> Places;
		// (the shadows laid apart and not drawn: they lie on the sea, where the light function lays them)
		Painter.Paint(F, Eye, Drift, Sun, Light, Points, Colours, &Shadows, &ShadowColours, &Places);
		Out.Puffs = Painter.Drawn();
		Out.Dropped = Painter.Dropped();
		const std::vector<int> Triangles = Painter.Triangles(Painter.RoomFor(F));
		const Vec Seen = Eye - Drift; // (the eye in the field's own frame, where the points are)
		// (the picture built up far to near, each triangle over what is behind it; the colours kept multiplied by their
		// alpha while they are laid, and divided out at the end)
		const size_t Pixels = static_cast<size_t>(W) * static_cast<size_t>(Rows);
		std::vector<double> Paint(Pixels * 4, 0.0);
		Corner V[3];
		for (size_t K = 0; K + 5 < Triangles.size(); K += 6)
		{
			// (one face of each pair: the painter lays both, a picture needs one)
			bool bAny = false;
			for (int J = 0; J < 3; ++J)
			{
				const size_t I = static_cast<size_t>(Triangles[K + static_cast<size_t>(J)]);
				V[J].Way = Points[I] - Seen;
				V[J].R = Colours[I * 4] / 255.0;
				V[J].G = Colours[I * 4 + 1] / 255.0;
				V[J].B = Colours[I * 4 + 2] / 255.0;
				V[J].A = Colours[I * 4 + 3] / 255.0;
				bAny = bAny || V[J].A > 0.0;
				if (Sheet && Places.size() >= (I + 1) * 4)
				{
					V[J].U = Places[I * 4];
					V[J].V = Places[I * 4 + 1];
					V[J].Behind = Places[I * 4 + 2];
					V[J].Ahead = Places[I * 4 + 3];
				}
			}
			if (!bAny)
			{
				continue;
			}
			const Vec& A = V[0].Way;
			const Vec& B = V[1].Way;
			const Vec& C = V[2].Way;
			const Vec BxC = B.Cross(C), CxA = C.Cross(A), AxB = A.Cross(B);
			const double Det = A.Dot(BxC);
			if (std::fabs(Det) < 1e-9)
			{
				continue;
			}
			// (the box on the picture: the corners, and points along each edge, since an edge runs as a great circle)
			double LeastRow = 1e30, MostRow = -1e30;
			double Columns[12];
			int Taken = 0;
			for (int Edge = 0; Edge < 3; ++Edge)
			{
				const Vec& From = V[Edge].Way;
				const Vec& To = V[(Edge + 1) % 3].Way;
				static const double Alongs[4] = { 0.0, 0.25, 0.5, 0.75 };
				for (const double Along : Alongs)
				{
					const Vec At = From + (To - From) * Along;
					double Column = 0.0, Row = 0.0;
					PlaceOf(At, W, H, Column, Row);
					LeastRow = std::min(LeastRow, Row);
					MostRow = std::max(MostRow, Row);
					Columns[Taken++] = Column;
				}
			}
			const int Y0 = std::max(0, static_cast<int>(std::floor(LeastRow)) - 2);
			const int Y1 = std::min(Rows - 1, static_cast<int>(std::ceil(MostRow)) + 2);
			if (Y1 < Y0)
			{
				continue;
			}
			// (the zenith inside the triangle, or a span of over half a turn: every column)
			auto Inside = [&](const Vec& Way)
			{
				const double WA = Way.Dot(BxC), WB = Way.Dot(CxA), WC = Way.Dot(AxB);
				return (WA >= 0.0 && WB >= 0.0 && WC >= 0.0 && Det > 0.0) || (WA <= 0.0 && WB <= 0.0 && WC <= 0.0 && Det < 0.0);
			};
			int X0 = 0, Span = W;
			if (!Inside(Vec(0.0, 0.0, 1.0)))
			{
				for (int J = 1; J < Taken; ++J)
				{
					const double Each = Columns[J];
					int Back = J - 1;
					for (; Back >= 0 && Columns[Back] > Each; --Back)
					{
						Columns[Back + 1] = Columns[Back];
					}
					Columns[Back + 1] = Each;
				}
				// (the widest gap between the samples round the turn: the arc that holds them is what is left)
				int Gap = 0;
				double Widest = Columns[0] + W - Columns[Taken - 1];
				for (int J = 0; J + 1 < Taken; ++J)
				{
					if (Columns[J + 1] - Columns[J] > Widest)
					{
						Widest = Columns[J + 1] - Columns[J];
						Gap = J + 1;
					}
				}
				const double Arc = W - Widest;
				if (Arc < 0.5 * W)
				{
					X0 = static_cast<int>(std::floor(Columns[Gap])) - 2;
					Span = static_cast<int>(std::ceil(Arc)) + 5;
				}
			}
			Span = std::min(Span, W);
			// (the sheet's level, as many of its pixels to one of the picture's as this triangle covers)
			int Level = 0;
			if (Sheet)
			{
				double AX = 0.0, AY = 0.0, BX = 0.0, BY = 0.0, CX = 0.0, CY = 0.0;
				PlaceOf(A, W, H, AX, AY);
				PlaceOf(B, W, H, BX, BY);
				PlaceOf(C, W, H, CX, CY);
				const double Area = std::fabs((BX - AX) * (CY - AY) - (BY - AY) * (CX - AX));
				const double Side = Sheet->Sides[0];
				const double OnSheet = std::fabs((V[1].U - V[0].U) * (V[2].V - V[0].V) - (V[1].V - V[0].V) * (V[2].U - V[0].U)) * Side * Side;
				Level = Area > 1e-9 && OnSheet > Area ? static_cast<int>(std::floor(0.5 * std::log2(OnSheet / Area) + 0.5)) : 0;
			}
			++Out.Triangles;
			for (int Y = Y0; Y <= Y1; ++Y)
			{
				for (int Step = 0; Step < Span; ++Step)
				{
					const int X = ((X0 + Step) % W + W) % W;
					const Vec Way = WayOf(X + 0.5, Y + 0.5, W, H);
					++Out.Tested;
					const double WA = Way.Dot(BxC), WB = Way.Dot(CxA), WC = Way.Dot(AxB);
					const bool bIn = Det > 0.0 ? (WA >= 0.0 && WB >= 0.0 && WC >= 0.0) : (WA <= 0.0 && WB <= 0.0 && WC <= 0.0);
					if (!bIn)
					{
						continue;
					}
					const double Sum = WA + WB + WC;
					if (std::fabs(Sum) < 1e-12)
					{
						continue;
					}
					// (where the line from the eye meets the triangle, between its corners: the shares are the corners'
					// own, so the colours run straight across the triangle as the chip runs them)
					const double LA = WA / Sum, LB = WB / Sum, LC = WC / Sum;
					double Alpha = V[0].A * LA + V[1].A * LB + V[2].A * LC;
					if (Alpha <= 0.0)
					{
						continue;
					}
					double R = V[0].R * LA + V[1].R * LB + V[2].R * LC;
					double G = V[0].G * LA + V[1].G * LB + V[2].G * LC;
					double Blue = V[0].B * LA + V[1].B * LB + V[2].B * LC;
					if (Sheet)
					{
						// (M_Cloud's own light: the shape's from the side, from behind the eye and from ahead, twice the
						// sheet's number; its alpha times the mesh's)
						double Read[4];
						Sheet->Read(V[0].U * LA + V[1].U * LB + V[2].U * LC, V[0].V * LA + V[1].V * LB + V[2].V * LC, Level, Read);
						const double Behind = V[0].Behind * LA + V[1].Behind * LB + V[2].Behind * LC;
						const double Ahead = V[0].Ahead * LA + V[1].Ahead * LB + V[2].Ahead * LC;
						const double Lit = 2.0 * (Read[0] + (Read[1] - Read[0]) * Behind + (Read[2] - Read[0]) * Ahead);
						R *= Lit;
						G *= Lit;
						Blue *= Lit;
						Alpha = std::min(1.0, Alpha * Read[3]);
						if (Alpha <= 0.0)
						{
							continue;
						}
					}
					double* Into = &Paint[(static_cast<size_t>(Y) * W + X) * 4];
					const double Keep = 1.0 - Alpha;
					Into[0] = Into[0] * Keep + R * Alpha;
					Into[1] = Into[1] * Keep + G * Alpha;
					Into[2] = Into[2] * Keep + Blue * Alpha;
					Into[3] = Into[3] * Keep + Alpha;
				}
			}
		}
		// (a deck closing to an overcast, seen from under its base: its torn lids leave gaps that a closed sky has not, so
		// the gaps are shut behind the lids with the lids' own mean colour, none at seven eighths and wholly at eight;
		// the sea scene's rule for the plan's cover line: at least nine tenths painted at a full overcast, never less as
		// the cover rises)
		const double Close = DeckClosing(F, Eye);
		if (Close > 0.0)
		{
			double Lit[4] = { 0.0, 0.0, 0.0, 0.0 };
			for (size_t K = 0; K < Pixels; ++K)
			{
				for (int J = 0; J < 4; ++J)
				{
					Lit[J] += Paint[K * 4 + static_cast<size_t>(J)];
				}
			}
			if (Lit[3] > 1e-9)
			{
				for (size_t K = 0; K < Pixels; ++K)
				{
					double* Into = &Paint[K * 4];
					const double Behind = Close * (1.0 - Clamp(Into[3], 0.0, 1.0));
					for (int J = 0; J < 3; ++J)
					{
						Into[J] += Lit[J] / Lit[3] * Behind;
					}
					Into[3] += Behind;
				}
			}
		}
		// (the bytes: the colour on its own (the alpha divided out), halved as the fragment sheet measures light, so a
		// sunlit rim the sheet lights past white keeps it)
		for (size_t K = 0; K < Pixels; ++K)
		{
			const double* From = &Paint[K * 4];
			const double Alpha = Clamp(From[3], 0.0, 1.0);
			uint8_t* To = &Out.Rgba[K * 4];
			if (Alpha > 1e-6)
			{
				for (int J = 0; J < 3; ++J)
				{
					To[J] = static_cast<uint8_t>(Clamp(From[J] / Alpha * 127.5 + 0.5, 0.0, 255.0));
				}
			}
			To[3] = static_cast<uint8_t>(Clamp(Alpha * 255.0 + 0.5, 0.0, 255.0));
		}
		// (where there is no cloud, the colour of the cloud beside it: the chip's smoothing between pixels then draws no
		// dark rim round every cloud's edge)
		for (int Pass = 0; Pass < 2; ++Pass)
		{
			std::vector<uint8_t> Was = Out.Rgba;
			for (int Y = 0; Y < Rows; ++Y)
			{
				for (int X = 0; X < W; ++X)
				{
					const size_t Here = (static_cast<size_t>(Y) * W + X) * 4;
					if (Was[Here] != 0 || Was[Here + 1] != 0 || Was[Here + 2] != 0)
					{
						continue;
					}
					int Found = 0, Sum[3] = { 0, 0, 0 };
					for (int DY = -1; DY <= 1; ++DY)
					{
						for (int DX = -1; DX <= 1; ++DX)
						{
							const int AY = Y + DY;
							if (AY < 0 || AY >= Rows)
							{
								continue;
							}
							const int AX = ((X + DX) % W + W) % W;
							const size_t There = (static_cast<size_t>(AY) * W + AX) * 4;
							if (Was[There] != 0 || Was[There + 1] != 0 || Was[There + 2] != 0)
							{
								++Found;
								for (int J = 0; J < 3; ++J)
								{
									Sum[J] += Was[There + static_cast<size_t>(J)];
								}
							}
						}
					}
					for (int J = 0; Found > 0 && J < 3; ++J)
					{
						Out.Rgba[Here + static_cast<size_t>(J)] = static_cast<uint8_t>(Sum[J] / Found);
					}
				}
			}
		}
	}

	// ---- The shade ------------------------------------------------------------------------------------------------

	namespace
	{
		/** A cumulus shade picture's height: the clouds' own middle band, where the sun's line through each cloud is read. */
		double ShadeZ(const AirSky::Field& F)
		{
			return F.Base + 0.45 * std::max(0.0, F.Top - F.Base);
		}

		/** A cloud as it stands for the shade: its place with the drift, its middle's height, how much is shown. */
		bool ShadeCloud(const AirSky::Field& F, int Index, const Vec& Drift, double& OutX, double& OutY, double& OutHigh, double& OutShow)
		{
			const AirSky::Cloud& C = F.Clouds[static_cast<size_t>(Index)];
			// (as air lays its shadows: 600 m and more across, and no scrap left in a hole)
			if (C.bScrap || C.W < 600.0)
			{
				return false;
			}
			const AirSky::Placed Here = F.Place(Index, F.TargetX, F.TargetY, Drift);
			if (Here.Show <= 0.02 || Here.Squash <= 0.02)
			{
				return false;
			}
			OutX = C.At.X + Here.Shift.X;
			OutY = C.At.Y + Here.Shift.Y;
			OutHigh = C.At.Z + Here.Lift + 0.45 * C.H * Here.Squash;
			OutShow = Here.Show * Here.Squash;
			return true;
		}
	}

	Vec ShadowPlace(const AirSky::Field& F, int CloudIndex, const Vec& Sun, double Z)
	{
		if (CloudIndex < 0 || CloudIndex >= static_cast<int>(F.Clouds.size()))
		{
			return Vec();
		}
		double X = 0.0, Y = 0.0, High = 0.0, Show = 0.0;
		if (!ShadeCloud(F, CloudIndex, Vec(), X, Y, High, Show))
		{
			const AirSky::Cloud& C = F.Clouds[static_cast<size_t>(CloudIndex)];
			X = C.At.X;
			Y = C.At.Y;
			High = C.At.Z + 0.45 * C.H;
		}
		const Vec Way = Sun.Unit(Vec(0.0, 0.0, 1.0));
		const double Along = (Z - High) / std::max(0.05, Way.Z);
		return Vec(X + Way.X * Along, Y + Way.Y * Along, Z);
	}

	AirSky::DeckShade ShadeOf(const AirSky::Field& F, const Vec& Sun, const Vec& Drift, int Side)
	{
		if (F.Sky.bDeck)
		{
			// (a deck's shade is air's own, the same picture byte for byte: one lid, its holes letting the sun through)
			return AirSky::ShadeOf(F, Side);
		}
		AirSky::DeckShade S;
		if (Side <= 0 || F.Clouds.empty())
		{
			return S;
		}
		const Vec Way = Sun.Unit(Vec(0.0, 0.0, 1.0));
		const double Z = ShadeZ(F);
		const double Step = AirSky::Period / Side;
		const size_t N = static_cast<size_t>(Side) * static_cast<size_t>(Side);
		std::vector<double> Through(N, 1.0);
		// (each shadow stretched down-sun as the sun stands lower, as the air painter's shadows are)
		const double Stretch = Clamp(1.0 / std::max(0.05, Way.Z), 1.0, 1.8);
		const Vec Flat = Vec(Way.X, Way.Y, 0.0).Unit(Vec(0.0, -1.0, 0.0));
		const Vec Across = Vec(-Flat.Y, Flat.X, 0.0);
		int Laid = 0;
		for (int Index = 0; Index < static_cast<int>(F.Clouds.size()); ++Index)
		{
			double CX = 0.0, CY = 0.0, High = 0.0, Show = 0.0;
			if (!ShadeCloud(F, Index, Drift, CX, CY, High, Show))
			{
				continue;
			}
			const double Along = (Z - High) / std::max(0.05, Way.Z);
			const double MX = CX + Way.X * Along, MY = CY + Way.Y * Along;
			// (the shadow the breadth of the cloud's own body)
			const double R = 0.42 * F.Clouds[static_cast<size_t>(Index)].W;
			const double Reach = R * Stretch;
			++Laid;
			const int X0 = static_cast<int>(std::floor((MX - Reach - F.Origin.X) / Step)) - 1;
			const int X1 = static_cast<int>(std::ceil((MX + Reach - F.Origin.X) / Step)) + 1;
			const int Y0 = static_cast<int>(std::floor((MY - Reach - F.Origin.Y) / Step)) - 1;
			const int Y1 = static_cast<int>(std::ceil((MY + Reach - F.Origin.Y) / Step)) + 1;
			for (int Y = Y0; Y <= Y1; ++Y)
			{
				for (int X = X0; X <= X1; ++X)
				{
					const double PX = F.Origin.X + (X + 0.5) * Step, PY = F.Origin.Y + (Y + 0.5) * Step;
					const double DX = PX - MX, DY = PY - MY;
					// (along the sun and across it: the shadow an ellipse, longer down-sun)
					const double Down = (DX * Flat.X + DY * Flat.Y) / Stretch, Side2 = DX * Across.X + DY * Across.Y;
					const double Reached = std::hypot(Down, Side2) / std::max(1.0, R);
					// (its heart whole, then softened out to its edge: a cloud's edge is never a line on the sea)
					const double Dark = (1.0 - Smooth((Reached - 0.55) / 0.45)) * Show;
					if (Dark <= 0.0)
					{
						continue;
					}
					const int AX = ((X % Side) + Side) % Side, AY = ((Y % Side) + Side) % Side;
					double& Here = Through[static_cast<size_t>(AY) * Side + AX];
					Here *= 1.0 - (1.0 - CumulusThrough) * Dark;
				}
			}
		}
		if (Laid == 0)
		{
			return S;
		}
		S.Side = Side;
		S.Origin = F.Origin;
		S.Period = AirSky::Period;
		S.Z = Z;
		S.Top = F.Top;
		S.Through = CumulusThrough;
		S.Light.resize(N);
		for (size_t K = 0; K < N; ++K)
		{
			S.Light[K] = static_cast<uint8_t>(Clamp(Through[K] * 255.0 + 0.5, 0.0, 255.0));
		}
		return S;
	}

	// ---- Rain -----------------------------------------------------------------------------------------------------

	Rain RainUnder(const AirSky::Field& F, double Heavy, uint64_t Seed)
	{
		Rain Out;
		Out.Heavy = Clamp(Heavy, 0.0, 1.0);
		const int Most = static_cast<int>(std::lround(MostCurtains * Out.Heavy));
		if (Most <= 0 || F.Clouds.empty())
		{
			return Out;
		}
		// The darkest clouds: the biggest, the tallest and the least lit of those shown whole, between CurtainsFrom and
		// CurtainsTo of the board's middle (the rain at the eye is the weather fall's, and the clouds past the reach are
		// not drawn).
		std::vector<std::pair<double, int>> Dark;
		for (int Index = 0; Index < static_cast<int>(F.Clouds.size()); ++Index)
		{
			const AirSky::Cloud& C = F.Clouds[static_cast<size_t>(Index)];
			if (C.bScrap)
			{
				continue;
			}
			const AirSky::Placed Here = F.Place(Index, F.TargetX, F.TargetY, Vec());
			if (Here.Show < 0.9 || Here.Squash < 0.9)
			{
				continue;
			}
			const double X = C.At.X + Here.Shift.X, Y = C.At.Y + Here.Shift.Y;
			const double Away = std::hypot(X - F.TargetX, Y - F.TargetY);
			if (Away < CurtainsFrom || Away > CurtainsTo)
			{
				continue;
			}
			double Lit = 0.0;
			int Count = 0;
			for (int K = 0; K < C.Count; ++K)
			{
				const size_t At = static_cast<size_t>(C.First + K);
				if (At < F.Puffs.size())
				{
					Lit += F.Puffs[At].Lit;
					++Count;
				}
			}
			const double Mean = Count > 0 ? Lit / Count : 1.0;
			Dark.emplace_back(C.W * C.H * (1.4 - Clamp(Mean, 0.0, 1.0)), Index);
		}
		std::sort(Dark.begin(), Dark.end(), [](const std::pair<double, int>& A, const std::pair<double, int>& B)
		{
			return A.first != B.first ? A.first > B.first : A.second < B.second;
		});
		TheaterFx::Dice Roll(Seed * 0x9E3779B97F4A7C15ull + 0x5EA1);
		const Vec Lee = Vec(F.Wind.X, F.Wind.Y, 0.0).Unit(Vec(1.0, 0.0, 0.0));
		// (down its height: its head inside the base, thickest a little under it, thinning to a mist at the sea; across
		// each of its four crossed sheets, none at the edges: the air battle's shafts)
		static const double Downs[5] = { 0.0, 0.12, 0.45, 0.8, 1.0 };
		static const double Thicks[5] = { 0.55, 1.0, 0.9, 0.65, 0.2 };
		static const double Acrosses[5] = { -1.0, -0.5, 0.0, 0.5, 1.0 };
		static const double Weights[5] = { 0.0, 0.6, 1.0, 0.6, 0.0 };
		// [EST: rain of 1 to 16 mm an hour dims a line through a curtain 1.5 km across by roughly a third to nine tenths;
		// the crossed sheets add to that where they cross]
		const double Peak = 0.1 + 0.2 * Out.Heavy;
		for (const std::pair<double, int>& Each : Dark)
		{
			if (static_cast<int>(Out.Curtains.size()) >= Most)
			{
				break;
			}
			const AirSky::Cloud& C = F.Clouds[static_cast<size_t>(Each.second)];
			const AirSky::Placed Here = F.Place(Each.second, F.TargetX, F.TargetY, Vec());
			const double MX = C.At.X + Here.Shift.X, MY = C.At.Y + Here.Shift.Y;
			bool bNear = false;
			for (const Curtain& Other : Out.Curtains)
			{
				bNear = bNear || std::hypot(MX - Other.Mid.X, MY - Other.Mid.Y) < 5000.0;
			}
			// (as many numbers drawn for every cloud looked at, kept or not, so the laying stays the same where it is kept)
			const double Turned = Roll.U(0.0, 0.5 * Pi);
			if (bNear)
			{
				continue;
			}
			Curtain One;
			One.Cloud = Each.second;
			One.Mid = Vec(MX, MY, 0.0);
			One.Top = C.At.Z + Here.Lift + 100.0;
			One.Foot = 0.0;
			// (a cumulus shower's curtain about a quarter of its cloud across, as air's; a deck's rain a heavier band in
			// the general rain, one and a half tiles across [EST: the bands of heavier rain in frontal rain are a few
			// kilometres across])
			One.Radius = F.Sky.bDeck ? Clamp(1.5 * C.W, 1500.0, 3000.0) : Clamp(0.28 * C.W, 500.0, 2200.0);
			// (the rain lags behind its cloud as it falls into the slower air under it)
			One.Lag = Lee * (-0.22 * (One.Top - One.Foot));
			One.Peak = Peak;
			Out.Curtains.push_back(One);
			for (int Sheet = 0; Sheet < 4; ++Sheet)
			{
				const double Turn = Turned + Sheet * Pi / 4.0;
				const double SX = std::cos(Turn), SY = std::sin(Turn);
				const int First = static_cast<int>(Out.Points.size());
				for (int Row = 0; Row < 5; ++Row)
				{
					const double Down = Downs[Row];
					const double CentreX = MX + One.Lag.X * Down, CentreY = MY + One.Lag.Y * Down;
					const double Wide = One.Radius * (1.0 + 0.25 * Down);
					for (int Col = 0; Col < 5; ++Col)
					{
						Out.Points.emplace_back(CentreX + SX * (Acrosses[Col] * Wide), CentreY + SY * (Acrosses[Col] * Wide),
							One.Top - Down * (One.Top - One.Foot));
						Out.Alpha.push_back(static_cast<uint8_t>(Clamp(255.0 * Peak * Thicks[Row] * Weights[Col] + 0.5, 0.0, 255.0)));
					}
				}
				for (int Row = 0; Row < 4; ++Row)
				{
					for (int Col = 0; Col < 4; ++Col)
					{
						const int A = First + Row * 5 + Col, B = A + 1, Cc = A + 6, D = A + 5;
						// (both faces of every triangle: a curtain is seen from either side)
						const int Six[12] = { A, B, Cc, A, Cc, D, A, Cc, B, A, D, Cc };
						Out.Triangles.insert(Out.Triangles.end(), Six, Six + 12);
					}
				}
			}
		}
		return Out;
	}

	Squall SquallOf(const Rain& R)
	{
		Squall Out;
		if (R.Curtains.empty())
		{
			return Out;
		}
		// (the heaviest: the widest of the curtains, the first laid where they are alike, the clouds coming darkest first)
		const Curtain* Worst = &R.Curtains.front();
		for (const Curtain& One : R.Curtains)
		{
			Worst = One.Radius > Worst->Radius ? &One : Worst;
		}
		Out.bOn = true;
		// (its foot on the sea, lagged downwind of its cloud)
		Out.X = Worst->Mid.X + Worst->Lag.X;
		Out.Y = Worst->Mid.Y + Worst->Lag.Y;
		Out.R = Worst->Radius * 1.25;
		Out.Heavy = R.Heavy;
		return Out;
	}

	// ---- The deck's far sheet -------------------------------------------------------------------------------------

	double FarSheetSolid(const AirSky::Field& F)
	{
		// (an overcast's whole, a broken deck's two thirds or so, so a pale strip can stay at the horizon under it)
		return 0.4 + 0.6 * Smooth(0.6, 1.0, F.Sky.CoverLaid());
	}

	Rgb FarSheetColour(const AirSky::Field& F, double EyeZ)
	{
		// (its colour by where the eye is: over the tops, the deck's top far off, pale and a little shaded; under the
		// base, the base's grey, darker on a closed day; between, the one into the other)
		const double Over = Clamp((EyeZ - F.Base) / std::max(1.0, F.Top - F.Base), 0.0, 1.0);
		const double Closed = Smooth(0.85, 1.0, F.Sky.CoverLaid());
		const Rgb Tops = AirSky::SunSide.Mix(AirSky::ShadeSide, 0.18);
		const Rgb Under = AirSky::BaseSide * (1.0 - 0.15 * Closed);
		return Under.Mix(Tops, Over);
	}
}
