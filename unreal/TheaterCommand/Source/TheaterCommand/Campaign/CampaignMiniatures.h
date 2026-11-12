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

	/** A ball, or an egg if Size is not the same each way. */
	void Ball(const FVector& Center, const FVector& Size, const FLinearColor& Colour, int32 Rings = 6, int32 Sides = 10);

	/** A flat shape with upright sides: an outline (in the figure's own plan, x forward, y to its right) raised from Foot to Top. */
	void Slab(const FVector& Where, double Scale, double Turn, const TArray<FVector2D>& Outline, double Foot, double Top, const FLinearColor& Colour);

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
	void Fleet(int32 Battleships, int32 Carriers, int32 Cruisers, int32 Destroyers, int32 Submarines, double Unit, const FLinearColor& Nation);

	/** An air fleet: a fighter and a bomber (whichever it has) over a patch of airfield. */
	void AirFleet(int32 Fighters, int32 Bombers, double Unit, const FLinearColor& Nation, const FString& Tag);

	// ---- Scenery for the table.

	/** A house: four walls and a pitched roof, its ridge along its length. */
	void House(const FVector& Where, double Length, double Width, double WallHeight, double RoofHeight, const FLinearColor& Wall, const FLinearColor& Roof, double Turn = 0.0);

	/** A fir: a cone. A broad-leaved tree: a ball on a short trunk. */
	void Fir(const FVector& Where, double Height, double Radius, const FLinearColor& Leaf);
	void Broadleaf(const FVector& Where, double Height, double Radius, const FLinearColor& Leaf);

	/**
	 * A town standing on the table: a cluster of houses, more of them for a city, a great building with a dome
	 * for a capital, a church for a city, and a works with a chimney for each pair of factories.
	 */
	void Town(const FVector& Where, double Unit, int32 Houses, bool bCapital, bool bCity, int32 Works, int32 Seed);

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

private:
	int32 Add(const FVector& Position, const FVector& Normal, const FColor& Colour);
	void Face(int32 A, int32 B, int32 C, const FVector& Outward);
	void Quad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& Outward, const FColor& Colour);
};
