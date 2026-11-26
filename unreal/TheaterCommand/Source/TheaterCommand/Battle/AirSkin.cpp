#include "Battle/AirSkin.h"

#include "Battle/AirUnitColours.h"
#include "Battle/LandJson.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>

namespace AirSkin
{
	namespace
	{
		std::atomic<const Overlay*> SkinNow{ nullptr };

		/** A key that would make paint depend on the date, or carry marks: refused anywhere in the file. */
		bool SkinKeyRefused(const std::string& Key)
		{
			std::string Low = Key;
			for (char& C : Low)
			{
				C = static_cast<char>(C >= 'A' && C <= 'Z' ? C - 'A' + 'a' : C);
			}
			return Low == "from" || Low.find("month") != std::string::npos || Low.find("year") != std::string::npos
				|| Low.find("scheme") != std::string::npos || Low.find("mark") != std::string::npos;
		}

		bool SkinKeysClean(const LandJson::Json& Node, std::string& Why)
		{
			if (Node.IsObject())
			{
				for (size_t K = 0; K < Node.Keys.size(); ++K)
				{
					if (SkinKeyRefused(Node.Keys[K]))
					{
						Why = "the key \"" + Node.Keys[K] + "\" (paint never by date, and no marks)";
						return false;
					}
					if (!SkinKeysClean(Node.Values[K], Why))
					{
						return false;
					}
				}
			}
			else if (Node.IsArray())
			{
				// (only the check list holds objects; a list of numbers has no keys to look at)
				for (const LandJson::Json& Item : Node.Items)
				{
					if ((Item.IsObject() || Item.IsArray()) && !SkinKeysClean(Item, Why))
					{
						return false;
					}
				}
			}
			return true;
		}

		bool SkinHex(const std::string& Text, size_t Digits, uint64_t& Out)
		{
			if (Text.size() != Digits)
			{
				return false;
			}
			uint64_t V = 0;
			for (const char C : Text)
			{
				const int D = C >= '0' && C <= '9' ? C - '0' : C >= 'a' && C <= 'f' ? C - 'a' + 10 : C >= 'A' && C <= 'F' ? C - 'A' + 10 : -1;
				if (D < 0)
				{
					return false;
				}
				V = (V << 4) | static_cast<uint64_t>(D);
			}
			Out = V;
			return true;
		}

		bool SkinPercents(const LandJson::Json& List, size_t Count, std::vector<uint8_t>& Out)
		{
			if (!List.IsArray() || List.Items.size() != Count)
			{
				return false;
			}
			Out.resize(Count);
			for (size_t K = 0; K < Count; ++K)
			{
				const LandJson::Json& N = List.Items[K];
				if (!N.IsNumber() || N.Value < 0.0 || N.Value > 100.0 || N.Value != std::floor(N.Value))
				{
					return false;
				}
				Out[K] = static_cast<uint8_t>(N.Value);
			}
			return true;
		}

		bool SkinLevel(const LandJson::Json& Node, size_t Count, size_t PaintCount, Level& Out, std::string& Why, const char* Name)
		{
			if (!Node.IsObject())
			{
				Why = std::string("no ") + Name + " mesh";
				return false;
			}
			if (!SkinPercents(Node["blend"], Count, Out.Blend) || !SkinPercents(Node["soot"], Count, Out.Soot) || !SkinPercents(Node["shade"], Count, Out.Shade))
			{
				Why = std::string(Name) + ": blend, soot and shade must each be one whole number 0 to 100 a point (" + std::to_string(Count) + " points)";
				return false;
			}
			const LandJson::Json& Re = Node["repaint"];
			if (!Re.IsArray() || Re.Items.size() % 2 != 0)
			{
				Why = std::string(Name) + ": no repaint list of pairs";
				return false;
			}
			Out.Repaint.clear();
			for (size_t K = 0; K + 1 < Re.Items.size(); K += 2)
			{
				const LandJson::Json& P = Re.Items[K];
				const LandJson::Json& C = Re.Items[K + 1];
				if (!P.IsNumber() || !C.IsNumber() || P.Value < 0.0 || P.Value >= static_cast<double>(Count) || C.Value < 0.0 || C.Value >= static_cast<double>(PaintCount)
					|| P.Value != std::floor(P.Value) || C.Value != std::floor(C.Value) || C.Value > 255.0
					|| (!Out.Repaint.empty() && static_cast<uint32_t>(P.Value) <= Out.Repaint.back().first))
				{
					Why = std::string(Name) + ": a repaint outside the points or paints, or out of order";
					return false;
				}
				Out.Repaint.emplace_back(static_cast<uint32_t>(P.Value), static_cast<uint8_t>(C.Value));
			}
			return true;
		}

		double SkinLinear(uint32_t Byte)
		{
			const double C = static_cast<double>(Byte) / 255.0;
			return C <= 0.04045 ? C / 12.92 : std::pow((C + 0.055) / 1.055, 2.4);
		}

		uint32_t SkinEncoded(double L)
		{
			L = std::min(std::max(L, 0.0), 1.0);
			const double C = L <= 0.0031308 ? L * 12.92 : 1.055 * std::pow(L, 1.0 / 2.4) - 0.055;
			return static_cast<uint32_t>(std::lround(std::min(std::max(C, 0.0), 1.0) * 255.0));
		}
	}

	int Level::PaintAt(size_t Point, int BasePaint) const
	{
		const auto At = std::lower_bound(Repaint.begin(), Repaint.end(), std::make_pair(static_cast<uint32_t>(Point), static_cast<uint8_t>(0)));
		return At != Repaint.end() && At->first == Point ? static_cast<int>(At->second) : BasePaint;
	}

	bool Read(const std::string& Text, size_t NearCount, size_t FarCount, size_t PaintCount, Overlay& Out, std::string& Why)
	{
		LandJson::Json Root;
		if (!LandJson::Parse(Text, Root, Why))
		{
			return false;
		}
		if (!Root.IsObject())
		{
			Why = "not an object";
			return false;
		}
		if (!SkinKeysClean(Root, Why))
		{
			return false;
		}
		Overlay O;
		O.Of = Root["skin_of"].Str("");
		if (O.Of.empty())
		{
			Why = "no skin_of";
			return false;
		}
		const LandJson::Json& Colours = Root["colours"];
		if (!Colours.IsArray() || Colours.Items.size() != PaintCount)
		{
			Why = "not " + std::to_string(PaintCount) + " colours";
			return false;
		}
		for (const LandJson::Json& C : Colours.Items)
		{
			uint64_t V = 0;
			if (!C.IsString() || !SkinHex(C.Text, 6, V))
			{
				Why = "a colour that is not RRGGBB";
				return false;
			}
			O.Colours.push_back(static_cast<uint32_t>(V));
		}
		if (!SkinLevel(Root["near"], NearCount, PaintCount, O.Near, Why, "near") || !SkinLevel(Root["far"], FarCount, PaintCount, O.Far, Why, "far"))
		{
			return false;
		}
		O.BaseSha256 = Root["base_sha256"].Str("");
		O.SourceSha256 = Root["source_sha256"].Str("");
		for (const std::string* Sha : { &O.BaseSha256, &O.SourceSha256 })
		{
			uint64_t Part = 0;
			if (Sha->size() != 64 || !SkinHex(Sha->substr(0, 16), 16, Part) || !SkinHex(Sha->substr(16, 16), 16, Part) || !SkinHex(Sha->substr(32, 16), 16, Part)
				|| !SkinHex(Sha->substr(48, 16), 16, Part))
			{
				Why = "no base_sha256 or source_sha256";
				return false;
			}
		}
		const LandJson::Json& Panel = Root["panel_blend"];
		if (!Panel.IsNumber() || Panel.Value < 0.0 || Panel.Value > 100.0 || Panel.Value != std::floor(Panel.Value))
		{
			Why = "no panel_blend 0 to 100";
			return false;
		}
		O.PanelBlend = static_cast<int>(Panel.Value);
		if (!SkinHex(Root["base_fnv64"].Str(""), 16, O.BaseFnv64) || !Root["base_bytes"].IsNumber())
		{
			Why = "no base_fnv64 or base_bytes";
			return false;
		}
		O.BaseBytes = static_cast<uint64_t>(Root["base_bytes"].Num());
		const LandJson::Json& Checks = Root["check"];
		for (const LandJson::Json& C : Checks.Items)
		{
			Check Ch;
			Ch.bFar = C["shape"].Str("") == "far";
			Ch.Point = static_cast<size_t>(C["point"].Num(-1.0) < 0.0 ? 0.0 : C["point"].Num());
			Ch.Paint = C["paint"].Int(0);
			uint64_t A = 0, B = 0;
			if (!SkinHex(C["rgb"].Str(""), 6, A) || !SkinHex(C["shaded"].Str(""), 6, B) || Ch.Point >= O.LevelOf(Ch.bFar).Count())
			{
				Why = "a check point not whole";
				return false;
			}
			Ch.Rgb = static_cast<uint32_t>(A);
			Ch.Shaded = static_cast<uint32_t>(B);
			O.Checks.push_back(Ch);
		}
		Out = std::move(O);
		return true;
	}

	void Set(const Overlay* Now)
	{
		SkinNow.store(Now, std::memory_order_release);
	}

	bool Active()
	{
		return SkinNow.load(std::memory_order_acquire) != nullptr;
	}

	const Overlay* For(const std::string& ModelName)
	{
		const Overlay* O = SkinNow.load(std::memory_order_acquire);
		return O && O->Of == ModelName ? O : nullptr;
	}

	uint32_t Law(const std::vector<uint32_t>& Colours, int Paint, int Blend, int Soot, double Shade)
	{
		if (Colours.empty())
		{
			return 0;
		}
		const size_t P = static_cast<size_t>(std::min(std::max(Paint, 0), static_cast<int>(Colours.size()) - 1));
		double C[3];
		auto Into = [&Colours](size_t Which, double Out3[3])
		{
			Out3[0] = SkinLinear((Colours[Which] >> 16) & 0xFF);
			Out3[1] = SkinLinear((Colours[Which] >> 8) & 0xFF);
			Out3[2] = SkinLinear(Colours[Which] & 0xFF);
		};
		Into(P, C);
		if (P == 0 && Colours.size() > 1 && Blend > 0)
		{
			double B[3];
			Into(1, B);
			const double T = std::min(Blend, 100) / 100.0;
			for (int I = 0; I < 3; ++I)
			{
				C[I] = C[I] + (B[I] - C[I]) * T;
			}
		}
		if (Colours.size() > 11 && Soot > 0)
		{
			double S[3];
			Into(11, S);
			const double T = 0.72 * std::min(Soot, 100) / 100.0;
			for (int I = 0; I < 3; ++I)
			{
				C[I] = C[I] + (S[I] - C[I]) * T;
			}
		}
		return (SkinEncoded(C[0] * Shade) << 16) | (SkinEncoded(C[1] * Shade) << 8) | SkinEncoded(C[2] * Shade);
	}

	double ShadeFactor(int Shade, double Strength)
	{
		const double S = std::min(std::max(Strength, 0.0), 1.0);
		const double Own = std::min(std::max(Shade, 0), 100) / 100.0;
		return 1.0 - S * (1.0 - Own);
	}

	bool OntoModel(const Overlay& O, AirUnitColours::Model& M)
	{
		if (M.Colours.size() != O.Colours.size() || M.Near.Count() != O.Near.Count() || M.Far.Count() != O.Far.Count())
		{
			return false;
		}
		for (size_t K = 0; K < O.Colours.size(); ++K)
		{
			M.Colours[K].R = static_cast<uint8_t>((O.Colours[K] >> 16) & 0xFF);
			M.Colours[K].G = static_cast<uint8_t>((O.Colours[K] >> 8) & 0xFF);
			M.Colours[K].B = static_cast<uint8_t>(O.Colours[K] & 0xFF);
		}
		for (const bool bFar : { false, true })
		{
			const Level& L = O.LevelOf(bFar);
			AirUnitColours::Mesh& Me = bFar ? M.Far : M.Near;
			Me.Blend.assign(L.Blend.begin(), L.Blend.end());
			Me.Soot.assign(L.Soot.begin(), L.Soot.end());
			for (const std::pair<uint32_t, uint8_t>& Re : L.Repaint)
			{
				Me.Paints[Re.first] = static_cast<int>(Re.second);
			}
		}
		return true;
	}

	uint32_t PanelUnder(const Overlay& O)
	{
		// (the median blend of her near paint-0 points, as the tool read it from the skin: mixed as the law mixes it)
		return Law(O.Colours, 0, O.PanelBlend, 0);
	}

	uint64_t Fnv64(const std::string& Bytes)
	{
		uint64_t H = 0xCBF29CE484222325ull;
		for (const char C : Bytes)
		{
			H ^= static_cast<uint8_t>(C);
			H *= 0x100000001B3ull;
		}
		return H;
	}
}
