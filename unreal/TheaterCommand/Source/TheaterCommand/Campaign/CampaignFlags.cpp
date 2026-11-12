#include "Campaign/CampaignFlags.h"

#include "Engine/Texture2D.h"
#include "TextureResource.h"

namespace
{
	FLinearColor Hex(const TCHAR* Code)
	{
		return FLinearColor(FColor::FromHex(Code));
	}

	bool InPolygon(const TArray<FVector2D>& Points, double X, double Y)
	{
		bool bInside = false;
		for (int32 i = 0, j = Points.Num() - 1; i < Points.Num(); j = i++)
		{
			if ((Points[i].Y > Y) != (Points[j].Y > Y)
				&& X < (Points[j].X - Points[i].X) * (Y - Points[i].Y) / (Points[j].Y - Points[i].Y) + Points[i].X)
			{
				bInside = !bInside;
			}
		}
		return bInside;
	}

	/**
	 * A flag being painted. The field is three by two: x runs from 0 at the hoist (the pole) to 1.5 at the fly,
	 * and y runs from 0 at the top to 1 at the bottom. Shapes are laid on one over another, like cut cloth.
	 */
	class FFlagPainter
	{
	public:
		static constexpr double Fly = 1.5;

		const int32 SizeX;
		const int32 SizeY;
		TArray<FLinearColor> Pixels;

		FFlagPainter(int32 InSizeX, int32 InSizeY)
			: SizeX(InSizeX), SizeY(InSizeY)
		{
			Pixels.Init(FLinearColor::Black, SizeX * SizeY);
		}

		/** Lays a colour wherever Inside says, within the box given. Edges are softened by testing sixteen points a pixel. */
		template <typename TInside>
		void Fill(const FLinearColor& Colour, double X0, double Y0, double X1, double Y1, TInside&& Inside)
		{
			constexpr int32 Samples = 4;
			const int32 I0 = FMath::Clamp(FMath::FloorToInt32(X0 / Fly * SizeX), 0, SizeX - 1);
			const int32 I1 = FMath::Clamp(FMath::CeilToInt32(X1 / Fly * SizeX), 0, SizeX - 1);
			const int32 J0 = FMath::Clamp(FMath::FloorToInt32(Y0 * SizeY), 0, SizeY - 1);
			const int32 J1 = FMath::Clamp(FMath::CeilToInt32(Y1 * SizeY), 0, SizeY - 1);
			for (int32 J = J0; J <= J1; ++J)
			{
				for (int32 I = I0; I <= I1; ++I)
				{
					int32 Hits = 0;
					for (int32 B = 0; B < Samples; ++B)
					{
						for (int32 A = 0; A < Samples; ++A)
						{
							const double X = (I + (A + 0.5) / Samples) * Fly / SizeX;
							const double Y = (J + (B + 0.5) / Samples) / SizeY;
							if (Inside(X, Y))
							{
								++Hits;
							}
						}
					}
					if (Hits > 0)
					{
						FLinearColor& Pixel = Pixels[J * SizeX + I];
						Pixel = FMath::Lerp(Pixel, Colour, static_cast<float>(Hits) / (Samples * Samples));
					}
				}
			}
		}

		void Field(const FLinearColor& Colour)
		{
			for (FLinearColor& Pixel : Pixels)
			{
				Pixel = Colour;
			}
		}

		void Rect(const FLinearColor& Colour, double X0, double Y0, double X1, double Y1)
		{
			Fill(Colour, X0, Y0, X1, Y1, [=](double X, double Y) { return X >= X0 && X < X1 && Y >= Y0 && Y < Y1; });
		}

		/** Bands one under another, from the top. Shares says how deep each is; left out, they are equal. */
		void Rows(const TArray<FLinearColor>& Colours, const TArray<double>& Shares = {})
		{
			double Total = 0.0;
			for (int32 i = 0; i < Colours.Num(); ++i)
			{
				Total += Shares.IsValidIndex(i) ? Shares[i] : 1.0;
			}
			double At = 0.0;
			for (int32 i = 0; i < Colours.Num(); ++i)
			{
				const double Depth = (Shares.IsValidIndex(i) ? Shares[i] : 1.0) / Total;
				Rect(Colours[i], 0.0, At, Fly, i == Colours.Num() - 1 ? 1.0 : At + Depth);
				At += Depth;
			}
		}

		/** Bands side by side, from the hoist. */
		void Columns(const TArray<FLinearColor>& Colours, const TArray<double>& Shares = {})
		{
			double Total = 0.0;
			for (int32 i = 0; i < Colours.Num(); ++i)
			{
				Total += Shares.IsValidIndex(i) ? Shares[i] : 1.0;
			}
			double At = 0.0;
			for (int32 i = 0; i < Colours.Num(); ++i)
			{
				const double Width = (Shares.IsValidIndex(i) ? Shares[i] : 1.0) / Total * Fly;
				Rect(Colours[i], At, 0.0, i == Colours.Num() - 1 ? Fly : At + Width, 1.0);
				At += Width;
			}
		}

		void Disc(const FLinearColor& Colour, double CX, double CY, double Radius)
		{
			Fill(Colour, CX - Radius, CY - Radius, CX + Radius, CY + Radius,
				[=](double X, double Y) { return (X - CX) * (X - CX) + (Y - CY) * (Y - CY) < Radius * Radius; });
		}

		void Ring(const FLinearColor& Colour, double CX, double CY, double Inner, double Outer)
		{
			Fill(Colour, CX - Outer, CY - Outer, CX + Outer, CY + Outer, [=](double X, double Y)
			{
				const double D = (X - CX) * (X - CX) + (Y - CY) * (Y - CY);
				return D < Outer * Outer && D >= Inner * Inner;
			});
		}

		/** A disc with a bite taken out of it by another: a crescent moon, or the blade of a sickle. */
		void Crescent(const FLinearColor& Colour, double CX, double CY, double Radius, double BiteX, double BiteY, double BiteRadius)
		{
			Fill(Colour, CX - Radius, CY - Radius, CX + Radius, CY + Radius, [=](double X, double Y)
			{
				return (X - CX) * (X - CX) + (Y - CY) * (Y - CY) < Radius * Radius
					&& (X - BiteX) * (X - BiteX) + (Y - BiteY) * (Y - BiteY) >= BiteRadius * BiteRadius;
			});
		}

		void Polygon(const FLinearColor& Colour, const TArray<FVector2D>& Points)
		{
			FBox2D Box(ForceInit);
			for (const FVector2D& Point : Points)
			{
				Box += Point;
			}
			Fill(Colour, Box.Min.X, Box.Min.Y, Box.Max.X, Box.Max.Y, [&Points](double X, double Y) { return InPolygon(Points, X, Y); });
		}

		/** A star with one point upward, or turned clockwise by Turn degrees. Inner is how deep the notches are cut. */
		void Star(const FLinearColor& Colour, double CX, double CY, double Radius, int32 PointCount = 5, double Inner = 0.382, double Turn = 0.0)
		{
			TArray<FVector2D> Points;
			for (int32 i = 0; i < PointCount * 2; ++i)
			{
				const double Angle = FMath::DegreesToRadians(Turn) + i * UE_DOUBLE_PI / PointCount;
				const double R = i % 2 == 0 ? Radius : Radius * Inner;
				Points.Add(FVector2D(CX + R * FMath::Sin(Angle), CY - R * FMath::Cos(Angle)));
			}
			Polygon(Colour, Points);
		}

		/** A straight bar from one point to another, square at the ends. */
		void Bar(const FLinearColor& Colour, double X0, double Y0, double X1, double Y1, double HalfWidth)
		{
			FVector2D Along(X1 - X0, Y1 - Y0);
			Along.Normalize();
			const FVector2D Across(-Along.Y * HalfWidth, Along.X * HalfWidth);
			Polygon(Colour, { FVector2D(X0, Y0) + Across, FVector2D(X1, Y1) + Across, FVector2D(X1, Y1) - Across, FVector2D(X0, Y0) - Across });
		}

		/** A cross running the whole width and height of the flag: the Scandinavian kind when it is set toward the hoist. */
		void LongCross(const FLinearColor& Colour, double CX, double CY, double HalfWidth)
		{
			Rect(Colour, CX - HalfWidth, 0.0, CX + HalfWidth, 1.0);
			Rect(Colour, 0.0, CY - HalfWidth, Fly, CY + HalfWidth);
		}

		/** A cross with arms of a fixed length, standing clear of the edges. */
		void ShortCross(const FLinearColor& Colour, double CX, double CY, double Arm, double HalfWidth)
		{
			Rect(Colour, CX - HalfWidth, CY - Arm, CX + HalfWidth, CY + Arm);
			Rect(Colour, CX - Arm, CY - HalfWidth, CX + Arm, CY + HalfWidth);
		}

		/** A cross whose arms widen toward their ends. */
		void IronCross(const FLinearColor& Colour, double CX, double CY, double Arm, double Narrow, double Wide)
		{
			const double L = Arm, A = Narrow, B = Wide;
			TArray<FVector2D> Points = {
				{ -A, -A }, { -B, -L }, { B, -L }, { A, -A }, { L, -B }, { L, B },
				{ A, A }, { B, L }, { -B, L }, { -A, A }, { -L, B }, { -L, -B } };
			for (FVector2D& Point : Points)
			{
				Point += FVector2D(CX, CY);
			}
			Polygon(Colour, Points);
		}

		/** A shield: square at the top, round at the foot. */
		void Shield(const FLinearColor& Colour, double CX, double Top, double HalfWidth, double Depth)
		{
			Rect(Colour, CX - HalfWidth, Top, CX + HalfWidth, Top + Depth - HalfWidth);
			Fill(Colour, CX - HalfWidth, Top + Depth - HalfWidth, CX + HalfWidth, Top + Depth, [=](double X, double Y)
			{
				const double DY = Y - (Top + Depth - HalfWidth);
				return DY >= 0.0 && (X - CX) * (X - CX) + DY * DY < HalfWidth * HalfWidth;
			});
		}

		/** The Union Flag, filling the box given: the United Kingdom's own flag, or the corner of a dominion's. */
		void UnionFlag(double X0, double Y0, double X1, double Y1)
		{
			const FLinearColor Blue = Hex(TEXT("012169")), White = Hex(TEXT("FFFFFF")), Red = Hex(TEXT("C8102E"));
			const double Depth = Y1 - Y0, Aspect = (X1 - X0) / Depth;
			// A point measured from the middle of the box, in box heights, with y upward.
			auto Local = [=](double X, double Y, double& PX, double& PY)
			{
				PX = ((X - X0) / (X1 - X0) - 0.5) * Aspect;
				PY = 0.5 - (Y - Y0) / Depth;
				return X >= X0 && X < X1 && Y >= Y0 && Y < Y1;
			};
			// How far a point is from the middle line of the diagonal arm it lies on; positive on the arm's anticlockwise side.
			auto FromDiagonal = [=](double PX, double PY)
			{
				FVector2D Arm((PX >= 0.0 ? 1.0 : -1.0) * Aspect * 0.5, (PY >= 0.0 ? 1.0 : -1.0) * 0.5);
				Arm.Normalize();
				return PX * -Arm.Y + PY * Arm.X;
			};
			Rect(Blue, X0, Y0, X1, Y1);
			Fill(White, X0, Y0, X1, Y1, [&](double X, double Y)
			{
				double PX, PY;
				return Local(X, Y, PX, PY) && FMath::Abs(FromDiagonal(PX, PY)) < 0.1;
			});
			// The red diagonals sit to one side of the white ones, turning like a pinwheel.
			Fill(Red, X0, Y0, X1, Y1, [&](double X, double Y)
			{
				double PX, PY;
				if (!Local(X, Y, PX, PY))
				{
					return false;
				}
				const double Side = FromDiagonal(PX, PY);
				return Side > 0.0 && Side < 1.0 / 15.0;
			});
			Fill(White, X0, Y0, X1, Y1, [&](double X, double Y)
			{
				double PX, PY;
				return Local(X, Y, PX, PY) && (FMath::Abs(PX) < 1.0 / 6.0 || FMath::Abs(PY) < 1.0 / 6.0);
			});
			Fill(Red, X0, Y0, X1, Y1, [&](double X, double Y)
			{
				double PX, PY;
				return Local(X, Y, PX, PY) && (FMath::Abs(PX) < 0.1 || FMath::Abs(PY) < 0.1);
			});
		}

		/** A darker line round the edge, so a pale flag does not melt into pale ground. */
		void Hem()
		{
			const int32 Border = FMath::Max(2, SizeY / 90);
			for (int32 J = 0; J < SizeY; ++J)
			{
				for (int32 I = 0; I < SizeX; ++I)
				{
					// The picture is stored twice as wide as it is high for a flag half as wide again as high,
					// so a hem of even thickness is a third more pixels across than down.
					const int32 Edge = FMath::Min(FMath::Min(I, SizeX - 1 - I) * 3 / 4, FMath::Min(J, SizeY - 1 - J));
					if (Edge < Border)
					{
						FLinearColor& Pixel = Pixels[J * SizeX + I];
						Pixel = FMath::Lerp(Pixel, Pixel * 0.42f, 0.8f);
						Pixel.A = 1.0f;
					}
				}
			}
		}
	};

	using FDesign = TFunction<void(FFlagPainter&)>;

	const FLinearColor White = FLinearColor(FColor::FromHex(TEXT("FFFFFF")));
	const FLinearColor Black = FLinearColor(FColor::FromHex(TEXT("111111")));
	const FLinearColor Gold = FLinearColor(FColor::FromHex(TEXT("FFD21F")));

	void RedEnsign(FFlagPainter& P, const FLinearColor& Ground)
	{
		P.Field(Ground);
		P.UnionFlag(0.0, 0.0, 0.75, 0.5);
	}

	/** The flags, by the tag each nation goes by in the campaign data. */
	const TMap<FString, FDesign>& Designs()
	{
		static TMap<FString, FDesign> Map;
		if (Map.Num() > 0)
		{
			return Map;
		}

		// ---------------------------------------------------------------- the great powers
		Map.Add(TEXT("GER"), [](FFlagPainter& P)
		{
			// Black, white and red, with the iron cross.
			P.Rows({ Black, White, Hex(TEXT("D10A10")) });
			P.IronCross(White, 0.75, 0.5, 0.36, 0.07, 0.2);
			P.IronCross(Black, 0.75, 0.5, 0.32, 0.036, 0.16);
		});
		Map.Add(TEXT("UK"), [](FFlagPainter& P)
		{
			P.UnionFlag(0.0, 0.0, FFlagPainter::Fly, 1.0);
		});
		Map.Add(TEXT("FRA"), [](FFlagPainter& P)
		{
			P.Columns({ Hex(TEXT("002395")), White, Hex(TEXT("ED2939")) });
		});
		Map.Add(TEXT("VIC"), [](FFlagPainter& P)
		{
			// The tricolour with the Marshal's axe on the white, as on the head of state's standard.
			P.Columns({ Hex(TEXT("002395")), White, Hex(TEXT("ED2939")) });
			const FLinearColor Axe = Hex(TEXT("B08A1E"));
			P.Rect(Axe, 0.735, 0.24, 0.765, 0.78);
			P.Crescent(Axe, 0.66, 0.36, 0.12, 0.60, 0.36, 0.12);
			P.Crescent(Axe, 0.84, 0.36, 0.12, 0.90, 0.36, 0.12);
		});
		Map.Add(TEXT("FFR"), [](FFlagPainter& P)
		{
			// Free France: the tricolour with the cross of Lorraine.
			P.Columns({ Hex(TEXT("002395")), White, Hex(TEXT("ED2939")) });
			const FLinearColor Red = Hex(TEXT("ED2939"));
			P.Rect(Red, 0.725, 0.2, 0.775, 0.8);
			P.Rect(Red, 0.65, 0.31, 0.85, 0.36);
			P.Rect(Red, 0.6, 0.47, 0.9, 0.52);
		});
		Map.Add(TEXT("ITA"), [](FFlagPainter& P)
		{
			// The kingdom's tricolour with the arms of Savoy: a white cross on red, bordered blue.
			P.Columns({ Hex(TEXT("008C45")), Hex(TEXT("F4F5F0")), Hex(TEXT("CD212A")) });
			P.Rect(Hex(TEXT("0B3A8F")), 0.615, 0.29, 0.885, 0.71);
			P.Rect(Hex(TEXT("D21F2B")), 0.65, 0.325, 0.85, 0.675);
			P.Rect(White, 0.65, 0.465, 0.85, 0.535);
			P.Rect(White, 0.715, 0.325, 0.785, 0.675);
		});
		Map.Add(TEXT("SOV"), [](FFlagPainter& P)
		{
			// Red, with the gold hammer and sickle under a gold-edged star in the corner by the pole.
			P.Field(Hex(TEXT("CC0000")));
			P.Star(Gold, 0.3, 0.15, 0.075);
			P.Star(Hex(TEXT("CC0000")), 0.3, 0.15, 0.043);
			P.Crescent(Gold, 0.31, 0.4, 0.14, 0.27, 0.385, 0.125); // the blade
			P.Bar(Gold, 0.235, 0.515, 0.185, 0.565, 0.022); // its handle
			P.Bar(Gold, 0.4, 0.56, 0.235, 0.33, 0.016); // the hammer's shaft
			P.Bar(Gold, 0.185, 0.36, 0.28, 0.292, 0.03); // and its head
		});
		Map.Add(TEXT("USA"), [](FFlagPainter& P)
		{
			// Thirteen stripes and forty-eight stars, as it was until 1959.
			for (int32 i = 0; i < 13; ++i)
			{
				P.Rect(i % 2 == 0 ? Hex(TEXT("B22234")) : White, 0.0, i / 13.0, FFlagPainter::Fly, i == 12 ? 1.0 : (i + 1) / 13.0);
			}
			const double CantonWidth = 0.62, CantonDepth = 7.0 / 13.0;
			P.Rect(Hex(TEXT("3C3B6E")), 0.0, 0.0, CantonWidth, CantonDepth);
			for (int32 Row = 0; Row < 6; ++Row)
			{
				for (int32 Column = 0; Column < 8; ++Column)
				{
					P.Star(White, (Column + 0.5) * CantonWidth / 8.0, (Row + 0.5) * CantonDepth / 6.0, 0.031);
				}
			}
		});
		Map.Add(TEXT("JAP"), [](FFlagPainter& P)
		{
			P.Field(White);
			P.Disc(Hex(TEXT("BC002D")), 0.75, 0.5, 0.3);
		});
		Map.Add(TEXT("CHN"), [](FFlagPainter& P)
		{
			// The Republic: a white sun in a blue sky, on a red ground.
			const FLinearColor Blue = Hex(TEXT("0A1C8C"));
			P.Field(Hex(TEXT("E0101A")));
			P.Rect(Blue, 0.0, 0.0, 0.75, 0.5);
			P.Star(White, 0.375, 0.25, 0.19, 12, 0.62);
			P.Disc(Blue, 0.375, 0.25, 0.112);
			P.Disc(White, 0.375, 0.25, 0.094);
		});

		// ---------------------------------------------------------------- western and northern Europe
		Map.Add(TEXT("POL"), [](FFlagPainter& P) { P.Rows({ White, Hex(TEXT("DC143C")) }); });
		Map.Add(TEXT("BEL"), [](FFlagPainter& P) { P.Columns({ Black, Hex(TEXT("FAE042")), Hex(TEXT("ED2939")) }); });
		Map.Add(TEXT("NED"), [](FFlagPainter& P) { P.Rows({ Hex(TEXT("AE1C28")), White, Hex(TEXT("21468B")) }); });
		Map.Add(TEXT("LUX"), [](FFlagPainter& P) { P.Rows({ Hex(TEXT("EF3340")), White, Hex(TEXT("00A3E0")) }); });
		Map.Add(TEXT("IRL"), [](FFlagPainter& P) { P.Columns({ Hex(TEXT("169B62")), White, Hex(TEXT("FF883E")) }); });
		Map.Add(TEXT("DEN"), [](FFlagPainter& P)
		{
			P.Field(Hex(TEXT("C8102E")));
			P.LongCross(White, 0.5, 0.5, 0.0715);
		});
		Map.Add(TEXT("NOR"), [](FFlagPainter& P)
		{
			P.Field(Hex(TEXT("BA0C2F")));
			P.LongCross(White, 0.5, 0.5, 0.125);
			P.LongCross(Hex(TEXT("00205B")), 0.5, 0.5, 0.0625);
		});
		Map.Add(TEXT("SWE"), [](FFlagPainter& P)
		{
			P.Field(Hex(TEXT("006AA7")));
			P.LongCross(Hex(TEXT("FECC00")), 0.56, 0.5, 0.1);
		});
		Map.Add(TEXT("FIN"), [](FFlagPainter& P)
		{
			P.Field(White);
			P.LongCross(Hex(TEXT("003580")), 0.56, 0.5, 0.136);
		});
		Map.Add(TEXT("ICE"), [](FFlagPainter& P)
		{
			P.Field(Hex(TEXT("02529C")));
			P.LongCross(White, 0.5, 0.5, 0.11);
			P.LongCross(Hex(TEXT("DC1E35")), 0.5, 0.5, 0.055);
		});
		Map.Add(TEXT("SUI"), [](FFlagPainter& P)
		{
			P.Field(Hex(TEXT("DA291C")));
			P.ShortCross(White, 0.75, 0.5, 0.31, 0.095);
		});
		Map.Add(TEXT("ESP"), [](FFlagPainter& P)
		{
			// Red, yellow and red, with the black eagle carrying the quartered shield.
			P.Rows({ Hex(TEXT("AA151B")), Hex(TEXT("F1BF00")), Hex(TEXT("AA151B")) }, { 1.0, 2.0, 1.0 });
			P.Polygon(Black, { { 0.6, 0.38 }, { 0.36, 0.3 }, { 0.4, 0.55 }, { 0.5, 0.66 }, { 0.6, 0.6 } });
			P.Polygon(Black, { { 0.6, 0.38 }, { 0.84, 0.3 }, { 0.8, 0.55 }, { 0.7, 0.66 }, { 0.6, 0.6 } });
			P.Disc(Black, 0.6, 0.31, 0.045);
			P.Shield(Hex(TEXT("AA151B")), 0.6, 0.4, 0.085, 0.27);
			P.Rect(White, 0.6, 0.4, 0.685, 0.52);
			P.Rect(Hex(TEXT("F1BF00")), 0.515, 0.52, 0.6, 0.6);
		});
		Map.Add(TEXT("POR"), [](FFlagPainter& P)
		{
			// Green and red, with the golden sphere and the shield.
			P.Columns({ Hex(TEXT("046A38")), Hex(TEXT("DA291C")) }, { 2.0, 3.0 });
			P.Disc(Hex(TEXT("FFE012")), 0.6, 0.5, 0.25);
			P.Disc(Hex(TEXT("DA291C")), 0.6, 0.5, 0.2);
			P.Ring(Hex(TEXT("FFE012")), 0.6, 0.5, 0.125, 0.15);
			P.Shield(Hex(TEXT("DA291C")), 0.6, 0.35, 0.115, 0.31);
			P.Shield(White, 0.6, 0.385, 0.08, 0.235);
			for (const FVector2D& Dot : { FVector2D(0.6, 0.43), FVector2D(0.6, 0.49), FVector2D(0.6, 0.55), FVector2D(0.555, 0.49), FVector2D(0.645, 0.49) })
			{
				P.Disc(Hex(TEXT("003399")), Dot.X, Dot.Y, 0.019);
			}
		});

		// ---------------------------------------------------------------- the Baltic and the east
		Map.Add(TEXT("EST"), [](FFlagPainter& P) { P.Rows({ Hex(TEXT("0072CE")), Black, White }); });
		Map.Add(TEXT("LAT"), [](FFlagPainter& P) { P.Rows({ Hex(TEXT("9E3039")), White, Hex(TEXT("9E3039")) }, { 2.0, 1.0, 2.0 }); });
		Map.Add(TEXT("LIT"), [](FFlagPainter& P) { P.Rows({ Hex(TEXT("FDB913")), Hex(TEXT("006A44")), Hex(TEXT("C1272D")) }); });
		Map.Add(TEXT("SVK"), [](FFlagPainter& P) { P.Rows({ White, Hex(TEXT("0B4EA2")), Hex(TEXT("EE1C25")) }); });
		Map.Add(TEXT("CZE"), [](FFlagPainter& P)
		{
			P.Rows({ White, Hex(TEXT("D7141A")) });
			P.Polygon(Hex(TEXT("11457E")), { { 0.0, 0.0 }, { 0.75, 0.5 }, { 0.0, 1.0 } });
		});
		Map.Add(TEXT("AUT"), [](FFlagPainter& P) { P.Rows({ Hex(TEXT("ED2939")), White, Hex(TEXT("ED2939")) }); });
		Map.Add(TEXT("HUN"), [](FFlagPainter& P)
		{
			// Red, white and green, with the small arms: the striped half and the double cross.
			P.Rows({ Hex(TEXT("CE2939")), White, Hex(TEXT("477050")) });
			P.Shield(Hex(TEXT("CE2939")), 0.75, 0.36, 0.1, 0.3);
			for (int32 i = 0; i < 2; ++i)
			{
				P.Rect(White, 0.65, 0.405 + i * 0.09, 0.75, 0.45 + i * 0.09);
			}
			P.Rect(White, 0.79, 0.4, 0.81, 0.58);
			P.Rect(White, 0.765, 0.43, 0.835, 0.45);
			P.Rect(White, 0.755, 0.485, 0.845, 0.505);
			P.Rect(Gold, 0.7, 0.325, 0.8, 0.36); // the crown, in a word
		});
		Map.Add(TEXT("ROM"), [](FFlagPainter& P) { P.Columns({ Hex(TEXT("002B7F")), Hex(TEXT("FCD116")), Hex(TEXT("CE1126")) }); });
		Map.Add(TEXT("BUL"), [](FFlagPainter& P) { P.Rows({ White, Hex(TEXT("00966E")), Hex(TEXT("D62612")) }); });
		Map.Add(TEXT("YUG"), [](FFlagPainter& P) { P.Rows({ Hex(TEXT("003893")), White, Hex(TEXT("DE0000")) }); });
		Map.Add(TEXT("CRO"), [](FFlagPainter& P)
		{
			// Red, white and blue with the chequered shield.
			P.Rows({ Hex(TEXT("D5141C")), White, Hex(TEXT("17479E")) });
			P.Shield(White, 0.75, 0.3, 0.125, 0.4);
			for (int32 Row = 0; Row < 5; ++Row)
			{
				for (int32 Column = 0; Column < 5; ++Column)
				{
					if ((Row + Column) % 2 == 1)
					{
						const double X = 0.625 + Column * 0.05, Y = 0.3 + Row * 0.066;
						P.Fill(Hex(TEXT("D5141C")), X, Y, X + 0.05, Y + 0.066, [=](double PX, double PY)
						{
							const double DY = PY - 0.575;
							return PX >= X && PX < X + 0.05 && PY >= Y && PY < Y + 0.066 && (DY < 0.0 || (PX - 0.75) * (PX - 0.75) + DY * DY < 0.125 * 0.125);
						});
					}
				}
			}
		});
		Map.Add(TEXT("ALB"), [](FFlagPainter& P)
		{
			// Red, with the black two-headed eagle.
			P.Field(Hex(TEXT("D81E05")));
			P.Polygon(Black, { { 0.75, 0.34 }, { 0.47, 0.22 }, { 0.5, 0.5 }, { 0.6, 0.62 }, { 0.7, 0.58 }, { 0.66, 0.8 }, { 0.75, 0.74 } });
			P.Polygon(Black, { { 0.75, 0.34 }, { 1.03, 0.22 }, { 1.0, 0.5 }, { 0.9, 0.62 }, { 0.8, 0.58 }, { 0.84, 0.8 }, { 0.75, 0.74 } });
			P.Polygon(Black, { { 0.75, 0.4 }, { 0.66, 0.16 }, { 0.6, 0.22 }, { 0.7, 0.28 } });
			P.Polygon(Black, { { 0.75, 0.4 }, { 0.84, 0.16 }, { 0.9, 0.22 }, { 0.8, 0.28 } });
		});
		Map.Add(TEXT("GRE"), [](FFlagPainter& P)
		{
			// Nine stripes of blue and white, with the white cross in the corner.
			const FLinearColor Blue = Hex(TEXT("0D5EAF"));
			for (int32 i = 0; i < 9; ++i)
			{
				P.Rect(i % 2 == 0 ? Blue : White, 0.0, i / 9.0, FFlagPainter::Fly, i == 8 ? 1.0 : (i + 1) / 9.0);
			}
			const double Canton = 5.0 / 9.0;
			P.Rect(Blue, 0.0, 0.0, Canton, Canton);
			P.Rect(White, Canton * 0.4, 0.0, Canton * 0.6, Canton);
			P.Rect(White, 0.0, Canton * 0.4, Canton, Canton * 0.6);
		});
		Map.Add(TEXT("TUR"), [](FFlagPainter& P)
		{
			P.Field(Hex(TEXT("E30A17")));
			P.Crescent(White, 0.56, 0.5, 0.25, 0.625, 0.5, 0.2);
			P.Star(White, 0.79, 0.5, 0.125, 5, 0.382, -90.0);
		});

		// ---------------------------------------------------------------- the Middle East and Africa
		Map.Add(TEXT("IRQ"), [](FFlagPainter& P)
		{
			// Black, white and green, with two white stars on a red field by the pole.
			P.Rows({ Black, White, Hex(TEXT("007A3D")) });
			P.Polygon(Hex(TEXT("CE1126")), { { 0.0, 0.0 }, { 0.42, 1.0 / 3.0 }, { 0.42, 2.0 / 3.0 }, { 0.0, 1.0 } });
			P.Star(White, 0.17, 0.39, 0.075, 7, 0.45);
			P.Star(White, 0.17, 0.61, 0.075, 7, 0.45);
		});
		Map.Add(TEXT("IRN"), [](FFlagPainter& P)
		{
			// Green, white and red, with the lion and the sun.
			P.Rows({ Hex(TEXT("239F40")), White, Hex(TEXT("DA0000")) });
			const FLinearColor Lion = Hex(TEXT("D9A300"));
			P.Star(Lion, 0.8, 0.44, 0.085, 12, 0.7);
			P.Rect(Lion, 0.66, 0.47, 0.84, 0.56);
			P.Disc(Lion, 0.66, 0.46, 0.045);
			P.Rect(Lion, 0.67, 0.55, 0.7, 0.63);
			P.Rect(Lion, 0.8, 0.55, 0.83, 0.63);
			P.Bar(Lion, 0.63, 0.44, 0.6, 0.33, 0.01);
		});
		Map.Add(TEXT("EGY"), [](FFlagPainter& P)
		{
			P.Field(Hex(TEXT("0B7A3B")));
			P.Crescent(White, 0.72, 0.5, 0.27, 0.8, 0.5, 0.225);
			P.Star(White, 0.83, 0.37, 0.055);
			P.Star(White, 0.9, 0.5, 0.055);
			P.Star(White, 0.83, 0.63, 0.055);
		});
		Map.Add(TEXT("ETH"), [](FFlagPainter& P) { P.Rows({ Hex(TEXT("078930")), Hex(TEXT("FCDD09")), Hex(TEXT("DA121A")) }); });
		Map.Add(TEXT("AFG"), [](FFlagPainter& P)
		{
			P.Columns({ Black, Hex(TEXT("BE0000")), Hex(TEXT("007A36")) });
			P.Ring(White, 0.75, 0.5, 0.13, 0.16);
			P.Rect(White, 0.71, 0.44, 0.79, 0.56);
		});
		Map.Add(TEXT("SAF"), [](FFlagPainter& P)
		{
			// Orange, white and blue, with the three small flags in the middle.
			P.Rows({ Hex(TEXT("F47B20")), White, Hex(TEXT("001489")) });
			P.UnionFlag(0.5, 0.42, 0.66, 0.58);
			P.Rect(Hex(TEXT("F47B20")), 0.67, 0.42, 0.83, 0.58);
			P.Rect(White, 0.71, 0.42, 0.75, 0.58);
			P.Rect(White, 0.79, 0.42, 0.83, 0.58);
			P.Rect(Hex(TEXT("D5141C")), 0.84, 0.42, 1.0, 0.473);
			P.Rect(White, 0.84, 0.473, 1.0, 0.527);
			P.Rect(Hex(TEXT("001489")), 0.84, 0.527, 1.0, 0.58);
			P.Rect(Hex(TEXT("007A36")), 0.84, 0.42, 0.885, 0.58);
		});

		// ---------------------------------------------------------------- the dominions and India
		Map.Add(TEXT("CAN"), [](FFlagPainter& P)
		{
			// The red ensign with the shield of Canada.
			RedEnsign(P, Hex(TEXT("C8102E")));
			P.Shield(White, 1.12, 0.52, 0.13, 0.36);
			P.Rect(Hex(TEXT("C8102E")), 0.99, 0.52, 1.12, 0.62);
			P.Rect(Hex(TEXT("F1BF00")), 1.12, 0.52, 1.25, 0.62);
			P.Rect(Hex(TEXT("00247D")), 0.99, 0.62, 1.25, 0.72);
			P.Star(Hex(TEXT("1E7B34")), 1.12, 0.79, 0.045, 3, 0.5, 180.0);
		});
		Map.Add(TEXT("AUS"), [](FFlagPainter& P)
		{
			RedEnsign(P, Hex(TEXT("00247D")));
			P.Star(White, 0.375, 0.75, 0.15, 7, 0.45);
			P.Star(White, 1.125, 0.83, 0.07, 7, 0.45);
			P.Star(White, 0.94, 0.45, 0.07, 7, 0.45);
			P.Star(White, 1.125, 0.17, 0.07, 7, 0.45);
			P.Star(White, 1.29, 0.37, 0.07, 7, 0.45);
			P.Star(White, 1.2, 0.56, 0.04);
		});
		Map.Add(TEXT("NZL"), [](FFlagPainter& P)
		{
			RedEnsign(P, Hex(TEXT("00247D")));
			for (const FVector& At : { FVector(1.125, 0.2, 0.07), FVector(1.3, 0.42, 0.065), FVector(0.97, 0.48, 0.07), FVector(1.125, 0.8, 0.08) })
			{
				P.Star(White, At.X, At.Y, At.Z);
				P.Star(Hex(TEXT("CC142B")), At.X, At.Y, At.Z * 0.68);
			}
		});
		Map.Add(TEXT("IND"), [](FFlagPainter& P)
		{
			// The red ensign with the Star of India.
			RedEnsign(P, Hex(TEXT("C8102E")));
			P.Star(Gold, 1.12, 0.62, 0.22, 16, 0.6);
			P.Disc(Hex(TEXT("4F9BD9")), 1.12, 0.62, 0.115);
			P.Star(White, 1.12, 0.62, 0.075);
		});

		// ---------------------------------------------------------------- Asia and the Pacific
		Map.Add(TEXT("MAN"), [](FFlagPainter& P)
		{
			// Manchukuo: yellow, with four stripes in the corner.
			P.Field(Hex(TEXT("F2C300")));
			const FLinearColor Stripes[] = { Hex(TEXT("D6202A")), Hex(TEXT("17479E")), White, Black };
			for (int32 i = 0; i < 4; ++i)
			{
				P.Rect(Stripes[i], 0.0, i * 0.125, 0.75, (i + 1) * 0.125);
			}
		});
		Map.Add(TEXT("THA"), [](FFlagPainter& P)
		{
			P.Rows({ Hex(TEXT("A51931")), White, Hex(TEXT("2D2A4A")), White, Hex(TEXT("A51931")) }, { 1.0, 1.0, 2.0, 1.0, 1.0 });
		});
		Map.Add(TEXT("MON"), [](FFlagPainter& P)
		{
			P.Columns({ Hex(TEXT("C4272F")), Hex(TEXT("015197")), Hex(TEXT("C4272F")) });
			P.Star(Gold, 0.25, 0.2, 0.06);
			P.Disc(Gold, 0.25, 0.36, 0.05);
			P.Rect(Gold, 0.17, 0.45, 0.33, 0.49);
			P.Disc(Gold, 0.25, 0.6, 0.065);
			P.Rect(Gold, 0.17, 0.7, 0.33, 0.74);
			P.Rect(Gold, 0.13, 0.42, 0.155, 0.8);
			P.Rect(Gold, 0.345, 0.42, 0.37, 0.8);
		});
		Map.Add(TEXT("PHI"), [](FFlagPainter& P)
		{
			P.Rows({ Hex(TEXT("0038A8")), Hex(TEXT("CE1126")) });
			P.Polygon(White, { { 0.0, 0.0 }, { 0.866, 0.5 }, { 0.0, 1.0 } });
			P.Star(Gold, 0.29, 0.5, 0.15, 8, 0.6);
			P.Disc(Gold, 0.29, 0.5, 0.085);
			P.Star(Gold, 0.07, 0.12, 0.045);
			P.Star(Gold, 0.07, 0.88, 0.045);
			P.Star(Gold, 0.72, 0.5, 0.045);
		});

		// ---------------------------------------------------------------- the Americas
		Map.Add(TEXT("BRA"), [](FFlagPainter& P)
		{
			P.Field(Hex(TEXT("009C3B")));
			P.Polygon(Hex(TEXT("FFDF00")), { { 0.13, 0.5 }, { 0.75, 0.1 }, { 1.37, 0.5 }, { 0.75, 0.9 } });
			P.Disc(Hex(TEXT("002776")), 0.75, 0.5, 0.25);
			P.Fill(White, 0.5, 0.25, 1.0, 0.75, [](double X, double Y)
			{
				const double InDisc = (X - 0.75) * (X - 0.75) + (Y - 0.5) * (Y - 0.5);
				const double FromArc = FMath::Sqrt((X - 0.62) * (X - 0.62) + (Y - 1.02) * (Y - 1.02));
				return InDisc < 0.25 * 0.25 && FMath::Abs(FromArc - 0.56) < 0.024;
			});
			for (const FVector2D& At : { FVector2D(0.66, 0.62), FVector2D(0.75, 0.66), FVector2D(0.84, 0.6), FVector2D(0.78, 0.56), FVector2D(0.9, 0.4) })
			{
				P.Star(White, At.X, At.Y, 0.022);
			}
		});
		Map.Add(TEXT("MEX"), [](FFlagPainter& P)
		{
			P.Columns({ Hex(TEXT("006847")), White, Hex(TEXT("CE1126")) });
			P.Disc(Hex(TEXT("8C6239")), 0.75, 0.47, 0.085);
			P.Polygon(Hex(TEXT("8C6239")), { { 0.75, 0.42 }, { 0.6, 0.36 }, { 0.68, 0.52 } });
			P.Polygon(Hex(TEXT("8C6239")), { { 0.75, 0.42 }, { 0.9, 0.36 }, { 0.82, 0.52 } });
			P.Fill(Hex(TEXT("3E7B3A")), 0.6, 0.5, 0.9, 0.68, [](double X, double Y)
			{
				const double D = FMath::Sqrt((X - 0.75) * (X - 0.75) + (Y - 0.47) * (Y - 0.47));
				return Y > 0.53 && D > 0.125 && D < 0.155;
			});
		});
		Map.Add(TEXT("ARG"), [](FFlagPainter& P)
		{
			P.Rows({ Hex(TEXT("74ACDF")), White, Hex(TEXT("74ACDF")) });
			P.Star(Hex(TEXT("F6B40E")), 0.75, 0.5, 0.13, 16, 0.62);
			P.Disc(Hex(TEXT("F6B40E")), 0.75, 0.5, 0.075);
		});
		Map.Add(TEXT("CHL"), [](FFlagPainter& P)
		{
			P.Rows({ White, Hex(TEXT("D52B1E")) });
			P.Rect(Hex(TEXT("0039A6")), 0.0, 0.0, 0.5, 0.5);
			P.Star(White, 0.25, 0.25, 0.13);
		});
		Map.Add(TEXT("CUB"), [](FFlagPainter& P)
		{
			P.Rows({ Hex(TEXT("002A8F")), White, Hex(TEXT("002A8F")), White, Hex(TEXT("002A8F")) });
			P.Polygon(Hex(TEXT("CF142B")), { { 0.0, 0.0 }, { 0.866, 0.5 }, { 0.0, 1.0 } });
			P.Star(White, 0.29, 0.5, 0.16);
		});
		Map.Add(TEXT("COL"), [](FFlagPainter& P) { P.Rows({ Hex(TEXT("FCD116")), Hex(TEXT("003893")), Hex(TEXT("CE1126")) }, { 2.0, 1.0, 1.0 }); });
		Map.Add(TEXT("PER"), [](FFlagPainter& P) { P.Columns({ Hex(TEXT("D91023")), White, Hex(TEXT("D91023")) }); });

		return Map;
	}

	/** Other names the same nations go by. */
	FString Canonical(const FString& Tag)
	{
		static const TMap<FString, FString> Aliases = {
			{ TEXT("JPN"), TEXT("JAP") }, { TEXT("CHI"), TEXT("CHN") }, { TEXT("ROC"), TEXT("CHN") }, { TEXT("GBR"), TEXT("UK") },
			{ TEXT("ENG"), TEXT("UK") }, { TEXT("USSR"), TEXT("SOV") }, { TEXT("SU"), TEXT("SOV") }, { TEXT("US"), TEXT("USA") },
			{ TEXT("HOL"), TEXT("NED") }, { TEXT("NLD"), TEXT("NED") }, { TEXT("SPA"), TEXT("ESP") }, { TEXT("SWI"), TEXT("SUI") },
			{ TEXT("CHE"), TEXT("SUI") }, { TEXT("ROU"), TEXT("ROM") }, { TEXT("RSA"), TEXT("SAF") }, { TEXT("ZAF"), TEXT("SAF") },
			{ TEXT("RAJ"), TEXT("IND") }, { TEXT("SIA"), TEXT("THA") }, { TEXT("NZ"), TEXT("NZL") }, { TEXT("MGL"), TEXT("MON") },
			{ TEXT("MCK"), TEXT("MAN") }, { TEXT("PRS"), TEXT("IRN") }, { TEXT("PER_"), TEXT("IRN") }, { TEXT("DNK"), TEXT("DEN") },
			{ TEXT("EIR"), TEXT("IRL") }, { TEXT("IRE"), TEXT("IRL") }, { TEXT("GRC"), TEXT("GRE") }, { TEXT("PRT"), TEXT("POR") },
			{ TEXT("BGR"), TEXT("BUL") }, { TEXT("LTU"), TEXT("LIT") }, { TEXT("LVA"), TEXT("LAT") }, { TEXT("VFR"), TEXT("VIC") },
			{ TEXT("FRE"), TEXT("FFR") }, { TEXT("PHL"), TEXT("PHI") }, { TEXT("EGT"), TEXT("EGY") }, { TEXT("ISL"), TEXT("ICE") },
			{ TEXT("DEU"), TEXT("GER") }, { TEXT("CSK"), TEXT("CZE") }, { TEXT("HRV"), TEXT("CRO") }, { TEXT("CHILE"), TEXT("CHL") },
		};
		const FString* Found = Aliases.Find(Tag);
		return Found ? *Found : Tag;
	}
}

bool CampaignFlags::Paint(const FString& Tag, const FLinearColor& NationColour, TArray<FLinearColor>& OutPixels)
{
	FFlagPainter Painter(SizeX, SizeY);
	const FDesign* Design = Designs().Find(Canonical(Tag));
	if (Design)
	{
		(*Design)(Painter);
	}
	else
	{
		// No flag known: the nation's own colour, with a pale band top and bottom.
		const FLinearColor Pale = FMath::Lerp(NationColour, FLinearColor::White, 0.5f);
		Painter.Rows({ Pale, NationColour, Pale }, { 1.0, 4.0, 1.0 });
	}
	Painter.Hem();
	OutPixels = MoveTemp(Painter.Pixels);
	return Design != nullptr;
}

TArray<FString> CampaignFlags::KnownTags()
{
	TArray<FString> Tags;
	Designs().GetKeys(Tags);
	return Tags;
}

UTexture2D* CampaignFlags::MakeTexture(const TArray<FLinearColor>& Pixels, const FName& Name)
{
	if (Pixels.Num() != SizeX * SizeY)
	{
		return nullptr;
	}
	UTexture2D* Texture = UTexture2D::CreateTransient(SizeX, SizeY, PF_B8G8R8A8, Name);
	if (!Texture)
	{
		return nullptr;
	}
	Texture->SRGB = true;
	Texture->Filter = TF_Trilinear;
	Texture->AddressX = TA_Clamp;
	Texture->AddressY = TA_Clamp;
	Texture->NeverStream = true;

	// The picture at full size, then at half, a quarter and so on, so that a flag seen from far off
	// is a smooth small picture and not a shimmering sample of the big one.
	FTexturePlatformData* Platform = Texture->GetPlatformData();
	TArray<FLinearColor> Level = Pixels;
	int32 Width = SizeX, Depth = SizeY;
	for (int32 Mip = 0;; ++Mip)
	{
		if (Mip > 0)
		{
			Platform->Mips.Add(new FTexture2DMipMap(Width, Depth, 1));
		}
		FTexture2DMipMap& Target = Platform->Mips[Mip];
		Target.BulkData.Lock(LOCK_READ_WRITE);
		FColor* Out = reinterpret_cast<FColor*>(Target.BulkData.Realloc(static_cast<int64>(Width) * Depth * sizeof(FColor)));
		for (int32 i = 0; i < Width * Depth; ++i)
		{
			Out[i] = Level[i].ToFColor(true);
			Out[i].A = 255;
		}
		Target.BulkData.Unlock();
		if (Width == 1 && Depth == 1)
		{
			break;
		}
		const int32 HalfWidth = FMath::Max(1, Width / 2), HalfDepth = FMath::Max(1, Depth / 2);
		TArray<FLinearColor> Half;
		Half.SetNumUninitialized(HalfWidth * HalfDepth);
		for (int32 J = 0; J < HalfDepth; ++J)
		{
			for (int32 I = 0; I < HalfWidth; ++I)
			{
				const int32 I0 = FMath::Min(I * 2, Width - 1), I1 = FMath::Min(I * 2 + 1, Width - 1);
				const int32 J0 = FMath::Min(J * 2, Depth - 1), J1 = FMath::Min(J * 2 + 1, Depth - 1);
				Half[J * HalfWidth + I] = (Level[J0 * Width + I0] + Level[J0 * Width + I1] + Level[J1 * Width + I0] + Level[J1 * Width + I1]) * 0.25f;
			}
		}
		Level = MoveTemp(Half);
		Width = HalfWidth;
		Depth = HalfDepth;
	}
	Texture->UpdateResource();
	return Texture;
}
