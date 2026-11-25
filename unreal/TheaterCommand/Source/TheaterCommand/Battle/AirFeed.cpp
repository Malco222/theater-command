#include "Battle/AirFeed.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace AirFeed
{
	namespace
	{
		const Json& Nothing()
		{
			static const Json Empty;
			return Empty;
		}

		const std::string& NoText()
		{
			static const std::string Empty;
			return Empty;
		}

		/** A plain recursive reader over the text. */
		struct Reader
		{
			const char* P = nullptr;
			const char* End = nullptr;
			std::string Error;
			int Depth = 0;

			void Space()
			{
				while (P < End && (*P == ' ' || *P == '\t' || *P == '\n' || *P == '\r')) ++P;
			}

			bool Fail(const char* Why)
			{
				if (Error.empty()) Error = Why;
				return false;
			}

			static void PutUtf8(std::string& Out, unsigned Code)
			{
				if (Code < 0x80) Out.push_back(static_cast<char>(Code));
				else if (Code < 0x800)
				{
					Out.push_back(static_cast<char>(0xC0 | (Code >> 6)));
					Out.push_back(static_cast<char>(0x80 | (Code & 0x3F)));
				}
				else if (Code < 0x10000)
				{
					Out.push_back(static_cast<char>(0xE0 | (Code >> 12)));
					Out.push_back(static_cast<char>(0x80 | ((Code >> 6) & 0x3F)));
					Out.push_back(static_cast<char>(0x80 | (Code & 0x3F)));
				}
				else
				{
					Out.push_back(static_cast<char>(0xF0 | (Code >> 18)));
					Out.push_back(static_cast<char>(0x80 | ((Code >> 12) & 0x3F)));
					Out.push_back(static_cast<char>(0x80 | ((Code >> 6) & 0x3F)));
					Out.push_back(static_cast<char>(0x80 | (Code & 0x3F)));
				}
			}

			bool Hex4(unsigned& OutCode)
			{
				if (End - P < 4) return Fail("a short \\u escape");
				OutCode = 0;
				for (int K = 0; K < 4; ++K)
				{
					const char C = P[K];
					OutCode <<= 4;
					if (C >= '0' && C <= '9') OutCode |= static_cast<unsigned>(C - '0');
					else if (C >= 'a' && C <= 'f') OutCode |= static_cast<unsigned>(C - 'a' + 10);
					else if (C >= 'A' && C <= 'F') OutCode |= static_cast<unsigned>(C - 'A' + 10);
					else return Fail("a bad \\u escape");
				}
				P += 4;
				return true;
			}

			bool ReadString(std::string& Out)
			{
				// P is at the opening quote.
				++P;
				Out.clear();
				while (P < End)
				{
					const char C = *P++;
					if (C == '"') return true;
					if (C != '\\') { Out.push_back(C); continue; }
					if (P >= End) break;
					const char Esc = *P++;
					switch (Esc)
					{
					case '"': Out.push_back('"'); break;
					case '\\': Out.push_back('\\'); break;
					case '/': Out.push_back('/'); break;
					case 'b': Out.push_back('\b'); break;
					case 'f': Out.push_back('\f'); break;
					case 'n': Out.push_back('\n'); break;
					case 'r': Out.push_back('\r'); break;
					case 't': Out.push_back('\t'); break;
					case 'u':
					{
						unsigned Code = 0;
						if (!Hex4(Code)) return false;
						if (Code >= 0xD800 && Code <= 0xDBFF && End - P >= 6 && P[0] == '\\' && P[1] == 'u')
						{
							P += 2;
							unsigned Low = 0;
							if (!Hex4(Low)) return false;
							if (Low >= 0xDC00 && Low <= 0xDFFF) Code = 0x10000 + ((Code - 0xD800) << 10) + (Low - 0xDC00);
						}
						PutUtf8(Out, Code);
						break;
					}
					default: return Fail("a bad escape");
					}
				}
				return Fail("a string left open");
			}

			bool ReadValue(Json& Out)
			{
				Space();
				if (P >= End) return Fail("the text ended early");
				if (++Depth > 200) return Fail("nested too deep");
				bool bOk = true;
				const char C = *P;
				if (C == '{')
				{
					Out.Kind = Json::Type::Object;
					++P;
					Space();
					if (P < End && *P == '}') { ++P; }
					else
					{
						for (;;)
						{
							Space();
							if (P >= End || *P != '"') { bOk = Fail("a field without a name"); break; }
							std::string Key;
							if (!ReadString(Key)) { bOk = false; break; }
							Space();
							if (P >= End || *P != ':') { bOk = Fail("a field without a colon"); break; }
							++P;
							Out.Keys.push_back(std::move(Key));
							Out.Values.emplace_back();
							if (!ReadValue(Out.Values.back())) { bOk = false; break; }
							Space();
							if (P < End && *P == ',') { ++P; continue; }
							if (P < End && *P == '}') { ++P; break; }
							bOk = Fail("an object left open");
							break;
						}
					}
				}
				else if (C == '[')
				{
					Out.Kind = Json::Type::Array;
					++P;
					Space();
					if (P < End && *P == ']') { ++P; }
					else
					{
						for (;;)
						{
							Out.Items.emplace_back();
							if (!ReadValue(Out.Items.back())) { bOk = false; break; }
							Space();
							if (P < End && *P == ',') { ++P; continue; }
							if (P < End && *P == ']') { ++P; break; }
							bOk = Fail("an array left open");
							break;
						}
					}
				}
				else if (C == '"')
				{
					Out.Kind = Json::Type::String;
					bOk = ReadString(Out.Text);
				}
				else if (C == 't' && End - P >= 4 && std::strncmp(P, "true", 4) == 0) { Out.Kind = Json::Type::Bool; Out.Flag = true; P += 4; }
				else if (C == 'f' && End - P >= 5 && std::strncmp(P, "false", 5) == 0) { Out.Kind = Json::Type::Bool; Out.Flag = false; P += 5; }
				else if (C == 'n' && End - P >= 4 && std::strncmp(P, "null", 4) == 0) { Out.Kind = Json::Type::Null; P += 4; }
				else if (C == '-' || (C >= '0' && C <= '9'))
				{
					// strtod wants a terminated string: copy the number's characters (never more than a few dozen).
					const char* Start = P;
					while (P < End && (std::strchr("+-0123456789.eE", *P) != nullptr)) ++P;
					std::string Digits(Start, static_cast<size_t>(P - Start));
					char* Stop = nullptr;
					Out.Kind = Json::Type::Number;
					Out.Value = std::strtod(Digits.c_str(), &Stop);
					if (Stop == Digits.c_str()) bOk = Fail("a bad number");
				}
				else bOk = Fail("an unexpected character");
				--Depth;
				return bOk;
			}
		};

		std::string Lower(const std::string& S)
		{
			std::string Out = S;
			for (char& C : Out) if (C >= 'A' && C <= 'Z') C = static_cast<char>(C - 'A' + 'a');
			return Out;
		}

		std::vector<std::pair<double, double>> Ring(const Json& Points)
		{
			std::vector<std::pair<double, double>> Out;
			Out.reserve(Points.Count());
			for (const Json& Pt : Points.Items)
				if (Pt.IsArray() && Pt.Count() >= 2) Out.emplace_back(Pt.At(0).Num(), Pt.At(1).Num());
			return Out;
		}

		AirShow::Loadout LoadoutOf(const Json& L)
		{
			AirShow::Loadout Out;
			Out.Conv = L["conv"].Num(0.0);
			for (const Json& S : L["silent"].Items) if (S.IsString()) Out.Silent.push_back(S.Text);
			const Json& Reach = L["reach"];
			if (Reach.IsObject())
			{
				Out.ReachRifle = Reach["rifle"].Num(Out.ReachRifle);
				Out.ReachHeavy = Reach["hmg"].Num(Out.ReachHeavy);
			}
			for (const Json& G : L["guns"].Items)
			{
				AirShow::Gun Row;
				Row.Kind = G["kind"].Str("rifle");
				Row.Mount = G["mount"].Str("fixed");
				Row.N = std::max(1, G["n"].Int(1));
				Row.Rate = G["rate"].Num(0.0);
				Row.Mv = G["mv"].Num(800.0);
				for (const Json& A : G["at"].Items)
				{
					if (!A.IsString()) continue;
					// A silent muzzle (a turret the mark had not got, a gun taken out that month) never fires.
					bool bSilent = false;
					for (const std::string& S : Out.Silent) if (S == A.Text) bSilent = true;
					if (!bSilent) Row.At.push_back(A.Text);
				}
				if (Row.At.empty() && G["at"].Count() > 0) continue;
				const Json& Arc = G["arc"];
				if (Arc.IsObject())
				{
					Row.bArc = true;
					const Json& Yaw = Arc["yaw"];
					const Json& Pitch = Arc["pitch"];
					if (Yaw.Count() >= 2) { Row.Yaw0 = Yaw.At(0).Num(0.0); Row.Yaw1 = Yaw.At(1).Num(360.0); }
					if (Pitch.Count() >= 2) { Row.Pitch0 = Pitch.At(0).Num(-90.0); Row.Pitch1 = Pitch.At(1).Num(90.0); }
				}
				Out.Guns.push_back(std::move(Row));
			}
			return Out;
		}

		std::vector<int> Ints(const Json& J)
		{
			std::vector<int> Out;
			for (const Json& V : J.Items) if (V.IsNumber()) Out.push_back(V.Int());
			return Out;
		}
	}

	const Json& Json::operator[](const char* Key) const
	{
		if (Kind != Type::Object) return Nothing();
		for (size_t K = 0; K < Keys.size(); ++K)
			if (Keys[K] == Key) return Values[K];
		return Nothing();
	}

	const Json& Json::At(size_t Index) const
	{
		if (Kind != Type::Array || Index >= Items.size()) return Nothing();
		return Items[Index];
	}

	int Json::Int(int Default) const
	{
		if (Kind == Type::String)
		{
			char* Stop = nullptr;
			const long N = std::strtol(Text.c_str(), &Stop, 10);
			return Stop == Text.c_str() ? Default : static_cast<int>(N);
		}
		if (Kind != Type::Number) return Default;
		return static_cast<int>(std::lround(Value));
	}

	const std::string& Json::Str() const
	{
		return Kind == Type::String ? Text : NoText();
	}

	bool Parse(const std::string& Source, Json& OutValue, std::string& OutError)
	{
		Reader R;
		R.P = Source.data();
		R.End = Source.data() + Source.size();
		OutValue = Json();
		if (!R.ReadValue(OutValue))
		{
			OutError = R.Error + " at character " + std::to_string(static_cast<long long>(R.P - Source.data()));
			return false;
		}
		R.Space();
		if (R.P != R.End)
		{
			OutError = "text after the value at character " + std::to_string(static_cast<long long>(R.P - Source.data()));
			return false;
		}
		return true;
	}

	int SideOf(const std::string& Side)
	{
		if (Side == "att") return 0;
		if (Side == "def") return 1;
		return -1;
	}

	AirShow::Act ActOf(const std::string& Act)
	{
		if (Act == "idle") return AirShow::Act::Idle;
		if (Act == "fight") return AirShow::Act::Fight;
		if (Act == "rout") return AirShow::Act::Rout;
		if (Act == "down") return AirShow::Act::Down;
		return AirShow::Act::March;
	}

	bool ReadSky(const Json& J, Sky& Out)
	{
		if (!J.IsObject()) return false;
		Out = Sky();
		Out.W = J["w"].Num(Out.W);
		Out.H = J["h"].Num(Out.H);
		Out.Scale = J["scale"].Num(Out.Scale);
		const Json& Target = J["target"];
		Out.TargetX = Target["x"].Num(Out.TargetX);
		Out.TargetY = Target["y"].Num(Out.TargetY);
		Out.TargetR = Target["r"].Num(Out.TargetR);
		Out.FlakR = J["flakR"].Num(Out.FlakR);
		for (const Json& B : J["bands"].Items) Out.Bands.push_back(B.Num());
		if (Out.Bands.size() < 3) Out.Bands = { 1800.0, 4500.0, 7200.0 };

		const Json& Place = J["place"];
		Out.Title = J["title"].Str(Place["title"].Str("").c_str());
		Out.Date = J["date"].Str("");
		// The sides: the bridge's sky names them (the recorder adds att/def); else the place's nat is the raider's.
		const Json& Att = J["att"];
		const Json& Def = J["def"];
		Out.AttNat = Att["nat"].Str(Place["nat"].Str("").c_str());
		Out.DefNat = Def["nat"].Str("");
		Out.AttAdj = Att["adj"].Str("");
		Out.DefAdj = Def["adj"].Str("");
		Out.bAttHuman = Att["human"].Truth(true);
		Out.bDefHuman = Def["human"].Truth(false);

		Out.PlaceKind = Place["kind"].Str("");
		Out.PlaceName = Place["name"].Str("");
		Out.From = Place["from"].Str("land");
		Out.bNight = Place["night"].Truth(false);
		Out.CardChance = Place["chance"].Num(0.0);
		const Json& When = Place["when"];
		Out.Year = When["year"].Int(Out.Year);
		Out.Month = When["month"].Int(Out.Month);
		Out.FleetName = Place["ships"]["law"]["fleets"].At(0)["name"].Str("");
		const Json& Stage = Place["ships"]["stage"];
		Out.StageKey = Stage["key"].Str("");
		Out.StageAimX = Stage["aim"].At(0).Num(0.0);
		Out.StageAimY = Stage["aim"].At(1).Num(0.0);
		Out.StageTurn = Stage["turn"].Num(0.0);

		const Json& Models = Place["models"];
		for (size_t K = 0; K < Models.Keys.size(); ++K)
		{
			Out.Models[Models.Keys[K] + ":ftr"] = Models.Values[K]["ftr"].Str("");
			Out.Models[Models.Keys[K] + ":bmr"] = Models.Values[K]["bmr"].Str("");
		}
		// The defender's nation where the sky has no def block: the other one the place's models name.
		if (Out.DefNat.empty())
			for (const std::string& Nat : Models.Keys)
				if (Nat != Out.AttNat) { Out.DefNat = Nat; break; }

		const Json& Guns = Place["guns"];
		for (size_t K = 0; K < Guns.Keys.size(); ++K)
		{
			const Json& Nat = Guns.Values[K];
			if (Nat["ftr"].IsObject()) Out.Guns[Guns.Keys[K] + ":ftr"] = LoadoutOf(Nat["ftr"]);
			if (Nat["bmr"].IsObject()) Out.Guns[Guns.Keys[K] + ":bmr"] = LoadoutOf(Nat["bmr"]);
		}

		const Json& Ground = Place["ground"];
		Out.Terrain = Ground["terrain"].Str("");
		for (const Json& R : Ground["land"].Items) Out.Land.push_back(Ring(R));
		// An inland board (place.ground.inland: no coast within it) comes with no land rings, and every "over land" here
		// is a board with land: the Rhineland and Dnepropetrovsk raids flew over open sea (the art director, 24 November).
		// It is one ring of land round the board, wider than anything laid under it (the sea sheet runs 300 km out).
		if (Out.Land.empty() && Ground["inland"].Truth(false))
		{
			const double Wide = 400000.0;
			Out.Land.push_back({ { -Wide, -Wide }, { Out.W + Wide, -Wide }, { Out.W + Wide, Out.H + Wide }, { -Wide, Out.H + Wide } });
		}
		for (const Json& R : Ground["target"].Items) Out.TargetOutline.push_back(Ring(R));
		for (const Json& R : Ground["rivers"].Items) Out.Rivers.push_back(Ring(R));
		const Json& North = Ground["north"];
		if (North.IsArray() || North.IsObject())
		{
			const double NX = North.IsArray() ? North.At(0).Num(0.0) : North["x"].Num(0.0);
			const double NY = North.IsArray() ? North.At(1).Num(1.0) : North["y"].Num(1.0);
			if (std::hypot(NX, NY) > 0.5)
			{
				Out.NorthX = NX / std::hypot(NX, NY);
				Out.NorthY = NY / std::hypot(NX, NY);
				Out.bNorthGiven = true;
			}
		}

		const Json& Lat = Place["lat"];
		if (Lat.IsNumber() && std::fabs(Lat.Num(0.0)) <= 90.0)
		{
			Out.Latitude = Lat.Num(0.0);
		}

		// The day's sky (sky.ts): kept as given; a bundle from before it has none, and cumulus stands in.
		const Json& Cloud = Place["cloud"];
		if (Cloud.IsObject())
		{
			Out.Cloud.bGiven = true;
			Out.Cloud.bDeck = Cloud["kind"].Str("cumulus") == "deck";
			Out.Cloud.Base = Cloud["base"].Num(Out.Cloud.Base);
			Out.Cloud.Top = Cloud["top"].Num(Out.Cloud.Top);
			Out.Cloud.Chance = Cloud["chance"].Num(0.0);
			Out.Cloud.Zone = Cloud["zone"].Str("");
			// (how much of the sky is cloud, and its word: sky.ts's cover, from 20 November; none before it)
			const Json& Cover = Cloud["cover"];
			Out.Cloud.Cover = Cover.IsNumber() && std::isfinite(Cover.Num(0.0)) ? std::max(0.0, std::min(1.0, Cover.Num(0.0))) : -1.0;
			Out.Cloud.Word = Cloud["sky"].Str("");
		}
		Out.TrailM = TrailHeight(Out.Cloud.Zone, Out.Month, Out.Latitude);
		return true;
	}

	double TrailHeight(const std::string& Zone, int Month, double Latitude)
	{
		// The day's vapour-trail height (the audit's 4.12): a fixed table by the weather zone and the month (from 0), so
		// that the same battle draws the same trails every time, never a draw of its own. The record: the Eighth's
		// bombers over north Germany in winter trailed from about 15,000 feet (4,600 m), in summer from about 25,000;
		// in the subtropics no trail forms under about 30,000 feet. (Northern seasons; the war's southern battles are
		// all in the tropics' row.)
		const bool bKnown = std::isfinite(Latitude);
		const bool bTropic = Zone.find("tropical") != std::string::npos || (bKnown && std::fabs(Latitude) < 30.0);
		const bool bNorth = Zone.find("Europe") != std::string::npos || (bKnown && std::fabs(Latitude) >= 45.0);
		const int M = ((Month % 12) + 12) % 12;
		const bool bWinter = M <= 2 || M >= 10, bSummer = M >= 4 && M <= 8;
		if (bTropic) return 9000.0;
		if (bNorth) return bWinter ? 4600.0 : bSummer ? 7600.0 : 6100.0;
		return bWinter ? 6000.0 : bSummer ? 8500.0 : 7300.0;
	}

	AirSky::Placing PlacingOf(const Sky& S, const std::vector<AirShow::Group>& First, uint64_t Seed)
	{
		AirSky::Placing P;
		// (a live battle's sky is handed no seed: its title, date and place stand in, the same for the same battle)
		uint64_t Named = 1469598103934665603ull;
		for (const char Ch : S.Title + "|" + S.Date + "|" + S.PlaceName)
		{
			Named ^= static_cast<unsigned char>(Ch);
			Named *= 1099511628211ull;
		}
		P.Seed = Seed ? Seed : Named;
		P.TargetX = S.TargetX;
		P.TargetY = S.TargetY;
		P.bShips = S.PlaceKind == "harbour" || S.PlaceKind == "fleet";
		// (the deck keeps 1.5 to 2.5 km under the lowest of the raider's bombers above 2,000 m, and over any at 2,000 m
		// or lower: the level and torpedo bombers at Pearl Harbor and Midway, not the dive bombers over them)
		double Band = 0.0, Low = 0.0;
		for (const AirShow::Group& G : First)
		{
			if (G.Side != 0 || G.bFighter || G.Alt <= 0.0) continue;
			if (G.Alt > 2000.0) Band = Band > 0.0 ? std::min(Band, G.Alt) : G.Alt;
			else Low = std::max(Low, G.Alt);
		}
		if (Band <= 0.0 && Low <= 0.0)
		{
			const double Middle = S.Bands.size() > 1 ? S.Bands[1] : 4500.0;
			(Middle > 2000.0 ? Band : Low) = Middle;
		}
		P.Band = Band;
		P.LowBand = Low;
		return P;
	}

	void Apply(const Sky& S, AirShow::Show& Show)
	{
		Show.SetBoard(S.TargetX, S.TargetY, S.FlakR, S.Scale);
		Show.TargetR = S.TargetR;
		Show.Year = S.Year;
		Show.Month = S.Month + 1; // (the place's month counts from 0, as the rules' calendar does)
		Show.TrailM = S.TrailM;
		for (const auto& Pair : S.Guns)
		{
			const size_t Colon = Pair.first.find(':');
			if (Colon == std::string::npos) continue;
			Show.SetLoadout(Pair.first.substr(0, Colon), Pair.first.substr(Colon + 1) == "ftr", Pair.second);
		}
	}

	bool ReadView(const Json& J, const Sky& S, Frame& Out)
	{
		if (!J.IsObject()) return false;
		Out = Frame();
		Top& H = Out.Head;
		H.T = J["t"].Num();
		H.Limit = J["limit"].Num();
		H.Speed = J["speed"].Num(1.0);
		H.Me = J["me"].Str("att");
		H.BalanceAtt = J["balance"]["att"].Num(0.5);
		H.BreakingAtt = J["breaking"]["att"].Num(0.0);
		H.BreakingDef = J["breaking"]["def"].Num(0.0);
		H.Contact = J["contact"].IsNumber() ? J["contact"].Num() : -1.0;
		const Json& Over = J["over"];
		if (Over.IsObject())
		{
			H.bOver = true;
			H.Winner = Over["winner"].Str("");
			H.Level = Over["level"].Str("");
			for (const Json& L : Over["lines"].Items) if (L.IsString()) H.OverLines.push_back(L.Text);
		}
		const Json& SkyNow = J["sky"];
		if (SkyNow.IsObject())
		{
			H.Chance = SkyNow["chance"].Num(0.0);
			H.bFired = SkyNow["fired"].Truth(false);
			H.bWinding = SkyNow["winding"].Truth(false);
			H.NearAtt.At = SkyNow["near"]["att"]["at"].Num(H.BreakingAtt);
			H.NearAtt.Why = SkyNow["near"]["att"]["why"].Str("");
			H.NearDef.At = SkyNow["near"]["def"]["at"].Num(H.BreakingDef);
			H.NearDef.Why = SkyNow["near"]["def"]["why"].Str("");
			const Json& Score = SkyNow["score"];
			H.LostAtt = Score["lost"]["att"].Int();
			H.LostDef = Score["lost"]["def"].Int();
			H.UpAtt = Score["up"]["att"].Int();
			H.UpDef = Score["up"]["def"].Int();
			H.Worth = Score["worth"].Num();
			H.Ratio = Score["ratio"].Num(1.0);
			H.bCut = Score["cut"].Truth(false);
			H.bRuined = Score["ruined"].Truth(false);
			const Json& Called = SkyNow["called"];
			for (size_t K = 0; K < Called.Keys.size(); ++K)
				H.Called[std::atoi(Called.Keys[K].c_str())] = Called.Values[K].Str();
			if (SkyNow["me"].IsString()) H.Me = SkyNow["me"].Text;
		}
		else
		{
			H.NearAtt.At = H.BreakingAtt;
			H.NearDef.At = H.BreakingDef;
		}

		for (const Json& U : J["units"].Items)
		{
			const Json& X = U["extra"];
			AirShow::Group G;
			G.Id = U["id"].Int();
			G.Side = SideOf(U["side"].Str());
			if (G.Side < 0) continue;
			const std::string Kind = U["kind"].Str("ftr");
			G.bFighter = Kind != "bmr";
			G.Name = U["name"].Str("");
			G.Nation = G.Side == 0 ? S.AttNat : S.DefNat;
			G.Model = X["model"].Str("");
			G.Cool = X["cool"].Str("");
			if (G.Model.empty())
			{
				G.bModelGuessed = true;
				auto M = S.Models.find(G.Nation + (G.bFighter ? ":ftr" : ":bmr"));
				if (M != S.Models.end()) G.Model = M->second;
			}
			G.State = U["state"].Str("steady");
			G.Form = X["form"].Str(G.bFighter ? "fours" : "box");
			G.Style = X["style"].Str("");
			G.Box = X["box"].Str("tight");
			G.Role = X["role"].Str("");
			G.Strike = X["strike"].Str("");
			G.Job = X["job"].Str("");
			G.X = U["x"].Num();
			G.Y = U["y"].Num();
			// (the height drawn: the battle's drawAlt where it gives one, else its own band; the band kept as CountAlt)
			G.CountAlt = X["alt"].Num(X["drawAlt"].Num(0.0));
			G.Alt = X["drawAlt"].Num(G.CountAlt);
			G.Face = U["face"].Num();
			G.W = U["w"].Num(400.0);
			G.D = U["d"].Num(300.0);
			G.Speed = U["speed"].Num(0.0);
			G.Ammo = X["ammo"].Num(1.0);
			// The pilots' brief (shown, never counted): whom the group is making for, the box an escort guards, its skill.
			G.Quarry = X["quarry"].Int(-1);
			G.With = X["with"].Int(-1);
			G.Skill = X["skill"].Num(-1.0);
			G.Run = X["run"].Num(0.0);
			G.Figures = U["figures"].Int();
			G.Figures0 = std::max(G.Figures, U["figures0"].Int(G.Figures));
			G.Per = std::max(1, X["per"].Int(1));
			G.N = X["n"].Int(G.Figures * G.Per);
			G.Boxes = X["boxes"].Int(0);
			G.Doing = ActOf(U["act"].Str("march"));
			G.Engaged = U["engaged"].Int(-1);
			for (const Json& F : X["fire"].Items)
			{
				AirShow::Fire Row;
				Row.On = F["on"].Int(-1);
				Row.Rate = F["rate"].Num(0.0);
				Row.Pass = F["pass"].Str("");
				// The beat of the passes on a box: going in, or standing off, and the air seconds of it left.
				Row.Go = F["go"].Num(-1.0);
				Row.Wait = F["wait"].Num(-1.0);
				G.Firing.push_back(Row);
			}
			for (const Json& St : X["strag"].Items)
			{
				AirShow::Strag Row;
				Row.Id = St["id"].Int();
				Row.Eng = Ints(St["eng"]);
				Row.bFeather = St["prop"].Str("feather") == "feather";
				Row.bFire = St["fire"].Truth(false);
				Row.Cover = St["cover"].Int(-1);
				Row.Back = St["back"].Num(0.0);
				Row.Down = St["down"].Num(0.0);
				Row.bShown = St["model"].Int(1) != 0;
				G.Strags.push_back(std::move(Row));
			}
			for (const Json& Fl : X["flak"].Items) if (Fl.IsString()) G.Flak.push_back(Fl.Text);
			if (const Json& Ace = X["ace"]; Ace.IsObject())
			{
				G.AceName = Ace["name"].Str("");
				G.AceKills = std::max(0, Ace["kills"].Int(0));
				const Json& By = Ace["by"];
				for (size_t K = 0; K < By.Keys.size() && K < By.Values.size(); ++K)
					if (const int V = By.Values[K].Int(0); V > 0) G.AceBy[By.Keys[K]] = V;
			}

			Label L;
			L.Id = G.Id;
			L.Side = G.Side;
			L.Name = G.Name;
			L.State = G.State;
			L.Act = U["act"].Str("march");
			L.Job = G.Job;
			L.Order = U["order"].IsObject() ? U["order"]["do"].Str("") : U["order"].Str("");
			L.N = G.N;
			L.N0 = X["n0"].Int(G.N);
			L.Figures = G.Figures;
			L.Ammo = G.Ammo;
			L.Fuel = X["fuel"].Num(1.0);
			L.Band = X["band"].Int(1);
			Out.Labels.push_back(std::move(L));
			Out.Groups.push_back(std::move(G));
		}

		for (const Json& E : J["events"].Items)
		{
			const Json& X = E["extra"];
			AirShow::Event Ev;
			Ev.T = E["t"].Num(H.T);
			Ev.What = E["what"].Str("");
			Ev.Cue = E["cue"].Str("");
			Ev.Text = E["text"].Str("");
			Ev.ChuteCue = X["chuteCue"].Str("");
			Ev.Unit = E["unit"].Int(-1);
			Ev.Side = SideOf(E["side"].Str(X["side"].Str("").c_str()));
			Ev.bAt = E["x"].IsNumber();
			Ev.X = E["x"].Num(X["x"].Num(0.0));
			Ev.Y = E["y"].Num(X["y"].Num(0.0));
			Ev.Kind = X["kind"].Str("");
			Ev.Chutes = X["chutes"].Int(0);
			Ev.Alt = X["alt"].Num(-1.0);
			const Json& By = X["by"];
			Ev.bByFlak = By.IsString() && By.Text == "flak";
			Ev.By = By.IsNumber() ? By.Int() : -1;
			// The straggler's id: air:lost names it as strag; air:straggle and air:limped as id.
			Ev.Strag = X["strag"].IsNumber() ? X["strag"].Int()
			         : (Ev.What == "air:straggle" || Ev.What == "air:limped" || Ev.What == "air:fireout" || Ev.What == "air:regain") ? X["id"].Int(-1) : -1;
			Ev.Eng = Ints(X["eng"]);
			Ev.With = X["with"].Int(X["on"].Int(-1));
			Ev.Pass = X["pass"].Str("");
			Ev.Ship = X["ship"].Int(-1);
			Ev.How = X["how"].Str("");
			Ev.Strike = X["strike"].Str("");
			Ev.bBig = X["big"].Truth(false);
			Ev.Name = X["name"].Str("");
			// The way down and the hits survived (the air rules' fates, 20 November).
			Ev.Fid = X["fid"].Int(-1);
			Ev.Cause = X["cause"].Str("");
			const std::string Hand = X["side"].Str("");
			Ev.Hand = Hand == "port" ? -1 : Hand == "starboard" ? 1 : 0;
			Ev.Roll = X["roll"].Int(0);
			Ev.Look = X["look"].Int(0);
			Ev.EngineAt = X["engineAt"].Num(-1.0);
			Ev.FireAt = X["fire"].IsNumber() ? X["fire"].Num() : -1.0;
			Ev.bFire = X["fire"].Truth(false);
			Ev.bFeather = X["prop"].Str("") == "feather";
			Ev.Boom = X["boom"].Num(-1.0);
			Ev.Dur = X["dur"].Num(-1.0);
			Ev.bDitch = X["ditch"].Truth(false);
			Ev.Out = X["out"].Int(0);
			for (const Json& At : X["chuteAt"].Items) Ev.ChuteAt.push_back(At.Num(0.0));
			Ev.Mark = X["mark"].Str("");
			Ev.For = X["for"].Num(0.0);
			Ev.Slot = X["slot"].Int(-1);
			// A fighter that fell to a box's gunners: that box.
			Ev.GunBox = X["gunner"].IsObject() ? X["gunner"]["box"].Int(-1) : -1;
			Ev.GunSeat = X["gunner"].IsObject() ? X["gunner"]["seat"].Int(-1) : -1;
			// Where the battle has it hit (engine, wing, tail, cockpit, fuselage): the gunnery lays its strikes there.
			Ev.Part = X["hit"].Str("");
			// The models it moves (AR1): a loss's models, a straggler's model (1 where the recording does not say).
			Ev.Models = Ev.What == "air:lost" ? X["models"].Int(1) : Ev.What == "air:straggle" ? X["model"].Int(1) : 1;
			Out.Events.push_back(std::move(Ev));
		}

		// view.ships: the land diff's pass-through of the sim's ships ({ guns, pieces }), or a plain list.
		const Json& Ships = J["ships"];
		const Json& Pieces = Ships.IsObject() ? Ships["pieces"] : Ships;
		for (const Json& P : Pieces.Items)
		{
			ShipRow Row;
			Row.Id = P["id"].Int();
			Row.Name = P["name"].Str("");
			Row.Class = Lower(P["cls"].Str(P["class"].Str("").c_str()));
			Row.Nation = P["nat"].Str("");
			Row.Berth = P["berth"].Str("");
			Row.X = P["x"].Num();
			Row.Y = P["y"].Num();
			Row.Face = P["face"].Num();
			Row.Hits = P["hits"].Int(0);
			Row.bDock = P["dock"].Truth(false);
			const Json& Sunk = P["sunk"];
			Row.bSunk = !Sunk.IsNull() && !(Sunk.Kind == Json::Type::Bool && !Sunk.Flag);
			Row.SunkHow = Sunk.IsString() ? Sunk.Text : Sunk.IsObject() ? Sunk["how"].Str("") : std::string();

			AirShow::Ship Ship;
			Ship.Id = Row.Id;
			Ship.At = AirShow::Vec(Row.X, Row.Y, 0.0);
			Ship.Face = Row.Face;
			Ship.Class = Row.Class;
			Ship.bSunk = Row.bSunk;
			// (for her anti-aircraft guns: whose she is, her size, her side, the share of her guns manned)
			Ship.Nation = Row.Nation;
			Ship.Name = Row.Name;
			Ship.Side = !S.AttNat.empty() && Row.Nation == S.AttNat ? 0 : 1;
			const HullSize Size = HullOf(Row.Class, Row.Name, Row.Nation);
			Ship.Long = Size.Long;
			Ship.Beam = Size.Beam;
			Ship.Manned = Ships.IsObject() && Ships["guns"].IsNumber() ? std::clamp(Ships["guns"].Num(), 0.0, 1.0) : 1.0;
			Out.Ships.push_back(Ship);
			Out.ShipRows.push_back(std::move(Row));
		}
		return true;
	}

	bool ReadAfter(const Json& J, After& Out)
	{
		if (!J.IsObject()) return false;
		Out = After();
		for (const Json& L : J["lines"].Items) if (L.IsString()) Out.Lines.push_back(L.Text);
		Out.Level = J["level"].Str("");
		Out.Reckoning = J["reckoning"].Str("");
		for (const Json& G : J["groups"].Items)
		{
			AfterRow Row;
			Row.Name = G["name"].Str("");
			Row.Side = G["side"].Str("");
			Row.Kind = G["kind"].Str(G["bmr"].Truth(false) ? "bmr" : "ftr");
			Row.Words = G["words"].Str("");
			Row.Flew = G["flew"].Int(0);
			Row.Lost = G["lost"].IsNumber() ? G["lost"].Int(0) : G["lostFtr"].Int(0) + G["lostBmr"].Int(0);
			Out.Groups.push_back(std::move(Row));
		}
		// Each side's fighters and bombers lost: the card's own totals (after.lost, the rules' afterLost, the sums of its
		// rows); a card from before them (no lost block), the same sums made here from its rows.
		const Json& Lost = J["lost"];
		if (Lost.IsObject())
		{
			Out.LostAttFtr = Lost["att"]["ftr"].Int();
			Out.LostAttBmr = Lost["att"]["bmr"].Int();
			Out.LostDefFtr = Lost["def"]["ftr"].Int();
			Out.LostDefBmr = Lost["def"]["bmr"].Int();
			return true;
		}
		for (const Json& G : J["groups"].Items)
		{
			const std::string Side = G["side"].Str("");
			if (Side != "att" && Side != "def") continue;
			int Ftr = G["lostFtr"].Int(0), Bmr = G["lostBmr"].Int(0);
			// (a row that gives one lost, by its kind)
			if (!G["lostFtr"].IsNumber() && !G["lostBmr"].IsNumber() && G["lost"].IsNumber())
				(G["kind"].Str(G["bmr"].Truth(false) ? "bmr" : "ftr") == "bmr" ? Bmr : Ftr) = G["lost"].Int(0);
			(Side == "att" ? Out.LostAttFtr : Out.LostDefFtr) += Ftr;
			(Side == "att" ? Out.LostAttBmr : Out.LostDefBmr) += Bmr;
		}
		return true;
	}

	bool ReadRecording(const std::string& Source, Recording& Out, std::string& OutError)
	{
		Json Root;
		if (!Parse(Source, Root, OutError)) return false;
		if (!Root["recording"].Truth(false))
		{
			OutError = "not a recorded air battle (no recording mark)";
			return false;
		}
		Out = Recording();
		Out.Key = Root["key"].Str("");
		Out.Seed = static_cast<uint64_t>(static_cast<int64_t>(Root["seed"].Num(0.0))); // (through a signed number: a negative seed is the same on every chip)
		if (!ReadSky(Root["sky"], Out.Board))
		{
			OutError = "the recording has no sky";
			return false;
		}
		Out.Views.reserve(Root["views"].Count());
		for (const Json& V : Root["views"].Items)
		{
			Frame F;
			if (ReadView(V, Out.Board, F)) Out.Views.push_back(std::move(F));
		}
		// A view that named no model for a group (the first, written before the battle's first second) takes the model
		// the next view to name one gives it: every aircraft is made from the first view, and the place's main bomber
		// standing in turned torpedo and level bombers into dive bombers (the pre-check, 06:08Z).
		for (size_t K = 0; K < Out.Views.size(); ++K)
		{
			for (AirShow::Group& G : Out.Views[K].Groups)
			{
				if (!G.bModelGuessed) continue;
				for (size_t Later = K + 1; Later < Out.Views.size() && G.bModelGuessed; ++Later)
				{
					for (const AirShow::Group& N : Out.Views[Later].Groups)
					{
						if (N.Id == G.Id && !N.bModelGuessed && !N.Model.empty())
						{
							G.Model = N.Model;
							G.bModelGuessed = false;
							break;
						}
					}
				}
			}
		}
		ReadAfter(Root["after"], Out.Card);
		if (Out.Views.empty())
		{
			OutError = "the recording has no views";
			return false;
		}
		return true;
	}

	HullSize HullOf(const std::string& Class, const std::string& Name, const std::string& Nation)
	{
		// (length overall and beam, metres; Nevada, Oklahoma, Arizona and Pennsylvania with the bulges of their refits)
		static const struct { const char* Name; double Long, Beam; } Named[] = {
			{ "Nevada", 177.7, 32.9 }, { "Oklahoma", 177.7, 32.9 }, { "Arizona", 185.3, 32.4 }, { "Pennsylvania", 185.3, 32.4 },
			{ "Tennessee", 190.3, 29.7 }, { "California", 190.3, 29.7 }, { "Maryland", 190.3, 29.7 }, { "West Virginia", 190.3, 29.7 },
			{ "Utah", 158.1, 32.3 }, { "Helena", 185.4, 18.8 }, { "Honolulu", 185.4, 18.8 }, { "St. Louis", 185.4, 18.8 },
			{ "San Francisco", 179.2, 18.8 }, { "Cassin", 104.0, 10.6 }, { "Downes", 104.0, 10.6 }, { "Shaw", 104.0, 10.6 },
			{ "Tautog", 93.6, 8.3 }, { "Narwhal", 113.1, 10.1 },
			{ "Akagi", 260.7, 31.3 }, { "Kaga", 247.6, 32.5 }, { "Hiryu", 227.4, 22.3 }, { "Soryu", 227.5, 21.3 },
			{ "Haruna", 222.0, 31.0 }, { "Kirishima", 222.0, 31.0 }, { "Tone", 201.6, 19.4 }, { "Chikuma", 201.6, 19.4 },
		};
		for (const auto& Ship : Named)
		{
			if (Name == Ship.Name) return { Ship.Long, Ship.Beam };
		}
		const bool bJapan = Nation == "JAP" || Nation == "JPN";
		if (Class == "bb" || Class == "ag") return bJapan ? HullSize{ 222.0, 31.0 } : HullSize{ 190.0, 32.0 };
		if (Class == "cv") return bJapan ? HullSize{ 250.0, 30.0 } : HullSize{ 247.0, 28.0 };
		if (Class == "dd") return bJapan ? HullSize{ 118.5, 10.8 } : HullSize{ 104.0, 10.6 };
		if (Class == "ss") return { 95.0, 8.3 };
		return bJapan ? HullSize{ 201.6, 19.4 } : HullSize{ 185.0, 19.0 };
	}
}
