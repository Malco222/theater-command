// The light shapes' test (Battle/AirLight, AS1): every aircraft in Content/Models with a far part of more triangles than
// AirLight::LightMost leaves whole is taken down as the air battle takes it down for its copies past LightFromM, and
// the light shape is held to what can be seen at that distance. Usage: air_light_test <model.json ...>; a model that is
// not an aircraft is passed over.
//
// At LightFromM (1,500 m) through the battle's own lens (55 degrees across a frame drawn 1,077 pixels wide, before it is
// filled out to the screen) a pixel is 1.45 m. The light shape must lie within half a pixel of the far one, both ways
// (its points from the far surface, and the far points from its surface), with a mean under a tenth of a pixel; its
// length, span and height within half a pixel; each paint's share of the skin within three points of the far shape's;
// and no more triangles than asked.
#include "Battle/AirLight.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	/** The numbers of the array Key in the object that begins at the first Part ("\"far\": {"). */
	std::vector<double> PartNumbers(const std::string& Text, const char* Part, const char* Key)
	{
		std::vector<double> Out;
		size_t At = Text.find(std::string("\"") + Part + "\": {");
		if (At == std::string::npos) At = Text.find(std::string("\"") + Part + "\":{");
		const size_t Found = At == std::string::npos ? std::string::npos : Text.find(Key, At);
		const size_t Open = Found == std::string::npos ? std::string::npos : Text.find('[', Found);
		const size_t Close = Open == std::string::npos ? std::string::npos : Text.find(']', Open);
		if (Close == std::string::npos) return Out;
		const char* P = Text.c_str() + Open + 1;
		const char* End = Text.c_str() + Close;
		while (P < End)
		{
			char* Next = nullptr;
			const double V = std::strtod(P, &Next);
			if (Next == P) { ++P; continue; }
			Out.push_back(V);
			P = Next;
		}
		return Out;
	}

	struct Box
	{
		double Lo[3] = { 1e300, 1e300, 1e300 }, Hi[3] = { -1e300, -1e300, -1e300 };
		void Take(const std::vector<double>& Points)
		{
			for (size_t K = 0; K + 2 < Points.size(); K += 3)
				for (int A = 0; A < 3; ++A) { Lo[A] = std::min(Lo[A], Points[K + A]); Hi[A] = std::max(Hi[A], Points[K + A]); }
		}
	};

	/** Each paint's share of the skin (by area); a triangle's paint as AirLight takes it. */
	std::map<uint32_t, double> Shares(const std::vector<double>& Points, const std::vector<int>& Triangles, const std::vector<uint32_t>& Tones)
	{
		std::map<uint32_t, double> Out;
		double All = 0.0;
		for (size_t T = 0; T + 2 < Triangles.size(); T += 3)
		{
			const size_t I0 = static_cast<size_t>(Triangles[T]), I1 = static_cast<size_t>(Triangles[T + 1]), I2 = static_cast<size_t>(Triangles[T + 2]);
			const double AX = Points[I1 * 3] - Points[I0 * 3], AY = Points[I1 * 3 + 1] - Points[I0 * 3 + 1], AZ = Points[I1 * 3 + 2] - Points[I0 * 3 + 2];
			const double BX = Points[I2 * 3] - Points[I0 * 3], BY = Points[I2 * 3 + 1] - Points[I0 * 3 + 1], BZ = Points[I2 * 3 + 2] - Points[I0 * 3 + 2];
			const double CX = AY * BZ - AZ * BY, CY = AZ * BX - AX * BZ, CZ = AX * BY - AY * BX;
			const double Area = 0.5 * std::sqrt(CX * CX + CY * CY + CZ * CZ);
			const uint32_t T0 = Tones[I0], T1 = Tones[I1], T2 = Tones[I2];
			const uint32_t Tone = (T0 == T1 || T0 == T2) ? T0 : (T1 == T2 ? T1 : T0);
			Out[Tone] += Area;
			All += Area;
		}
		for (auto& Pair : Out) Pair.second = All > 0.0 ? Pair.second / All : 0.0;
		return Out;
	}
}

int main(int Argc, char** Argv)
{
	const double Pi = 3.14159265358979323846;
	const double Pixel = AirLight::LightFromM * 2.0 * std::tan(55.0 * 0.5 * Pi / 180.0) / 1077.0;
	std::printf("A pixel at %.0f m through the battle's lens: %.2f m\n", AirLight::LightFromM, Pixel);
	int Bad = 0, Models = 0, Lightened = 0;
	for (int A = 1; A < Argc; ++A)
	{
		std::ifstream In(Argv[A]);
		std::stringstream Buf;
		Buf << In.rdbuf();
		const std::string Text = Buf.str();
		if (Text.find("\"kind\": \"aircraft\"") == std::string::npos && Text.find("\"kind\":\"aircraft\"") == std::string::npos) continue;
		++Models;
		AirLight::Shape Far;
		Far.Points = PartNumbers(Text, "far", "\"points\"");
		for (const double V : PartNumbers(Text, "far", "\"triangles\"")) Far.Triangles.push_back(static_cast<int>(V));
		for (const double V : PartNumbers(Text, "far", "\"paints\"")) Far.Tones.push_back(static_cast<uint32_t>(V));
		const int FarTriangles = static_cast<int>(Far.Triangles.size() / 3);
		const int Most = AirLight::LightMost(FarTriangles);
		if (Far.Points.empty() || Far.Tones.size() * 3 != Far.Points.size())
		{
			std::printf("%s: no far part read  FAIL\n", Argv[A]);
			++Bad;
			continue;
		}
		if (Most <= 0)
		{
			std::printf("%s: far part %d triangles, drawn as it is\n", Argv[A], FarTriangles);
			continue;
		}
		++Lightened;
		const auto Began = std::chrono::steady_clock::now();
		const AirLight::Made Light = AirLight::Lighten(Far, Most);
		const double Ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Began).count();
		const AirLight::Apart Out = AirLight::Distance(Light.Points, Far.Points, Far.Triangles);
		const AirLight::Apart Back = AirLight::Distance(Far.Points, Light.Points, Light.Triangles);
		Box FarBox, LightBox;
		FarBox.Take(Far.Points);
		LightBox.Take(Light.Points);
		double Extent = 0.0;
		for (int K = 0; K < 3; ++K) Extent = std::max(Extent, std::fabs((FarBox.Hi[K] - FarBox.Lo[K]) - (LightBox.Hi[K] - LightBox.Lo[K])));
		const std::map<uint32_t, double> FarShares = Shares(Far.Points, Far.Triangles, Far.Tones);
		const std::map<uint32_t, double> LightShares = Shares(Light.Points, Light.Triangles, Light.Tones);
		double PaintOff = 0.0;
		uint32_t PaintWorst = 0;
		for (const auto& [Tone, Share] : FarShares)
		{
			const auto Found = LightShares.find(Tone);
			const double Off = std::fabs(Share - (Found == LightShares.end() ? 0.0 : Found->second));
			if (Off > PaintOff) { PaintOff = Off; PaintWorst = Tone; }
		}
		const bool bOk = Light.Kept > 0 && Light.Kept <= Most && Out.Most <= 0.5 * Pixel && Back.Most <= 0.5 * Pixel && Out.Mean <= 0.1 * Pixel
			&& Back.Mean <= 0.1 * Pixel && Extent <= 0.5 * Pixel && PaintOff <= 0.03;
		std::printf("%s: %d triangles (%d joined) to %d of %d asked, %zu points, %.1f ms; apart %.3f m most, %.4f mean (light from far), %.3f / %.4f (far from light) = %.2f / %.2f px; size off %.3f m; paint %u off %.1f points%s\n",
			Argv[A], FarTriangles, Light.From, Light.Kept, Most, Light.Points.size() / 3, Ms, Out.Most, Out.Mean, Back.Most, Back.Mean,
			std::max(Out.Most, Back.Most) / Pixel, std::max(Out.Mean, Back.Mean) / Pixel, Extent, PaintWorst, PaintOff * 100.0, bOk ? "" : "  FAIL");
		if (!bOk) ++Bad;
	}
	std::printf("%d aircraft, %d given light shapes, %d failed\n", Models, Lightened, Bad);
	return Bad == 0 && Lightened > 0 ? 0 : 1;
}
