#pragma once

// Real dogfights (release 2, row realDogfights): each fighter type's own figures for the flight core
// (AirShow::Show::SetTypes), from the historian's table (Battle/AirPerf, Content/Data/Air/performance.json), by the model
// a battle's group flies, its nation and the battle's date. Plain C++: the game and the cloud tools both use it.

#include "Battle/AirPerf.h"
#include "Battle/AirShow.h"

#include <memory>
#include <string>

namespace AirDogfightTypes
{
	/** A type's figures from its row of the table. */
	inline AirShow::Show::TypeFigures FiguresOf(const AirPerf::Table& T, const AirPerf::Row& R)
	{
		const AirPerf::Flight F = T.LimitsOf(R);
		AirShow::Show::TypeFigures Out;
		Out.VMin = F.VMin;
		Out.VMax = F.VMax;
		Out.VTop = F.VTop;
		Out.VDive = F.VDive;
		Out.RMin = F.RMin;
		Out.GMax = F.GMax;
		Out.RollMax = F.RollMax;
		Out.RollTime = F.RollTime;
		// (the roll chart: its peak and the speed of it, its rate at a high speed and that speed; km/h and degrees in the
		// table, m/s and radians here)
		Out.RollPeakV = R.RollKmh / 3.6;
		Out.RollFast = R.RollFast * 3.14159265358979323846 / 180.0;
		Out.RollFastV = R.RollFastKmh / 3.6;
		Out.Climb = F.ClimbMost;
		Out.Style = R.Style == "turn" ? 1 : R.Style == "dive" ? 2 : R.Style == "both" ? 3 : 0;
		return Out;
	}

	/** The battle's date as the table reads it: a year with its fraction, its month counted from 1 (June 1943 is 1943.46;
	 * a board's month counts from 0, so its Month + 1). */
	inline double YearOf(int Year, int Month) { return Year + (Month - 0.5) / 12.0; }

	/** The finder the flight core asks for each fighter it makes: the table's row for the model as named, of that
	 * nation, flying the mark of the battle's date; none for a model the table does not know or a type not a fighter. */
	inline AirShow::Show::TypeFinder FinderOf(std::shared_ptr<const AirPerf::Table> Table, double Year)
	{
		return [Table, Year](const std::string& Model, const std::string& Nation, AirShow::Show::TypeFigures& Out)
		{
			if (!Table || Model.empty()) return false;
			const AirPerf::Row* R = Table->Of(Model, Nation, Year);
			if (!R || !R->IsFighter()) return false;
			Out = FiguresOf(*Table, *R);
			return true;
		};
	}
}
