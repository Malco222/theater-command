#include "weather_fall_picture.h"

#include "weather_fall_chip.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace WeatherFallPicture
{
	namespace
	{
		namespace C = WeatherFallChipCpp;

		C::float4 Four(const float (&V)[4])
		{
			return C::float4(V[0], V[1], V[2], V[3]);
		}

		double Dot3(const double A[3], const double B[3])
		{
			return A[0] * B[0] + A[1] * B[1] + A[2] * B[2];
		}

		/** One corner as the picture sees it: its sample place, its depth, and the corner's own (x across, y along). */
		struct Seen
		{
			double SX = 0.0, SY = 0.0, Z = 0.0, U = 0.0, V = 0.0;
		};

		/** A triangle of a drop laid over the picture: each sample inside it nearer than the ground there takes the
		 * drop's soft shape (perspective-correct across it), its alpha and its colour. */
		void Lay(const Seen& A, const Seen& B, const Seen& D, float Kind, double Alpha, double Tone, const LandFieldPicture::Lens& L, const std::vector<float>& Depth,
			std::vector<float>& Light, std::vector<float>& Cover)
		{
			const double X0 = std::floor(std::min({ A.SX, B.SX, D.SX })), X1 = std::ceil(std::max({ A.SX, B.SX, D.SX }));
			const double Y0 = std::floor(std::min({ A.SY, B.SY, D.SY })), Y1 = std::ceil(std::max({ A.SY, B.SY, D.SY }));
			const int IX0 = std::max(0, static_cast<int>(X0)), IX1 = std::min(L.W - 1, static_cast<int>(X1));
			const int IY0 = std::max(0, static_cast<int>(Y0)), IY1 = std::min(L.H - 1, static_cast<int>(Y1));
			if (IX0 > IX1 || IY0 > IY1) return;
			const double Area = (B.SX - A.SX) * (D.SY - A.SY) - (D.SX - A.SX) * (B.SY - A.SY);
			if (std::fabs(Area) < 1e-9) return;
			C::FFallChip Chip;
			Chip.Pad = 0.0f;
			const double IA = 1.0 / A.Z, IB = 1.0 / B.Z, ID = 1.0 / D.Z;
			for (int Y = IY0; Y <= IY1; ++Y)
				for (int X = IX0; X <= IX1; ++X)
				{
					const double PX = X + 0.5, PY = Y + 0.5;
					const double WB = ((PX - A.SX) * (D.SY - A.SY) - (D.SX - A.SX) * (PY - A.SY)) / Area;
					const double WD = ((B.SX - A.SX) * (PY - A.SY) - (PX - A.SX) * (B.SY - A.SY)) / Area;
					const double WA = 1.0 - WB - WD;
					if (WA < 0.0 || WB < 0.0 || WD < 0.0) continue;
					const double Inv = WA * IA + WB * IB + WD * ID;
					const double Z = 1.0 / Inv;
					const size_t K = static_cast<size_t>(Y) * static_cast<size_t>(L.W) + static_cast<size_t>(X);
					if (!(Z < Depth[K])) continue;
					const double U = (WA * A.U * IA + WB * B.U * IB + WD * D.U * ID) / Inv;
					const double V = (WA * A.V * IA + WB * B.V * IB + WD * D.V * ID) / Inv;
					const double Shape = Chip.Shape(C::float2(static_cast<float>(U), static_cast<float>(V)), Kind);
					const double Got = std::clamp(Alpha * Shape, 0.0, 1.0);
					if (Got <= 0.0) continue;
					Light[K] = static_cast<float>(Light[K] * (1.0 - Got) + Tone * Got);
					Cover[K] = static_cast<float>(1.0 - (1.0 - Cover[K]) * (1.0 - Got));
				}
		}
	}

	std::function<void(const LandFieldPicture::Lens&, const std::vector<float>&, std::vector<float>&, std::vector<float>&)> Over(const WeatherFall::Recipe& R, const Asked& A, int* Drawn)
	{
		return [R, A, Drawn](const LandFieldPicture::Lens& L, const std::vector<float>& Depth, std::vector<float>& Light, std::vector<float>& Cover)
		{
			if (A.Kind == WeatherFall::Kind::None || A.Strength <= 0.0) return;
			const double PixelAngle = 2.0 * std::atan(L.Tan) / std::max(1, L.PixelsAcross);
			const double Step = R.StepOf(A.Kind);
			const float Kind = A.Kind == WeatherFall::Kind::Snow ? 1.0f : 0.0f;
			int Laid = 0;
			uint64_t LayerIndex = 0;
			for (const WeatherFall::Layer& Layer : R.Of(A.Kind))
			{
				// (the game's own seeds and placing: FWeatherFall::Make and Tick)
				const WeatherFall::Mesh M = WeatherFall::Make(Layer, Step, R.MostDrops, A.Seed * 0x9E3779B97F4A7C15ull + LayerIndex * 0x632BE59BD9B4E019ull + static_cast<uint64_t>(A.Kind));
				++LayerIndex;
				const WeatherFall::Params P = WeatherFall::Now(R, Layer, A.Kind, A.Strength, A.WindX, A.WindY, A.Clock, PixelAngle);
				const double Side = Layer.Box;
				double Grid[3];
				for (int Axis = 0; Axis < 3; ++Axis) Grid[Axis] = std::round(L.Eye[Axis] / Side) * Side;
				const C::float3 Eye(static_cast<float>(L.Eye[0] - Grid[0]), static_cast<float>(L.Eye[1] - Grid[1]), static_cast<float>(L.Eye[2] - Grid[2]));
				C::FFallChip Chip;
				Chip.Pad = 0.0f;
				for (int I = 0; I < M.DropCount; ++I)
				{
					Seen Corner[4];
					double Shown = 0.0, Own = 1.0;
					bool bFront = true;
					for (int Q = 0; Q < 4 && bFront; ++Q)
					{
						const size_t K = static_cast<size_t>(I) * 4 + static_cast<size_t>(Q);
						const C::float3 Home(M.Points[K * 3], M.Points[K * 3 + 1], M.Points[K * 3 + 2]);
						const C::float2 Cor(M.Corners[K * 2], M.Corners[K * 2 + 1]);
						const C::float4 Drop(M.Drops[K * 4], M.Drops[K * 4 + 1], M.Drops[K * 4 + 2], M.Drops[K * 4 + 3]);
						const C::FFallOut Out = Chip.Vertex(Home, Cor, Drop, Eye, Four(P.Box), Four(P.Move), Four(P.Look), Four(P.Lens));
						Shown = Out.Look.x;
						Own = Out.Look.y;
						if (Shown <= 0.0) break;
						const double Rel[3] = { Grid[0] + Out.At.x - L.Eye[0], Grid[1] + Out.At.y - L.Eye[1], Grid[2] + Out.At.z - L.Eye[2] };
						const double VX = Dot3(Rel, L.Right), VY = Dot3(Rel, L.Up), VZ = Dot3(Rel, L.Fwd);
						if (VZ < 0.05)
						{
							bFront = false;
							break;
						}
						Corner[Q].SX = 0.5 * L.W * (1.0 + VX / (VZ * L.Tan));
						Corner[Q].SY = 0.5 * L.H - 0.5 * L.W * VY / (VZ * L.Tan);
						Corner[Q].Z = VZ;
						Corner[Q].U = Out.Look.z;
						Corner[Q].V = Out.Look.w;
					}
					if (Shown <= 0.0 || !bFront) continue;
					const double Alpha = Shown * P.Opacity, Tone = A.InkOverHaze * P.Tone * Own;
					Lay(Corner[0], Corner[1], Corner[2], Kind, Alpha, Tone, L, Depth, Light, Cover);
					Lay(Corner[0], Corner[2], Corner[3], Kind, Alpha, Tone, L, Depth, Light, Cover);
					++Laid;
				}
			}
			if (Drawn) *Drawn = Laid;
			std::printf("  %s at %.2f drawn over the picture: %d drops laid (the chip's own arithmetic, Effects/WeatherFallChip.inl)\n", WeatherFall::NameOf(A.Kind), A.Strength, Laid);
		};
	}
}
