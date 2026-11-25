#pragma once

// Rain and snow drawn over the cloud painter's pictures (release 2, the look bible's G10): each drop placed by the
// graphics chip's own arithmetic (Effects/WeatherFallChip.inl, as C++: weather_fall_chip.h), its streak or flake laid
// over the picture with the chip's own soft shape, hidden behind what stands nearer, in the haze's colour as M_LandFall
// draws it. For Tools/land_field's pictures (LAND_FIELD_FALL): what the art director judges before the Mac.

#include "Effects/WeatherFallCore.h"
#include "land_field_picture.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace WeatherFallPicture
{
	struct Asked
	{
		WeatherFall::Kind Kind = WeatherFall::Kind::None;
		double Strength = 0.0;
		/** The wind, metres a second along the field's X (east) and Y (north). */
		double WindX = 2.6, WindY = 1.2;
		/** Seconds the weather has run. */
		double Clock = 37.3;
		uint64_t Seed = 1;
		/** The drops' colour as a share of the haze's (LandWeather's InkOverHaze). */
		double InkOverHaze = 1.15;
	};

	/** What LandFieldPicture::Sky::Over takes: the drops of Asked, the recipe's layers, over a picture. Drawn: a count
	 * of the drops laid in the last picture, for the log. */
	std::function<void(const LandFieldPicture::Lens&, const std::vector<float>&, std::vector<float>&, std::vector<float>&)> Over(const WeatherFall::Recipe& R, const Asked& A, int* Drawn = nullptr);
}
