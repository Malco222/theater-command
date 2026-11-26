#pragma once

#include "Battle/LandField.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

/**
 * The open steppe for the ground battle (Brian, 22 November 2026, 00:12Z: "Should we do a battle in an open field so we
 * can see them battle more clearly", and "Let's make it an epic battle with artillery, tanks, infantry"): Prokhorovka,
 * July 1943, the Soviet tank army's attack across the farmland south-west of the station. The recording lays the
 * steppe over the rules' plains (Tools/land_record, openSteppe: field.steppe) and LandField lays the ground as the
 * steppe where it is given (LandField::SteppeLay): the great fields of the collective farms, wheat and rye standing ripe
 * and pasture and fallow between, each half a kilometre and more across with no hedge round it, a grassy strip at most
 * where two meet; a balka in from a flank, scrub in its floor; a railway on its bank along the other flank with its
 * telegraph line; the state farm's whitewashed cottages under thatch with their wattle fences, well sweeps and haystacks,
 * a third of them burnt to their chimneys. Plain C++, so the cloud test lays exactly what the game draws.
 *
 * What lives here: the lay made ready for the ground's questions (Plan: which farm field a place is in, how far it is
 * from the railway and how high the bank stands there), the steppe's colours, the east kit's models read into the
 * ground's bases (the scenery modeller's battle models, battle-models/east: our own designs), and the railway's mesh.
 * LandField asks these where it lays the steppe; the rest of the ground (heights, lanes, parcels, tiles, the ring) is
 * LandField's own.
 *
 * Every colour is the art director's to check (sRGB, as the eye sees a July morning on the steppe; the battle camera's
 * grade greys them).
 */
namespace LandSteppe
{
	/** The steppe's crops, as SteppeLay's digits. */
	enum Crop : int
	{
		CropNone = 0,
		CropWheat = 1,
		CropRye = 2,
		CropPasture = 3,
		CropFallow = 4
	};

	/** A colour, 8-bit sRGB as numbers (LandField mixes them as its own). */
	struct Tone
	{
		double R = 0.0, G = 0.0, B = 0.0;
	};

	/** The lay made ready: the farms' fields, the railway's line and its bank. */
	class Plan
	{
	public:
		explicit Plan(const LandField::SteppeLay& Lay);

		/**
		 * The farm field at a place: its number (0 or more, under a million: the same field the same number everywhere)
		 * and its crop (CropWheat ... CropFallow); OutGap (if asked) the metres from the place to its field's nearest edge.
		 * Beyond the fields laid (past 6 km round the battle), pasture.
		 */
		int FieldAt(double X, double Y, int& OutCrop, double* OutGap) const;
		/** A rules cell's crop (CropNone where the cell is not a crop), by its index. */
		int CellCrop(size_t Cell) const { return Cell < Lay.Crop.size() ? int(Lay.Crop[Cell]) : int(CropNone); }

		bool HasRail() const { return Rail.size() >= 2; }
		/** The railway's middle at a northing (straight on past the line's ends) and its way there (unit, northward). */
		double RailX(double Y) const;
		void RailWay(double Y, double& OutX, double& OutY) const;
		/** Metres from the railway's middle line (a lot where there is none). */
		double RailDistance(double X, double Y) const;
		/** How far the bank lifts the ground at a place above the land (0 off it): a flat top 3.4 m either side of the
		 * line, its sides falling one in one and a half, the knees rounded. */
		double BankRise(double X, double Y) const;
		double Bank() const { return Lay.RailBank; }
		/** The railway's two ends as laid (the south and the north), for the meshes. */
		double RailY0() const { return Rail.empty() ? 0.0 : Rail.front().second; }
		double RailY1() const { return Rail.empty() ? 0.0 : Rail.back().second; }

		const LandField::SteppeLay& Lay;

	private:
		std::vector<double> Cos, Sin; // (the turn, once)
		int Cols = 0, Rows = 0;
		/** The railway resampled every 10 m along its northing, smoothed (it runs south to north, so x is a function of y). */
		std::vector<std::pair<double, double>> Rail;
	};

	/**
	 * The ground's colour on a farm field, from the crop and the noise LandField gives the place: N1 a field's patches
	 * (-1 to 1, some 30 m), N2 its fine mottle (-1 to 1, some 7 m), Broad the slow swell of colour across the country
	 * (-1 to 1), Lands the tractor's drill bands (-1 to 1, across the field's length), Streak along its rows (-1 to 1),
	 * Edge the metres in from the field's edge (the grassy strip where two fields meet).
	 */
	Tone FieldColour(int Crop, double N1, double N2, double Broad, double Lands, double Streak, double Edge);
	/** The grassy strip where two fields (or a field and a road, a wood, a garden) meet, and how much of it shows Edge metres
	 * in from the edge: all of it at the edge itself (the parcels' edge pixels lie 1 m in), so the ground's colour runs on unbroken from one field to the next. */
	Tone StripTone(double N2, double Broad);
	double StripShare(double Edge);
	/** The same field seen from far off (the ring), its edge's strip a faint line. */
	Tone FarFieldColour(int Crop, double N1, double Broad, double Gap);
	/** Rough ground on the steppe (a balka's steep sides, the railway's bank, waste ground): dry grass, greener in hollows. */
	Tone RoughColour(double N1, double N2);
	/** The railway's bed: ballast and cinders at the middle (D metres from the line), grass taking the bank's sides. */
	Tone BankColour(double D, double N1, double N2, const Tone& Under);
	/** Black earth thrown up and trodden (for the shell holes' word "blackearth", LandCrater). */
	Tone BlackEarth();
	/** A dirt road's floor (black earth worn to grey dust, as the kit's mud_road dried) and its wheel ruts. */
	Tone RoadDust();
	Tone RoadRut();

	/** Whether a steppe crop is standing corn (wheat and rye: a tank lays it flat). */
	inline bool IsCorn(int Crop) { return Crop == CropWheat || Crop == CropRye; }

	/**
	 * One of the east kit's models (a battle model of kind "scenery": points, facings, paints and triangles for near
	 * and far, its paints' colours) read into a ground base, in the ground's frame (x forward, y left, z up: the model's y
	 * is to its right, so it is mirrored, and each triangle wound counter-clockwise seen from the side its facing says).
	 * FarFrom: where the far level takes over. False and why on a fault (the base is then left as it was).
	 */
	bool ReadKit(const std::string& Text, double FarFrom, LandField::BaseMesh& Out, std::string& OutError);
	/** A plain stand-in for a kit model whose file is not given (the cloud test without the kit, a build without it):
	 * a white box under a straw roof, a post, a hurdle, a stack. Plain on purpose: the kit is what is meant to be seen. */
	void PlainKit(LandField::Base B, LandField::BaseMesh& Out);
	/** The kit's file name for a base (Khata, BurntChimney, TelegraphPole, WattleFence, WellSweep, Haystack, Birch), or empty. */
	const char* KitName(LandField::Base B);

	/**
	 * The railway laid: its bank swept at its own points (Paint gives the ground's colour; the ground's own mesh leaves the
	 * bank out and sinks a little under it), the ballast's shoulders on its top, a sleeper every 0.55 m and two rails 1.524
	 * m apart (the Russian broad gauge), from Y0 to Y1 along its line. Far: the bank and the ballast only, coarser.
	 * Ground gives the land with the bank on it; Target the mesh for a place (null where nothing is drawn). OutKm: how
	 * much was laid.
	 */
	void LayRail(const Plan& P, double Y0, double Y1, bool bFar, const std::function<double(double, double)>& Ground, const std::function<Tone(double, double)>& Paint, const std::function<LandField::Mesh*(double, double)>& Target, double& OutKm);

	/** The telegraph poles along the railway, every Step metres, Off metres to its west: their feet (x, y) and their heading. */
	void PolesAlongRail(const Plan& P, double Y0, double Y1, double Step, double Off, std::vector<std::pair<double, double>>& OutAt, std::vector<double>& OutYaw);
}
