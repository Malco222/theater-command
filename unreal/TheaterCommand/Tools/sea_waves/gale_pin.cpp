// THE GALE'S PIN (the release 2 ocean plan, step 14(c), the gale; the sea scene's first condition: below today's 13.8 m/s
// cap, FleetWaves' outputs stay byte for byte as made, and the gale acts only behind its switch). A digest of everything
// the wave core gives at winds up to Raised (13.8 m/s), under its own cap and under the gale's (Make's every field, the
// whitecaps' gate, the frame's drawn sea and the stage's numbers, SeaNear, the sum, its slopes and foam, the ride, the
// grid and the lane), of Make at its edges (no wind, a cap of 0 or less, every PixelTrains from 0 to 16, bearings round the
// whole circle and past it, fetches of a metre and ten thousand kilometres), of the meshes (the clipmap at 96 and 128 at
// every ring count, and the patch), of where the chip puts the points (PlaceOf, now and before), the weights, the drawn
// surface over a fixed point, SeaNear's shift, the whitecaps' fade and their picture's shape, of the chip's own text in
// float (FleetWavesGpu.inl through Gpu::Run, every mode, now and before, and an empty MPC), and of the whitecaps' picture
// and a shore (SeaFace::MakeFoam, MakeShore: Pearl's and Midway's sea), held to gale.sha, which was written by this same
// file built against the wave core at 942d9295 (ocean-r2's head, before the gale). It calls nothing the gale added, so it
// builds against both. Exact bits: doubles and floats by their bytes. Its pins are g++'s (the runner's); a line
// "<hex>  <name>@clang" is clang's own, where the two lay a float differently at 942d9295 itself. Plain C++20, no engine.
// Usage: gale_pin [gale.sha]   (no file: it prints the digests; with one it checks them)
#include "Battle/FleetWaves.h"
#include "Battle/SeaFace.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	using namespace FleetWaves;

	/** FNV-1a, 64 bits, over exact bytes. */
	struct Digest
	{
		uint64_t H = 1469598103934665603ULL;
		long long Count = 0;
		void Bytes(const void* P, size_t N)
		{
			const unsigned char* B = static_cast<const unsigned char*>(P);
			for (size_t K = 0; K < N; ++K)
			{
				H ^= B[K];
				H *= 1099511628211ULL;
			}
			Count += static_cast<long long>(N);
		}
		void D(double V) { Bytes(&V, sizeof(V)); }
		void F(float V) { Bytes(&V, sizeof(V)); }
		void I(long long V) { Bytes(&V, sizeof(V)); }
		void S(const std::string& V) { Bytes(V.data(), V.size()); }
		std::string Hex() const
		{
			char T[32];
			std::snprintf(T, sizeof(T), "%016llx", static_cast<unsigned long long>(H));
			return T;
		}
	};

	void TrainOf(Digest& G, const Train& T)
	{
		for (double V : { T.Kx, T.Ky, T.K, T.W, T.A, T.Q, T.Phase0, T.L, T.Spread }) G.D(V);
		G.I(T.Band);
	}

	void SeaOf(Digest& G, const Sea& S)
	{
		G.I(S.ND);
		G.I(S.NP);
		for (int J = 0; J < MostDrawn; ++J) TrainOf(G, S.D[J]);
		for (int J = 0; J < MostPixel; ++J) TrainOf(G, S.P[J]);
		for (double V : { S.Hs, S.Tp, S.Lp, S.WindU, S.CoxMunk, S.Fold, S.FoldSoft, S.FarHead, S.Wp, S.Force, S.Qb, S.InBands, S.DetailMss, S.TailTau, S.Downwind, S.CapTile })
			G.D(V);
		for (double V : S.Read) G.D(V);
		G.I(S.bSwell ? 1 : 0);
		G.I(static_cast<long long>(S.Seed));
	}

	Sea Made(double U, uint64_t Seed, double SwellH, double SwellT, double Fetch, double Cap, int Pixel = 12)
	{
		Asked A;
		A.WindU = U;
		A.DownwindR = 0.3 + 0.01 * static_cast<double>(Seed % 97);
		A.Seed = Seed;
		A.SwellH = SwellH;
		A.SwellT = SwellT;
		A.SwellFromR = 2.1;
		A.Fetch = Fetch;
		A.Cap = Cap;
		A.PixelTrains = Pixel;
		return Make(A);
	}

	Drawn FrameOf(const Sea& S, double Ex, double Ey, double Ez, double T, const double* Subject, const Drawn* Last, int N = 96)
	{
		const double Eye[3] = { Ex, Ey, Ez }, Look[3] = { Ex + 100.0, Ey + 100.0, 0.0 };
		return DrawnFor(S, Eye, Look, 60.0, Subject, T, Last, Last == nullptr, RingsFor(S, N), N);
	}

	void MeshOf(Digest& G, const Mesh& M)
	{
		G.I(static_cast<long long>(M.Points.size()));
		for (const GridPoint& P : M.Points)
		{
			for (float V : { P.X, P.Y, P.AxisX, P.AxisY }) G.F(V);
			G.I(P.Ring * 16 + P.Kind);
		}
		for (int T : M.Triangles) G.I(T);
	}

	void PlacedOf(Digest& G, const Placed& P)
	{
		for (double V : { P.RestX, P.RestY, P.Cell, P.X, P.Y, P.Z, P.Ease }) G.D(V);
		G.I((P.bCollapsed ? 1 : 0) | (P.bDropped ? 2 : 0) | (P.bNotch ? 4 : 0));
	}

	void Float4Of(Digest& G, const Gpu::float4& V)
	{
		for (float X : { V.x, V.y, V.z, V.w }) G.F(X);
	}

	void DrawnOf(Digest& G, const Drawn& D)
	{
		for (double V : { D.EyeX, D.EyeY, D.EyeBeforeX, D.EyeBeforeY, D.C0, D.SeaT, D.SeaTBefore, D.PxPerRad, D.OriginX, D.OriginY }) G.D(V);
		G.I(D.Rings);
		G.I(D.N);
		for (int K = 0; K < MostRings; ++K)
		{
			G.D(D.RingX[K]);
			G.D(D.RingY[K]);
		}
		const PatchAt& P = D.Patch;
		for (double V : { P.X, P.Y, P.Half, P.Cell, P.SubjectX, P.SubjectY, P.Full, P.EaseEnd, P.Snap, P.Presence, P.WantCell }) G.D(V);
		G.I((P.bOn ? 1 : 0) | (P.bWhole ? 2 : 0) | (P.bSubject ? 4 : 0) | (P.bForced ? 8 : 0));
	}

	void NearOf(Digest& G, const Near& N)
	{
		G.I(N.NL);
		for (double V : { N.Ex, N.Ey, N.Pick }) G.D(V);
		for (const NearLevel& L : N.L)
		{
			for (double V : { L.X0, L.Y0, L.Cell, L.Inv, L.Mx, L.My, L.Reach, L.SInv }) G.D(V);
			G.I(L.Nodes);
			G.I(L.SNodes);
			for (float V : L.H) G.F(V);
			for (float V : L.S) G.F(V);
		}
	}
}

int main(int argc, char** argv)
{
	std::map<std::string, Digest> Parts;
	// Make, at every tenth of a metre a second to Raised, three seeds, no swell and two, no fetch and one, under both caps
	{
		Digest& G = Parts["make"];
		for (int K = 1; K <= 138; ++K)
		{
			const double U = 0.1 * K;
			for (uint64_t Seed : { 1ULL, 239ULL, 4242ULL })
			{
				for (int Swell = 0; Swell < 3; ++Swell)
				{
					for (double Fetch : { 0.0, 30000.0 })
					{
						for (double Cap : { Raised, GaleCap })
						{
							SeaOf(G, Made(U, Seed, Swell == 1 ? 1.5 : (Swell == 2 ? 3.0 : 0.0), Swell == 1 ? 11.0 : (Swell == 2 ? 16.0 : 0.0), Fetch, Cap));
						}
					}
				}
			}
			SeaOf(G, Made(U, 77, 0.0, 0.0, 0.0, GaleCap, 16));
			SeaOf(G, Made(U, 77, 0.0, 0.0, 0.0, Raised, 0));
		}
		// (and the laws themselves, below the cap)
		for (int K = 1; K <= 138; ++K)
		{
			const double U = 0.1 * K;
			double Fold = 0.0, Soft = 0.0, Far = 0.0, Hs = 0.0, Wp = 0.0;
			FoldOf(U, Fold, Soft, Far);
			FetchLaw(U, 12345.0 + 300.0 * K, Hs, Wp);
			for (double V : { Fold, Soft, Far, Hs, Wp, Spectrum(0.5 + 0.01 * K, 0.8, 2.0), SpreadAt(0.5 + 0.01 * K, 0.8), CoxMunkOf(U), MonahanOf(U), ForceOf(U) }) G.D(V);
		}
	}
	// The drawn sea, the stage's numbers, SeaNear, the sum, its slopes and foam, the ride, the grid: winds to Raised
	for (double U : { 3.0, 6.0, 8.0, 10.5, 13.8 })
	{
		for (double Cap : { Raised, GaleCap })
		{
			for (int Swell = 0; Swell < 2; ++Swell)
			{
				const Sea S = Made(U, 4242, Swell ? 2.5 : 0.0, Swell ? 12.0 : 0.0, 0.0, Cap);
				const double Subject[2] = { 600.0, 1450.0 };
				const Drawn D0 = FrameOf(S, 3210.0, -987.0, 14.0, 222.0, Subject, nullptr);
				const Drawn D1 = FrameOf(S, 3213.4, -985.9, 14.0, 222.0 + 1.0 / 60.0, Subject, &D0);
				Digest& Gd = Parts["drawn"];
				DrawnOf(Gd, D0);
				DrawnOf(Gd, D1);
				Digest& Gs = Parts["stage"];
				for (const Drawn* D : { &D0, &D1 })
				{
					for (const auto& [Name, V] : StageList(StageParams(S, *D, 100.0)))
					{
						Gs.S(Name);
						for (float X : { V.R, V.G, V.B, V.A }) Gs.F(X);
					}
				}
				Digest& Gn = Parts["near"];
				Near Nr;
				BuildNear(S, D1, Nr);
				NearOf(Gn, Nr);
				Near Rest;
				BuildNear(S, D1, Rest, MostRings, false);
				NearOf(Gn, Rest);
				Digest& Gm = Parts["sum"];
				for (int P = 0; P < 200; ++P)
				{
					const double X = D1.EyeX + (Mix(9100 + P) - 0.5) * 3000.0, Y = D1.EyeY + (Mix(9300 + P) - 0.5) * 3000.0, T = 222.0 + 7.3 * P;
					const Disp Dp = Displace(S, X, Y, T);
					const Slopes Sl = SlopesAt(S, X, Y, T);
					const Foam Fa = FoamAt(S, X, Y, T), Fd = FoamDrawn(S, X, Y, T, 30.0 + 40.0 * P, 900.0);
					for (double V : { Dp.X, Dp.Y, Dp.Z, Sl.Sx, Sl.Sy, Sl.Squeeze, Sl.J, Sl.Mss, Fa.Head, Fa.Tail, Fa.J, Fd.Head, Fd.Tail, Fd.J }) Gm.D(V);
					for (double V : Fa.Lag) Gm.D(V);
					if (P < 40)
					{
						for (double V : { HeightDrawn(S, D1, X, Y), HeightAt(S, X, Y, T), DrawnMeshZ(S, D1, X, Y), MainMeshZ(S, D1, X, Y), ChordBound(S, D1, X, Y),
								 CrestTop(S, D1, X, Y), CellAt(D1, X, Y), SampleNear(Nr, X, Y), SampleNearRest(Rest, X, Y) })
							Gm.D(V);
					}
				}
				Digest& Gr = Parts["ride"];
				for (double Long : { 95.0, 180.0, 250.0 })
				{
					HullRide H;
					RideTrains(S, Long, 0.1 * Long, 0.7, 12.0, 9.0 + Long / 30.0, H);
					Gr.I(H.N);
					for (int I = 0; I < H.N; ++I)
					{
						Gr.I(H.Of[I]);
						for (const std::complex<double>& C : { H.C[I].Heave, H.C[I].Pitch, H.C[I].Roll })
						{
							Gr.D(C.real());
							Gr.D(C.imag());
						}
					}
					double Heave = 0.0, Pitch = 0.0, Roll = 0.0;
					RideAt(S, H, 512.0, -77.0, 333.3, nullptr, Heave, Pitch, Roll);
					for (double V : { Heave, Pitch, Roll }) Gr.D(V);
				}
				if (Swell == 0)
				{
					Digest& Gg = Parts["grid"];
					Gg.I(RingsFor(S, 96));
					Gg.I(RingsFor(S, 128));
					const Mesh M = Grid(96, RingsFor(S, 96));
					for (const GridPoint& P : M.Points)
					{
						for (float V : { P.X, P.Y, P.AxisX, P.AxisY }) Gg.F(V);
						Gg.I(P.Ring * 16 + P.Kind);
					}
					for (int T : M.Triangles) Gg.I(T);
				}
			}
		}
	}
	// The wake lane, winds to Raised
	{
		Digest& G = Parts["lanes"];
		std::vector<LanePoint> Wake;
		for (int Ship = 0; Ship < 6; ++Ship)
		{
			for (int K = 0; K < 30; ++K)
			{
				LanePoint P;
				P.X = 30.0 + 140.0 * Ship + 9.0 * K;
				P.Y = -400.0 + 25.0 * K + 3.0 * Ship;
				P.Age = 4.0 * K;
				P.Beam = 9.0 + 3.0 * Ship;
				P.Ship = Ship;
				Wake.push_back(P);
			}
		}
		const std::vector<OilDisc> Oil = { { 200.0, 300.0, 45.0 }, { -333.0, 12.0, 15.0 } };
		for (double U : { 0.0, 4.0, 8.0, 13.8 })
		{
			std::vector<uint8_t> RG;
			PaintLanes(Wake, Oil, LaneWindowFor(37.3 + U, -11.9), U, RG);
			G.Bytes(RG.data(), RG.size());
		}
	}
	// SeaFace: the whitecaps' picture as the air sea and the fleet's read it, and a shore (Pearl's and Midway's sea)
	{
		Digest& G = Parts["seaface"];
		const std::vector<uint8_t> Foam = SeaFace::MakeFoam(512, 160.0);
		G.Bytes(Foam.data(), Foam.size());
		SeaFace::Part Isle;
		Isle.X = { -3000.0, 2500.0, 1800.0, -2200.0 };
		Isle.Y = { -1500.0, -1800.0, 2100.0, 1600.0 };
		Isle.Triangles = { 0, 1, 2, 0, 2, 3 };
		const SeaFace::Shore Sh = SeaFace::MakeShore({ Isle }, 70.0, 128);
		G.I(Sh.Size);
		for (double V : { Sh.West, Sh.South, Sh.Span }) G.D(V);
		G.Bytes(Sh.Pixels.data(), Sh.Pixels.size());
	}

	// Make at its edges (the second review of the gale): no wind and the least, the cap given as 0 or less (Raised), winds
	// over the cap under a cap that means Raised, every PixelTrains from 0 to 16, bearings round the whole circle and
	// beyond it, fetches of a metre and of ten thousand kilometres
	{
		Digest& G = Parts["edges"];
		for (double U : { 0.0, 1e-9, 0.25, 0.5, 0.75, 6.0, 13.8, 13.81, 20.0, 30.0 })
		{
			for (double Cap : { Raised, 0.0, -3.0, GaleCap })
			{
				if (Cap == GaleCap && U > Raised) continue; // (the gale's own winds: not 942d9295's)
				for (int Pixel = 0; Pixel <= 16; ++Pixel) SeaOf(G, Made(U, 1234, 0.0, 0.0, 0.0, Cap, Pixel));
				for (double Fetch : { 1.0, 1e7 }) SeaOf(G, Made(U, 99, 0.0, 0.0, Fetch, Cap));
			}
		}
		for (double U : { 0.0, 6.0, 13.8 })
		{
			for (double Cap : { Raised, GaleCap })
			{
				for (double Down : { -7.0, -Pi, 0.0, 1.0, Pi, 5.0, TwoPi, 9.5 })
				{
					for (double From : { -2.0, 0.0, Pi, 7.0 })
					{
						for (int Swell = 0; Swell < 3; ++Swell)
						{
							Asked A;
							A.WindU = U;
							A.DownwindR = Down;
							A.Seed = 4242;
							A.SwellH = Swell == 1 ? 1.8 : (Swell == 2 ? 3.0 : 0.0);
							A.SwellT = Swell == 1 ? 12.0 : (Swell == 2 ? 16.0 : 0.0);
							A.SwellFromR = From;
							A.Cap = Cap;
							SeaOf(G, Make(A));
						}
					}
				}
			}
		}
	}
	// The meshes: the clipmap at 96 and at 128 at every ring count, and the subject's patch
	{
		Digest& G = Parts["mesh"];
		for (int Rings = 1; Rings <= 8; ++Rings) MeshOf(G, Grid(96, Rings));
		for (int Rings = 1; Rings <= 7; ++Rings) MeshOf(G, Grid(128, Rings));
		MeshOf(G, Patch());
	}
	// Where the chip puts the points (PlaceOf, now and before), the weights at a point, the drawn surface over a fixed point,
	// SeaNear's shift, the whitecaps' fade and their picture's shape under a head; and the chip's own text in float
	// (FleetWavesGpu.inl through Gpu::Run: the point's place and rest, now and before, the pixel's light and foam) and an
	// empty MPC under a NaN-keeping shim; at 96 and at 128
	{
		Digest& Gp = Parts["place"];
		Digest& Gc = Parts["chip"];
		const Mesh Pm = Patch();
		const std::vector<uint8_t> Picture = SeaFace::MakeFoam(256, 160.0);
		for (double U : { 3.0, 8.0, 13.8 })
		{
			for (double Cap : { Raised, GaleCap })
			{
				for (int Swell = 0; Swell < 2; ++Swell)
				{
					for (int N : { 96, 128 })
					{
						const Sea S = Made(U, 239, Swell ? 2.5 : 0.0, Swell ? 12.0 : 0.0, 0.0, Cap);
						const double Subject[2] = { -800.0, 1900.0 };
						const Drawn D0 = FrameOf(S, -1234.0, 2345.0, 18.0, 444.0, Subject, nullptr, N);
						const Drawn D = FrameOf(S, -1231.5, 2347.0, 18.0, 444.0 + 1.0 / 60.0, Subject, &D0, N);
						const Mesh M = Grid(D.N, D.Rings);
						for (int Before = 0; Before < 2; ++Before)
						{
							for (size_t K = 0; K < M.Points.size(); K += 37) PlacedOf(Gp, PlaceOf(S, D, M.Points[K], Before == 1));
							for (size_t K = 0; K < Pm.Points.size(); K += 41) PlacedOf(Gp, PlaceOf(S, D, Pm.Points[K], Before == 1));
						}
						Near Nr;
						BuildNear(S, D, Nr);
						for (int P = 0; P < 60; ++P)
						{
							const double X = D.EyeX + (Mix(9500 + P) - 0.5) * 2400.0, Y = D.EyeY + (Mix(9700 + P) - 0.5) * 2400.0;
							double W[MostDrawn] = {};
							WeightsAt(S, D, X, Y, W);
							for (double V : W) Gp.D(V);
							double Rest[2] = { 0.0, 0.0 }, Sx = 0.0, Sy = 0.0;
							if (P < 20) Gp.D(DrawnSurfaceZ(S, D, X, Y, Rest));
							Gp.D(Rest[0]);
							Gp.D(Rest[1]);
							NearShift(Nr, X, Y, Sx, Sy);
							for (double V : { Sx, Sy, FoamFade(S, 5.0 + 90.0 * P, D.PxPerRad), HeadShapeAt(S, Picture, 256, X, Y, D.SeaT) }) Gp.D(V);
						}
						// the chip's text in float
						const double Units = 100.0;
						const ForStage Fs = StageParams(S, D, Units);
						for (int Before = 0; Before < 2; ++Before)
						{
							Gpu::Inputs In = Gpu::From(Fs, Before == 1);
							for (const Mesh* From : { &M, &Pm })
							{
								for (size_t K = 0; K < From->Points.size(); K += (From == &M ? 53 : 61))
								{
									const GridPoint& G = From->Points[K];
									In.Local = Gpu::float2(G.X, G.Y);
									In.Vtx = Gpu::float2(static_cast<float>(G.Ring), static_cast<float>(G.Kind));
									In.Axis = Gpu::float2(G.AxisX, G.AxisY);
									for (float Mode : { 0.0f, 1.0f })
									{
										In.Mode = Mode;
										Float4Of(Gc, Gpu::Run(In));
									}
								}
							}
						}
						Gpu::Inputs In = Gpu::From(Fs, false);
						for (int P = 0; P < 120; ++P)
						{
							const double Rr = 4000.0 * Mix(9900 + P) * Mix(9900 + P), Bb = TwoPi * Mix(10100 + P);
							const double X = D.EyeX + Rr * std::sin(Bb), Y = D.EyeY + Rr * std::cos(Bb);
							In.Local = Gpu::float2(static_cast<float>((Y - D.OriginY) * Units), static_cast<float>((X - D.OriginX) * Units));
							In.Depth = static_cast<float>(std::max(Rr, 3.0) * Units);
							In.Cell = static_cast<float>(CellAt(D, X, Y) * Units);
							for (float Mode : { 2.0f, 3.0f })
							{
								In.Mode = Mode;
								Float4Of(Gc, Gpu::Run(In));
							}
						}
					}
				}
			}
		}
		const ForStage E = EmptyStage();
		Gpu::Inputs In = Gpu::From(E, false);
		const Mesh G7 = Grid(96, 7);
		for (size_t K = 0; K < G7.Points.size(); K += 97)
		{
			In.Local = Gpu::float2(G7.Points[K].X, G7.Points[K].Y);
			In.Vtx = Gpu::float2(static_cast<float>(G7.Points[K].Ring), static_cast<float>(G7.Points[K].Kind));
			for (float Mode : { 0.0f, 1.0f, 2.0f, 3.0f })
			{
				In.Mode = Mode;
				In.Depth = 1000.0f * static_cast<float>(K % 7);
				Float4Of(Gc, Gpu::Run(In, true));
			}
		}
	}

	std::string Text;
	for (const auto& [Name, G] : Parts)
	{
		char Line[160];
		std::snprintf(Line, sizeof(Line), "%s  %s (%lld bytes)\n", G.Hex().c_str(), Name.c_str(), G.Count);
		Text += Line;
	}
	if (argc < 2)
	{
		std::fputs(Text.c_str(), stdout);
		return 0;
	}
	std::ifstream File(argv[1]);
	std::map<std::string, std::string> Pinned;
	std::string Line;
	while (std::getline(File, Line))
	{
		if (Line.empty() || Line[0] == '#') continue;
		std::istringstream In(Line);
		std::string Hex, Name;
		In >> Hex >> Name;
		Pinned[Name] = Hex;
	}
	int Failed = 0;
	for (const auto& [Name, G] : Parts)
	{
#ifdef __clang__
		// (clang lays the grid's floats a little differently from g++, at 942d9295 as now: its own pin, where it has one)
		if (Pinned.count(Name + "@clang")) Pinned[Name] = Pinned[Name + "@clang"];
#endif
		const bool bOk = Pinned.count(Name) && Pinned[Name] == G.Hex();
		Failed += bOk ? 0 : 1;
		std::printf("  %s %s: %s (%lld bytes), pinned at 942d9295 %s\n", bOk ? "ok  " : "FAIL", Name.c_str(), G.Hex().c_str(), G.Count,
			Pinned.count(Name) ? Pinned[Name].c_str() : "nothing");
	}
	for (const auto& [Name, Hex] : Pinned)
	{
		if (!Parts.count(Name) && Name.find('@') == std::string::npos)
		{
			std::printf("  FAIL %s is pinned but no longer made\n", Name.c_str());
			++Failed;
		}
	}
	std::printf("gale_pin: %zu digests below the cap (13.8 m/s), %d changed since 942d9295\n", Parts.size(), Failed);
	return Failed == 0 ? 0 : 1;
}
