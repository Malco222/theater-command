#pragma once

// The engine's names DrawMarks uses, stood in for the cloud (the release 2 ocean plan, step 1: Tools/sea_waves/
// marks_pool_test.cpp plays the game's own DrawMarks, cut out of Battle/SeaBattle.cpp by marks_extract.py, without the
// engine). Each is written to behave as the engine's does where what DrawMarks draws hangs on it:
//   - FMath's Clamp, Min, Max, Lerp, RoundToInt32, FloorToInt32 and Frac as UnrealMathUtility.h has them;
//   - TArray, TMap and TSet as containers, a TMap walked in the order the engine's would (its sparse array: in the order
//     added, a removed pair's place taken again by the next added, the last freed first);
//   - UProceduralMeshComponent's CreateMeshSection, UpdateMeshSection, ClearMeshSection and SetMaterial as
//     ProceduralMeshComponent.cpp and MeshComponent.cpp have them: a section made afresh (every point, a normal (0,0,1), a
//     UV of 0 and white where an array has not one a point; a whole number of triangles, each index past the last point
//     taken as the last, every triangle kept, one with a point twice too (no area: the chip draws nothing of it); the
//     bounds over every point; the render state dirtied, so the proxy is made afresh); its points sent again only with as
//     many as it holds, each array taken only where it holds one a point, else left as it was (else the engine's error,
//     counted here), the render state kept; cleared to nothing (the render state dirtied, where there was a section); a
//     material set dirtying the render state only when it changes.
// Nothing here is the game's: what is drawn is the game's own text played through these.

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using int32 = std::int32_t;
using uint8 = std::uint8_t;
using uint32 = std::uint32_t;
using int64 = std::int64_t;
using uint64 = std::uint64_t;
using TCHAR = char;
#define TEXT(x) x
#define UTF8_TO_TCHAR(x) (x)
#define TCHAR_TO_UTF8(x) (x)
#define UE_DOUBLE_PI (3.141592653589793238462643383279502884197169399)
#define INDEX_NONE (-1)

namespace MarksShim
{
	[[noreturn]] inline void Fail(const char* What)
	{
		std::fprintf(stderr, "marks_shim: %s\n", What);
		std::abort();
	}
	/** What the game logged (UE_LOG), in order. */
	inline std::vector<std::string> Logged;
	__attribute__((format(printf, 1, 2))) inline void Log(const char* Format, ...)
	{
		char Text[1024];
		va_list Args;
		va_start(Args, Format);
		std::vsnprintf(Text, sizeof(Text), Format, Args);
		va_end(Args);
		Logged.push_back(Text);
	}
}

struct FLogCategoryTheater
{
};
inline FLogCategoryTheater LogTheater;
#define UE_LOG(Category, Verbosity, Format, ...) ((void)(Category), MarksShim::Log(Format, ##__VA_ARGS__))

struct FMath
{
	template <class T>
	static T Clamp(const T X, const T Lo, const T Hi) { return X < Lo ? Lo : X < Hi ? X : Hi; }
	template <class T>
	static T Min(const T A, const T B) { return A <= B ? A : B; }
	template <class T>
	static T Max(const T A, const T B) { return A >= B ? A : B; }
	template <class A, class B, class = std::enable_if_t<!std::is_same_v<A, B>>>
	static std::common_type_t<A, B> Min(const A X, const B Y) { return Min<std::common_type_t<A, B>>(X, Y); }
	template <class A, class B, class = std::enable_if_t<!std::is_same_v<A, B>>>
	static std::common_type_t<A, B> Max(const A X, const B Y) { return Max<std::common_type_t<A, B>>(X, Y); }
	template <class T, class U>
	static T Lerp(const T& A, const T& B, const U& Alpha) { return static_cast<T>(A + Alpha * (B - A)); }
	static double Pow(double A, double B) { return std::pow(A, B); }
	static double Sqrt(double A) { return std::sqrt(A); }
	static double Sin(double A) { return std::sin(A); }
	static double Cos(double A) { return std::cos(A); }
	static double Abs(double A) { return std::fabs(A); }
	static double Frac(double A) { return A - std::floor(A); }
	static int32 FloorToInt32(double A) { return static_cast<int32>(std::floor(A)); }
	static int32 RoundToInt32(double A) { return FloorToInt32(A + 0.5); }
	static double DegreesToRadians(double A) { return A * (UE_DOUBLE_PI / 180.0); }
};

struct FVector
{
	double X = 0.0, Y = 0.0, Z = 0.0;
	FVector() = default;
	FVector(double InX, double InY, double InZ) : X(InX), Y(InY), Z(InZ) {}
	FVector operator+(const FVector& V) const { return FVector(X + V.X, Y + V.Y, Z + V.Z); }
	FVector operator-(const FVector& V) const { return FVector(X - V.X, Y - V.Y, Z - V.Z); }
	FVector operator*(double S) const { return FVector(X * S, Y * S, Z * S); }
	FVector operator/(double S) const { return FVector(X / S, Y / S, Z / S); }
	/** The cross product, as the engine's ^. */
	FVector operator^(const FVector& V) const { return FVector(Y * V.Z - Z * V.Y, Z * V.X - X * V.Z, X * V.Y - Y * V.X); }
	bool operator==(const FVector& V) const { return X == V.X && Y == V.Y && Z == V.Z; }
	bool operator!=(const FVector& V) const { return !(*this == V); }
	static const FVector ZeroVector;
	static const FVector UpVector;
};
inline const FVector FVector::ZeroVector(0.0, 0.0, 0.0);
inline const FVector FVector::UpVector(0.0, 0.0, 1.0);

struct FVector2D
{
	double X = 0.0, Y = 0.0;
	FVector2D() = default;
	FVector2D(double InX, double InY) : X(InX), Y(InY) {}
	FVector2D operator+(const FVector2D& V) const { return FVector2D(X + V.X, Y + V.Y); }
	FVector2D operator-(const FVector2D& V) const { return FVector2D(X - V.X, Y - V.Y); }
	FVector2D operator*(double S) const { return FVector2D(X * S, Y * S); }
	FVector2D operator/(double S) const { return FVector2D(X / S, Y / S); }
	bool operator==(const FVector2D& V) const { return X == V.X && Y == V.Y; }
	bool operator!=(const FVector2D& V) const { return !(*this == V); }
	FVector2D GetSafeNormal(double Tolerance = 1.e-8) const
	{
		const double Sum = X * X + Y * Y;
		if (Sum > Tolerance)
		{
			const double Scale = 1.0 / std::sqrt(Sum);
			return FVector2D(X * Scale, Y * Scale);
		}
		return FVector2D(0.0, 0.0);
	}
	static double Distance(const FVector2D& A, const FVector2D& B) { return std::sqrt((A.X - B.X) * (A.X - B.X) + (A.Y - B.Y) * (A.Y - B.Y)); }
	static const FVector2D ZeroVector;
};
inline const FVector2D FVector2D::ZeroVector(0.0, 0.0);

struct FColor
{
	uint8 R = 0, G = 0, B = 0, A = 0;
	FColor() = default;
	FColor(uint8 InR, uint8 InG, uint8 InB, uint8 InA = 255) : R(InR), G(InG), B(InB), A(InA) {}
	bool operator==(const FColor& C) const { return R == C.R && G == C.G && B == C.B && A == C.A; }
	bool operator!=(const FColor& C) const { return !(*this == C); }
};

template <class K, class V>
struct TPair
{
	K Key{};
	V Value{};
	TPair() = default;
	TPair(const K& InKey, const V& InValue) : Key(InKey), Value(InValue) {}
};

template <class T>
class TArray
{
	std::vector<T> D;

public:
	TArray() = default;
	TArray(std::initializer_list<T> List) : D(List) {}
	int32 Num() const { return static_cast<int32>(D.size()); }
	bool IsValidIndex(int32 K) const { return K >= 0 && K < Num(); }
	T& operator[](int32 K)
	{
		if (!IsValidIndex(K)) MarksShim::Fail("a TArray read past its end");
		return D[static_cast<size_t>(K)];
	}
	const T& operator[](int32 K) const
	{
		if (!IsValidIndex(K)) MarksShim::Fail("a TArray read past its end");
		return D[static_cast<size_t>(K)];
	}
	int32 Add(const T& X)
	{
		D.push_back(X);
		return Num() - 1;
	}
	int32 Add(T&& X)
	{
		D.push_back(std::move(X));
		return Num() - 1;
	}
	T& AddDefaulted_GetRef()
	{
		D.emplace_back();
		return D.back();
	}
	void Append(std::initializer_list<T> List) { D.insert(D.end(), List.begin(), List.end()); }
	void Append(const TArray& Other) { D.insert(D.end(), Other.D.begin(), Other.D.end()); }
	T& Last()
	{
		if (D.empty()) MarksShim::Fail("Last() of an empty TArray");
		return D.back();
	}
	const T& Last() const
	{
		if (D.empty()) MarksShim::Fail("Last() of an empty TArray");
		return D.back();
	}
	void RemoveAt(int32 K, int32 Count = 1)
	{
		if (K < 0 || Count < 0 || K + Count > Num()) MarksShim::Fail("RemoveAt past a TArray's end");
		D.erase(D.begin() + K, D.begin() + K + Count);
	}
	void Reset(int32 Slack = 0)
	{
		D.clear();
		D.reserve(static_cast<size_t>(Slack));
	}
	void Empty() { D.clear(); }
	void Reserve(int32 N) { D.reserve(static_cast<size_t>(N)); }
	void SetNum(int32 N) { D.resize(static_cast<size_t>(N)); }
	void SetNumUninitialized(int32 N) { D.resize(static_cast<size_t>(N)); }
	template <class P>
	void Sort(const P& Pred) { std::sort(D.begin(), D.end(), Pred); }
	T* GetData() { return D.data(); }
	const T* GetData() const { return D.data(); }
	auto begin() { return D.begin(); }
	auto end() { return D.end(); }
	auto begin() const { return D.begin(); }
	auto end() const { return D.end(); }
};

/** The engine's TMap, walked as its sparse array walks: in the order added, a removed pair's place taken by the next
 * added (the last freed first), a value added again for a key already there put in its place. */
template <class KeyT, class ValueT>
class TMap
{
	struct Place
	{
		bool bUsed = false;
		TPair<KeyT, ValueT> Pair;
	};
	std::vector<Place> Places;
	std::vector<int32> Free;
	std::unordered_map<KeyT, int32> At;

	int32 Lay(const KeyT& Key)
	{
		int32 Where;
		if (!Free.empty())
		{
			Where = Free.back();
			Free.pop_back();
		}
		else
		{
			Where = static_cast<int32>(Places.size());
			Places.emplace_back();
		}
		Places[static_cast<size_t>(Where)].bUsed = true;
		Places[static_cast<size_t>(Where)].Pair = TPair<KeyT, ValueT>(Key, ValueT());
		At[Key] = Where;
		return Where;
	}
	void Drop(int32 Where)
	{
		Place& P = Places[static_cast<size_t>(Where)];
		At.erase(P.Pair.Key);
		P.bUsed = false;
		P.Pair = TPair<KeyT, ValueT>();
		Free.push_back(Where);
	}

public:
	int32 Num() const { return static_cast<int32>(At.size()); }
	bool Contains(const KeyT& Key) const { return At.count(Key) > 0; }
	ValueT* Find(const KeyT& Key)
	{
		const auto It = At.find(Key);
		return It == At.end() ? nullptr : &Places[static_cast<size_t>(It->second)].Pair.Value;
	}
	const ValueT* Find(const KeyT& Key) const
	{
		const auto It = At.find(Key);
		return It == At.end() ? nullptr : &Places[static_cast<size_t>(It->second)].Pair.Value;
	}
	ValueT& operator[](const KeyT& Key)
	{
		ValueT* Found = Find(Key);
		if (!Found) MarksShim::Fail("a TMap asked for a key it does not hold");
		return *Found;
	}
	ValueT& FindOrAdd(const KeyT& Key)
	{
		ValueT* Found = Find(Key);
		return Found ? *Found : Places[static_cast<size_t>(Lay(Key))].Pair.Value;
	}
	ValueT& Add(const KeyT& Key, const ValueT& Value)
	{
		ValueT& Into = FindOrAdd(Key);
		Into = Value;
		return Into;
	}
	ValueT& Add(const KeyT& Key)
	{
		ValueT& Into = FindOrAdd(Key);
		Into = ValueT();
		return Into;
	}
	int32 Remove(const KeyT& Key)
	{
		const auto It = At.find(Key);
		if (It == At.end()) return 0;
		Drop(It->second);
		return 1;
	}
	void Empty()
	{
		Places.clear();
		Free.clear();
		At.clear();
	}

	class TIterator
	{
		TMap* Map;
		int32 Index;
		void Skip()
		{
			while (Index < static_cast<int32>(Map->Places.size()) && !Map->Places[static_cast<size_t>(Index)].bUsed) ++Index;
		}

	public:
		explicit TIterator(TMap* InMap) : Map(InMap), Index(0) { Skip(); }
		explicit operator bool() const { return Index < static_cast<int32>(Map->Places.size()); }
		TIterator& operator++()
		{
			++Index;
			Skip();
			return *this;
		}
		const KeyT& Key() const { return Map->Places[static_cast<size_t>(Index)].Pair.Key; }
		ValueT& Value() const { return Map->Places[static_cast<size_t>(Index)].Pair.Value; }
		void RemoveCurrent() { Map->Drop(Index); }
	};
	TIterator CreateIterator() { return TIterator(this); }

	template <class MapT, class PairT>
	class TRange
	{
		MapT* Map;
		size_t Index;
		void Skip()
		{
			while (Index < Map->Places.size() && !Map->Places[Index].bUsed) ++Index;
		}

	public:
		TRange(MapT* InMap, size_t InIndex) : Map(InMap), Index(InIndex) { Skip(); }
		PairT& operator*() const { return Map->Places[Index].Pair; }
		TRange& operator++()
		{
			++Index;
			Skip();
			return *this;
		}
		bool operator!=(const TRange& Other) const { return Index != Other.Index; }
	};
	TRange<TMap, TPair<KeyT, ValueT>> begin() { return TRange<TMap, TPair<KeyT, ValueT>>(this, 0); }
	TRange<TMap, TPair<KeyT, ValueT>> end() { return TRange<TMap, TPair<KeyT, ValueT>>(this, Places.size()); }
	TRange<const TMap, const TPair<KeyT, ValueT>> begin() const { return TRange<const TMap, const TPair<KeyT, ValueT>>(this, 0); }
	TRange<const TMap, const TPair<KeyT, ValueT>> end() const { return TRange<const TMap, const TPair<KeyT, ValueT>>(this, Places.size()); }
};

template <class T>
class TSet
{
	std::unordered_set<T> D;

public:
	void Add(const T& X) { D.insert(X); }
	bool Contains(const T& X) const { return D.count(X) > 0; }
	int32 Remove(const T& X) { return static_cast<int32>(D.erase(X)); }
	int32 Num() const { return static_cast<int32>(D.size()); }
	void Empty() { D.clear(); }
};

template <class T>
using TSharedPtr = std::shared_ptr<T>;
template <class T>
using TObjectPtr = T*;

struct FString
{
	std::string S;
	FString() = default;
	FString(const char* Text) : S(Text) {}
	const char* operator*() const { return S.c_str(); }
};

// ---------------------------------------------------------------------------------------------------------------------
// The procedural mesh, as ProceduralMeshComponent.cpp makes, sends again and clears its sections.

struct FProcMeshTangent
{
	FVector TangentX = FVector(1.0, 0.0, 0.0);
	bool bFlipTangentY = false;
	bool operator==(const FProcMeshTangent& T) const { return TangentX == T.TangentX && bFlipTangentY == T.bFlipTangentY; }
};

struct FProcMeshVertex
{
	FVector Position;
	FVector Normal = FVector(0.0, 0.0, 1.0);
	FProcMeshTangent Tangent;
	FColor Color = FColor(255, 255, 255);
	FVector2D UV0;
	bool operator==(const FProcMeshVertex& V) const { return Position == V.Position && Normal == V.Normal && Tangent == V.Tangent && Color == V.Color && UV0 == V.UV0; }
	bool operator!=(const FProcMeshVertex& V) const { return !(*this == V); }
};

struct FMarksBox
{
	FVector Min, Max;
	bool bValid = false;
	void Init() { bValid = false; }
	void Add(const FVector& P)
	{
		if (!bValid)
		{
			Min = Max = P;
			bValid = true;
			return;
		}
		Min = FVector(std::min(Min.X, P.X), std::min(Min.Y, P.Y), std::min(Min.Z, P.Z));
		Max = FVector(std::max(Max.X, P.X), std::max(Max.Y, P.Y), std::max(Max.Z, P.Z));
	}
	bool operator==(const FMarksBox& B) const { return bValid == B.bValid && (!bValid || (Min == B.Min && Max == B.Max)); }
};

struct FProcMeshSection
{
	std::vector<FProcMeshVertex> ProcVertexBuffer;
	std::vector<uint32> ProcIndexBuffer;
	FMarksBox SectionLocalBox;
	bool bSectionVisible = true;
	void Reset()
	{
		ProcVertexBuffer.clear();
		ProcIndexBuffer.clear();
		SectionLocalBox.Init();
		bSectionVisible = true;
	}
};

class UMaterialInterface
{
};
class UMaterialInstanceDynamic : public UMaterialInterface
{
};

class UProceduralMeshComponent
{
public:
	std::vector<FProcMeshSection> ProcMeshSections;
	std::vector<UMaterialInterface*> Materials;
	/** The calls since Tally() last cleared them: sections made, points sent again, sent again with the wrong number of
	 * points (the engine's error: nothing sent), cleared, and materials set; and the times the render state was dirtied
	 * (MarkRenderStateDirty: the engine makes the render proxy afresh at the frame's end, once however many times). */
	int Creates = 0, Updates = 0, Mismatched = 0, Clears = 0, MaterialsSet = 0, Dirtied = 0;
	void Tally() { Creates = Updates = Mismatched = Clears = MaterialsSet = Dirtied = 0; }

	void CreateMeshSection(int32 SectionIndex, const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UV0,
		const TArray<FColor>& VertexColors, const TArray<FProcMeshTangent>& Tangents, bool /*bCreateCollision*/)
	{
		++Creates;
		if (SectionIndex >= static_cast<int32>(ProcMeshSections.size())) ProcMeshSections.resize(static_cast<size_t>(SectionIndex) + 1);
		FProcMeshSection& Section = ProcMeshSections[static_cast<size_t>(SectionIndex)];
		Section.Reset();
		const int32 NumVerts = Vertices.Num();
		Section.ProcVertexBuffer.resize(static_cast<size_t>(NumVerts));
		for (int32 K = 0; K < NumVerts; ++K)
		{
			FProcMeshVertex& V = Section.ProcVertexBuffer[static_cast<size_t>(K)];
			V.Position = Vertices[K];
			V.Normal = Normals.Num() == NumVerts ? Normals[K] : FVector(0.0, 0.0, 1.0);
			V.UV0 = UV0.Num() == NumVerts ? UV0[K] : FVector2D(0.0, 0.0);
			V.Color = VertexColors.Num() == NumVerts ? VertexColors[K] : FColor(255, 255, 255);
			V.Tangent = Tangents.Num() == NumVerts ? Tangents[K] : FProcMeshTangent();
			Section.SectionLocalBox.Add(V.Position);
		}
		// (FMath::Min(Triangles[i], NumVerts - 1) for each of a whole number of triangles, every one kept)
		const int32 MaxIndex = NumVerts - 1;
		const int32 Whole = (Triangles.Num() / 3) * 3;
		Section.ProcIndexBuffer.resize(static_cast<size_t>(Whole));
		for (int32 K = 0; K < Whole; ++K)
		{
			Section.ProcIndexBuffer[static_cast<size_t>(K)] = static_cast<uint32>(std::min(Triangles[K], MaxIndex));
		}
		++Dirtied;
	}

	void UpdateMeshSection(int32 SectionIndex, const TArray<FVector>& Vertices, const TArray<FVector>& Normals, const TArray<FVector2D>& UV0, const TArray<FColor>& VertexColors,
		const TArray<FProcMeshTangent>& Tangents)
	{
		++Updates;
		if (SectionIndex >= static_cast<int32>(ProcMeshSections.size()))
		{
			++Mismatched;
			return;
		}
		FProcMeshSection& Section = ProcMeshSections[static_cast<size_t>(SectionIndex)];
		const int32 NumVerts = Vertices.Num();
		if (NumVerts != static_cast<int32>(Section.ProcVertexBuffer.size()))
		{
			++Mismatched; // (the engine: "Trying to update a procedural mesh component section with a different number of vertices")
			return;
		}
		Section.SectionLocalBox.Init();
		for (int32 K = 0; K < NumVerts; ++K)
		{
			FProcMeshVertex& V = Section.ProcVertexBuffer[static_cast<size_t>(K)];
			V.Position = Vertices[K];
			Section.SectionLocalBox.Add(V.Position);
			if (Normals.Num() == NumVerts) V.Normal = Normals[K];
			if (Tangents.Num() == NumVerts) V.Tangent = Tangents[K];
			if (UV0.Num() == NumVerts) V.UV0 = UV0[K];
			if (VertexColors.Num() == NumVerts) V.Color = VertexColors[K];
		}
	}

	void ClearMeshSection(int32 SectionIndex)
	{
		++Clears;
		if (SectionIndex < static_cast<int32>(ProcMeshSections.size()))
		{
			ProcMeshSections[static_cast<size_t>(SectionIndex)].Reset();
			++Dirtied;
		}
	}

	void SetMaterial(int32 ElementIndex, UMaterialInterface* Material)
	{
		++MaterialsSet;
		if (ElementIndex < static_cast<int32>(Materials.size()) && Materials[static_cast<size_t>(ElementIndex)] == Material) return; // (already set: nothing done)
		if (ElementIndex >= static_cast<int32>(Materials.size())) Materials.resize(static_cast<size_t>(ElementIndex) + 1, nullptr);
		Materials[static_cast<size_t>(ElementIndex)] = Material;
		++Dirtied;
	}
	UMaterialInterface* GetMaterial(int32 ElementIndex) const { return ElementIndex < static_cast<int32>(Materials.size()) ? Materials[static_cast<size_t>(ElementIndex)] : nullptr; }
};
