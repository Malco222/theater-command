#include "Campaign/CampaignMiniatures.h"
#include "Campaign/CampaignMiniatureParts.h"
#include "TheaterCommand.h"

/**
 * The warships, each class built in code to the measurements of the real ships, in metres: lying on the
 * water with the bows forward, the middle of the ship at the origin and the waterline at height nothing.
 *
 * A hull is a skin over cross-sections from stern to bow, with a deck laid across it; on it stand the
 * things that tell one navy's ships from another's: how many turrets and how many guns in each, one funnel
 * or two or three, the shape of the bridge, the colour of the paint and of the deck.
 *
 * Ships are in scale with one another (a destroyer is under half the length of a battleship) but not with
 * the soldiers and aircraft: a battleship drawn to their scale would be longer than a province.
 */
namespace ShipParts
{
	using MiniatureParts::At;
	using MiniatureParts::Skin;

	static FLinearColor Hex(const TCHAR* Code)
	{
		return FLinearColor(FColor::FromHex(Code));
	}

	static const FLinearColor Boot = Hex(TEXT("23262A"));     // the dark band at the waterline
	static const FLinearColor Steel = Hex(TEXT("2E3134"));
	static const FLinearColor Planks = Hex(TEXT("A08C68"));
	static const FLinearColor Canvas = Hex(TEXT("B7B09A"));

	/** What a navy's ships look like: the grey of the hull, the lighter grey of the upper works, the deck. */
	struct FLivery
	{
		FLinearColor Hull, Upper, Deck, Flight;
	};

	static FLivery LiveryOf(const FString& Tag)
	{
		if (Tag == TEXT("GER")) return { Hex(TEXT("666D72")), Hex(TEXT("8A9297")), Planks, Hex(TEXT("6A6E70")) };
		if (Tag == TEXT("USA")) return { Hex(TEXT("4A5868")), Hex(TEXT("7B8794")), Hex(TEXT("4A5868")), Hex(TEXT("3E4A58")) };
		if (Tag == TEXT("JAP") || Tag == TEXT("JPN")) return { Hex(TEXT("555C61")), Hex(TEXT("666D72")), Hex(TEXT("B09A70")), Hex(TEXT("A89268")) };
		if (Tag == TEXT("ITA")) return { Hex(TEXT("8E969A")), Hex(TEXT("A6ADB0")), Planks, Hex(TEXT("7A7F82")) };
		if (Tag == TEXT("FRA") || Tag == TEXT("VIC") || Tag == TEXT("FFR")) return { Hex(TEXT("7E8890")), Hex(TEXT("98A1A8")), Planks, Hex(TEXT("70767A")) };
		if (Tag == TEXT("SOV")) return { Hex(TEXT("6E777C")), Hex(TEXT("838C91")), Hex(TEXT("7A4A3A")), Hex(TEXT("6A6E70")) };
		return { Hex(TEXT("737C82")), Hex(TEXT("8D969B")), Planks, Hex(TEXT("666C70")) }; // the Royal Navy's grey, and the default
	}

	/** The shape of a hull. */
	struct FHull
	{
		double Length = 200.0, Beam = 25.0;
		/** The height of the deck above the water amidships; how much more at the bow and at the stern. */
		double Deck = 6.0, Sheer = 2.5, Aft = 0.0;
		/** How far the stem leans forward at the deck; how full the stern is (1 square, 0 pointed). */
		double Rake = 6.0, Stern = 0.45;
		/** A raised forecastle: forward of this the deck is so much higher (destroyers). 0 for none. */
		double Break = 0.0, Step = 0.0;
	};

	/** Half the width of the deck at a distance along the hull (-0.5 the stern, +0.5 the bow). */
	static double HalfBeam(const FHull& H, double T)
	{
		if (T >= 0.0)
		{
			return H.Beam * 0.5 * FMath::Pow(FMath::Max(0.0, 1.0 - FMath::Pow(T / 0.5, 1.9)), 0.62);
		}
		const double Full = FMath::Pow(FMath::Max(0.0, 1.0 - FMath::Pow(-T / 0.5, 3.2)), 0.5);
		return H.Beam * 0.5 * FMath::Lerp(Full, FMath::Max(Full, 0.55), H.Stern * FMath::Clamp((-T - 0.3) / 0.2, 0.0, 1.0));
	}

	static double DeckAt(const FHull& H, double X)
	{
		const double T = X / H.Length;
		double Z = H.Deck + H.Sheer * FMath::Square(FMath::Max(0.0, T) / 0.5) + H.Aft * FMath::Square(FMath::Max(0.0, -T) / 0.5);
		if (H.Step > 0.0 && X > H.Break)
		{
			Z += H.Step;
		}
		return Z;
	}

	/** The hull and its deck. */
	static void Hull(FMiniatureMesh& M, const FHull& H, const FLivery& L)
	{
		const int32 Stations = 34;
		TArray<TArray<FVector>> Rings;
		TArray<FVector> Port, Starboard;
		for (int32 K = 0; K <= Stations; ++K)
		{
			const double T = -0.5 + static_cast<double>(K) / Stations;
			const double X = T * H.Length;
			const double Top = FMath::Max(HalfBeam(H, T), 0.25), Z = DeckAt(H, X);
			// The stem leans forward toward the deck; the sides flare out a little above the water toward the bow.
			const double Lean = T > 0.3 ? H.Rake * FMath::Square((T - 0.3) / 0.2) : 0.0;
			const double Water = Top * (T > 0.15 ? FMath::Lerp(0.94, 0.55, (T - 0.15) / 0.35) : 0.94);
			TArray<FVector>& Ring = Rings.AddDefaulted_GetRef();
			Ring.Add(At(X + Lean, -Top, Z));
			Ring.Add(At(X + Lean * 0.5, -(Top + Water) * 0.5, Z * 0.5));
			Ring.Add(At(X, -Water, 0.0));
			Ring.Add(At(X, -Water * 0.8, -2.0));
			Ring.Add(At(X, 0.0, -3.5));
			Ring.Add(At(X, Water * 0.8, -2.0));
			Ring.Add(At(X, Water, 0.0));
			Ring.Add(At(X + Lean * 0.5, (Top + Water) * 0.5, Z * 0.5));
			Ring.Add(At(X + Lean, Top, Z));
			Port.Add(At(X + Lean, -Top, Z + 0.05));
			Starboard.Add(At(X + Lean, Top, Z + 0.05));
		}
		Skin(M, Rings, [&L](const FVector& Place, const FVector&, int32) { return Place.Z < 0.9 ? Boot : L.Hull; });
		// The deck, laid in strips across the hull.
		const FColor DeckPaint = L.Deck.ToFColor(true);
		for (int32 K = 0; K < Stations; ++K)
		{
			M.Quad(Port[K], Starboard[K], Starboard[K + 1], Port[K + 1], FVector::UpVector, DeckPaint);
		}
	}

	/** A ship being built: its hull, its colours, and the mesh. */
	struct FYard
	{
		FMiniatureMesh& M;
		FHull H;
		FLivery L;

		double Deck(double X) const
		{
			return DeckAt(H, X) + 0.05;
		}

		/** A house on the deck: from one distance forward to another, so wide, so high, standing at a level. Its corners are rounded, as plating is. */
		void House(double X0, double X1, double Half, double Z, double High, const FLinearColor& Colour) const
		{
			const double Fore = FMath::Max(X0, X1), Rear = FMath::Min(X0, X1);
			const double Corner = FMath::Min3(Half * 0.6, (Fore - Rear) * 0.3, 3.0);
			TArray<FVector2D> Plan;
			const FVector2D Middles[4] = { { Fore - Corner, Half - Corner }, { Rear + Corner, Half - Corner }, { Rear + Corner, -Half + Corner }, { Fore - Corner, -Half + Corner } };
			for (int32 Quarter = 0; Quarter < 4; ++Quarter)
			{
				for (int32 K = 0; K <= 3; ++K)
				{
					const double Angle = (Quarter + K / 3.0) * UE_DOUBLE_HALF_PI;
					Plan.Add(Middles[Quarter] + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Corner);
				}
			}
			M.Stack(FVector::ZeroVector, 1.0, 0.0, { Plan, Plan }, { Z, Z + High }, Colour, true);
		}

		/** A crane, for boats and aircraft: a post and a jib. */
		void Crane(double X, double Side, double Z, double High) const
		{
			M.Rod(At(X, Side, Z), At(X, Side, Z + High), 0.35, L.Upper, 6);
			M.Rod(At(X, Side, Z + High), At(X - High * 0.9, Side * 0.5, Z + High * 0.7), 0.2, L.Upper, 5);
		}

		/** A funnel: oval, leaning back a little, with a dark cap. */
		void Funnel(double X, double Z, double High, double Long, double Wide, double Lean = 0.06) const
		{
			auto Oval = [](double Cx, double A, double B)
			{
				TArray<FVector2D> Plan;
				for (int32 K = 0; K < 14; ++K)
				{
					Plan.Add(FVector2D(Cx + FMath::Cos(K * UE_DOUBLE_TWO_PI / 14.0) * A, FMath::Sin(K * UE_DOUBLE_TWO_PI / 14.0) * B));
				}
				return Plan;
			};
			M.Stack(FVector::ZeroVector, 1.0, 0.0, { Oval(X, Long * 0.5, Wide * 0.5), Oval(X - High * Lean, Long * 0.47, Wide * 0.47) }, { Z, Z + High * 0.88 }, L.Upper, true);
			M.Stack(FVector::ZeroVector, 1.0, 0.0, { Oval(X - High * Lean, Long * 0.48, Wide * 0.48), Oval(X - High * Lean * 1.1, Long * 0.46, Wide * 0.46) }, { Z + High * 0.88, Z + High }, Steel, true);
		}

		/** A mast, with a yard across it and a top. */
		void Mast(double X, double Z, double High, double Lean = 0.05) const
		{
			M.Limb(At(X, 0.0, Z), At(X - High * Lean, 0.0, Z + High), 0.5, 0.2, L.Upper, 6);
			M.Rod(At(X - High * Lean * 0.7, -High * 0.16, Z + High * 0.72), At(X - High * Lean * 0.7, High * 0.16, Z + High * 0.72), 0.15, L.Upper, 4);
			M.Pillar(At(X - High * Lean * 0.55, 0.0, Z + High * 0.55), 1.2, 1.2, 0.8, L.Upper, 8);
		}

		/**
		 * A gun turret on its barbette: so many barrels of a length, in a house of a size, facing forward (+1) or
		 * aft (-1). Level raises it to fire over the turret in front of it.
		 */
		void Turret(double X, double Side, double Z, double Size, int32 Barrels, double Long, double Facing, double Bore = 0.45) const
		{
			const double Half = Size * 0.5;
			M.Pillar(At(X, Side, Z - 0.2), Half * 0.95, Half * 0.95, 0.9, L.Upper, 14);
			// The gunhouse: narrower in front, with a sloped face.
			const double F = Facing;
			M.Stack(FVector::ZeroVector, 1.0, 0.0,
				{ { { X + F * Half * 1.25, Side - Half * 0.62 }, { X + F * Half * 1.25, Side + Half * 0.62 }, { X + F * Half * 0.2, Side + Half * 0.95 }, { X - F * Half * 1.0, Side + Half * 0.8 }, { X - F * Half * 1.0, Side - Half * 0.8 }, { X + F * Half * 0.2, Side - Half * 0.95 } },
					{ { X + F * Half * 0.95, Side - Half * 0.55 }, { X + F * Half * 0.95, Side + Half * 0.55 }, { X + F * Half * 0.15, Side + Half * 0.85 }, { X - F * Half * 0.95, Side + Half * 0.72 }, { X - F * Half * 0.95, Side - Half * 0.72 }, { X + F * Half * 0.15, Side - Half * 0.85 } } },
				{ Z + 0.7, Z + 0.7 + Size * 0.32 }, L.Upper, false);
			for (int32 K = 0; K < Barrels; ++K)
			{
				const double Out = Side + (K - (Barrels - 1) * 0.5) * Size * (Barrels > 3 ? 0.24 : 0.32);
				M.Limb(At(X + F * Half * 1.05, Out, Z + 0.7 + Size * 0.15), At(X + F * (Half * 1.05 + Long), Out, Z + 0.7 + Size * 0.15 + Long * 0.07), Bore, Bore * 0.7, L.Upper, 6);
				M.Rod(At(X + F * Half * 1.0, Out, Z + 0.7 + Size * 0.15), At(X + F * (Half * 1.05 + Long * 0.2), Out, Z + 0.7 + Size * 0.15 + Long * 0.014), Bore * 1.5, Canvas, 6);
			}
		}

		/** A rangefinder: a small round house with arms out to each side. */
		void Rangefinder(double X, double Z, double Arm) const
		{
			M.Pillar(At(X, 0.0, Z), Arm * 0.3, Arm * 0.28, Arm * 0.3, L.Upper, 10);
			M.Rod(At(X, -Arm, Z + Arm * 0.2), At(X, Arm, Z + Arm * 0.2), Arm * 0.09, L.Upper, 6);
		}

		/** A ship's boat on the deck. */
		void Boat(double X, double Side, double Z, double Long) const
		{
			M.Ball(At(X, Side, Z + Long * 0.08), FVector(Long, Long * 0.28, Long * 0.2), Hex(TEXT("D6D0C0")), 3, 8);
		}

		/** Light anti-aircraft guns: a pair of thin barrels on a small round base. */
		void LightGun(double X, double Side, double Z) const
		{
			M.Pillar(At(X, Side, Z), 1.1, 1.1, 0.7, L.Upper, 8);
			M.Rod(At(X, Side - 0.25, Z + 1.0), At(X + 1.6, Side - 0.25, Z + 2.2), 0.09, Steel, 4);
			M.Rod(At(X, Side + 0.25, Z + 1.0), At(X + 1.6, Side + 0.25, Z + 2.2), 0.09, Steel, 4);
		}
	};

	/** How a battleship's guns are set out, and what stands amidships. */
	struct FCapital
	{
		FHull H;
		/** The big turrets, bow to stern: how far forward, raised or not, how many barrels. */
		struct FBig
		{
			double X;
			bool bRaised;
			int32 Barrels;
		};
		TArray<FBig> Guns;
		double GunSize = 11.0, GunLong = 17.0;
		/** The funnels: how far forward each is. */
		TArray<double> Funnels;
		double FunnelHigh = 14.0, FunnelLean = 0.06;
		/** The bridge tower: how far forward, how high, and whether it is a tall many-storeyed tower (the Japanese pagoda). */
		double TowerX = 30.0, TowerHigh = 22.0;
		bool bPagoda = false;
		int32 SideTurrets = 3;
	};

	static void Capital(FMiniatureMesh& M, const FCapital& C, const FLivery& L)
	{
		const FYard Y{ M, C.H, L };
		Hull(M, C.H, L);
		const double Fore = C.Guns[0].X, Aftmost = C.Guns.Last().X;
		// The big guns.
		for (const FCapital::FBig& G : C.Guns)
		{
			const double Facing = G.X > 0.0 ? 1.0 : -1.0;
			Y.Turret(G.X, 0.0, Y.Deck(G.X) + (G.bRaised ? C.GunSize * 0.36 : 0.0), C.GunSize, G.Barrels, C.GunLong, Facing, 0.5);
			if (G.bRaised)
			{
				M.Pillar(At(G.X, 0.0, Y.Deck(G.X)), C.GunSize * 0.5, C.GunSize * 0.5, C.GunSize * 0.36, L.Upper, 14);
			}
		}
		// The superstructure between the foremost raised turret and the after one: two decks of it.
		double Front = Fore, Back = Aftmost;
		for (const FCapital::FBig& G : C.Guns)
		{
			if (G.X > 0.0) Front = FMath::Min(Front, G.X);
			else Back = FMath::Max(Back, G.X);
		}
		Front -= C.GunSize * 0.9;
		Back += C.GunSize * 0.9;
		const double Z0 = Y.Deck(0.0), Wide = C.H.Beam * 0.5;
		Y.House(Front, Back, Wide * 0.62, Z0, 3.2, L.Upper);
		Y.House(Front - 4.0, Back + 8.0, Wide * 0.44, Z0 + 3.2, 3.0, L.Upper);
		// The bridge tower.
		const double Tz = Z0 + 6.2;
		if (C.bPagoda)
		{
			for (int32 K = 0; K < 6; ++K)
			{
				Y.House(C.TowerX + 5.0 - K * 0.5, C.TowerX - 5.0 + K * 0.4, 4.6 - K * 0.35, Tz + K * C.TowerHigh / 6.5, C.TowerHigh / 6.5 - 0.5, L.Upper);
				Y.House(C.TowerX + 5.6 - K * 0.5, C.TowerX - 5.4 + K * 0.4, 5.1 - K * 0.35, Tz + (K + 1) * C.TowerHigh / 6.5 - 0.5, 0.5, L.Hull);
			}
		}
		else
		{
			Y.House(C.TowerX + 6.0, C.TowerX - 6.0, 6.0, Tz, C.TowerHigh * 0.35, L.Upper);
			Y.House(C.TowerX + 4.5, C.TowerX - 4.0, 4.4, Tz + C.TowerHigh * 0.35, C.TowerHigh * 0.3, L.Upper);
			Y.House(C.TowerX + 3.0, C.TowerX - 3.0, 3.0, Tz + C.TowerHigh * 0.65, C.TowerHigh * 0.25, L.Upper);
			// The bridge windows, a dark band.
			Y.House(C.TowerX + 6.1, C.TowerX + 5.0, 5.4, Tz + C.TowerHigh * 0.2, 1.0, Steel);
		}
		Y.Rangefinder(C.TowerX, Tz + C.TowerHigh * (C.bPagoda ? 0.95 : 0.9), 7.0);
		Y.Mast(C.TowerX - 3.0, Tz + C.TowerHigh * 0.9, C.TowerHigh * 0.5);
		// Funnels, the mainmast, the after control tower with its rangefinder.
		for (const double X : C.Funnels)
		{
			Y.Funnel(X, Z0 + 6.2, C.FunnelHigh, C.H.Beam * 0.34, C.H.Beam * 0.2, C.FunnelLean);
		}
		Y.Mast(Back + 16.0, Z0 + 6.2, C.TowerHigh * 0.95);
		Y.House(Back + 10.0, Back + 3.0, 4.0, Z0 + 6.2, 6.0, L.Upper);
		Y.Rangefinder(Back + 6.0, Z0 + 12.2, 5.0);
		// The smaller turrets along each side, boats, and light guns.
		for (const double Hand : { -1.0, 1.0 })
		{
			for (int32 K = 0; K < C.SideTurrets; ++K)
			{
				const double X = FMath::Lerp(Front - 8.0, Back + 8.0, C.SideTurrets > 1 ? static_cast<double>(K) / (C.SideTurrets - 1) : 0.5);
				Y.Turret(X, Hand * Wide * 0.72, Z0 + 0.3, 5.0, 2, 5.5, X > (Front + Back) * 0.5 ? 1.0 : -1.0, 0.2);
			}
			Y.Boat((Front + Back) * 0.5 - 6.0, Hand * Wide * 0.3, Z0 + 6.2, 10.0);
			for (const double X : { Front - 16.0, (Front + Back) * 0.5 + 10.0, Back + 22.0 })
			{
				Y.LightGun(X, Hand * Wide * 0.5, X > Front - 10.0 || X < Back + 10.0 ? Y.Deck(X) : Z0 + 3.2);
			}
		}
		// A catapult on the quarterdeck with the ship's floatplane on it, and the crane that lifts it back aboard.
		const double Tail = -C.H.Length * 0.43;
		M.Box(At(Tail, 0.0, Y.Deck(Tail) + 0.6), FVector(18.0, 2.0, 1.2), L.Upper);
		M.Vehicle(FMiniatureMesh::MainFighterOf(MiniatureParts::NationWanted()), At(Tail + 2.0, 0.0, Y.Deck(Tail) + 2.6), 1.0, 0.0);
		Y.Crane(Tail - 12.0, 0.0, Y.Deck(Tail - 12.0), 12.0);
		for (const double Hand : { -1.0, 1.0 })
		{
			Y.Crane((Front + Back) * 0.5 + 4.0, Hand * Wide * 0.42, Z0 + 6.2, 9.0);
			// Searchlights on the funnel platforms, and rows of light guns along the upper deck.
			for (const double X : C.Funnels)
			{
				M.Pillar(At(X + 5.0, Hand * Wide * 0.26, Z0 + 6.2), 0.9, 0.9, 2.2, L.Upper, 8);
				M.Ball(At(X + 5.0, Hand * Wide * 0.26, Z0 + 9.2), FVector(1.6, 1.6, 1.6), Steel, 3, 8);
			}
			for (int32 K = 0; K < 6; ++K)
			{
				Y.LightGun(FMath::Lerp(Front - 6.0, Back + 6.0, K / 5.0), Hand * Wide * 0.5, Z0 + 3.2);
			}
		}
		// The ensign staff at the stern and the jack staff at the bow; anchors and their cables on the forecastle.
		M.Rod(At(-C.H.Length * 0.49, 0.0, Y.Deck(-C.H.Length * 0.49)), At(-C.H.Length * 0.495, 0.0, Y.Deck(-C.H.Length * 0.49) + 7.0), 0.12, L.Upper, 4);
		for (const double Hand : { -1.0, 1.0 })
		{
			M.Rod(At(C.H.Length * 0.42, Hand * 1.5, Y.Deck(C.H.Length * 0.42) + 0.1), At(C.H.Length * 0.33, Hand * 1.2, Y.Deck(C.H.Length * 0.33) + 0.1), 0.18, Steel, 4);
		}
	}

	static void Battleship(FMiniatureMesh& M)
	{
		const FString Tag = MiniatureParts::NationWanted();
		const FLivery L = LiveryOf(Tag);
		FCapital C;
		if (Tag == TEXT("GER"))
		{
			// Bismarck: 251 m long, 36 m in the beam; four turrets of two 38 cm guns, one funnel.
			C.H = { 251.0, 36.0, 7.0, 2.5, 0.0, 9.0, 0.5 };
			C.Guns = { { 74.0, false, 2 }, { 56.0, true, 2 }, { -60.0, true, 2 }, { -78.0, false, 2 } };
			C.Funnels = { 4.0 };
			C.TowerX = 32.0;
			C.TowerHigh = 24.0;
		}
		else if (Tag == TEXT("USA"))
		{
			// Iowa: 270 m long, 33 m in the beam; three turrets of three 16-inch guns, two funnels, a long fine bow.
			C.H = { 270.0, 33.0, 6.5, 4.5, 0.0, 12.0, 0.6 };
			C.Guns = { { 72.0, false, 3 }, { 52.0, true, 3 }, { -72.0, false, 3 } };
			C.GunLong = 20.0;
			C.Funnels = { 10.0, -14.0 };
			C.TowerX = 30.0;
			C.TowerHigh = 26.0;
			C.SideTurrets = 4;
		}
		else if (Tag == TEXT("JAP") || Tag == TEXT("JPN"))
		{
			// Yamato: 263 m long, 39 m in the beam; three turrets of three 46 cm guns, one raked funnel, a pagoda of a bridge.
			C.H = { 263.0, 38.9, 6.0, 4.5, 1.0, 10.0, 0.5 };
			C.Guns = { { 64.0, false, 3 }, { 44.0, true, 3 }, { -66.0, false, 3 } };
			C.GunSize = 12.5;
			C.GunLong = 20.0;
			C.Funnels = { -6.0 };
			C.FunnelLean = 0.22;
			C.TowerX = 20.0;
			C.TowerHigh = 30.0;
			C.bPagoda = true;
		}
		else if (Tag == TEXT("ITA"))
		{
			// Littorio: 238 m long, 33 m in the beam; three turrets of three 15-inch guns, two funnels.
			C.H = { 238.0, 32.8, 6.5, 3.0, 0.0, 9.0, 0.5 };
			C.Guns = { { 62.0, false, 3 }, { 44.0, true, 3 }, { -62.0, true, 3 } };
			C.Funnels = { 8.0, -10.0 };
			C.TowerX = 26.0;
		}
		else if (Tag == TEXT("FRA") || Tag == TEXT("VIC") || Tag == TEXT("FFR"))
		{
			// Richelieu: 248 m long, 33 m in the beam; both turrets forward, four 380 mm guns in each.
			C.H = { 248.0, 33.0, 6.5, 3.0, 0.0, 9.0, 0.5 };
			C.Guns = { { 70.0, false, 4 }, { 48.0, true, 4 } };
			C.GunSize = 12.5;
			C.Funnels = { -22.0 };
			C.FunnelLean = 0.3;
			C.TowerX = 22.0;
			C.TowerHigh = 26.0;
			C.SideTurrets = 4;
		}
		else
		{
			// King George V: 227 m long, 31 m in the beam; four guns forward, two above them, four aft; two funnels.
			C.H = { 227.0, 31.4, 6.5, 2.0, 0.0, 6.0, 0.5 };
			C.Guns = { { 64.0, false, 4 }, { 48.0, true, 2 }, { -64.0, false, 4 } };
			C.GunSize = 11.5;
			C.GunLong = 15.0;
			C.Funnels = { 8.0, -16.0 };
			C.TowerX = 28.0;
			C.TowerHigh = 20.0;
			C.SideTurrets = 4;
		}
		Capital(M, C, L);
		if (Tag == TEXT("ITA"))
		{
			// The red and white stripes the Italian navy painted on its forecastles for its own airmen.
			const FYard Y{ M, C.H, L };
			for (int32 K = 0; K < 7; ++K)
			{
				const double X = 78.0 + K * 5.0;
				M.Box(At(X, 0.0, Y.Deck(X) + 0.06), FVector(2.4, HalfBeam(C.H, X / C.H.Length) * 1.7, 0.08), K % 2 ? Hex(TEXT("E8E4DA")) : Hex(TEXT("B01818")));
			}
		}
	}

	static void Cruiser(FMiniatureMesh& M)
	{
		const FString Tag = MiniatureParts::NationWanted();
		const FLivery L = LiveryOf(Tag);
		FCapital C;
		C.GunSize = 7.0;
		C.GunLong = 9.5;
		C.FunnelHigh = 10.0;
		C.TowerHigh = 15.0;
		C.SideTurrets = 2;
		if (Tag == TEXT("GER"))
		{
			// Admiral Hipper: 203 m; four turrets of two 20 cm guns, one funnel.
			C.H = { 203.0, 21.3, 6.0, 2.5, 0.0, 9.0, 0.45 };
			C.Guns = { { 62.0, false, 2 }, { 50.0, true, 2 }, { -50.0, true, 2 }, { -62.0, false, 2 } };
			C.Funnels = { 4.0 };
			C.TowerX = 30.0;
		}
		else if (Tag == TEXT("USA"))
		{
			// A heavy cruiser of the Baltimore kind: 205 m; three turrets of three 8-inch guns, two funnels.
			C.H = { 205.0, 21.6, 6.0, 3.5, 0.0, 9.0, 0.6 };
			C.Guns = { { 60.0, false, 3 }, { 47.0, true, 3 }, { -58.0, false, 3 } };
			C.Funnels = { 8.0, -8.0 };
			C.TowerX = 28.0;
		}
		else if (Tag == TEXT("JAP") || Tag == TEXT("JPN"))
		{
			// Takao: 204 m; five turrets of two 20 cm guns, three of them forward; a massive bridge, a raked funnel.
			C.H = { 204.0, 20.7, 5.5, 3.5, 0.5, 9.0, 0.45 };
			C.Guns = { { 72.0, false, 2 }, { 60.0, true, 2 }, { 48.0, false, 2 }, { -52.0, true, 2 }, { -64.0, false, 2 } };
			C.Funnels = { 4.0, -10.0 };
			C.FunnelLean = 0.2;
			C.TowerX = 28.0;
			C.TowerHigh = 18.0;
			C.bPagoda = true;
		}
		else
		{
			// A County-class cruiser: 192 m; four turrets of two 8-inch guns, and three tall funnels.
			C.H = { 192.0, 20.8, 7.0, 1.0, 0.0, 5.0, 0.5 };
			C.Guns = { { 58.0, false, 2 }, { 47.0, true, 2 }, { -47.0, true, 2 }, { -58.0, false, 2 } };
			C.Funnels = { 14.0, 0.0, -14.0 };
			C.FunnelHigh = 12.0;
			C.TowerX = 30.0;
			C.TowerHigh = 13.0;
		}
		Capital(M, C, L);
	}

	/** A destroyer: about 115 m, a raised forecastle, single guns fore and aft, two funnels, torpedo tubes between them. */
	static void Destroyer(FMiniatureMesh& M)
	{
		const FString Tag = MiniatureParts::NationWanted();
		const FLivery L = LiveryOf(Tag);
		const bool bFlush = Tag == TEXT("USA"); // the American destroyers had one unbroken deck
		FHull H{ Tag == TEXT("GER") ? 123.0 : 114.0, 11.6, bFlush ? 4.0 : 3.0, bFlush ? 3.0 : 1.5, 0.0, 5.0, 0.4 };
		if (!bFlush)
		{
			H.Break = 22.0;
			H.Step = 2.2;
		}
		const FYard Y{ M, H, L };
		Hull(M, H, L);
		const double Z = Y.Deck(0.0);
		// Guns: two forward, one firing over the other; two or three aft.
		Y.Turret(42.0, 0.0, Y.Deck(42.0), 3.6, Tag == TEXT("USA") ? 1 : 1, 5.0, 1.0, 0.16);
		Y.Turret(34.0, 0.0, Y.Deck(34.0) + 2.3, 3.6, 1, 5.0, 1.0, 0.16);
		M.Pillar(At(34.0, 0.0, Y.Deck(34.0)), 2.0, 2.0, 2.3, L.Upper, 10);
		Y.Turret(-38.0, 0.0, Z + 2.3, 3.6, 1, 5.0, -1.0, 0.16);
		M.Pillar(At(-38.0, 0.0, Z), 2.0, 2.0, 2.3, L.Upper, 10);
		Y.Turret(-46.0, 0.0, Z, 3.6, 1, 5.0, -1.0, 0.16);
		// The bridge, the two funnels, the mast.
		Y.House(30.0, 22.0, 3.8, Y.Deck(26.0), 4.5, L.Upper);
		Y.House(29.0, 23.0, 3.0, Y.Deck(26.0) + 4.5, 2.6, L.Upper);
		Y.House(29.1, 28.4, 2.8, Y.Deck(26.0) + 5.0, 0.9, Steel);
		Y.Rangefinder(25.0, Y.Deck(26.0) + 7.1, 2.4);
		Y.Mast(21.5, Y.Deck(21.5), 17.0);
		Y.House(20.0, -26.0, 2.6, Z, 2.2, L.Upper);
		Y.Funnel(12.0, Z + 2.2, 7.5, 5.0, 2.8, 0.1);
		Y.Funnel(-6.0, Z + 2.2, 7.0, 4.6, 2.6, 0.1);
		// Two sets of torpedo tubes on the centreline, and depth charges on the stern.
		for (const double X : { 3.0, -16.0 })
		{
			M.Pillar(At(X, 0.0, Z + 2.2), 1.6, 1.6, 0.6, L.Upper, 8);
			for (int32 K = 0; K < 4; ++K)
			{
				M.Rod(At(X - 3.6, (K - 1.5) * 0.7, Z + 3.3), At(X + 3.6, (K - 1.5) * 0.7, Z + 3.3), 0.3, L.Upper, 6);
			}
		}
		for (const double Hand : { -1.0, 1.0 })
		{
			Y.Boat(-10.0, Hand * 3.6, Z + 0.4, 6.5);
			Y.LightGun(-24.0, Hand * 2.6, Z + 2.2);
			for (int32 K = 0; K < 4; ++K)
			{
				M.Rod(At(-52.0 - K * 0.9, Hand * 1.2 - 0.5, Z + 0.5), At(-52.0 - K * 0.9, Hand * 1.2 + 0.5, Z + 0.5), 0.35, Steel, 6);
			}
		}
	}

	/** An aircraft carrier: a hull under a long flat flight deck, the island and funnel on the starboard side, aircraft ranged aft. */
	static void Carrier(FMiniatureMesh& M)
	{
		const FString Tag = MiniatureParts::NationWanted();
		const FLivery L = LiveryOf(Tag);
		const bool bJapan = Tag == TEXT("JAP") || Tag == TEXT("JPN");
		const FHull H{ Tag == TEXT("USA") ? 247.0 : (bJapan ? 257.0 : 243.0), Tag == TEXT("USA") ? 28.0 : 29.0, 9.0, 1.5, 0.0, 9.0, 0.6 };
		const FYard Y{ M, H, L };
		Hull(M, H, L);
		// The hangar as a long house on the hull, and the flight deck over it, a little shorter than the ship and wider.
		const double Top = 17.5, Long = H.Length * (bJapan ? 0.93 : 0.97), Wide = H.Beam * (Tag == TEXT("USA") ? 0.58 : 0.5);
		Y.House(H.Length * 0.4, -H.Length * 0.44, H.Beam * 0.44, Y.Deck(0.0), Top - Y.Deck(0.0) - 0.8, L.Hull);
		M.Stack(FVector::ZeroVector, 1.0, 0.0,
			{ { { Long * 0.5, -Wide * 0.72 }, { Long * 0.5, Wide * 0.72 }, { Long * 0.36, Wide }, { -Long * 0.42, Wide }, { -Long * 0.5, Wide * 0.8 }, { -Long * 0.5, -Wide * 0.8 }, { -Long * 0.42, -Wide }, { Long * 0.36, -Wide } },
				{ { Long * 0.5, -Wide * 0.72 }, { Long * 0.5, Wide * 0.72 }, { Long * 0.36, Wide }, { -Long * 0.42, Wide }, { -Long * 0.5, Wide * 0.8 }, { -Long * 0.5, -Wide * 0.8 }, { -Long * 0.42, -Wide }, { Long * 0.36, -Wide } } },
			{ Top - 0.8, Top }, L.Flight, false);
		// The markings on the deck: a line down the middle, lines across, and the lifts.
		const FLinearColor Line = Tag == TEXT("USA") ? Hex(TEXT("C9C9C0")) : Hex(TEXT("E8E4DA"));
		for (int32 K = 0; K < 22; ++K)
		{
			M.Box(At(-Long * 0.46 + K * Long * 0.043, 0.0, Top + 0.04), FVector(Long * 0.024, 0.5, 0.06), Line);
		}
		for (const double X : { Long * 0.3, 0.0, -Long * 0.3 })
		{
			M.Box(At(X, 0.0, Top + 0.03), FVector(14.0, 13.0, 0.05), L.Flight * 0.8f);
		}
		if (bJapan)
		{
			// The sun disc painted on the fore end of the deck.
			M.Marking(At(Long * 0.4, 0.0, Top + 0.08), 7.5, TEXT("JAP"), FVector::UpVector);
		}
		// The island: bridge, funnel and mast, on the starboard side.
		const double Side = Wide * 0.86;
		M.Box(At(18.0, Side, Top + 3.5), FVector(26.0, 5.0, 7.0), L.Upper);
		M.Box(At(24.0, Side, Top + 9.0), FVector(10.0, 4.4, 4.0), L.Upper);
		M.Box(At(29.05, Side, Top + 9.4), FVector(0.2, 4.0, 1.0), Steel);
		M.Limb(At(22.0, Side, Top + 11.0), At(21.0, Side, Top + 24.0), 0.5, 0.2, L.Upper, 6);
		M.Rod(At(21.3, Side - 4.0, Top + 19.0), At(21.3, Side + 4.0, Top + 19.0), 0.15, L.Upper, 4);
		{
			TArray<FVector2D> Foot, Head;
			for (int32 K = 0; K < 12; ++K)
			{
				const double C = FMath::Cos(K * UE_DOUBLE_TWO_PI / 12.0), S = FMath::Sin(K * UE_DOUBLE_TWO_PI / 12.0);
				Foot.Add(FVector2D(8.0 + C * 6.0, Side + S * 2.2));
				Head.Add(FVector2D(7.0 + C * 5.6, Side + S * 2.0));
			}
			M.Stack(FVector::ZeroVector, 1.0, 0.0, { Foot, Head }, { Top + 7.0, Top + 15.0 }, L.Upper, true);
			M.Stack(FVector::ZeroVector, 1.0, 0.0, { Head, Head }, { Top + 15.0, Top + 16.2 }, Steel, true);
		}
		// Guns along the edges of the deck.
		for (const double Hand : { -1.0, 1.0 })
		{
			for (const double X : { Long * 0.42, Long * 0.34, -Long * 0.34, -Long * 0.42 })
			{
				Y.Turret(X, Hand * (Wide + 1.5), Top - 3.5, 4.0, 2, 4.5, X > 0.0 ? 1.0 : -1.0, 0.14);
			}
		}
		// The air group ranged aft, wings touching: the navy's own fighters, to the scale of the ship.
		const FString Plane = FMiniatureMesh::MainFighterOf(Tag);
		for (int32 Row = 0; Row < 3; ++Row)
		{
			for (int32 K = 0; K < 2; ++K)
			{
				M.Vehicle(Plane, At(-Long * 0.2 - Row * 13.0, (K - 0.5) * 13.0, Top + 2.0), 1.0, 0.0);
			}
		}
	}

	/** A submarine: a long narrow hull awash, a conning tower amidships, a gun on the deck before it. */
	static void Submarine(FMiniatureMesh& M)
	{
		const FString Tag = MiniatureParts::NationWanted();
		const FLivery L = LiveryOf(Tag);
		const double Long = Tag == TEXT("USA") ? 95.0 : (Tag == TEXT("GER") ? 67.0 : 80.0);
		const FHull H{ Long, Tag == TEXT("USA") ? 8.3 : 6.2, 1.6, 0.9, 0.0, 2.0, 0.0 };
		const FYard Y{ M, H, FLivery{ L.Hull * 0.8f, L.Hull, L.Hull * 0.7f, L.Hull } };
		Hull(M, H, Y.L);
		const double Z = Y.Deck(0.0);
		// The conning tower, with the periscopes and the platform for the anti-aircraft gun behind it.
		M.Stack(FVector::ZeroVector, 1.0, 0.0,
			{ { { 6.0, 0.0 }, { 4.0, 1.5 }, { -3.0, 1.5 }, { -5.0, 0.0 }, { -3.0, -1.5 }, { 4.0, -1.5 } }, { { 5.4, 0.0 }, { 3.6, 1.3 }, { -2.8, 1.3 }, { -4.6, 0.0 }, { -2.8, -1.3 }, { 3.6, -1.3 } } },
			{ Z, Z + 4.6 }, Y.L.Upper, true);
		M.Rod(At(1.5, 0.0, Z + 4.6), At(1.5, 0.0, Z + 9.0), 0.12, Steel, 4);
		M.Rod(At(0.0, 0.0, Z + 4.6), At(0.0, 0.0, Z + 7.5), 0.1, Steel, 4);
		M.Pillar(At(-7.0, 0.0, Z), 2.0, 2.0, 2.2, Y.L.Upper, 10);
		Y.LightGun(-7.0, 0.0, Z + 2.2);
		// The deck gun forward of the tower.
		M.Pillar(At(14.0, 0.0, Z), 0.8, 0.8, 1.0, Y.L.Upper, 8);
		M.Limb(At(13.4, 0.0, Z + 1.6), At(18.0, 0.0, Z + 2.0), 0.2, 0.13, Steel, 6);
		// The net cutter at the bow and the jumping wire from bow to tower to stern.
		M.Rod(At(Long * 0.47, 0.0, Y.Deck(Long * 0.47)), At(5.0, 0.0, Z + 4.8), 0.05, Steel, 3);
		M.Rod(At(-4.0, 0.0, Z + 4.6), At(-Long * 0.47, 0.0, Y.Deck(-Long * 0.47)), 0.05, Steel, 3);
	}

	static const MiniatureParts::FModel Models[] = {
		{ TEXT("Battleship"), &Battleship, TEXT("Battleship"), TEXT("UK"), true, 300.0, 0.0 },
		{ TEXT("Carrier"), &Carrier, TEXT("Aircraft carrier"), TEXT("USA"), true, 290.0, 0.0 },
		{ TEXT("Cruiser"), &Cruiser, TEXT("Heavy cruiser"), TEXT("UK"), true, 240.0, 0.0 },
		{ TEXT("Destroyer"), &Destroyer, TEXT("Destroyer"), TEXT("UK"), true, 150.0, 0.0 },
		{ TEXT("Submarine"), &Submarine, TEXT("Submarine"), TEXT("GER"), true, 120.0, 0.0 },
	};
}

TConstArrayView<MiniatureParts::FModel> MiniatureParts::ShipModels()
{
	return ShipParts::Models;
}
