#pragma once

// The engine's names today's light and haze lines use, stood in for the cloud (the release 2 ocean plan, step 3:
// Tools/sea_waves/sea_day_test.cpp holds the light core's seed mode to the game's own text, cut out of Battle/SeaBattle.cpp,
// Battle/AirBattle.cpp, Battle/AirStage.cpp, Battle/SeaUnder.cpp and Campaign/CampaignBattleLook.{h,cpp} by
// day_extract.py, without the engine). Each is written to behave as the engine's does where those lines hang on it:
//   - FMath's Clamp, Min, Max, IsNearlyEqual, DegreesToRadians and RadiansToDegrees (doubles: times pi / 180 and 180 /
//     pi) as UnrealMathUtility.h has them; Sin, Cos, Asin, Atan, Atan2, Sqrt, Pow and Fmod as the C library's (the
//     engine's own Atan2 is a fit of its own: so seed mode takes the stage's sun as the scene worked it out, and the
//     cloud holds the core's sun to the same text in the same maths, not to the engine's last bit);
//   - FLinearColor and FVector2D as values; FString, TArray, FCommandLine, FParse (a switch found anywhere on the line,
//     whatever the case, its value to the next space, or to a comma as the engine's separator rule; a float or a double
//     read from just after it) and FCString::Atod;
//   - the height fog component's four numbers and colour, set as its setters set them; a lamp's temperature; a material's
//     parameters, kept by name;
//   - UE_LOG, kept as lines.
// Nothing here is the game's: what is checked is the game's own text compiled against these.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

using int32 = std::int32_t;
using uint64 = std::uint64_t;
using TCHAR = char;
#define TEXT(x) x
#define UE_DOUBLE_PI (3.141592653589793238462643383279502884197169399)

namespace DayShim
{
	/** The command line the game's lines read (empty: nothing asked). */
	inline std::string Line;
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
	/** Where Match is in Stream, whatever the case (FCString::Strifind): null where it is not. */
	inline const char* Find(const char* Stream, const char* Match)
	{
		const size_t N = std::char_traits<char>::length(Match);
		for (const char* At = Stream; *At; ++At)
		{
			size_t K = 0;
			while (K < N && At[K] && std::tolower(static_cast<unsigned char>(At[K])) == std::tolower(static_cast<unsigned char>(Match[K]))) ++K;
			if (K == N) return At;
		}
		return nullptr;
	}
}

struct FLogCategoryTheater
{
};
inline FLogCategoryTheater LogTheater;
#define UE_LOG(Category, Verbosity, Format, ...) ((void)(Category), DayShim::Log(Format, ##__VA_ARGS__))

struct FMath
{
	template <class T>
	static T Clamp(const T X, const T Lo, const T Hi) { return X < Lo ? Lo : X < Hi ? X : Hi; }
	template <class T>
	static T Min(const T A, const T B) { return A <= B ? A : B; }
	template <class T>
	static T Max(const T A, const T B) { return A >= B ? A : B; }
	static bool IsNearlyEqual(float A, float B, float Tolerance) { return std::fabs(A - B) <= Tolerance; }
	static double DegreesToRadians(double A) { return A * (UE_DOUBLE_PI / 180.0); }
	static double RadiansToDegrees(double A) { return A * (180.0 / UE_DOUBLE_PI); }
	static double Sin(double A) { return std::sin(A); }
	static double Cos(double A) { return std::cos(A); }
	static double Asin(double A) { return std::asin(A); }
	static double Atan(double A) { return std::atan(A); }
	static double Atan2(double Y, double X) { return std::atan2(Y, X); }
	static double Sqrt(double A) { return std::sqrt(A); }
	static double Pow(double A, double B) { return std::pow(A, B); }
	static double Fmod(double A, double B) { return std::fmod(A, B); }
};

struct FLinearColor
{
	float R = 0.0f, G = 0.0f, B = 0.0f, A = 0.0f;
	FLinearColor() = default;
	FLinearColor(float InR, float InG, float InB, float InA = 1.0f) : R(InR), G(InG), B(InB), A(InA) {}
	FLinearColor operator*(float S) const { return FLinearColor(R * S, G * S, B * S, A * S); }
	static const FLinearColor White;
};
inline const FLinearColor FLinearColor::White(1.0f, 1.0f, 1.0f, 1.0f);

struct FVector
{
	double X = 0.0, Y = 0.0, Z = 0.0;
	FVector() = default;
	FVector(double InX, double InY, double InZ) : X(InX), Y(InY), Z(InZ) {}
};

struct FVector2D
{
	double X = 0.0, Y = 0.0;
	FVector2D() = default;
	FVector2D(double InX, double InY) : X(InX), Y(InY) {}
};

template <class T>
struct TArray
{
	std::vector<T> Items;
	TArray() = default;
	TArray(std::initializer_list<T> In) : Items(In) {}
	int32 Num() const { return static_cast<int32>(Items.size()); }
	bool IsValidIndex(int32 K) const { return K >= 0 && K < Num(); }
	T& operator[](int32 K) { return Items[static_cast<size_t>(K)]; }
	const T& operator[](int32 K) const { return Items[static_cast<size_t>(K)]; }
	void Add(const T& V) { Items.push_back(V); }
	void Empty() { Items.clear(); }
};

struct FString
{
	std::string S;
	FString() = default;
	FString(const char* In) : S(In ? In : "") {}
	explicit FString(std::string In) : S(std::move(In)) {}
	const char* operator*() const { return S.c_str(); }
	bool IsEmpty() const { return S.empty(); }
	/** As the engine's: whatever the case. */
	bool Contains(const char* Match) const { return DayShim::Find(S.c_str(), Match) != nullptr; }
	FString TrimStartAndEnd() const
	{
		size_t A = 0, B = S.size();
		while (A < B && std::isspace(static_cast<unsigned char>(S[A]))) ++A;
		while (B > A && std::isspace(static_cast<unsigned char>(S[B - 1]))) --B;
		return FString(S.substr(A, B - A));
	}
	int32 ParseIntoArray(TArray<FString>& Out, const char* Delimiter, bool bCullEmpty) const
	{
		Out.Empty();
		const std::string D(Delimiter);
		size_t At = 0;
		while (true)
		{
			const size_t Next = S.find(D, At);
			const std::string Bit = S.substr(At, Next == std::string::npos ? std::string::npos : Next - At);
			if (!bCullEmpty || !Bit.empty()) Out.Add(FString(Bit));
			if (Next == std::string::npos) break;
			At = Next + D.size();
		}
		return Out.Num();
	}
};

struct FCommandLine
{
	static const TCHAR* Get() { return DayShim::Line.c_str(); }
};

struct FCString
{
	static double Atod(const TCHAR* Text) { return std::atof(Text); }
};

struct FParse
{
	static bool Value(const TCHAR* Stream, const TCHAR* Match, FString& Out, bool bShouldStopOnSeparator = true)
	{
		const char* At = DayShim::Find(Stream, Match);
		if (!At) return false;
		At += std::char_traits<char>::length(Match);
		std::string V;
		for (; *At && !std::isspace(static_cast<unsigned char>(*At)); ++At)
		{
			if (bShouldStopOnSeparator && (*At == ',' || *At == ')')) break;
			V += *At;
		}
		Out = FString(V);
		return true;
	}
	static bool Value(const TCHAR* Stream, const TCHAR* Match, float& Out)
	{
		const char* At = DayShim::Find(Stream, Match);
		if (!At) return false;
		Out = static_cast<float>(std::atof(At + std::char_traits<char>::length(Match)));
		return true;
	}
	static bool Value(const TCHAR* Stream, const TCHAR* Match, double& Out)
	{
		const char* At = DayShim::Find(Stream, Match);
		if (!At) return false;
		Out = std::atof(At + std::char_traits<char>::length(Match));
		return true;
	}
	/** A flag, as a whole word after a '-' or '/'. */
	static bool Param(const TCHAR* Stream, const TCHAR* Match)
	{
		const size_t N = std::char_traits<char>::length(Match);
		for (const char* At = DayShim::Find(Stream, Match); At; At = DayShim::Find(At + 1, Match))
		{
			const bool bStart = At > Stream && (At[-1] == '-' || At[-1] == '/');
			const char End = At[N];
			if (bStart && (End == 0 || std::isspace(static_cast<unsigned char>(End)) || End == '=')) return true;
		}
		return false;
	}
};

/** The height fog component's numbers, as its setters leave them (world units; FogDensity and FogHeightFalloff as the
 * editor shows them). */
struct UExponentialHeightFogComponent
{
	float StartDistance = 0.0f, FogDensity = 0.02f, FogHeightFalloff = 0.2f, FogMaxOpacity = 1.0f;
	FLinearColor FogInscatteringColor = FLinearColor(0.447f, 0.638f, 1.0f, 1.0f);
	void SetStartDistance(float V) { StartDistance = V; }
	void SetFogDensity(float V) { FogDensity = V; }
	void SetFogHeightFalloff(float V) { FogHeightFalloff = V; }
	void SetFogMaxOpacity(float V) { FogMaxOpacity = V; }
	void SetFogInscatteringColor(const FLinearColor& V) { FogInscatteringColor = V; }
};

/** A lamp's temperature, as LightTheBattle reads and sets it. */
struct FLampStandIn
{
	float Temperature = 6500.0f;
	int32 Sets = 0;
	void SetTemperature(float V)
	{
		Temperature = V;
		++Sets;
	}
};

/** U1's view under the water, as LightTheBattle asks it (absent above the water). */
struct FUnderStandIn
{
	float Share = 1.0f;
	float LightShare() const { return Share; }
};

/** A material's parameters, kept by name. */
struct FMaterialStandIn
{
	std::map<std::string, FLinearColor> Vectors;
	std::map<std::string, float> Scalars;
	void SetVectorParameterValue(const char* Name, const FLinearColor& V) { Vectors[Name] = V; }
	void SetScalarParameterValue(const char* Name, float V) { Scalars[Name] = V; }
};

/** The air scene's state, as its haze asks it: over the sea (a stage) or over land with none. */
struct FAirStateStandIn
{
	struct FBoard
	{
		std::string StageKey;
		std::vector<int32> Land;
	};
	struct FHeld
	{
		FBoard Board;
	};
	FHeld Held;
	bool bValid = true;
	bool IsValid() const { return bValid; }
	const FHeld* operator->() const { return &Held; }
};
