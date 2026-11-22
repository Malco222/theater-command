#pragma once

// TEST AND PREVIEW ONLY: a measure for Bargains::FMeasure from the kit fonts' own advances (font_widths.h, written from
// Content/Fonts by font_widths.py), as Slate's FontMeasure gives them: the advances summed, the letter spacing added
// after each letter, a line as high as the face's ascender to descender.

#include "Campaign/BargainLook.h"
#include "font_widths.h"

#include <string_view>

namespace LookMeasure
{
	inline const FontWidths::FFaceTable& TableOf(const Bargains::FFace& Face)
	{
		using Bargains::EFamily;
		using Bargains::EStyle;
		if (Face.Family == EFamily::Courier)
		{
			return Face.Style == EStyle::Bold ? FontWidths::CourierBold : FontWidths::CourierRegular;
		}
		return Face.Style == EStyle::Bold ? FontWidths::SerifBold : Face.Style == EStyle::Italic ? FontWidths::SerifItalic : FontWidths::SerifRegular;
	}

	/** The next code point of UTF-8 text from At (moved past it); 0xFFFD for a broken sequence. */
	inline unsigned NextCode(std::string_view Text, size_t& At)
	{
		const unsigned char Lead = static_cast<unsigned char>(Text[At++]);
		if (Lead < 0x80)
		{
			return Lead;
		}
		const int More = Lead >= 0xF0 ? 3 : Lead >= 0xE0 ? 2 : Lead >= 0xC0 ? 1 : 0;
		unsigned Code = More == 3 ? (Lead & 0x07u) : More == 2 ? (Lead & 0x0Fu) : (Lead & 0x1Fu);
		if (More == 0)
		{
			return 0xFFFD;
		}
		for (int K = 0; K < More; ++K)
		{
			if (At >= Text.size() || (static_cast<unsigned char>(Text[At]) & 0xC0u) != 0x80u)
			{
				return 0xFFFD;
			}
			Code = (Code << 6) | (static_cast<unsigned char>(Text[At++]) & 0x3Fu);
		}
		return Code;
	}

	inline double Advance(const FontWidths::FFaceTable& Table, unsigned Code)
	{
		if (Code >= 32 && Code <= 126)
		{
			return Table.Ascii[Code - 32];
		}
		for (int K = 0; K < 3; ++K)
		{
			if (FontWidths::ExtraCodes[K] == Code)
			{
				return Table.Extra[K];
			}
		}
		return 0.6;
	}

	inline Bargains::FPt Measure(std::string_view Text, const Bargains::FFace& Face)
	{
		const FontWidths::FFaceTable& Table = TableOf(Face);
		double Em = 0.0;
		int Letters = 0;
		for (size_t At = 0; At < Text.size();)
		{
			Em += Advance(Table, NextCode(Text, At));
			++Letters;
		}
		return Bargains::FPt{ (Em + Letters * Face.Spacing / 1000.0) * Face.Size, Table.LineH * Face.Size };
	}
}
