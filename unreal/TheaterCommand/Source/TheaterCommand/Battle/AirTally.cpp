#include "Battle/AirTally.h"

#include <algorithm>
#include <cmath>

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

	std::vector<std::vector<std::string>> KillRowsOf(const std::vector<std::pair<std::string, int>>& Groups)
	{
		std::vector<Row> Rows;
		for (const auto& [Tag, N] : Groups)
			for (int K = 0; K < N; K += PerRow) Rows.push_back(Row{ { Tag, std::min(PerRow, N - K) } });
		while (static_cast<int>(Rows.size()) > KillRows)
		{
			bool bJoined = false;
			// (neighbours from the end: the smallest share first)
			for (int I = static_cast<int>(Rows.size()) - 2; I >= 0; --I)
			{
				const Row Both = Joined(Rows[I], Rows[I + 1]);
				if (Width(Both) <= PerRow)
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
					const int Left = PerRow - Width(Rows[J]) - 1 - One.second;
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

	std::vector<std::string> KillsInOrder(const std::string& Nation, int Kills, const std::map<std::string, int>& By)
	{
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
		for (const std::vector<std::string>& R : KillRowsOf(Groups))
		{
			while (Out.size() % PerRow != 0) Out.push_back("");
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
		int MissionSlots, int KillSlots, int Seat)
	{
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
		const std::vector<std::string> Tags = KillsInOrder(Nation, Kills, By);
		for (size_t Row0 = 0; Row0 < Tags.size(); Row0 += PerRow)
		{
			std::vector<int> Filled;
			for (size_t K = Row0; K < std::min(Tags.size(), Row0 + PerRow); ++K)
				if (!Tags[K].empty()) Filled.push_back(static_cast<int>(K));
			for (size_t J = 0; J < Filled.size(); ++J)
			{
				const int K = Filled[J];
				if (K >= KillSlots) continue;
				Mark M;
				M.bKill = true;
				M.Slot = K;
				M.Row = K / PerRow;
				M.Tag = Tags[static_cast<size_t>(K)];
				M.Fade = KillFade + (1.0 - KillFade) * (Filled.size() > 1 ? static_cast<double>(J) / (Filled.size() - 1) : 1.0);
				Out.push_back(M);
			}
		}
		return Out;
	}
}
