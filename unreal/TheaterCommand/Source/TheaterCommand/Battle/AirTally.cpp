#include "Battle/AirTally.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

namespace AirTally
{
	std::string VictimSymbol(const std::string& Nation)
	{
		static const std::map<std::string, std::string> Victim = {
			{ "GER", "KILL-GER" }, { "JAP", "KILL-JAP" }, { "ITA", "KILL-ITA" }, { "SOV", "KILL-SOV" }, { "USA", "KILL-USA" },
			{ "UK", "KILL-UK" }, { "CAN", "KILL-UK" }, { "AUS", "KILL-UK" }, { "NZL", "KILL-UK" }, { "SAF", "KILL-UK" },
			{ "IND", "KILL-UK" }, { "FRA", "KILL-FRA" }, { "VIC", "KILL-FRA" }, { "FFR", "KILL-FRA" },
			// (the battle's own spellings of two of them, where a recording uses them)
			{ "JPN", "KILL-JAP" }, { "GBR", "KILL-UK" },
		};
		const auto Found = Victim.find(Nation);
		return Found == Victim.end() ? std::string("KILL-PLANE") : Found->second;
	}

	std::string OwnSymbol(const std::string& Nation)
	{
		if (Nation == "SOV") return "KILL-SOV";
		if (Nation == "JAP" || Nation == "JPN") return "KILL-BLOSSOM";
		return "";
	}

	namespace
	{
		using Row = std::vector<std::pair<std::string, int>>;

		int Width(const Row& R)
		{
			int N = 0;
			for (const auto& One : R) N += One.second;
			return N + static_cast<int>(R.size()) - 1;
		}

		Row Joined(const Row& A, const Row& B)
		{
			Row Out = A;
			Out.insert(Out.end(), B.begin(), B.end());
			return Out;
		}
	}

	std::vector<std::vector<std::string>> KillRowsOf(const std::vector<std::pair<std::string, int>>& Groups, int Across)
	{
		Across = std::max(1, Across);
		std::vector<Row> Rows;
		for (const auto& [Tag, N] : Groups)
			for (int K = 0; K < N; K += Across) Rows.push_back(Row{ { Tag, std::min(Across, N - K) } });
		while (static_cast<int>(Rows.size()) > KillRows)
		{
			bool bJoined = false;
			// (neighbours from the end: the smallest share first)
			for (int I = static_cast<int>(Rows.size()) - 2; I >= 0; --I)
			{
				const Row Both = Joined(Rows[I], Rows[I + 1]);
				if (Width(Both) <= Across)
				{
					Rows[I] = Both;
					Rows.erase(Rows.begin() + I + 1);
					bJoined = true;
					break;
				}
			}
			if (bJoined) continue;
			// (no neighbours fit: the rows past the third go back singly, the largest first, ties in their order)
			Row Rest;
			for (size_t R = KillRows; R < Rows.size(); ++R) Rest.insert(Rest.end(), Rows[R].begin(), Rows[R].end());
			Rows.resize(KillRows);
			std::stable_sort(Rest.begin(), Rest.end(), [](const auto& A, const auto& B) { return A.second > B.second; });
			for (const auto& One : Rest)
			{
				int Best = -1, BestLeft = 0;
				for (int J = 0; J < static_cast<int>(Rows.size()); ++J)
				{
					const int Left = Across - Width(Rows[J]) - 1 - One.second;
					if (Left < 0) continue;
					// (the row it leaves least room in; ties the later row)
					if (Best < 0 || Left < BestLeft || (Left == BestLeft && J > Best))
					{
						Best = J;
						BestLeft = Left;
					}
				}
				if (Best >= 0) Rows[Best].push_back(One);
			}
		}
		std::vector<std::vector<std::string>> Out;
		for (const Row& R : Rows)
		{
			std::vector<std::string> Line;
			for (const auto& [Tag, N] : R)
			{
				if (!Line.empty()) Line.push_back("");
				for (int K = 0; K < N; ++K) Line.push_back(Tag);
			}
			Out.push_back(Line);
		}
		return Out;
	}

	std::vector<std::string> KillsInOrder(const std::string& Nation, int Kills, const std::map<std::string, int>& By, int Across)
	{
		Across = std::max(1, Across);
		std::vector<std::string> Out;
		if (Kills <= 0) return Out;
		std::vector<std::pair<std::string, int>> Groups;
		const std::string Own = OwnSymbol(Nation);
		if (!Own.empty())
		{
			Groups.push_back({ Own, Kills });
		}
		else
		{
			std::map<std::string, int> Counts;
			int Recorded = 0;
			for (const auto& [Victim, N] : By)
			{
				if (N <= 0) continue;
				Counts[VictimSymbol(Victim)] += N;
				Recorded += N;
			}
			const int Rest = Kills - Recorded;
			if (Rest > 0)
			{
				// (the unrecorded join the commonest group, ties by name; plain aircraft when none is recorded)
				std::string Top = "KILL-PLANE";
				int TopN = -1;
				for (const auto& [Tag, N] : Counts)
					if (N > TopN) { Top = Tag; TopN = N; }
				Counts[Top] += Rest;
			}
			for (const auto& One : Counts) Groups.push_back(One);
			std::stable_sort(Groups.begin(), Groups.end(), [](const auto& A, const auto& B) { return A.second != B.second ? A.second > B.second : A.first < B.first; });
		}
		for (const std::vector<std::string>& R : KillRowsOf(Groups, Across))
		{
			while (Out.size() % static_cast<size_t>(Across) != 0) Out.push_back("");
			Out.insert(Out.end(), R.begin(), R.end());
		}
		return Out;
	}

	const std::vector<unsigned>& Tins()
	{
		static const std::vector<unsigned> Made = { 0xD4AA30u, 0xDCB443u, 0xC99F2Au, 0xD8B03Au };
		return Made;
	}

	std::vector<Mark> Fill(const std::string& Nation, int Missions, int Kills, const std::map<std::string, int>& By,
		int MissionSlots, int KillSlots, int Seat, int KillsAcross)
	{
		KillsAcross = std::max(1, KillsAcross);
		std::vector<Mark> Out;
		const int Flown = std::max(0, std::min(Missions, MissionSlots));
		const int TinCount = static_cast<int>(Tins().size());
		for (int K = 0; K < Flown; ++K)
		{
			Mark M;
			M.Slot = K;
			M.Row = K / PerRow;
			M.Tag = "BOMB";
			const int Rows = (Flown - 1) / PerRow + 1;
			M.Fade = Fade + (1.0 - Fade) * (Flown > 1 ? static_cast<double>(K) / (Flown - 1) : 1.0);
			M.Tin = (((Rows - 1 - M.Row + Seat) % TinCount) + TinCount) % TinCount;
			Out.push_back(M);
		}
		const std::vector<std::string> Tags = KillsInOrder(Nation, Kills, By, KillsAcross);
		const size_t Across = static_cast<size_t>(KillsAcross);
		for (size_t Row0 = 0; Row0 < Tags.size(); Row0 += Across)
		{
			std::vector<int> Filled;
			for (size_t K = Row0; K < std::min(Tags.size(), Row0 + Across); ++K)
				if (!Tags[K].empty()) Filled.push_back(static_cast<int>(K));
			for (size_t J = 0; J < Filled.size(); ++J)
			{
				const int K = Filled[J];
				if (K >= KillSlots) continue;
				Mark M;
				M.bKill = true;
				M.Slot = K;
				M.Row = K / KillsAcross;
				M.Tag = Tags[static_cast<size_t>(K)];
				M.Fade = KillFade + (1.0 - KillFade) * (Filled.size() > 1 ? static_cast<double>(J) / (Filled.size() - 1) : 1.0);
				Out.push_back(M);
			}
		}
		return Out;
	}
	// ---- An ace's name -------------------------------------------------------------------------------------------

	namespace
	{
		/** The characters of a UTF-8 string (a bad byte read as itself). */
		std::vector<char32_t> Decode(const std::string& Text)
		{
			std::vector<char32_t> Out;
			for (size_t K = 0; K < Text.size();)
			{
				const unsigned char C = static_cast<unsigned char>(Text[K]);
				int More = C >= 0xF0 ? 3 : C >= 0xE0 ? 2 : C >= 0xC0 ? 1 : 0;
				char32_t Point = More == 3 ? (C & 0x07u) : More == 2 ? (C & 0x0Fu) : More == 1 ? (C & 0x1Fu) : C;
				if (More > 0 && K + static_cast<size_t>(More) >= Text.size())
				{
					More = 0;
					Point = C;
				}
				for (int J = 1; J <= More; ++J) Point = (Point << 6) | (static_cast<unsigned char>(Text[K + static_cast<size_t>(J)]) & 0x3Fu);
				Out.push_back(Point);
				K += 1 + static_cast<size_t>(More);
			}
			return Out;
		}

		std::string Encode(char32_t C)
		{
			std::string Out;
			if (C < 0x80) Out += static_cast<char>(C);
			else if (C < 0x800) { Out += static_cast<char>(0xC0 | (C >> 6)); Out += static_cast<char>(0x80 | (C & 0x3F)); }
			else if (C < 0x10000) { Out += static_cast<char>(0xE0 | (C >> 12)); Out += static_cast<char>(0x80 | ((C >> 6) & 0x3F)); Out += static_cast<char>(0x80 | (C & 0x3F)); }
			else { Out += static_cast<char>(0xF0 | (C >> 18)); Out += static_cast<char>(0x80 | ((C >> 12) & 0x3F)); Out += static_cast<char>(0x80 | ((C >> 6) & 0x3F)); Out += static_cast<char>(0x80 | (C & 0x3F)); }
			return Out;
		}

		/** A letter's capital (Latin-1 and Latin Extended-A, Romanian's comma letters; the rules' names use no others). */
		std::string Capital(char32_t C)
		{
			if (C >= 'a' && C <= 'z') return Encode(C - 32);
			if (C == 0xDF) return "SS";
			if (C >= 0xE0 && C <= 0xFE && C != 0xF7) return Encode(C - 32);
			if (C == 0xFF) return Encode(0x178);
			if (C == 0x131) return "I";
			if ((C >= 0x100 && C <= 0x137) || (C >= 0x14A && C <= 0x177)) return Encode(C & ~static_cast<char32_t>(1));
			if ((C >= 0x139 && C <= 0x148) || (C >= 0x179 && C <= 0x17E)) return Encode((C & 1) ? C : C - 1);
			if (C == 0x219 || C == 0x21B) return Encode(C - 1);
			return Encode(C);
		}

		std::string Capitals(const std::string& Word)
		{
			std::string Out;
			for (char32_t C : Decode(Word)) Out += Capital(C);
			return Out;
		}

		std::vector<std::string> Words(const std::string& Text)
		{
			std::vector<std::string> Out;
			std::string One;
			for (char C : Text)
			{
				if (C == ' ' || C == '\t')
				{
					if (!One.empty()) Out.push_back(One);
					One.clear();
				}
				else
				{
					One += C;
				}
			}
			if (!One.empty()) Out.push_back(One);
			return Out;
		}

		/** His nation's language in the rules' list (aces.json lang; the rules' default is the British list). */
		std::string LangOf(const std::string& Nation)
		{
			static const std::map<std::string, std::string> Lang = {
				{ "USA", "us" }, { "UK", "uk" }, { "GBR", "uk" }, { "CAN", "uk" }, { "AUS", "uk" }, { "NZL", "uk" }, { "SAF", "uk" },
				{ "IND", "uk" }, { "IRE", "uk" }, { "GER", "de" }, { "AUT", "de" }, { "SOV", "ru" }, { "JAP", "ja" }, { "JPN", "ja" },
				{ "ITA", "it" }, { "FRA", "fr" }, { "VIC", "fr" }, { "BEL", "fr" }, { "POL", "pl" }, { "FIN", "fi" }, { "NED", "nl" },
				{ "NOR", "no" }, { "DEN", "no" }, { "SWE", "no" }, { "ROM", "ro" }, { "HUN", "hu" }, { "CHN", "zh" },
			};
			const auto Found = Lang.find(Nation);
			return Found == Lang.end() ? std::string("uk") : Found->second;
		}

		/** Every rank the rules may give (aces.json names.*.ranks, and the few ACES.md adds), with its short form by
		 * language where ACES.md gives one ("" where his nation's rank is not painted). Longest first when read. */
		struct FRank
		{
			const char* Lang;
			const char* Full;
			const char* Short;
		};
		const std::vector<FRank>& Ranks()
		{
			static const std::vector<FRank> All = {
				{ "uk", "Pilot Officer", "P/O" }, { "uk", "Flying Officer", "F/O" }, { "uk", "Flight Lieutenant", "F/LT" },
				{ "uk", "Squadron Leader", "S/LDR" }, { "uk", "Wing Commander", "W/CDR" }, { "uk", "Sergeant", "SGT" },
				{ "uk", "Flight Sergeant", "F/SGT" },
				{ "us", "Lieutenant", "LT." }, { "us", "Captain", "CAPT." }, { "us", "Major", "MAJ." }, { "us", "Lieutenant Colonel", "LT. COL." },
				// (a carrier group's ace holds his navy's ranks: the rules' names.us.navy and names.uk.navy, ACES.md's
				//  short forms; the Fleet Air Arm's plain Lieutenant is the only one in Britain's lists, the RAF has none)
				{ "us", "Ensign", "ENS." }, { "us", "Lieutenant (junior grade)", "LT.(JG)" }, { "us", "Lieutenant Commander", "LCDR." },
				{ "uk", "Sub-Lieutenant", "S/LT" }, { "uk", "Lieutenant", "LT" }, { "uk", "Lieutenant Commander", "LT CDR" },
				{ "uk", "Commander", "CDR" },
				{ "de", "Unteroffizier", "UFFZ." }, { "de", "Feldwebel", "FW." }, { "de", "Oberfeldwebel", "OFW." }, { "de", "Leutnant", "LT." },
				{ "de", "Oberleutnant", "OBLT." }, { "de", "Hauptmann", "HPTM." }, { "de", "Major", "MAJ." },
				{ "ru", "Junior Lieutenant", "ML. LT." }, { "ru", "Lieutenant", "LT." }, { "ru", "Senior Lieutenant", "ST. LT." },
				{ "ru", "Captain", "KAPT." }, { "ru", "Major", "MAJ." }, { "ru", "Lieutenant Colonel", "" },
				{ "it", "Sergente", "SERG." }, { "it", "Sergente Maggiore", "SERG. MAGG." }, { "it", "Maresciallo", "M.LLO" },
				{ "it", "Sottotenente", "S.TEN." }, { "it", "Tenente", "TEN." }, { "it", "Capitano", "CAP." }, { "it", "Maggiore", "MAGG." },
				// (no rank painted: Japan's, surname and initial only; and the nations ACES.md gives no short ranks for)
				{ "ja", "Petty Officer", "" }, { "ja", "Warrant Officer", "" }, { "ja", "Lieutenant", "" }, { "ja", "Lieutenant Commander", "" },
				{ "fr", "Sergent-chef", "" }, { "fr", "Sous-lieutenant", "" }, { "fr", "Lieutenant", "" }, { "fr", "Capitaine", "" }, { "fr", "Commandant", "" },
				{ "pl", "Sierżant", "" }, { "pl", "Podporucznik", "" }, { "pl", "Porucznik", "" }, { "pl", "Kapitan", "" }, { "pl", "Major", "" },
				{ "fi", "Lentomestari", "" }, { "fi", "Luutnantti", "" }, { "fi", "Kapteeni", "" }, { "fi", "Majuri", "" },
				{ "nl", "Sergeant", "" }, { "nl", "Luitenant", "" }, { "nl", "Kapitein", "" }, { "nl", "Majoor", "" },
				{ "no", "Sersjant", "" }, { "no", "Løytnant", "" }, { "no", "Kaptein", "" }, { "no", "Major", "" },
				{ "ro", "Adjutant", "" }, { "ro", "Locotenent", "" }, { "ro", "Căpitan", "" }, { "ro", "Maior", "" },
				{ "hu", "Őrmester", "" }, { "hu", "Hadnagy", "" }, { "hu", "Főhadnagy", "" }, { "hu", "Százados", "" },
				{ "zh", "Lieutenant", "" }, { "zh", "Captain", "" }, { "zh", "Major", "" },
			};
			return All;
		}

		/** A letter's width as painted, in capitals' heights (a space the hand's word space; a character with no cell as
		 * a space, acepanel.wide), and whether it has a cell. */
		double WideOf(const Letters& Hand, const std::string& Ch, bool& bHas)
		{
			bHas = false;
			if (Ch == " ") return Hand.Space;
			const auto Found = Hand.Cells.find(Ch);
			if (Found == Hand.Cells.end()) return Hand.Space;
			bHas = true;
			return Found->second.Wide;
		}

		double RunOf(const Letters& Hand, const std::string& Text)
		{
			const std::vector<char32_t> Chars = Decode(Text);
			double Run = 0.0;
			bool bHas = false;
			for (char32_t C : Chars) Run += WideOf(Hand, Encode(C), bHas);
			return Run + Hand.Gap * static_cast<double>(std::max<size_t>(1, Chars.size()) - 1);
		}

		double Number(const std::string& Json, const char* Key, double Else)
		{
			const std::string Want = std::string("\"") + Key + "\"";
			const size_t At = Json.find(Want);
			if (At == std::string::npos) return Else;
			const size_t Colon = Json.find(':', At + Want.size());
			if (Colon == std::string::npos) return Else;
			return std::atof(Json.c_str() + Colon + 1);
		}
	}

	bool ReadLetters(const std::string& Json, Letters& Out)
	{
		Out = Letters();
		Out.Width = static_cast<int>(Number(Json, "width", 0.0));
		Out.Height = static_cast<int>(Number(Json, "height", 0.0));
		Out.Cap = static_cast<int>(Number(Json, "cap", 0.0));
		Out.CellW = static_cast<int>(Number(Json, "cell_w", 0.0));
		Out.CellH = static_cast<int>(Number(Json, "cell_h", 0.0));
		Out.Mid = static_cast<int>(Number(Json, "mid", 0.0));
		Out.Space = Number(Json, "space", 0.45);
		Out.Gap = Number(Json, "gap", 0.16);
		const size_t From = Json.find("\"cells\"");
		if (From == std::string::npos || Out.Width <= 0 || Out.Height <= 0 || Out.Cap <= 0 || Out.CellW <= 0 || Out.CellH <= 0)
		{
			return false;
		}
		// (each cell a line: ["A", 0, 0, 0.72])
		for (size_t At = Json.find("[\"", From); At != std::string::npos; At = Json.find("[\"", At + 2))
		{
			const size_t Close = Json.find('"', At + 2);
			if (Close == std::string::npos) break;
			const std::string Ch = Json.substr(At + 2, Close - At - 2);
			int Col = 0, Row = 0;
			double Wide = 0.0;
			if (std::sscanf(Json.c_str() + Close + 1, " , %d , %d , %lf", &Col, &Row, &Wide) == 3 && !Ch.empty())
			{
				Out.Cells[Ch] = Letters::Cell{ Col, Row, Wide };
			}
		}
		return !Out.Cells.empty();
	}

	std::vector<std::string> NameTries(const std::string& Nation, const std::string& Title)
	{
		const std::string Lang = LangOf(Nation);
		std::vector<std::string> W = Words(Title);
		// His rank: the longest of the rules' ranks his title begins with (his own language's first on a tie).
		const FRank* Best = nullptr;
		size_t RankWords = 0;
		for (const FRank& R : Ranks())
		{
			const std::vector<std::string> Of = Words(R.Full);
			if (Of.empty() || Of.size() > W.size() || !std::equal(Of.begin(), Of.end(), W.begin())) continue;
			if (Of.size() > RankWords || (Of.size() == RankWords && Lang == R.Lang && Best && Lang != Best->Lang))
			{
				Best = &R;
				RankWords = Of.size();
			}
		}
		// (his nation's short form where ACES.md gives one; a rank of another nation's list, none)
		const std::string Short = Best && Lang == Best->Lang ? Best->Short : "";
		W.erase(W.begin(), W.begin() + static_cast<std::ptrdiff_t>(RankWords));
		// His names: a word set in small letters (de, van, von) goes with the next ("de Vries" a surname).
		std::vector<std::string> Names;
		for (size_t K = 0; K < W.size(); ++K)
		{
			std::string One = W[K];
			while (!One.empty() && One[0] >= 'a' && One[0] <= 'z' && K + 1 < W.size()) One += " " + W[++K];
			Names.push_back(One);
		}
		std::vector<std::string> Out;
		if (Names.empty())
		{
			if (!Short.empty()) Out.push_back(Short);
			return Out;
		}
		// (China's names come family first: his surname is the first)
		const bool bFamilyFirst = Lang == "zh";
		const std::string Surname = Capitals(bFamilyFirst ? Names.front() : Names.back());
		std::string Initials;
		for (size_t K = bFamilyFirst ? 1 : 0; K < (bFamilyFirst ? Names.size() : Names.size() - 1); ++K)
		{
			const std::vector<char32_t> Chars = Decode(Names[K]);
			if (Chars.empty()) continue;
			Initials += (Initials.empty() ? "" : " ") + Capital(Chars[0]) + ".";
		}
		auto Add = [&Out](const std::string& Form)
		{
			if (!Form.empty() && (Out.empty() || Out.back() != Form)) Out.push_back(Form);
		};
		const std::string Rank = Short.empty() ? "" : Short + " ";
		Add(Rank + (Initials.empty() ? Surname : bFamilyFirst ? Surname + " " + Initials : Initials + " " + Surname));
		Add(Rank + Surname);
		Add(Surname);
		return Out;
	}

	NameLaid LayName(const std::vector<std::string>& Tries, const Letters& Hand, double Long, double Cap, double CapMin)
	{
		NameLaid Out;
		if (Tries.empty() || Long <= 0.0 || Cap <= 0.0) return Out;
		size_t Chosen = Tries.size() - 1;
		for (size_t K = 0; K < Tries.size(); ++K)
		{
			const double Unit = RunOf(Hand, Tries[K]);
			if (std::min(Cap, Long / std::max(Unit, 1e-6)) >= CapMin)
			{
				Chosen = K;
				break;
			}
		}
		Out.Text = Tries[Chosen];
		Out.Cap = std::min(Cap, Long / std::max(RunOf(Hand, Out.Text), 1e-6));
		double S = 0.0;
		for (char32_t C : Decode(Out.Text))
		{
			const std::string Ch = Encode(C);
			bool bHas = false;
			const double Wide = WideOf(Hand, Ch, bHas);
			if (bHas)
			{
				Out.Letters.push_back(NameLetter{ Ch, S + 0.5 * Wide * Out.Cap, Wide * Out.Cap });
			}
			else if (Ch != " ")
			{
				Out.Missing += Ch;
			}
			S += (Wide + Hand.Gap) * Out.Cap;
		}
		Out.Run = std::max(0.0, S - Hand.Gap * Out.Cap);
		return Out;
	}

	double Lightness(unsigned Rgb)
	{
		auto Lin = [](unsigned C)
		{
			const double V = static_cast<double>(C & 0xFFu) / 255.0;
			return V <= 0.04045 ? V / 12.92 : std::pow((V + 0.055) / 1.055, 2.4);
		};
		const double Y = 0.2126 * Lin(Rgb >> 16) + 0.7152 * Lin(Rgb >> 8) + 0.0722 * Lin(Rgb);
		return Y > 216.0 / 24389.0 ? 116.0 * std::cbrt(Y) - 16.0 : 24389.0 / 27.0 * Y;
	}

	bool OliveDrab(unsigned Rgb)
	{
		const int R = static_cast<int>((Rgb >> 16) & 0xFFu), G = static_cast<int>((Rgb >> 8) & 0xFFu), B = static_cast<int>(Rgb & 0xFFu);
		// (ACES.md: dark, red and green within a tenth of each other, both well above blue. "Above" as a share of blue
		//  as well as a step, so the art director's matte olive drab 555A47 on the game's own Mustang, 14 above blue,
		//  is one; a grey or a navy blue-grey never is)
		return Lightness(Rgb) < 45.0 && std::abs(R - G) <= 0.1 * std::max(R, G) && std::min(R, G) - B >= 10
			&& std::min(R, G) >= 1.15 * B;
	}

	unsigned NameInk(unsigned SkinRgb, const std::string& Owner)
	{
		if (Owner == "USA" && OliveDrab(SkinRgb)) return 0xD9B43Au;
		return Lightness(SkinRgb) >= 50.0 ? 0x1A1A1Au : 0xF2F0E8u;
	}

	std::string SampleTitle(const std::string& Nation)
	{
		static const std::map<std::string, std::string> Sample = {
			{ "uk", "Flight Lieutenant Tom Harland" }, { "us", "Captain Jack Calder" }, { "de", "Oberleutnant Kurt Brenner" },
			{ "ru", "Senior Lieutenant Ivan Belov" }, { "ja", "Petty Officer Hiroshi Tanaka" }, { "it", "Tenente Giorgio Bellini" },
			{ "fr", "Capitaine Jean Arnaud" }, { "pl", "Porucznik Jan Nowicki" }, { "fi", "Luutnantti Eino Virtanen" },
			{ "nl", "Luitenant Jan de Vries" }, { "no", "Løytnant Ole Hansen" }, { "ro", "Locotenent Ion Popescu" },
			{ "hu", "Hadnagy László Nagy" }, { "zh", "Captain Wang Wei" },
		};
		const auto Found = Sample.find(LangOf(Nation));
		return Found == Sample.end() ? std::string("Flight Lieutenant Tom Harland") : Found->second;
	}
}
