// Rain and snow falling round the eye, worked out on the graphics chip (release 2, the look bible's G10: Brian, 25
// November, "better overall graphics"; the weather the campaign already draws, now seen falling). One point of one
// drop, worked out where the points are placed (the vertex shader), every frame, for the eye as it is then: where the
// drop has fallen to by the clock, carried round inside a box about the eye (so the same few thousand drops are always
// round it, wherever it goes, and each stays put in the world while it is near), turned to the eye, and how much of it
// shows. Nothing is moved or sent from the game's thread but a few numbers a frame (WeatherFall::Params).
//
// ONE TEXT, TWO COMPILERS (as AirSkyChip.inl). This file is read as it stands by Tools/build_content.py into
// M_LandFall's Custom node (HLSL, on the Mac's Metal by way of the shader compiler), and included by the cloud test
// (Tools/land_weather/weather_fall_chip.h, C++, beside a few lines that give C++ HLSL's float2, float3, float4 and
// their functions), which checks the points and paints the cloud's pictures with them. So it keeps to what the two
// languages share: floats written with an f, no swizzles but .x .y .z .w, no vector ?: (if and else instead), no
// references, no out parameters (a struct returned instead), no preprocessor lines, every helper defined before it is
// used. Keep it so.
//
// The numbers (each a float4, as the material's vector parameters give them; WeatherFall::Params makes them):
//   Home   the drop's place in its box as the mesh was made (metres, the mesh's own frame, about its middle)
//   Corner its corner: x across (-1 to 1), y along (0 the tail, 1 the head), as the mesh's first coordinates
//   Drop   x its fall in whole steps (Move.z each), y its phase (0 to 1), z its size (0 to 1), w its turn to show (0 to 1)
//   Eye    the eye (metres, the same frame)
//   Box    x the box's side (metres), y the clock (seconds, within the period), z the period (seconds), w the strength
//   Move   x, y the wind's carry (metres a second), z one step of fall (metres a second), w the flutter (metres)
//   Look   x the width (rain) or the flake's size (snow), y the streak's length (metres), z where it starts to show
//          near the eye, w where it is whole (metres)
//   Lens   x one pixel's angle (radians), y the least it is drawn across (pixels), z the kind (0 rain, 1 snow),
//          w where the far fade begins, as a share of half the box
// The period is the box's side over one step, so in one period every drop falls and drifts a whole number of boxes and
// is where it began: the clock is given within it, and the sums stay small whatever the hour.

struct FFallOut
{
	float3 At;
	float4 Look;
};

struct FFallChip
{
	float Pad;

	float Ease(float A, float B, float X)
	{
		float T = clamp((X - A) / (B - A), 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}

	float Wrapped(float V, float Side)
	{
		return V - Side * floor(V / Side + 0.5f);
	}

	float3 Across(float3 Way, float3 Toward)
	{
		float3 C = cross(Way, Toward);
		float L = length(C);
		if (L < 0.000001f)
		{
			C = cross(Way, float3(1.0f, 0.0f, 0.0f));
			L = length(C);
			if (L < 0.000001f)
			{
				C = float3(0.0f, 1.0f, 0.0f);
				L = 1.0f;
			}
		}
		return C / L;
	}

	FFallOut Vertex(float3 Home, float2 Corner, float4 Drop, float3 Eye, float4 Box, float4 Move, float4 Look, float4 Lens)
	{
		FFallOut Out;
		float Side = max(Box.x, 0.01f);
		float Clock = Box.y;
		float Period = max(Box.z, 0.01f);
		float Fall = Drop.x * Move.z;
		// Where it has fallen to: its own fall, the wind's carry, and a flake's flutter, each a whole number of turns
		// in the period.
		float3 Moved = Home + float3(Move.x * Clock, Move.y * Clock, 0.0f - Fall * Clock);
		if (Lens.z > 0.5f)
		{
			if (Move.w > 0.0f)
			{
				float Turn = 6.2831853f * Clock / Period;
				float K1 = floor(Period * (0.35f + 0.4f * Drop.y));
				float K2 = floor(Period * (0.25f + 0.35f * Drop.z));
				Moved = Moved + float3(sin(K1 * Turn + 6.2831853f * Drop.y), cos(K2 * Turn + 6.2831853f * Drop.z), 0.0f) * Move.w;
			}
		}
		// Carried round the box about the eye.
		float3 Rel = float3(Wrapped(Moved.x - Eye.x, Side), Wrapped(Moved.y - Eye.y, Side), Wrapped(Moved.z - Eye.z, Side));
		float3 Mid = Eye + Rel;
		float D = length(Rel);
		float3 Toward = float3(0.0f, 0.0f, 0.0f) - Rel;
		// One pixel there, in metres: nothing is drawn thinner than Lens.y of them, its share of the light kept.
		float Pixel = max(D, 0.05f) * Lens.x;
		float Least = Lens.y * Pixel;
		float Shown = 1.0f;
		float Wide = Look.x * (0.75f + 0.5f * Drop.z);
		if (Lens.z > 0.5f)
		{
			// A flake: a soft round sprite turned square to the eye.
			if (Wide < Least)
			{
				Shown = (Wide / Least) * (Wide / Least);
				Wide = Least;
			}
			float3 Facing = Toward / max(D, 0.001f);
			float3 A = Across(float3(0.0f, 0.0f, 1.0f), Facing);
			float3 B = cross(Facing, A);
			Out.At = Mid + A * (Corner.x * 0.5f * Wide) + B * ((Corner.y - 0.5f) * Wide);
		}
		else
		{
			// A drop: a streak along its way, turned about it to face the eye.
			if (Wide < Least)
			{
				Shown = Wide / Least;
				Wide = Least;
			}
			float3 Way = float3(Move.x, Move.y, 0.0f - Fall);
			Way = Way / max(length(Way), 0.001f);
			float3 A = Across(Way, Toward);
			Out.At = Mid + A * (Corner.x * 0.5f * Wide) + Way * ((Corner.y - 0.5f) * Look.y);
		}
		// How much shows: none near the lens and none toward the box's edge (so nothing is seen to come or go as the box
		// is carried), and only the drops whose turn the strength reaches.
		Shown = Shown * Ease(Look.z, Look.w, D) * (1.0f - Ease(Lens.w * 0.5f * Side, 0.5f * Side, D));
		if (Drop.w >= Box.w)
		{
			Shown = 0.0f;
		}
		if (Shown < 0.001f)
		{
			// (folded to a point: nothing drawn, nothing paid for in pixels)
			Out.At = Mid;
			Shown = 0.0f;
		}
		Out.Look = float4(Shown, 0.85f + 0.3f * Drop.z, Corner.x, Corner.y);
		return Out;
	}

	// The pixel's share of its sprite: a streak soft across and fading toward its tail; a flake a soft round dot.
	float Shape(float2 Corner, float Kind)
	{
		if (Kind > 0.5f)
		{
			float Y = Corner.y * 2.0f - 1.0f;
			float R = Corner.x * Corner.x + Y * Y;
			float S = clamp(1.0f - R, 0.0f, 1.0f);
			return S * sqrt(S);
		}
		float Across2 = clamp(1.0f - Corner.x * Corner.x, 0.0f, 1.0f);
		float Along = clamp(Corner.y, 0.0f, 1.0f);
		float Head = clamp((1.0f - Along) * 8.0f, 0.0f, 1.0f);
		return Across2 * Along * sqrt(Along) * Head;
	}
};
