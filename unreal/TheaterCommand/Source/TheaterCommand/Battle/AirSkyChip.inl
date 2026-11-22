// The clouds turned to the eye on the graphics chip (the speed gate's GS6): one point of one puff, worked out where the
// points are placed (the vertex shader), every frame, for the eye as it is then. It is AirSky::Painter::Paint's own
// arithmetic for a puff drawn from the sheet of shapes (its place, its turn to the eye, its light and shade, its fades
// by distance and by the lens), so that the picture is the painter's, and nothing is turned or sent from the game's
// thread as the eye moves (AirSky::Chip lays the table it reads, and lays it again only when the order of the puffs,
// far to near, has gone stale).
//
// ONE TEXT, TWO COMPILERS. This file is read as it stands by Tools/build_content.py into M_CloudTurned's Custom node
// (HLSL, on the Mac's Metal by way of the shader compiler), and included by the cloud test (Tools/air_show/
// air_sky_chip.h, C++, beside a few lines that give C++ HLSL's float3 and its functions), which checks every point
// against the painter's. So it keeps to what the two languages share: floats written with an f, no swizzles but .x .y
// .z .w, no vector ?: (if and else instead), no references, no out parameters (a struct returned instead), no
// preprocessor lines, every helper defined before it is used. Keep it so.
//
// The table (AirSky::Chip): row 0 is the header, eight floats four to a texel; each puff after it eight texels
// (TexelsPerPuff), far to near, so that the mesh's own order (a slot's 33 points in the slot's place) draws them as the
// painter lays them:
//   H0 the way toward the sun (x, y, z), half a sheet cell's width        H1 the base's paint (r, g, b), the floor's share
//   H2 the shaded side's paint, the sprite's span                          H3 the sunlit top's paint, the reach
//   H4 NearAll, FarAll, the puffs laid, the sides of a puff
//   T0 the puff's middle as placed (x, y, z), its radius                   T1 its cloud's middle (x, y), its foot, its height
//   T2 the cloud's width, its body's half width, its squash, the puff's own flatness
//   T3 the puff's alpha by the clearing, its own light, its trough, how shadowed by its own cloud
//   T4 the turn of its ring (cos, sin), its phase's sine, its mirror (1 or -1)
//   T5 its place on the sheet, as a top (u, v) and upright (u, v)
//   T6 how long it is drawn out, the way it is drawn out (cos, sin), the puff's own length
//   T7 its level (-1: an empty slot), its flags, a small cloud's sight, a base's span
// The paints in the header carry the hour's light and the cloud ink already (Painter::SetInk). Places are board metres
// in the field's own frame (the eye less the wind's carry), as the painter's.

struct FSkyChipOut
{
	float3 At;
	float4 Colour;
	float4 Sheet;
};

struct FSkyChipPuff
{
	float3 Mid;
	float Bw;
	float Bd;
	float3 F;
	float3 S;
	float SunZ;
	float FootZ;
	float PerHigh;
	float PerLower;
	float OwnLit;
	bool bOwnByHeight;
	bool bLid;
	float Trough;
	float Shadowed;
	float Facing;
	float Below;
	float Against;
	float Thick;
};

struct FSkyChip
{
	float Pad;

	float Smooth(float T)
	{
		float C = clamp(T, 0.0f, 1.0f);
		return C * C * (3.0f - 2.0f * C);
	}

	float NearOutline(float Off2)
	{
		return Smooth((Off2 - 0.3025f) * (1.0f / 0.6f));
	}

	float3 Tone(float L, float4 H1, float4 H2, float4 H3)
	{
		float3 Base = float3(H1.x, H1.y, H1.z);
		float3 Shade = float3(H2.x, H2.y, H2.z);
		float3 Sun = float3(H3.x, H3.y, H3.z);
		float3 Out = Shade + (Sun - Shade) * ((clamp(L, 0.35f, 0.9f) - 0.35f) / 0.55f);
		if (L < 0.35f)
		{
			Out = Base + (Shade - Base) * (clamp(L, 0.0f, 0.35f) / 0.35f);
		}
		return Out;
	}

	// The cloud's body as the eye sees it at a point of the puff (the painter's FaceAt): x the light on the body's face
	// where the eye's line through the point meets it, y how far across the body the point lies (squared), z how deep.
	float3 FaceAt(FSkyChipPuff P, float3 At)
	{
		float3 O = float3((At.x - P.Mid.x) * P.Bw, (At.y - P.Mid.y) * P.Bw, (At.z - P.Mid.z) * P.Bd);
		float Along = dot(O, P.F);
		float Off2 = max(0.0f, dot(O, O) - Along * Along);
		float Depth = sqrt(max(0.0f, 1.0f - Off2));
		float Toward = Depth + Along;
		float3 A = O - P.F * Toward;
		float Long2 = (A.x * A.x + A.y * A.y) * P.Bw * P.Bw + A.z * A.z * P.Bd * P.Bd;
		float Body = P.SunZ;
		if (Long2 > 1e-18f)
		{
			Body = dot(A, P.S) / sqrt(Long2);
		}
		return float3(Body, Off2, Depth);
	}

	// The painter's LitAt for a puff drawn from the sheet: x the light, y how far across the body the point lies.
	float4 LitAt(FSkyChipPuff P, float3 At, float Rim, float Own)
	{
		float3 Face = FaceAt(P, At);
		float Shape = 0.5f + 0.5f * (0.75f * Face.x + 0.25f * Own);
		if (P.bLid)
		{
			Shape = 0.64f + 0.5f * (0.2f * Face.x + 0.1f * Own);
		}
		float OwnLit = P.OwnLit;
		if (P.bOwnByHeight)
		{
			OwnLit = 0.2f + 0.65f * clamp((At.z - P.FootZ) * P.PerHigh, 0.0f, 1.0f);
		}
		float Lit = clamp(0.25f * OwnLit + 0.75f * Shape - P.Trough * Rim, 0.0f, 1.0f) * (1.0f - 0.55f * P.Shadowed * clamp(1.0f - P.Facing, 0.0f, 1.0f));
		float Edge = NearOutline(Face.y);
		if (P.bLid)
		{
			Edge = 0.0f;
		}
		float Graded = Lit * (1.0f - 0.6f * P.Below * (1.0f - 0.6f * Edge)) * (0.8f + 0.2f * Smooth((At.z - P.FootZ) * P.PerLower));
		float Light = Graded * (1.0f - 0.3f * P.Against * P.Thick * Face.z) + 0.25f * P.Against * Edge + 0.12f * P.Against * (1.0f - P.Thick);
		return float4(Light, Face.y, 0.0f, 0.0f);
	}

	// A point of a ring: its way out across the picture, turned by the puff's own phase and drawn out along its way
	// (the painter's TurnedC, TurnedS and DrawnOut). x, y, z the way; w the turned sine (TurnedS).
	float4 RingWay(int K, float Sides, float TurnC, float TurnS, float3 Right, float3 Up, float LongR, float LongU, float LongMore)
	{
		float A = 6.283185307f * (float)K / Sides;
		float Cs = cos(A);
		float Sn = sin(A);
		float TurnedC = Cs * TurnC - Sn * TurnS;
		float TurnedS = Sn * TurnC + Cs * TurnS;
		float On = (TurnedC * LongR + TurnedS * LongU) * LongMore;
		float3 Way = Right * (TurnedC + On * LongR) + Up * (TurnedS + On * LongU);
		return float4(Way.x, Way.y, Way.z, TurnedS);
	}

	float RingC(int K, float Sides, float TurnC, float TurnS)
	{
		float A = 6.283185307f * (float)K / Sides;
		return cos(A) * TurnC - sin(A) * TurnS;
	}

	float4 SheetAt(float CU, float CV, float Half, float Behind, float Ahead, float Lumpy, bool bLid, float Off2, float SX, float SY)
	{
		float Outline = 0.0f;
		if (Ahead > 0.0f && !bLid)
		{
			Outline = NearOutline(Off2);
		}
		return float4(CU + Half * SX, CV - Half * SY, Behind + Ahead * (1.0f - Lumpy) * (1.0f - Outline), Ahead * (Lumpy + (1.0f - Lumpy) * Outline));
	}

	FSkyChipOut Vertex(float4 H0, float4 H1, float4 H2, float4 H3, float4 H4, float4 T0, float4 T1, float4 T2, float4 T3, float4 T4, float4 T5, float4 T6, float4 T7,
		float3 Seen, int Slot, int K, int Ring)
	{
		FSkyChipOut Out;
		float3 C = float3(T0.x, T0.y, T0.z);
		Out.At = C;
		Out.Colour = float4(0.0f, 0.0f, 0.0f, 0.0f);
		Out.Sheet = float4(0.0f, 0.0f, 0.0f, 0.0f);
		int Lod = (int)floor(T7.x + 0.5f);
		if (Slot >= (int)floor(H4.z + 0.5f) || Lod < 0)
		{
			return Out;
		}
		int Flags = (int)floor(T7.y + 0.5f);
		bool bFlatFoot = (Flags & 1) != 0;
		bool bFoot = (Flags & 2) != 0;
		bool bLowLump = (Flags & 4) != 0;
		bool bLid = (Flags & 16) != 0;
		float R = T0.w;
		float CloudX = T1.x;
		float CloudY = T1.y;
		float FootZ = T1.z;
		float HeapH = T1.w;
		float HeapHigh = max(1.0f, HeapH);
		float W = T2.x;
		float Squash = T2.z;
		float AlphaShow = T3.x;
		float3 SunWay = float3(H0.x, H0.y, H0.z);
		float Reach = H3.w;
		float NearAll = H4.x;
		float FarAll = H4.y;
		float Sides = H4.w;

		// Its fades by distance (the painter's Haze, its near and far shares): a point far past them is never drawn.
		float3 MiddleC = float3(CloudX, CloudY, FootZ + 0.5f * HeapH);
		float Far = length(MiddleC - Seen);
		float Haze = Smooth((Reach - Far) / (Reach / 3.0f));
		if (T7.z > 0.0f)
		{
			Haze = Haze * Smooth((T7.z - Far) / (0.2f * T7.z));
		}
		float NearShare = Smooth((NearAll + 1000.0f - Far) / 1000.0f);
		float FarShare = Smooth((FarAll + 1500.0f - Far) / 1500.0f);
		float Share = NearShare;
		if (bFoot)
		{
			Share = NearShare;
		}
		else if (Lod == 3)
		{
			Share = 1.0f - NearShare;
		}
		else if (Lod == 2)
		{
			Share = 1.0f;
		}
		else if (Lod == 1)
		{
			Share = FarShare;
		}
		float Fade = Haze * Share;
		if (Haze <= 0.0f || Fade * AlphaShow < 0.002f)
		{
			return Out;
		}

		// Turned to the eye.
		float FootSpan = T7.w;
		bool bFarDome = Lod == 3 && Seen.z < FootZ + HeapHigh;
		float Flat = max(0.08f, T2.w * Squash);
		if (bLowLump)
		{
			Flat = max(0.08f, 0.85f * Squash);
		}
		if (bFoot)
		{
			Flat = clamp(0.5f * HeapHigh / (0.84f * FootSpan), 0.3f, 1.0f);
		}
		else if (bFarDome)
		{
			Flat = clamp(1.7f * HeapHigh / max(1.0f, W), 0.4f, 1.0f);
		}
		float3 Fwd = C - Seen;
		float Dist = length(Fwd);
		if (Dist > 1e-9f)
		{
			Fwd = Fwd / Dist;
		}
		else
		{
			Fwd = float3(0.0f, 0.0f, -1.0f);
		}
		float3 Right = cross(Fwd, float3(0.0f, 0.0f, 1.0f));
		if (length(Right) < 1e-6f)
		{
			Right = float3(1.0f, 0.0f, 0.0f);
		}
		else
		{
			Right = normalize(Right);
		}
		float3 Up = cross(Right, Fwd);
		Up.z = Up.z - Up.z * (1.0f - Flat);
		float Lens = 0.6f * R * T6.w;
		float Alpha = AlphaShow * Fade * clamp((Dist - Lens) / Lens, 0.0f, 1.0f);

		// Its light (the painter's: the cloud's body as the eye sees it, the puff's own lump, the sun ahead or behind).
		FSkyChipPuff P;
		P.Mid = float3(CloudX, CloudY, FootZ + 0.45f * HeapH);
		P.Bw = 1.0f / max(60.0f, T2.y);
		P.Bd = 1.0f / max(60.0f, 0.5f * HeapH);
		float FootOver = FootZ - Seen.z;
		P.Below = clamp(FootOver / 300.0f, 0.0f, 1.0f) * Smooth((Fwd.z - 0.2f) / 0.5f);
		P.Facing = -dot(Fwd, SunWay);
		float3 Fb = float3(Fwd.x * P.Bw, Fwd.y * P.Bw, Fwd.z * P.Bd);
		P.F = Fb / max(1e-9f, length(Fb));
		P.S = float3(SunWay.x * P.Bw, SunWay.y * P.Bw, SunWay.z * P.Bd);
		P.SunZ = SunWay.z;
		P.FootZ = FootZ;
		P.PerHigh = 1.0f / HeapHigh;
		P.PerLower = 1.0f / (0.38f * HeapHigh);
		P.OwnLit = T3.y;
		P.bOwnByHeight = bFoot || bLowLump;
		P.bLid = bLid;
		P.Trough = T3.z;
		P.Shadowed = T3.w;
		float SunAhead = Smooth((-P.Facing - 0.35f) / 0.55f);
		float BehindShare = Smooth((P.Facing - 0.35f) / 0.55f);
		float Beneath = clamp(FootOver / 300.0f, 0.0f, 1.0f) * Smooth((Fwd.z - 0.03f) / 0.25f) * (1.0f - BehindShare);
		P.Against = max(SunAhead, Beneath);
		P.Thick = Smooth((HeapHigh - 120.0f) / 600.0f);

		// The sheet's shape: which, turned which way, its light from behind the eye and from ahead.
		float CloudTop = FootZ + HeapH;
		bool bUpright = !bLid && (bFlatFoot || (Lod == 3 && Seen.z < CloudTop));
		float CU = T5.x;
		float CV = T5.y;
		if (bUpright)
		{
			CU = T5.z;
			CV = T5.w;
		}
		float SunAcross = dot(Right, SunWay);
		float SunUp = dot(Up, SunWay);
		float Toward = 1.570796327f;
		if (!bUpright)
		{
			if (SunAcross * SunAcross + SunUp * SunUp > 1e-8f)
			{
				Toward = atan2(SunUp, SunAcross);
			}
			Toward = Toward + 0.3f * T4.z;
		}
		float Turn = 1.570796327f - Toward;
		float CT = cos(Turn);
		float ST = sin(Turn);
		float Behind = BehindShare;
		float Ahead = min(P.Against, 1.0f - BehindShare);
		float Lumpy = (0.45f + 0.5f * Beneath) * (0.25f + 0.75f * P.Thick);
		if (bLid)
		{
			Lumpy = 0.85f;
		}
		float Half = H0.w;
		float Span = H2.w * R;
		if (bFoot)
		{
			Span = FootSpan;
		}

		// A puff drawn out along its way (a small cloud's), by as much as that way lies across the picture.
		float LongR = 0.0f;
		float LongU = 0.0f;
		float LongMore = 0.0f;
		if (T6.x > 1.001f)
		{
			float LR = T6.y * Right.x + T6.z * Right.y;
			float LU = T6.y * Up.x + T6.z * Up.y;
			float InPicture = sqrt(LR * LR + LU * LU);
			if (InPicture > 1e-6f)
			{
				LongR = LR / InPicture;
				LongU = LU / InPicture;
				LongMore = (T6.x - 1.0f) * min(1.0f, InPicture);
			}
		}

		// Its middle: a base's flat underside on the cloud's own foot, where the foot is in the picture.
		float3 Middle = C;
		if (bUpright)
		{
			float Foot = FootZ;
			float CloudAway = sqrt((CloudX - Seen.x) * (CloudX - Seen.x) + (CloudY - Seen.y) * (CloudY - Seen.y));
			if (bFlatFoot && FootOver > 0.0f && CloudAway > R)
			{
				float Mine = sqrt((C.x - Seen.x) * (C.x - Seen.x) + (C.y - Seen.y) * (C.y - Seen.y));
				Foot = Foot + clamp(FootOver * (Mine / CloudAway - 1.0f), -0.33f * Span, 0.33f * Span);
			}
			Middle.z = Foot + H1.w * Span * max(0.0f, Up.z);
		}
		float Shown = Alpha;
		if (bFlatFoot)
		{
			Shown = Alpha * Smooth(clamp(1.0f - (Seen.z - C.z) / (1.5f * R), 0.0f, 1.0f));
		}
		float Melt = 0.0f;
		if (bFlatFoot && !bFoot && !bLid)
		{
			Melt = Smooth((CloudTop - (Middle.z + 0.6f * Span)) / (0.6f * Span));
		}

		if (Ring == 0)
		{
			// (the middle point: lit where the puff's own middle is, its alpha the shown share)
			float4 Lit = LitAt(P, C, 0.0f, P.Facing);
			float3 Col = Tone(Lit.x, H1, H2, H3);
			Out.At = Middle;
			Out.Colour = float4(Col.x, Col.y, Col.z, clamp(Shown, 0.0f, 1.0f));
			Out.Sheet = SheetAt(CU, CV, Half, Behind, Ahead, Lumpy, bLid, Lit.y, 0.0f, 0.0f);
			return Out;
		}

		// A point of the heart ring (0.55 of the span out) or of the rim: lit at every other point, each between two given
		// the mean of its two neighbours' light and nearness to the outline, as the painter's Mend.
		float Out55 = 1.0f;
		float RimShare = 1.0f;
		if (Ring == 1)
		{
			Out55 = 0.55f;
			RimShare = 0.45f;
		}
		float4 Way = RingWay(K, Sides, T4.x, T4.y, Right, Up, LongR, LongU, LongMore);
		float3 At = Middle + float3(Way.x, Way.y, Way.z) * (Out55 * Span);
		float Light = 0.0f;
		float Off2 = 0.0f;
		if (K % 2 == 0)
		{
			float4 Lit = LitAt(P, At, RimShare, P.Facing);
			Light = Lit.x;
			Off2 = Lit.y;
		}
		else
		{
			int Kn = (K + 1) % (int)floor(Sides + 0.5f);
			float4 WayB = RingWay(K - 1, Sides, T4.x, T4.y, Right, Up, LongR, LongU, LongMore);
			float4 WayN = RingWay(Kn, Sides, T4.x, T4.y, Right, Up, LongR, LongU, LongMore);
			float4 LitB = LitAt(P, Middle + float3(WayB.x, WayB.y, WayB.z) * (Out55 * Span), RimShare, P.Facing);
			float4 LitN = LitAt(P, Middle + float3(WayN.x, WayN.y, WayN.z) * (Out55 * Span), RimShare, P.Facing);
			Light = 0.5f * (LitB.x + LitN.x);
			Off2 = 0.5f * (LitB.y + LitN.y);
		}
		float TurnedC = RingC(K, Sides, T4.x, T4.y);
		float TurnedS = Way.w;
		float SX = T4.w * (TurnedC * CT - TurnedS * ST);
		float SY = TurnedC * ST + TurnedS * CT;
		float Upward = Melt * Smooth((TurnedS + 0.3f) / 0.9f);
		float A = Shown * (1.0f - Upward);
		if (Ring == 1)
		{
			A = Shown * (1.0f - 0.35f * Upward);
		}
		float3 Col = Tone(Light, H1, H2, H3);
		Out.At = At;
		Out.Colour = float4(Col.x, Col.y, Col.z, clamp(A, 0.0f, 1.0f));
		Out.Sheet = SheetAt(CU, CV, Half, Behind, Ahead, Lumpy, bLid, Off2, Out55 * SX, Out55 * SY);
		return Out;
	}
};
