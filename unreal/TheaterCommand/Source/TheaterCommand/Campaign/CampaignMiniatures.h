#pragma once

#include "CoreMinimal.h"

/**
 * Small models for the army pieces, made in code: a rifleman, a tank, a field
 * gun, a lorry, a horseman, a mountain soldier by his rock, a bunker.
 *
 * They are the kind of figure a board game is sold with: simple, moulded in the
 * nation's colour, with the working parts picked out darker. Each is built from
 * boxes, cylinders and balls straight into one mesh, so a whole piece (its
 * counters, its figures, its pole) is drawn in one go however many figures
 * stand on it. Sculpted models can replace them later: everything that uses a
 * figure only asks for "the figure for this kind of corps".
 *
 * All sizes are in the caller's units, with the figure standing on z = 0 and
 * facing along -X (toward whoever is looking at the piece).
 */
struct FMiniatureMesh
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FColor> Colors;

	/** A box, given its middle, its size, and how far it is turned about the upright axis (degrees). */
	void Box(const FVector& Center, const FVector& Size, const FLinearColor& Colour, double Turn = 0.0, double Tip = 0.0);

	/** An upright cylinder or cone standing on Base: radius at the foot, radius at the top, height. */
	void Pillar(const FVector& Base, double FootRadius, double TopRadius, double Height, const FLinearColor& Colour, int32 Sides = 12);

	/** A rod from one point to another. */
	void Rod(const FVector& From, const FVector& To, double Radius, const FLinearColor& Colour, int32 Sides = 6);

	/** A rod that tapers from one end to the other: an arm, a leg, a rifle's stock. */
	void Limb(const FVector& From, const FVector& To, double FromRadius, double ToRadius, const FLinearColor& Colour, int32 Sides = 8, bool bEnds = true);

	/** A ball, or an egg if Size is not the same each way. */
	void Ball(const FVector& Center, const FVector& Size, const FLinearColor& Colour, int32 Rings = 6, int32 Sides = 10);

	/** A flat shape with upright sides: an outline (in the figure's own plan, x forward, y to its right) raised from Foot to Top. */
	void Slab(const FVector& Where, double Scale, double Turn, const TArray<FVector2D>& Outline, double Foot, double Top, const FLinearColor& Colour);

	// ---- Shapes for building real vehicles from their measurements. Each is set out from Where, turned by Turn,
	// with the vehicle's own x forward, y to its right and z up, in units of Scale (a metre of the real thing).

	/** A side view pushed through the vehicle's width: an outline in (forward, up), from Right0 to Right1 across. */
	void Profile(const FVector& Where, double Scale, double Turn, const TArray<FVector2D>& SideOutline, double Right0, double Right1, const FLinearColor& Colour);

	/** A shape that changes from one plan to another as it rises: two outlines of the same number of points, in (forward, right), at two heights. */
	void Loft(const FVector& Where, double Scale, double Turn, const TArray<FVector2D>& Foot, double FootHeight, const TArray<FVector2D>& Head, double HeadHeight, const FLinearColor& Colour);

	/**
	 * A shape built up in layers: a plan outline (forward, right) at each height, all of the same number of
	 * points. Flat-sided for welded plate; bSmooth rounds it into one skin, for cast armour.
	 */
	void Stack(const FVector& Where, double Scale, double Turn, const TArray<TArray<FVector2D>>& Plans, const TArray<double>& Heights, const FLinearColor& Colour, bool bSmooth);

	/** A flat disc facing the way given: the face of a wheel, the lens of a lamp. */
	void Disc(const FVector& Center, const FVector& Facing, double Radius, const FLinearColor& Colour, int32 Sides = 12);

	/**
	 * A national marking (as WingMarking) painted on a surface that faces the way given: the side of a hull, a
	 * turret roof. Curve wraps it round a surface that is rounded top to bottom, such as the side of a
	 * fuselage: it is the radius of that roundness.
	 */
	void Marking(const FVector& Where, double Radius, const FString& Tag, const FVector& Facing, double Curve = 0.0);

	/** Copies a finished model into this mesh, at a place, to a size, turned. */
	void Stamp(const FMiniatureMesh& Model, const FVector& Where, double Scale, double Turn);

	/**
	 * A real vehicle, built from its measurements: "PanzerIV", "T34", "Sherman", "Matilda". Metre is the size
	 * of a real metre on the table, so everything set out with the same Metre is in scale with everything
	 * else. False if there is no model of that name.
	 */
	bool Vehicle(const FString& Name, const FVector& Where, double Metre, double Turn = 0.0, bool bFine = true);

	/** The tank a nation's armour is known by, or nothing if none is modelled yet for it. */
	static FString MainTankOf(const FString& Tag);

	/** The year the campaign has reached: an army's tanks are those its nation had by then (the Tiger from 1943). */
	static int32 Year;
	/** By nation, the year its own designers have reached for tanks (X), fighters (Y) and bombers (Z): research moves it. */
	static TMap<FString, FIntVector> DesignYears;
	/** The year that counts for a nation's models of one branch (0 tanks, 1 fighters, 2 bombers): its own designers' if it has any. */
	static int32 YearOf(const FString& Tag, int32 Branch);
	bool MainTank(const FVector& Where, double Metre, const FString& Tag, double Turn = 0.0, bool bFine = true);

	/** The field gun a nation's artillery is known by, and what its motorised troops ride in: names for Vehicle(). */
	static FString MainGunOf(const FString& Tag);
	static FString MainCarrierOf(const FString& Tag);

	/**
	 * A nation's infantryman, 1.75 m tall when Metre is a real metre: his helmet, uniform, boots, kit and rifle
	 * as his army wore them. Any nation can be asked for; one with nothing special drawn wears a plain kit in
	 * its own colour.
	 */
	void Soldier(const FString& Tag, const FVector& Where, double Metre, double Turn = 0.0, bool bFine = true);

	/** The same nation's cavalryman on his horse, and its pioneer with a shovel in place of a rifle. */
	void Rider(const FString& Tag, const FVector& Where, double Metre, double Turn = 0.0, bool bFine = true);
	void Pioneer(const FString& Tag, const FVector& Where, double Metre, double Turn = 0.0, bool bFine = true);
	void Figure(const FString& Tag, int32 Pose, const FVector& Where, double Metre, double Turn, bool bFine);

	/** Every vehicle there is a model of; and a line about one, with its real length and height. */
	static TArray<FString> VehicleNames();
	static FString VehicleTitle(const FString& Name);
	static FString VehicleNation(const FString& Name);
	static TArray<FString> AircraftNames();
	static TArray<FString> ShipNames(const FString& Tag);
	/** How much room a model needs in a row, in metres; and how high an aircraft is shown flying (0 for things on the ground). */
	static double VehicleRoom(const FString& Name);
	static double VehicleAloft(const FString& Name);

	/** The fighter and the bomber a nation's air force is known by: names for Vehicle(). Nothing if none is modelled. */
	static FString MainFighterOf(const FString& Tag);
	static FString MainBomberOf(const FString& Tag);

	/** Dust: anything added below DustTop is tinted toward Dust, most at DustFoot. Off while DustTop is not above DustFoot. */
	double DustFoot = 0.0, DustTop = 0.0;
	float DustAmount = 0.0f;
	FColor Dust = FColor(138, 122, 92);

	// ---- Ships and aircraft. A ship lies on the water at Where, bows along Turn; an aeroplane flies above its stand.

	void Battleship(const FVector& Where, double Length, const FLinearColor& Nation, double Turn = 0.0);
	void Cruiser(const FVector& Where, double Length, const FLinearColor& Nation, double Turn = 0.0);
	void Destroyer(const FVector& Where, double Length, const FLinearColor& Nation, double Turn = 0.0);
	void Carrier(const FVector& Where, double Length, const FLinearColor& Nation, double Turn = 0.0);
	void Submarine(const FVector& Where, double Length, const FLinearColor& Nation, double Turn = 0.0);
	void Aeroplane(const FVector& Where, double Span, bool bBomber, const FLinearColor& Nation, const FString& Tag, double Turn = 0.0);

	/**
	 * A nation's wartime aircraft marking, lying flat with its middle at Where: the RAF's roundel, the American
	 * star, the Soviet red star, the Japanese red disc, the German straight-armed cross, and the rest. A nation
	 * with no marking drawn here gets a plain pale disc.
	 */
	void WingMarking(const FVector& Where, double Radius, const FString& Tag, double Turn = 0.0);

	/** A fleet: up to three ships for the classes it has most worth showing, lying together. Unit is one map unit. */
	void Fleet(int32 Battleships, int32 Carriers, int32 Cruisers, int32 Destroyers, int32 Submarines, double Unit, const FLinearColor& Nation, const FString& Tag, bool bUnderWay = false);

	/** An air fleet: a fighter and a bomber (whichever it has) over a patch of airfield. */
	void AirFleet(int32 Fighters, int32 Bombers, double Unit, const FLinearColor& Nation, const FString& Tag);

	// ---- Scenery for the table.

	/** A house: four walls and a pitched roof, its ridge along its length. */
	void House(const FVector& Where, double Length, double Width, double WallHeight, double RoofHeight, const FLinearColor& Wall, const FLinearColor& Roof, double Turn = 0.0);

	/** Where the windows of the houses built since this was set are noted, each as its place and the way it faces: for lighting them at night. Null to note none. */
	TArray<FVector>* Windows = nullptr;

	/** A fir: a cone. A broad-leaved tree: a ball on a short trunk. */
	void Fir(const FVector& Where, double Height, double Radius, const FLinearColor& Leaf);
	void Broadleaf(const FVector& Where, double Height, double Radius, const FLinearColor& Leaf);

	/**
	 * A town standing on the table: a cluster of houses, more of them for a city, a great building with a dome
	 * for a capital, a church for a city, and a works with a chimney for each pair of factories.
	 */
	void Town(const FVector& Where, double Unit, int32 Houses, bool bCapital, bool bCity, int32 Works, int32 Seed, TArray<FVector>* OutChimneys = nullptr, bool bHarbour = false);

	/** What the map tells the builder of a town about the place it is to stand in (art fix 9: cities worth capturing). */
	struct FTownSite
	{
		FVector2D Centre = FVector2D::ZeroVector; // on the map
		double Radius = 5.0; // how far its roofs reach from its middle, in map units
		double Unit = 21.0; // the size its houses are built to, in the table's units: a house is under one of them long
		double MapUnit = 10.0; // the table's units to one unit of the map
		int32 Tier = 0; // 0 a small place, 1 a town, 2 a city, 3 a capital
		int32 Factories = 0;
		int32 Seed = 0;
		int32 Look = 0; // roofs and walls by region: 0 middle Europe, 1 the north-west, 2 the south, 3 the north and east, 4 the far south
		int32 Landmark = 0; // a capital's own great building, by kind (see Township)
		bool bQuay = false; // it is a port, and its shore has been found:
		FVector2D Quay = FVector2D::ZeroVector; // where its quay stands on the map, at the water's edge
		double SeaBearing = 0.0; // and which way the open water lies from there, in degrees east of north
		bool bPlots = false; // ground is kept clear inside it for the buildings its owner will give it:
		double PlotsNear = 0.0, PlotsTurn = 0.0; // how far south of its middle that ground begins, and how far the whole quarter is turned (degrees)
		TFunction<FVector(double, double, double)> Ground; // a point of the map standing on the ground, lifted a little
		TFunction<bool(double, double)> Dry; // true where the map is land
	};
	/**
	 * A town as a place with a plan. A small place: a chapel and a street. A town: a church, a main street and a
	 * cross street. A city: a grid of blocks, tall at its heart, a church and a town hall on its square, a station,
	 * and a factory quarter with a shed and a stack for its factories. A capital: the same, greater, round a great
	 * square with one landmark of its own that is known from the air. A port has its quay at the water's edge,
	 * with cranes, warehouses and ships alongside. Roofs and walls are those of its part of Europe. OutQuarters
	 * takes the middles of its lit quarters, on the map, for its glow by night.
	 */
	void Township(const FTownSite& Site, TArray<FVector>* OutChimneys = nullptr, TArray<FVector2D>* OutQuarters = nullptr);
	/**
	 * One of the buildings a town is given by its owner, standing at the town's edge: Line is 0 a market, 1 farms,
	 * 2 a school, 3 a drill ground, 4 a hospital, 5 a town hall. Each grows with its level (1 to 3); bGoing puts
	 * builders' scaffolding beside it (or alone, at level 0) while the next level goes up.
	 */
	void Civic(const FVector& Where, double Unit, int32 Line, int32 Level, bool bGoing, const FLinearColor& Nation);

	// ---- The figures. Each stands at Where, turned by Turn degrees, in the nation's colour.

	void Rifleman(const FVector& Where, double Scale, const FLinearColor& Nation, double Turn = 0.0);
	void Tank(const FVector& Where, double Scale, const FLinearColor& Nation, double Turn = 0.0);
	void Lorry(const FVector& Where, double Scale, const FLinearColor& Nation, double Turn = 0.0);
	void Horseman(const FVector& Where, double Scale, const FLinearColor& Nation, double Turn = 0.0);
	void Bunker(const FVector& Where, double Scale, const FLinearColor& Nation, double Turn = 0.0);
	void FieldGun(const FVector& Where, double Scale, const FLinearColor& Nation, double Turn = 0.0);
	void Crag(const FVector& Where, double Scale);

	/**
	 * The figures for a force of these kinds of corps (inf, arm, art, mot, cav, mtn, fort), set out on a round base
	 * of the radius given: armour to the fore, the foot behind and beside it. Top is the height of the base.
	 */
	void Force(const TArray<FString>& Kinds, double BaseRadius, double Top, double Unit, const FLinearColor& Nation, const FString& Tag);

	/**
	 * What a nation's soldiers wore and what its vehicles were painted in the war years: German field grey and
	 * tank grey, American olive drab, British khaki, and so on. A nation not listed wears a plain khaki.
	 */
	static FLinearColor UniformColour(const FString& Tag);
	static FLinearColor VehicleColour(const FString& Tag);

	// ---- The bare makings, for the files that build models.
	int32 Add(const FVector& Position, const FVector& Normal, const FColor& Colour);
	void Face(int32 A, int32 B, int32 C, const FVector& Outward);
	void Quad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& Outward, const FColor& Colour);
};
