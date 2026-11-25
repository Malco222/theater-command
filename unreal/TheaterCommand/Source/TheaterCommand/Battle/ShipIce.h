#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

/**
 * ICE ON A SHIP (the look bible's S6, heavy weather: "ice on rails and guns in the Arctic and the winter Atlantic";
 * -TheaterSeaIce=, tier 2, step 6). LOOKS ONLY and off by default: the sea battle sets a frost on the stage only under
 * -TheaterSeaIce=1 (Battle/HeavyWeather), and a stage with no frost (every air battle, Pearl Harbor and Midway, and every
 * sea battle by default) lays her hull's colours exactly as before. Plain C++20, no engine: Tools/sea_waves/ice_test.cpp
 * holds it, on the battle modeller's own destroyer.
 *
 * Spray icing: the sea thrown over her freezes where the air is well below the sea's freezing point. How fast is
 * Overland's predictor (J. E. Overland, C. H. Pease, R. W. Preisendorfer and A. L. Comiskey, "Prediction of vessel icing",
 * Journal of Climate and Applied Meteorology 25, 1986; J. E. Overland, "Prediction of vessel icing for near-freezing sea
 * temperatures", Weather and Forecasting 5, 1990):
 *     PPR = Va (Tf - Ta) / (1 + 0.3 (Tw - Tf)),  Tf = -1.7 C, the freezing point of sea water,
 * Va the wind (m/s), Ta the air and Tw the sea (C); NOAA's classes from it: none below 0, light to 22.4 (under 0.7 cm an
 * hour), moderate to 53.3 (0.7 - 2), heavy to 83 (2 - 4), extreme over it (over 4 cm an hour), for ships of 20 to 75 m.
 * The air and the sea come from a climatology by latitude, longitude and month (ClimateAt: the northern seas the war's
 * convoys crossed by box, the open ocean by latitude). The ice on her when the battle opens is the passage's (PassageHours
 * of icing at the passage's wind), and it grows during the battle at the battle's own wind; a ship the stage lays later in
 * the battle carries what has grown by then.
 *
 * Drawn into her hull's VERTEX COLOURS as the stage lays her (AAirStage::LayShip, one line before her mesh is made, near
 * and far; her cut-out guns take their colours from her): a glazed pale grey-white, never snow-white, on what faces up,
 * forward or into the wind, the most forward and from the deck up a few metres (forecastle, guns, rails, bridge front,
 * masts), her sides below the main deck mostly bare (the sea washes them), nothing under the waterline. The paint's
 * alpha (art fix 12: painted metal at a half, chipping to bare metal; M_MapSurface) is raised a little under the ice, so
 * the chips show less through it, and never into the band 232 - 248 that marks paint laid on the water (240).
 *
 * Model frame: the stage's ship models lie with the bow toward -X, starboard -Y (the modeller's file: x forward, y right,
 * turned half round by FMiniatureMesh::Sculpted), Z up from the waterline, in world units (UnitsPerMetre). Numbers: [P]
 * published, [EST] an estimate, to be judged on the Mac.
 */
namespace ShipIce
{
	inline constexpr double Pi = 3.14159265358979323846;
	/** [P] The freezing point of sea water (C), Overland's Tf. */
	inline constexpr double FreezeSea = -1.7;
	/** [EST] Hours of icing on her at the battle's opening (the passage's weather of late; her crew chipping and steaming
	 * the rest away), the passage's least wind (m/s: the winter northern seas' mean, about force 5), the thickest she
	 * carries (cm) and the thickness that glazes most of what is exposed (cm: the share is 1 - e^(-t/ShareCm)). */
	inline constexpr double PassageHours = 8.0, PassageWind = 10.0, ThickMost = 15.0, ShareCm = 6.0;
	/** [EST] The most a point is glazed (its colour that share of the way to the glaze), and the glaze: sRGB as the
	 * mesh stores it (M_MapSurface raises it to 2.2), clear sea ice over grey paint, the paint greying through it. */
	inline constexpr double CoverMost = 0.85;
	inline constexpr int GlazeR = 198, GlazeG = 207, GlazeB = 214;
	/** The paint's alpha under the ice is raised toward this, never past AlphaMost: under the wake's band (232 - 248). */
	inline constexpr int AlphaIce = 224, AlphaMost = 230;

	/** The air and the sea surface (C) of a place and month, and the water named. */
	struct Climate
	{
		double Air = 20.0, Sea = 20.0;
		const char* Where = "the open ocean";
	};
	/** Latitude and longitude in degrees (west and south minus), Month from 0 (January), as SeaShow's Field. */
	Climate ClimateAt(double Lat, double Lon, int Month0);

	/** [P] Overland's predictor, and NOAA's classes from it. */
	double Ppr(double WindU, double Air, double Sea);
	enum class Icing
	{
		None,
		Light,
		Moderate,
		Heavy,
		Extreme,
	};
	Icing IcingOf(double Ppr);
	const char* Named(Icing Class);
	/** [P] How fast ice builds (cm an hour on a 20 - 75 m vessel): through NOAA's class edges (0, 0.7, 2, 4 cm an hour at
	 * PPR 0, 22.4, 53.3, 83), straight between them, rising at the heavy class's pace past it to 6 cm an hour [EST]. */
	double RateOf(double Ppr);
	/** [EST] A ship's share of that by her length: Overland's vessels were 20 - 75 m; a longer hull lifts less of the sea
	 * to her decks for each square metre of them: (75 / L)^(1/2), held to 0.35 - 1.2. */
	double ShipShare(double Long);

	/** What the sea battle sets on the stage (AAirStage::SetFrost): the icing rate of the passage and of the battle (cm an
	 * hour on a 75 m hull), the passage's hours, the battle time it opened at (the stage's clock), where the wind blows from
	 * (board radians, clockwise from north), and what it was made from, for the line it says. None at all by default: no
	 * frost, nothing changed. */
	struct Frost
	{
		double PassageRate = 0.0, BattleRate = 0.0, Hours = 0.0, Since = 0.0, WindFrom = 0.0;
		double Air = 0.0, Sea = 0.0, Wind = 0.0, PassagePpr = 0.0, BattlePpr = 0.0;
		const char* Where = "";
		bool On() const { return PassageRate > 0.0 || BattleRate > 0.0; }
	};
	/** The frost of a battle: its place and month, its wind (m/s, held to the gale's 24.4) and where it blows from, and
	 * the battle time it opens at. */
	Frost FrostOf(double Lat, double Lon, int Month0, double WindU, double WindFrom, double BeginT);
	/** Her ice: its thickness (cm) and the share of what is exposed it glazes (0 - 1), on a hull of Long metres, Hours into
	 * the battle (0 at its opening). */
	double ThickOf(const Frost& F, double Long, double Hours);
	double ShareOf(const Frost& F, double Long, double Hours);
	/** The line said when the frost is set (HeavyWeather): where, the air, the sea, the wind, the class and a destroyer's
	 * ice. */
	std::string Line(const Frost& F, int Month0);

	/** [P]/[EST] How exposed a point is to the spray (0 - 1), by where it is on her and which way it faces, in the model
	 * frame and metres: Along 0 at her stern to 1 at her stem; Up how high over the waterline; Deck her main deck's height
	 * there; (Nx, Ny, Nz) its facing; (Wx, Wy) the way toward where the wind comes from. Ice builds most forward (most of a
	 * cutter's spray ice on her forward third: N. Ryerson, "Superstructure spray and ice accumulations on a large U.S.
	 * Coast Guard cutter", Atmospheric Research 36, 1995), from the deck up a few metres and less above (Zakrzewski 1987),
	 * on what faces the sky, her bow or the wind; her side below the deck is washed bare (but for the flare at her stem),
	 * and the waterline's metre with it. */
	double Exposure(double Along, double Up, double Deck, double Nx, double Ny, double Nz, double Wx, double Wy);
	/** A point's own unevenness, 0.75 - 1.15, fixed by where it is (half-metre cells): ice lies in patches. */
	double Patch(double X, double Y, double Z);
	/** One colour glazed by Cover (0 - CoverMost): the colour that share of the way to the glaze, its alpha raised under
	 * the ice (never lowered, never past AlphaMost). Cover 0 gives the colour back exactly. */
	void GlazeOne(uint8_t& R, uint8_t& G, uint8_t& B, uint8_t& A, double Cover);

	namespace Detail
	{
		template <typename T>
		int CountOf(const T& Items)
		{
			if constexpr (requires { Items.Num(); })
			{
				return static_cast<int>(Items.Num());
			}
			else
			{
				return static_cast<int>(Items.size());
			}
		}
		inline double SmoothStep(double A, double B, double X)
		{
			const double T = std::clamp((X - A) / (B - A), 0.0, 1.0);
			return T * T * (3.0 - 2.0 * T);
		}
	}

	/** Her main deck's height along her (metres, model frame), from her own points: in each of Slices stations, the lowest
	 * point facing the sky more than a metre over the waterline; a station with none takes its neighbours'. Deck[0] is at
	 * her stern (most X), Deck[Slices - 1] at her stem. */
	inline constexpr int Slices = 24;
	template <typename PointT, typename NormalT>
	void DeckLine(const PointT* Points, const NormalT* Normals, int N, double Scale, double MinX, double MaxX, double High, double (&Deck)[Slices])
	{
		constexpr double None = 1.0e9;
		for (double& D : Deck)
		{
			D = None;
		}
		const double Span = std::max(1e-6, MaxX - MinX);
		for (int I = 0; I < N; ++I)
		{
			const double Z = Points[I].Z * Scale;
			if (Normals[I].Z > 0.8 && Z > 1.0)
			{
				const int K = std::clamp(static_cast<int>((MaxX - Points[I].X * Scale) / Span * Slices), 0, Slices - 1);
				Deck[K] = std::min(Deck[K], Z);
			}
		}
		bool bAny = false;
		for (const double D : Deck)
		{
			bAny = bAny || D < None;
		}
		for (int K = 0; K < Slices; ++K)
		{
			if (Deck[K] < None)
			{
				continue;
			}
			// (the nearest station either way that has a deck)
			double Got = bAny ? None : 0.25 * High;
			for (int Off = 1; Off < Slices && Got >= None; ++Off)
			{
				if (K - Off >= 0 && Deck[K - Off] < None) Got = Deck[K - Off];
				else if (K + Off < Slices && Deck[K + Off] < None) Got = Deck[K + Off];
			}
			Deck[K] = Got;
		}
	}

	/**
	 * Her points glazed: Points and Normals in world units of the model (bow -X, starboard -Y), Colours in place (R, G,
	 * B, A of 0 - 255), Share her ice's share (ShareOf), WindOffBow where the wind comes from off her bow (radians, 0
	 * dead ahead, plus from starboard). A Share of 0 touches nothing.
	 */
	template <typename PointT, typename NormalT, typename ColourT>
	void GlazeArrays(const PointT* Points, const NormalT* Normals, ColourT* Colours, int N, double UnitsPerMetre, double Share, double WindOffBow)
	{
		if (!(Share > 0.0) || N <= 0 || !(UnitsPerMetre > 0.0))
		{
			return;
		}
		const double Scale = 1.0 / UnitsPerMetre;
		double MinX = 1.0e18, MaxX = -1.0e18, High = 0.0;
		for (int I = 0; I < N; ++I)
		{
			MinX = std::min(MinX, Points[I].X * Scale);
			MaxX = std::max(MaxX, Points[I].X * Scale);
			High = std::max(High, Points[I].Z * Scale);
		}
		double Deck[Slices];
		DeckLine(Points, Normals, N, Scale, MinX, MaxX, High, Deck);
		const double Span = std::max(1e-6, MaxX - MinX);
		// (the way toward the wind's source in her frame: ahead is -X, starboard -Y)
		const double Wx = -std::cos(WindOffBow), Wy = -std::sin(WindOffBow);
		for (int I = 0; I < N; ++I)
		{
			const double X = Points[I].X * Scale, Y = Points[I].Y * Scale, Z = Points[I].Z * Scale;
			const double Along = (MaxX - X) / Span;
			const int K = std::clamp(static_cast<int>(Along * Slices), 0, Slices - 1);
			const double E = Exposure(Along, Z, Deck[K], Normals[I].X, Normals[I].Y, Normals[I].Z, Wx, Wy);
			if (E <= 0.0)
			{
				continue;
			}
			const double Cover = std::min(CoverMost, Share * E * Patch(X, Y, Z));
			uint8_t R = Colours[I].R, G = Colours[I].G, B = Colours[I].B, A = Colours[I].A;
			GlazeOne(R, G, B, A, Cover);
			Colours[I].R = R;
			Colours[I].G = G;
			Colours[I].B = B;
			Colours[I].A = A;
		}
	}

	/**
	 * The stage's hook (AAirStage::LayShip, one line before her mesh is made, near and far): her model glazed by the
	 * frost, her heading Face (board radians, the stage's FStageShip::Face) and the stage's clock (battle time). Nothing
	 * unless a frost is set, her points, facings and colours match, and she has ice. Her length from her own points.
	 */
	template <typename MeshT>
	void Glaze(MeshT& Model, double UnitsPerMetre, const Frost& F, double Face, double Clock)
	{
		if (!F.On())
		{
			return;
		}
		const int N = Detail::CountOf(Model.Vertices);
		if (N <= 0 || Detail::CountOf(Model.Normals) != N || Detail::CountOf(Model.Colors) != N || !(UnitsPerMetre > 0.0))
		{
			return;
		}
		double MinX = 1.0e18, MaxX = -1.0e18;
		for (int I = 0; I < N; ++I)
		{
			MinX = std::min(MinX, static_cast<double>(Model.Vertices[I].X));
			MaxX = std::max(MaxX, static_cast<double>(Model.Vertices[I].X));
		}
		const double Long = (MaxX - MinX) / UnitsPerMetre;
		const double Hours = std::max(0.0, Clock - F.Since) / 3600.0;
		const double Off = std::remainder(F.WindFrom - Face, 2.0 * Pi);
		GlazeArrays(&Model.Vertices[0], &Model.Normals[0], &Model.Colors[0], N, UnitsPerMetre, ShareOf(F, Long, Hours), Off);
	}
}
