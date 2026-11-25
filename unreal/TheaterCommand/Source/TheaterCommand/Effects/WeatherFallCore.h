#pragma once

// Rain and snow falling round the eye (release 2, the look bible's G10), the plain C++ half: what falls, in layers about
// the eye, the one mesh each layer is drawn from (made once), and the few numbers a frame its material takes (the drops
// themselves are carried, wrapped about the eye and turned to it on the graphics chip: WeatherFallChip.inl). It knows
// no scene: the land battle gives it its weather (LandWeather) and the sea's storms may give it theirs; it needs no
// ground. The cloud test (Tools/land_weather) builds it with the chip's own text and checks the two together.

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace WeatherFall
{
	enum class Kind : int
	{
		None = 0,
		Rain = 1,
		Snow = 2
	};

	const char* NameOf(Kind K);

	/**
	 * One layer of what falls: drops in a box about the eye (Box metres a side), Density of them a cubic metre at full
	 * strength, falling Fall metres a second (each its own, within Spread of it either way), carried by WindShare of the
	 * wind, Width metres across (a flake's size, for snow) and, for rain, Length long; Alpha the most any one shows (at
	 * full strength), Tone its colour's share of the ink; none shown nearer the lens than NearFrom, whole from NearFull,
	 * and fading out from FarFade of half the box to its edge; a flake's Flutter (metres either way). Order: its place
	 * among the scene's see-through things (near layers over the smoke, far ones under it).
	 */
	struct Layer
	{
		std::string Name;
		double Box = 10.0;
		double Density = 1.0;
		double Fall = 8.5;
		double Spread = 0.2;
		double WindShare = 1.0;
		double Width = 0.004;
		double Length = 0.5;
		double Alpha = 0.3;
		double Tone = 1.0;
		double NearFrom = 0.5;
		double NearFull = 1.5;
		double FarFade = 0.6;
		double Flutter = 0.0;
		int Order = 0;
	};

	/**
	 * The recipe: the layers of each kind and the step every fall and wind is a whole number of (Step, metres a second;
	 * the period, a layer's box over its step, is how long before every drop is back where it began), the least a drop
	 * is drawn across (LeastPx, pixels: thinner, it is drawn that wide and as much fainter), MostDrops (a layer's mesh at
	 * most), StrengthAlpha (at no strength the drops show this share of their alpha; at full, all of it), StrengthLength
	 * (at no strength a streak is this share of its length: a light rain's drops smaller and slower, its streaks shorter).
	 */
	struct Recipe
	{
		std::vector<Layer> Rain, Snow;
		double RainStep = 1.0 / 6.0, SnowStep = 1.0 / 15.0;
		double LeastPx = 1.25;
		int MostDrops = 60000;
		double StrengthAlpha = 0.35;
		double StrengthLength = 0.3;

		/** The recipe's own numbers (the art director's first round). */
		static Recipe Standing();
		/** Numbers read over them: Find(path, value) for paths such as "rain.near.density" or "least_px"; each layer's
		 * own name in the path. A layer named in the book but not in the recipe is not made. */
		void ReadOver(const std::function<bool(const char*, double&)>& Find);
		const std::vector<Layer>& Of(Kind K) const { return K == Kind::Snow ? Snow : Rain; }
		double StepOf(Kind K) const { return K == Kind::Snow ? SnowStep : RainStep; }
	};

	/** A layer's mesh, made once: each drop four points about its home (all at the home: the chip spreads them), in the
	 * mesh's own metres about its middle, with its corner (Corners, two a point) and its own numbers (Drops, four a point:
	 * its fall in steps, its phase, its size, its turn to show); two triangles a drop. Two points more, unreferenced, at
	 * the box's far corners either way, give the mesh its bounds. */
	struct Mesh
	{
		std::vector<float> Points;
		std::vector<float> Corners;
		std::vector<float> Drops;
		std::vector<int> Triangles;
		int DropCount = 0;
	};

	/** How many drops a layer's box holds (Density times its volume, at most MostDrops). */
	int DropsIn(const Layer& L, int MostDrops);
	/** The period of a layer: its box over the step (seconds). */
	double PeriodOf(const Layer& L, double Step);
	/** A layer's whole steps of fall for a drop at the given share (0 to 1) of its spread: never under one. */
	int StepsOf(const Layer& L, double Step, double Share);
	/** A wind's carry made a whole number of steps (metres a second). */
	double Stepped(double Wind, double Step);

	Mesh Make(const Layer& L, double Step, int MostDrops, uint64_t Seed);

	/** The numbers a layer's material takes this frame: Box, Move, Look and Lens, four floats each, as the chip reads
	 * them (WeatherFallChip.inl); Ink's share for this layer (its Tone, times its alpha's strength). */
	struct Params
	{
		float Box[4] = { 0, 0, 0, 0 };
		float Move[4] = { 0, 0, 0, 0 };
		float Look[4] = { 0, 0, 0, 0 };
		float Lens[4] = { 0, 0, 0, 0 };
		float Opacity = 1.0f;
		float Tone = 1.0f;
	};

	/**
	 * This frame's numbers for a layer. Strength 0 to 1 (0 draws none); WindX, WindY the wind (metres a second, in the
	 * mesh's own frame); Clock the seconds the weather has run (held while the scene's clock is held); PixelAngle one
	 * pixel's angle at the lens (radians: the lens's width over the picture's pixels across).
	 */
	Params Now(const Recipe& R, const Layer& L, Kind K, double Strength, double WindX, double WindY, double Clock, double PixelAngle);
}
