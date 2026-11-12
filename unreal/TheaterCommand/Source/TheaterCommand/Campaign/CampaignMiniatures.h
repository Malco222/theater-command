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
	void Force(const TArray<FString>& Kinds, double BaseRadius, double Top, double Unit, const FLinearColor& Nation);

private:
	int32 Add(const FVector& Position, const FVector& Normal, const FColor& Colour);
	void Face(int32 A, int32 B, int32 C, const FVector& Outward);
	void Quad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& Outward, const FColor& Colour);
};
