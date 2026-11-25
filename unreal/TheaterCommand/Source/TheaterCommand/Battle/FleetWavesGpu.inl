// THE FLEET SEA'S WAVES ON THE CHIP (the release 2 ocean plan, step 2): the one text the Custom node "FleetWaves" holds
// (M_FleetSea, M_FleetWake, M_FleetMark, M_FleetUnderSea; step 7) and Battle/FleetWaves.cpp compiles in float through its
// own stand-ins for the shader's types, so the cloud holds it to the double core (Tools/sea_waves/sea_waves_test.cpp).
// The common subset of C++ and HLSL only: float, float2, float4, int; sin, cos, floor, frac, abs, min, max, clamp, sqrt,
// exp, exp2, saturate, lerp; fixed loops; one helper struct. Every divisor is guarded. Every train's turns are taken into
// [-0.5, 0.5) before the multiply by two pi, so no phase reaches sin or cos larger than pi, however long the battle.
// Everything is in the stage's frame and units, from the MPC's Origin (FleetWaves::StageParams).
// Inputs (the Custom node's pins): Train0-9, Steep0-9, Pixel0-15, Rings0-3, Eye, Shape, Patch, PatchEase, Fold, Lean, Fade
// (float4, the MPC's); Local, Vtx, Axis (float2: the point's cells, or the pixel's rest; its ring and kind; its odd axes); Cell, Depth,
// Mode (float). Mode 0: the point's place (rest + shift, height, the weights' cell); 1: its rest (rest, the weights' cell);
// 2: the pixel's light (slopes, J, the slopes drawn); 3: its foam (the head's gate, which step 7 takes times the
// whitecaps' picture's shape read in the crest's frame, FleetWaves::HeadShapeAt; tail, squeeze, height over the crest top):
// the gate from every train at full weight, going to its mean (Fade.y, Fade.z) as the shortest train fades under the
// pixel, so the cover holds Monahan's at every depth (FleetWaves::FoamDrawn).
struct FleetWavesFns
{
	float Turn(float T) { return T - floor(T + 0.5f); }
	float Cheb(float2 A, float2 B) { return max(abs(A.x - B.x), abs(A.y - B.y)); }
	float2 Snap(float4 R[4], int K)
	{
		float4 V = R[K / 2];
		return (K - (K / 2) * 2) == 0 ? float2(V.x, V.y) : float2(V.z, V.w);
	}
	// (0 inside a ring, rising over its outer band to 1 by Half - 3 cells: CDLOD's morph, which the odd points take on their
	// shift and height; with the eye anywhere in its snap cell the outer two cells have eased whole)
	float Morph(float C, float Half, float D)
	{
		float B = (Half - 3.0f) * C;
		float A = B - Half * C * 0.125f;
		return saturate((D - A) / max(B - A, 1e-6f));
	}
	// (the first ring, from the eye out, whose square holds P; -1 beyond them all)
	int RingOf(float4 R[4], int Rings, float C0, float Half, float2 P)
	{
		int Found = -1;
		for (int K = 7; K >= 0; K--)
		{
			if (K < Rings && Cheb(P, Snap(R, K)) <= Half * C0 * exp2((float)K)) Found = K;
		}
		return Found;
	}
	// (the patch's two corners where a main cell's diagonal leaves a triangle of the main mesh inside its square, its notches:
	// (-H, H) and (H, -H) from its middle)
	float2 Corner(float4 Pt, int Q)
	{
		return float2(Pt.x, Pt.y) + (Q == 0 ? float2(-Pt.z, Pt.z) : float2(Pt.z, -Pt.z));
	}
	// (the cell the weights are taken at on ring K: its own, morphed two cells ahead)
	float RingCell(int K, float C0, float Half, float2 At, float2 P)
	{
		float C = C0 * exp2((float)K);
		return C * (1.0f + Morph(C, Half, Cheb(P, At) + 2.0f * C));
	}
	float MainCell(float4 R[4], int Rings, float C0, float Half, float2 At, float2 P)
	{
		int K = RingOf(R, Rings, C0, Half, P);
		return K < 0 ? 1e30f : RingCell(K, C0, Half, At, P);
	}
	float Weight(float L, float C)
	{
		float E = max(L, 1e-3f) * 0.125f;
		return saturate((E - C) / max(E - max(L, 1e-3f) / 12.0f, 1e-6f));
	}
	// (forward Gerstner at a rest point, each train weighted at cell C: the shift and the height)
	float4 Displace(float4 T[10], float4 S[10], float2 P, float C)
	{
		float4 D = float4(0.0f, 0.0f, 0.0f, 0.0f);
		for (int J = 0; J < 10; J++)
		{
			float W = Weight(S[J].y, C);
			float Th = 6.28318531f * Turn(T[J].x * P.x + T[J].y * P.y + T[J].w);
			float Kl = max(sqrt(T[J].x * T[J].x + T[J].y * T[J].y), 1e-12f);
			float Sh = -W * S[J].x * sin(Th) / Kl;
			D.x += Sh * T[J].x;
			D.y += Sh * T[J].y;
			D.z += W * T[J].z * cos(Th);
			D.w += W * T[J].z;
		}
		return D;
	}
	float2 Odd(float2 G) { return float2(abs(G.x - 2.0f * floor(G.x * 0.5f)), abs(G.y - 2.0f * floor(G.y * 0.5f))); }
	// (a main point's rest: its ring's snap and cells; an odd point on the ring's outer edge goes to its even neighbour toward
	// the ring's corners at (-, +) and (+, -), so the edge meets the next ring's points with no T-junction; the ring's outer
	// two cells have eased whole, so the surface is the same. The same two corners in the board's frame and the stage's.)
	float2 Moved(float4 R[4], int K, float C0, float Half, float2 G)
	{
		float C = C0 * exp2((float)K);
		float2 P = Snap(R, K) + G * C;
		float2 O = Odd(G);
		bool EdgeX = abs(abs(G.x) - Half) < 0.25f;
		bool EdgeY = abs(abs(G.y) - Half) < 0.25f;
		return P - float2(EdgeY ? O.x * C * (G.y > 0.0f ? 1.0f : -1.0f) : 0.0f, EdgeX ? O.y * C * (G.x > 0.0f ? 1.0f : -1.0f) : 0.0f);
	}
	// (a main point's shift and height: forward Gerstner at its rest, each train weighted at its cell there; over the ring's
	// outer band an odd point eases to the middle of its two even neighbours along its odd axes, CDLOD's morph taken on the
	// shift and height, so the rest grid stays the ring's own and the next ring's is met exactly at its edge)
	float4 Eased(float4 T[10], float4 S[10], float4 R[4], int K, float C0, float Half, float2 At, float2 G)
	{
		float C = C0 * exp2((float)K);
		float2 P0 = Snap(R, K) + G * C;
		float2 P = Moved(R, K, C0, Half, G);
		float4 D = Displace(T, S, P, RingCell(K, C0, Half, At, P));
		float2 O = Odd(G);
		float M = (P.x == P0.x && P.y == P0.y) ? Morph(C, Half, Cheb(P0, At)) : 0.0f;
		if (M > 0.0f && O.x + O.y > 0.0f)
		{
			float2 Pa = P0 - O * C;
			float2 Pb = P0 + O * C;
			float4 Da = Displace(T, S, Pa, RingCell(K, C0, Half, At, Pa));
			float4 Db = Displace(T, S, Pb, RingCell(K, C0, Half, At, Pb));
			D = lerp(D, (Da + Db) * 0.5f, M);
		}
		return D;
	}
	// (the same where the point would lie without the edge's fold: on the ring's outer two cells, where the fold moves
	// points, the morph is whole, so the folded triangles and these lie on the same flat pieces)
	float4 Unfolded(float4 T[10], float4 S[10], float4 R[4], int K, float C0, float Half, float2 At, float2 G)
	{
		float C = C0 * exp2((float)K);
		float2 P = Snap(R, K) + G * C;
		float4 D = Displace(T, S, P, RingCell(K, C0, Half, At, P));
		float2 O = Odd(G);
		float M = Morph(C, Half, Cheb(P, At));
		if (M > 0.0f && O.x + O.y > 0.0f)
		{
			float2 Pa = P - O * C;
			float2 Pb = P + O * C;
			float4 Da = Displace(T, S, Pa, RingCell(K, C0, Half, At, Pa));
			float4 Db = Displace(T, S, Pb, RingCell(K, C0, Half, At, Pb));
			D = lerp(D, (Da + Db) * 0.5f, M);
		}
		return D;
	}
	// (the main mesh's shift and height over a rest point, before the patch's drop: the cell of the first ring that holds
	// it, split along its 00-11 diagonal, its corners unfolded; nought beyond the rings)
	float4 MainAt(float4 T[10], float4 S[10], float4 R[4], int Rings, float C0, float Half, float2 At, float2 P)
	{
		int K = RingOf(R, Rings, C0, Half, P);
		float4 D = float4(0.0f, 0.0f, 0.0f, 0.0f);
		if (K >= 0)
		{
			float C = C0 * exp2((float)K);
			float2 Sn = Snap(R, K);
			float2 F = float2((P.x - Sn.x) / C, (P.y - Sn.y) / C);
			float2 G = float2(clamp(floor(F.x), -Half, Half - 1.0f), clamp(floor(F.y), -Half, Half - 1.0f));
			float2 U = F - G;
			float2 B = U.x >= U.y ? float2(1.0f, 0.0f) : float2(0.0f, 1.0f);
			float4 D00 = Unfolded(T, S, R, K, C0, Half, At, G);
			float4 DB = Unfolded(T, S, R, K, C0, Half, At, G + B);
			float4 D11 = Unfolded(T, S, R, K, C0, Half, At, G + float2(1.0f, 1.0f));
			D = D00 + (DB - D00) * max(U.x, U.y) + (D11 - DB) * min(U.x, U.y);
		}
		return D;
	}
};
float4 FwT[10] = { Train0, Train1, Train2, Train3, Train4, Train5, Train6, Train7, Train8, Train9 };
float4 FwS[10] = { Steep0, Steep1, Steep2, Steep3, Steep4, Steep5, Steep6, Steep7, Steep8, Steep9 };
float4 FwP[16] = { Pixel0, Pixel1, Pixel2, Pixel3, Pixel4, Pixel5, Pixel6, Pixel7, Pixel8, Pixel9, Pixel10, Pixel11, Pixel12, Pixel13, Pixel14, Pixel15 };
float4 FwR[4] = { Rings0, Rings1, Rings2, Rings3 };
FleetWavesFns Fw;
float4 FwOut = float4(0.0f, 0.0f, 0.0f, 0.0f);
float FwC0 = max(Shape.x, 1e-4f);
int FwRings = (int)clamp(Shape.y, 1.0f, 8.0f);
float FwHalf = max(Shape.w, 8.0f) * 0.5f;
float FwU = max(Lean.w, 1e-4f);
float2 FwEye = float2(Eye.x, Eye.y);
int FwMode = (int)(Mode + 0.5f);
if (FwMode <= 1)
{
	int FwK = (int)(Vtx.x + 0.5f);
	int FwKind = (int)(Vtx.y + 0.5f);
	float2 FwRest = float2(0.0f, 0.0f);
	float FwCell = 1e30f;
	float4 FwD = float4(0.0f, 0.0f, 0.0f, 0.0f);
	if (Vtx.x > -0.5f)
	{
		// A point of the clipmap's rings, or of its skirt.
		FwK = min(FwK, FwRings - 1);
		float FwC = FwC0 * exp2((float)FwK);
		float2 FwSnap = Fw.Snap(FwR, FwK);
		bool FwGone = false;
		if (FwKind >= 1 && FwKind <= 4)
		{
			float2 FwIn = Fw.Snap(FwR, max(FwK - 1, 0));
			int FwDx = (int)floor((FwIn.x - FwSnap.x) / FwC + 0.5f);
			int FwDy = (int)floor((FwIn.y - FwSnap.y) / FwC + 0.5f);
			FwGone = FwK < 1 || FwKind != 1 + FwDx + 2 * FwDy;
		}
		if (FwGone)
		{
			FwRest = FwSnap;
		}
		else if (FwKind == 6)
		{
			FwRest = FwSnap + Local * (200000.0f * FwU);
		}
		else
		{
			FwRest = Fw.Moved(FwR, FwK, FwC0, FwHalf, Local);
			FwCell = Fw.RingCell(FwK, FwC0, FwHalf, FwEye, FwRest);
			FwD = Fw.Eased(FwT, FwS, FwR, FwK, FwC0, FwHalf, FwEye, Local);
			// (under the subject's patch the main mesh drops out of sight; its points on the patch's edge stay)
			if (Patch.w > 0.0f && Fw.Cheb(FwRest, float2(Patch.x, Patch.y)) < Patch.z - 0.001f * FwU)
			{
				FwD.z -= FwD.w + 5.0f * FwU;
			}
		}
	}
	else if (Patch.w > 0.0f)
	{
		// A point of the subject's patch.
		float2 FwMid = float2(Patch.x, Patch.y);
		FwRest = FwMid + Local * Patch.w;
		float FwMain = Fw.MainCell(FwR, FwRings, FwC0, FwHalf, FwEye, FwRest);
		bool FwNotched = false;
		for (int FwQ = 0; FwQ < 2; FwQ++)
		{
			// (its notches: the main cell just inside the square at that corner, on the ring that holds it, cut along its
			// diagonal; none where that ring's fold has moved the diagonal's ends. A point in or on one lies on that diagonal,
			// laid as the main mesh lays it.)
			float2 FwCo = Fw.Corner(Patch, FwQ);
			float FwIn = 0.25f * Patch.w;
			int FwKn = Fw.RingOf(FwR, FwRings, FwC0, FwHalf, FwCo + (FwQ == 0 ? float2(FwIn, -FwIn) : float2(-FwIn, FwIn)));
			int FwKq = max(FwKn, 0);
			float FwCn = FwC0 * exp2((float)FwKq);
			float2 FwSq = Fw.Snap(FwR, FwKq);
			float2 FwA = FwCo + (FwQ == 0 ? float2(0.0f, -FwCn) : float2(-FwCn, 0.0f));
			float2 FwB = FwA + float2(FwCn, FwCn);
			float2 FwGa = float2(floor((FwA.x - FwSq.x) / FwCn + 0.5f), floor((FwA.y - FwSq.y) / FwCn + 0.5f));
			float2 FwGb = FwGa + float2(1.0f, 1.0f);
			bool FwOn = FwKn >= 0 && Fw.Cheb(Fw.Moved(FwR, FwKq, FwC0, FwHalf, FwGa), FwA) < 0.25f * FwCn && Fw.Cheb(Fw.Moved(FwR, FwKq, FwC0, FwHalf, FwGb), FwB) < 0.25f * FwCn;
			float FwSide = (FwQ == 0 ? 1.0f : -1.0f) * ((FwRest.y - FwRest.x) - (FwA.y - FwA.x));
			if (!FwNotched && FwOn && FwSide > -0.25f * Patch.w)
			{
				float FwTn = saturate(((FwRest.x - FwA.x) + (FwRest.y - FwA.y)) / (2.0f * FwCn));
				float4 FwDa = Fw.Eased(FwT, FwS, FwR, FwKq, FwC0, FwHalf, FwEye, FwGa);
				float4 FwDb = Fw.Eased(FwT, FwS, FwR, FwKq, FwC0, FwHalf, FwEye, FwGb);
				FwRest = FwA + (FwB - FwA) * FwTn;
				FwCell = Fw.MainCell(FwR, FwRings, FwC0, FwHalf, FwEye, FwRest);
				FwD = lerp(FwDa, FwDb, FwTn);
				FwNotched = true;
			}
		}
		if (!FwNotched && FwKind != 7)
		{
			// (its own cell out to PatchEase.z from the subject herself, unsnapped (PatchEase.xy from the patch's middle, so the
			// distance is taken in small numbers), eased to the main mesh's by PatchEase.w, its place with it onto the main
			// mesh's surface: from PatchEase.w it is the main mesh's own, so a snap of the square moves nothing there; and as far
			// again as the patch is not yet shown, Fade.x its presence: at 0 it is the main mesh's own surface everywhere)
			float FwDp = Fw.Cheb(Local * Patch.w, float2(PatchEase.x, PatchEase.y));
			float FwE = saturate((FwDp - PatchEase.z) / max(PatchEase.w - PatchEase.z, 1e-6f));
			FwE = 1.0f - saturate(Fade.x) * (1.0f - FwE);
			FwCell = lerp(Patch.w, max(Patch.w, FwMain), FwE);
			FwD = Fw.Displace(FwT, FwS, FwRest, FwCell);
			if (FwE > 0.0f)
			{
				FwD = lerp(FwD, Fw.MainAt(FwT, FwS, FwR, FwRings, FwC0, FwHalf, FwEye, FwRest), FwE);
			}
		}
		else if (!FwNotched)
		{
			// (its edge is the main mesh's own: the two points of the main mesh about it, as they lie, and between them)
			FwCell = FwMain;
			int FwKe = max(Fw.RingOf(FwR, FwRings, FwC0, FwHalf, FwRest), 0);
			float FwCe = FwC0 * exp2((float)FwKe);
			float2 FwSe = Fw.Snap(FwR, FwKe);
			bool FwAlongX = abs(abs(Local.y) - 48.0f) < 0.25f;
			float FwFrom = FwAlongX ? FwRest.x : FwRest.y;
			float FwAt = floor(((FwAlongX ? FwRest.x - FwSe.x : FwRest.y - FwSe.y)) / FwCe);
			float FwAcross = floor((FwAlongX ? FwRest.y - FwSe.y : FwRest.x - FwSe.x) / FwCe + 0.5f);
			float FwT0 = 0.0f;
			bool FwHave = false;
			float2 FwLast = float2(0.0f, 0.0f);
			float2 FwGLast = float2(0.0f, 0.0f);
			float2 FwGA = float2(0.0f, 0.0f);
			float2 FwGB = float2(0.0f, 0.0f);
			for (int FwN = -1; FwN <= 2; FwN++)
			{
				float FwI = FwAt + (float)FwN;
				float2 FwG = FwAlongX ? float2(FwI, FwAcross) : float2(FwAcross, FwI);
				float2 FwP2 = Fw.Moved(FwR, FwKe, FwC0, FwHalf, FwG);
				if (FwN > -1 && !FwHave)
				{
					float FwL0 = FwAlongX ? FwLast.x : FwLast.y;
					float FwL1 = FwAlongX ? FwP2.x : FwP2.y;
					if (FwL0 <= FwFrom && FwFrom <= FwL1 && FwL1 > FwL0)
					{
						FwGA = FwGLast;
						FwGB = FwG;
						FwT0 = (FwFrom - FwL0) / max(FwL1 - FwL0, 1e-9f);
						FwHave = true;
					}
				}
				FwLast = FwP2;
				FwGLast = FwG;
			}
			float4 FwDa = Fw.Eased(FwT, FwS, FwR, FwKe, FwC0, FwHalf, FwEye, FwGA);
			float4 FwDb = Fw.Eased(FwT, FwS, FwR, FwKe, FwC0, FwHalf, FwEye, FwGB);
			FwD = lerp(FwDa, FwDb, FwT0);
		}
	}
	else
	{
		// (no patch: its points gather at one, out of sight)
		FwRest = float2(Patch.x, Patch.y);
		FwD.z = -5.0f * FwU;
	}
	FwOut = FwMode == 0 ? float4(FwRest.x + FwD.x, FwRest.y + FwD.y, FwD.z, FwCell) : float4(FwRest.x, FwRest.y, FwCell, 0.0f);
}
else
{
	// The pixel: every train at full weight, faded where it is under 4 pixels (the foam's gate from them all unfaded).
	float2 FwRest = Local;
	float FwPx = max(Shape.z, 1e-6f) / max(Depth, 1e-3f);
	float FwQn = Lean.x;
	float FwSx = 0.0f;
	float FwSy = 0.0f;
	float FwXx = 0.0f;
	float FwYy = 0.0f;
	float FwXy = 0.0f;
	float FwUx = 0.0f;
	float FwUy = 0.0f;
	float FwUxy = 0.0f;
	float FwLeast = 1.0f;
	float FwZ = 0.0f;
	float FwTop = 0.0f;
	float FwMss = 0.0f;
	float FwLx[3] = { 0.0f, 0.0f, 0.0f };
	float FwLy[3] = { 0.0f, 0.0f, 0.0f };
	float FwLxy[3] = { 0.0f, 0.0f, 0.0f };
	for (int FwJ = 0; FwJ < 26; FwJ++)
	{
		float4 FwV = FwJ < 10 ? FwT[min(FwJ, 9)] : FwP[max(min(FwJ - 10, 15), 0)];
		float FwKt = sqrt(FwV.x * FwV.x + FwV.y * FwV.y);
		float FwK = 6.28318531f * FwKt;
		float FwFade = saturate((FwPx / max(FwKt, 1e-12f) - 4.0f) * 0.25f);
		float FwTh = 6.28318531f * Fw.Turn(FwV.x * FwRest.x + FwV.y * FwRest.y + FwV.w);
		float FwC = cos(FwTh);
		float FwSn = sin(FwTh);
		float FwKa = FwK * FwV.z;
		float FwQf = min(FwQn, FwKa);
		float FwQ = FwQf * FwFade;
		float FwNx = FwV.x / max(FwKt, 1e-12f);
		float FwNy = FwV.y / max(FwKt, 1e-12f);
		FwSx -= FwFade * FwV.z * 6.28318531f * FwV.x * FwSn;
		FwSy -= FwFade * FwV.z * 6.28318531f * FwV.y * FwSn;
		FwXx += FwQ * FwNx * FwNx * FwC;
		FwYy += FwQ * FwNy * FwNy * FwC;
		FwXy += FwQ * FwNx * FwNy * FwC;
		FwUx += FwQf * FwNx * FwNx * FwC;
		FwUy += FwQf * FwNy * FwNy * FwC;
		FwUxy += FwQf * FwNx * FwNy * FwC;
		FwLeast = FwV.z > 0.0f ? min(FwLeast, FwFade) : FwLeast;
		FwZ += FwJ < 10 ? FwV.z * FwC : 0.0f;
		FwTop += FwJ < 10 ? FwV.z : 0.0f;
		FwMss += FwFade * FwKa * FwKa * 0.5f;
		if (FwJ < 3)
		{
			// (the three largest trains, kept apart for the whitecaps' tails)
			FwLx[FwJ] = FwQf * FwNx * FwNx;
			FwLy[FwJ] = FwQf * FwNy * FwNy;
			FwLxy[FwJ] = FwQf * FwNx * FwNy;
		}
	}
	float FwFar = lerp(1.0f, 0.35f, saturate(Depth / (6000.0f * FwU)));
	FwMss += Lean.z * FwFar * FwFar;
	float FwJac = (1.0f - FwXx) * (1.0f - FwYy) - FwXy * FwXy;
	if (FwMode == 2)
	{
		FwOut = float4(FwSx, FwSy, FwJac, FwMss);
	}
	else
	{
		float FwSoft = max(Fold.y, 1e-4f);
		float FwJfull = (1.0f - FwUx) * (1.0f - FwUy) - FwUxy * FwUxy;
		float FwHead = lerp(Fade.y, saturate((Fold.x - FwJfull) / FwSoft), FwLeast);
		float FwTail = 0.0f;
		for (int FwL = 0; FwL < 4; FwL++)
		{
			float FwLag = FwL == 0 ? 1.0f : (FwL == 1 ? 2.0f : (FwL == 2 ? 4.0f : 8.0f));
			float FwAx = FwUx;
			float FwAy = FwUy;
			float FwAxy = FwUxy;
			for (int FwJ3 = 0; FwJ3 < 3; FwJ3++)
			{
				float FwTh0 = 6.28318531f * Fw.Turn(FwT[FwJ3].x * FwRest.x + FwT[FwJ3].y * FwRest.y + FwT[FwJ3].w);
				float FwThL = 6.28318531f * Fw.Turn(FwT[FwJ3].x * FwRest.x + FwT[FwJ3].y * FwRest.y + FwT[FwJ3].w + FwS[FwJ3].z * FwLag * 0.159154943f);
				float FwDc = cos(FwThL) - cos(FwTh0);
				FwAx += FwLx[FwJ3] * FwDc;
				FwAy += FwLy[FwJ3] * FwDc;
				FwAxy += FwLxy[FwJ3] * FwDc;
			}
			float FwJl = (1.0f - FwAx) * (1.0f - FwAy) - FwAxy * FwAxy;
			FwTail += saturate((Fold.x - FwJl) / FwSoft) * exp(-FwLag / max(Fold.z, 1e-3f));
		}
		FwTail = lerp(Fade.z, saturate(FwTail) * Fold.w, FwLeast);
		FwOut = float4(FwHead, FwTail, -(FwXx + FwYy), FwZ / max(FwTop, 1e-4f));
	}
}
return FwOut;
