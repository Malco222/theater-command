// The men's front by the place on the earth (LandMen::FrontAt): which armies' colours and finishes a battle with no
// front of its own is dressed in. Pinned here because the Pacific runs on past the date line: Hawaii, Midway and the
// Aleutians were dressed for the west until 25 November 2026 (the Japan and Pacific thread's finding).
#include "Battle/LandMen.h"
#include <cstdio>
#include <cstring>
#include <limits>
int main()
{
	struct Row { const char* Name; double Lat, Lon; const char* Want; };
	const Row Rows[] = {
		{ "Normandy", 49.12, -1.09, "west" }, { "no place given", 49.0, 0.0, "west" }, { "Prokhorovka", 51.03, 36.72, "east" },
		{ "Tobruk", 32.08, 23.96, "desert" }, { "Cassino", 41.49, 13.81, "south" }, { "Shanghai", 31.2, 121.5, "china" },
		{ "Hong Kong", 22.3, 114.2, "china" }, { "Fuzhou", 26.1, 119.3, "china" }, { "Formosa", 23.7, 121.0, "pacific" },
		{ "Pescadores", 23.6, 119.6, "pacific" }, { "Aparri, Luzon", 18.36, 121.6, "pacific" }, { "Manila", 14.6, 121.0, "pacific" },
		{ "Singapore", 1.35, 103.82, "pacific" }, { "Tarawa", 1.35, 172.93, "pacific" }, { "Okinawa", 26.3, 127.8, "pacific" },
		{ "Pearl Harbor", 21.47, -158.06, "pacific" }, { "Midway", 28.2, -177.37, "pacific" }, { "Midway past the line", 28.2, 182.63, "pacific" },
		{ "Dutch Harbor", 53.9, -166.5, "pacific" }, { "Attu", 52.9, 172.9, "pacific" }, { "Tahiti", -17.6, -149.5, "pacific" },
		{ "San Francisco", 37.77, -122.42, "west" }, { "Seattle", 47.6, -122.3, "west" }, { "Panama", 9.0, -79.5, "west" },
		{ "New York", 40.7, -74.0, "west" }, { "Ceylon", 7.0, 80.0, "pacific" }, { "Moscow", 55.75, 37.6, "east" },
		// (far past the line either way, and no number at all: one step, and no place given)
		{ "Midway, three times round", 28.2, -177.37 - 1080.0, "pacific" }, { "Tarawa, twice round", 1.35, 172.93 + 720.0, "pacific" },
		{ "no number", std::numeric_limits<double>::quiet_NaN(), 10.0, "west" }, { "endless east", 20.0, std::numeric_limits<double>::infinity(), "west" },
	};
	int Bad = 0;
	for (const Row& R : Rows)
	{
		const char* Got = LandMen::FrontAt(R.Lat, R.Lon);
		const bool bOk = std::strcmp(Got, R.Want) == 0;
		if (!bOk) ++Bad;
		std::printf("%-22s %7.2f %8.2f  %-8s %s\n", R.Name, R.Lat, R.Lon, Got, bOk ? "ok" : "WRONG");
	}
	std::printf("%d wrong\n", Bad);
	return Bad ? 1 : 0;
}
