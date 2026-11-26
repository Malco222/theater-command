#pragma once

// The engine's stand-ins for the ship branch of FMiniatureMesh::Sculpted, cut out of Campaign/CampaignMiniatures.cpp
// (extract_paint.py), so ship_paint_test builds it with no engine: FLinearColor and FColor as the engine has them (four
// floats; four bytes), FMath's Clamp, Min and Lerp (the engine's own template: A + Alpha * (B - A), through the colour's
// own operators), a TArray over std::vector, the reader's raw model and the fresh mesh that takes each point. Only what
// the branch touches. The sRGB table and ToFColor are the standard's (the engine's table agrees to well under a level);
// both the reference and the tree's own run through the same, so they are compared like for like, and ToFColor keeps the
// floats it was handed, so the test compares those bit for bit as well as the bytes.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

using int32 = std::int32_t;
using uint8 = std::uint8_t;

template <typename T>
struct TArray : std::vector<T>
{
	int32 Num() const { return static_cast<int32>(this->size()); }
	bool IsValidIndex(int32 Index) const { return Index >= 0 && Index < Num(); }
	void Add(const T& One) { this->push_back(One); }
	T* GetData() { return this->data(); }
	const T* GetData() const { return this->data(); }
};

struct FVector
{
	double X = 0.0, Y = 0.0, Z = 0.0;
};

struct FColor
{
	uint8 B = 0, G = 0, R = 0, A = 255;
	FColor() = default;
	FColor(uint8 InR, uint8 InG, uint8 InB, uint8 InA = 255) : B(InB), G(InG), R(InR), A(InA) {}
	static FColor FromHex(const char* Hex)
	{
		auto Nibble = [](char C) { return C >= '0' && C <= '9' ? C - '0' : C >= 'a' && C <= 'f' ? C - 'a' + 10 : C >= 'A' && C <= 'F' ? C - 'A' + 10 : 0; };
		const char* P = Hex[0] == '#' ? Hex + 1 : Hex;
		auto Byte = [&](int K) { return static_cast<uint8>(Nibble(P[2 * K]) * 16 + Nibble(P[2 * K + 1])); };
		return std::strlen(P) >= 8 ? FColor(Byte(0), Byte(1), Byte(2), Byte(3)) : FColor(Byte(0), Byte(1), Byte(2), 255);
	}
};

namespace PaintShim
{
	/** The floats the last ToFColor was handed (the test compares them bit for bit). */
	inline float LastLinear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	inline float SrgbToLinear(uint8 Level)
	{
		const double C = Level / 255.0;
		return static_cast<float>(C <= 0.04045 ? C / 12.92 : std::pow((C + 0.055) / 1.055, 2.4));
	}

	inline uint8 LinearToSrgb(float Lin)
	{
		const double C = std::min(1.0, std::max(0.0, static_cast<double>(Lin)));
		const double S = C <= 0.0031308 ? C * 12.92 : 1.055 * std::pow(C, 1.0 / 2.4) - 0.055;
		return static_cast<uint8>(std::floor(S * 255.0 + 0.5));
	}
}

struct FLinearColor
{
	float R = 0.0f, G = 0.0f, B = 0.0f, A = 0.0f;
	FLinearColor() = default;
	FLinearColor(float InR, float InG, float InB, float InA = 1.0f) : R(InR), G(InG), B(InB), A(InA) {}
	explicit FLinearColor(const FColor& C) : R(PaintShim::SrgbToLinear(C.R)), G(PaintShim::SrgbToLinear(C.G)), B(PaintShim::SrgbToLinear(C.B)), A(C.A / 255.0f) {}
	// (the engine's own operators, a channel at a time)
	FLinearColor operator+(const FLinearColor& ColorB) const { return FLinearColor(R + ColorB.R, G + ColorB.G, B + ColorB.B, A + ColorB.A); }
	FLinearColor operator-(const FLinearColor& ColorB) const { return FLinearColor(R - ColorB.R, G - ColorB.G, B - ColorB.B, A - ColorB.A); }
	FLinearColor operator*(float Scalar) const { return FLinearColor(R * Scalar, G * Scalar, B * Scalar, A * Scalar); }
	FColor ToFColor(bool bSRGB) const
	{
		PaintShim::LastLinear[0] = R;
		PaintShim::LastLinear[1] = G;
		PaintShim::LastLinear[2] = B;
		PaintShim::LastLinear[3] = A;
		const auto Byte = [bSRGB](float V) { return bSRGB ? PaintShim::LinearToSrgb(V) : static_cast<uint8>(std::floor(std::min(1.0f, std::max(0.0f, V)) * 255.0f + 0.5f)); };
		return FColor(Byte(R), Byte(G), Byte(B), static_cast<uint8>(std::floor(std::min(1.0f, std::max(0.0f, A)) * 255.0f + 0.5f)));
	}
};
inline FLinearColor operator*(float Scalar, const FLinearColor& Color)
{
	return Color.operator*(Scalar);
}

struct FMath
{
	template <class T>
	static T Clamp(const T X, const T Min, const T Max) { return Max < X ? Max : X < Min ? Min : X; }
	template <class T>
	static T Min(const T A, const T B) { return A <= B ? A : B; }
	// (UnrealMathUtility.h's own: "return (T)(A + Alpha * (B-A));")
	template <class T, class U>
	static T Lerp(const T& A, const T& B, const U& Alpha) { return (T)(A + Alpha * (B - A)); }
};

/** The reader's raw ship model, as much of it as the branch reads. */
struct FRawShip
{
	TArray<FVector> Points, Facings;
	TArray<int32> Paints, Triangles;
	TArray<float> Worn; // (the file's "shade")
	bool bShip = true;
	TArray<int32> Salt, Wear, Soot;
	TArray<bool> ShipMetal, ShipPainted;
	TArray<bool> ShipPlanks;
	FLinearColor Rust = FLinearColor(FColor::FromHex("8A4E28"));
	TArray<FLinearColor> PaintColours;
};

/** The fresh mesh: each point's colour as the branch adds it (its alpha the finish), and the floats it was made from. */
struct FFreshShip
{
	uint8 Finish = 255;
	std::vector<FColor> Colours;
	std::vector<float> Linear; // (four a point)
	void Add(const FVector&, const FVector&, const FColor& Colour)
	{
		Colours.push_back(FColor(Colour.R, Colour.G, Colour.B, Finish));
		Linear.insert(Linear.end(), PaintShim::LastLinear, PaintShim::LastLinear + 4);
	}
};
