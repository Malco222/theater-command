#include "Campaign/CampaignMiniatures.h"
#include "TheaterCommand.h"

/**
 * The infantryman of each nation, built in code to the measure of a real man: 1.75 m to the top of his head,
 * standing with his rifle held across his body.
 *
 * As with the vehicles, a figure is set out in metres (so far forward, so far to his right, so far up) and
 * built once for each nation, then copied wherever a soldier is wanted. What changes from army to army is
 * what changed in life: the shape of the helmet, the colour of the cloth, boots or puttees or gaiters, the
 * leather or webbing, what is carried on the back, and the length of the rifle.
 */
namespace SoldierParts
{
	static FLinearColor Hex(const TCHAR* Code)
	{
		return FLinearColor(FColor::FromHex(Code));
	}

	static FVector At(double F, double R, double U)
	{
		return FVector(-F, -R, U);
	}

	enum class EHelmet : uint8
	{
		Pot,      // a plain deep bowl: the American M1, and the default
		Stahl,    // the German helmet: flared down over the ears and neck
		Brodie,   // the British tin hat: a shallow bowl with a wide brim
		Soviet,   // the Soviet helmet: a bowl with a slight flare
		Adrian,   // the French helmet: a brim all round and a crest along the top
		Round,    // the Italian and Polish helmets: a smooth round bowl
		Eastern,  // the Japanese helmet: a round bowl with a small brim
	};

	/** What one army's rifleman wears and carries. */
	struct FKit
	{
		FLinearColor Tunic, Trousers, Boots, Lower, Straps, HelmetPaint, Pack;
		EHelmet Helmet = EHelmet::Pot;
		/** Tall boots to the knee; otherwise the lower leg is wrapped or gaitered in Lower. */
		bool bTallBoots = false;
		/** A pack on the back; a blanket roll worn across the body; the fluted canister of a gas mask at the back of the belt. */
		bool bPack = true, bRoll = false, bCanister = false;
		/** Two big pouches on the chest (British) rather than small ones on the belt. */
		bool bChestPouches = false;
		double Rifle = 1.11;
		bool bBayonet = false;
	};

	static FKit KitFor(const FString& Tag)
	{
		FKit K;
		K.Tunic = FMiniatureMesh::UniformColour(Tag);
		K.Trousers = K.Tunic * 0.94f;
		K.Boots = Hex(TEXT("2A2420"));
		K.Lower = K.Tunic * 0.9f;
		K.Straps = Hex(TEXT("5A4630"));
		K.HelmetPaint = K.Tunic * 0.72f;
		K.Pack = Hex(TEXT("8A8261"));

		if (Tag == TEXT("GER"))
		{
			K.Trousers = Hex(TEXT("666B66"));
			K.Boots = Hex(TEXT("1E1E1E"));
			K.Straps = Hex(TEXT("222222"));
			K.HelmetPaint = Hex(TEXT("4A5048"));
			K.Helmet = EHelmet::Stahl;
			K.bTallBoots = true;
			K.bPack = false;
			K.bCanister = true;
		}
		else if (Tag == TEXT("SOV"))
		{
			K.Trousers = Hex(TEXT("6A6846"));
			K.Boots = Hex(TEXT("2A2622"));
			K.HelmetPaint = Hex(TEXT("4A5238"));
			K.Helmet = EHelmet::Soviet;
			K.bTallBoots = true;
			K.bPack = false;
			K.bRoll = true;
			K.Pack = Hex(TEXT("7A705E"));
			K.Rifle = 1.23;
			K.bBayonet = true;
		}
		else if (Tag == TEXT("USA"))
		{
			K.Tunic = Hex(TEXT("8E8660"));
			K.Trousers = Hex(TEXT("6B6240"));
			K.Boots = Hex(TEXT("4A3626"));
			K.Lower = Hex(TEXT("B0A47E"));
			K.Straps = Hex(TEXT("9C9468"));
			K.HelmetPaint = Hex(TEXT("4F5234"));
			K.Helmet = EHelmet::Pot;
			K.Pack = Hex(TEXT("9C9468"));
			K.Rifle = 1.1;
		}
		else if (Tag == TEXT("UK") || Tag == TEXT("CAN") || Tag == TEXT("AUS") || Tag == TEXT("NZL") || Tag == TEXT("SAF") || Tag == TEXT("IND"))
		{
			K.Boots = Hex(TEXT("1E1E1E"));
			K.Lower = Hex(TEXT("A39A6E"));
			K.Straps = Hex(TEXT("A39A6E"));
			K.HelmetPaint = Hex(TEXT("5E6442"));
			K.Helmet = EHelmet::Brodie;
			K.Pack = Hex(TEXT("A39A6E"));
			K.bChestPouches = true;
			K.Rifle = 1.13;
		}
		else if (Tag == TEXT("FRA") || Tag == TEXT("VIC") || Tag == TEXT("FFR") || Tag == TEXT("BEL") || Tag == TEXT("ROM") || Tag == TEXT("YUG") || Tag == TEXT("GRE"))
		{
			K.Boots = Hex(TEXT("3A2C22"));
			K.HelmetPaint = K.Tunic * 0.78f;
			K.Helmet = EHelmet::Adrian;
			K.Rifle = 1.08;
		}
		else if (Tag == TEXT("ITA"))
		{
			K.Boots = Hex(TEXT("1E1E1E"));
			K.Straps = Hex(TEXT("5E6A58"));
			K.HelmetPaint = Hex(TEXT("66705C"));
			K.Helmet = EHelmet::Round;
			K.Pack = Hex(TEXT("6E7864"));
			K.Rifle = 1.02;
		}
		else if (Tag == TEXT("JAP") || Tag == TEXT("JPN"))
		{
			K.Boots = Hex(TEXT("4A3626"));
			K.HelmetPaint = Hex(TEXT("7A6A42"));
			K.Helmet = EHelmet::Eastern;
			K.Rifle = 1.27;
			K.bBayonet = true;
		}
		else if (Tag == TEXT("POL"))
		{
			K.Boots = Hex(TEXT("1E1E1E"));
			K.HelmetPaint = Hex(TEXT("5E5E44"));
			K.Helmet = EHelmet::Round;
		}
		else if (Tag == TEXT("HUN") || Tag == TEXT("FIN") || Tag == TEXT("SVK") || Tag == TEXT("BUL") || Tag == TEXT("ESP") || Tag == TEXT("CHN") || Tag == TEXT("CHI"))
		{
			// Armies that wore the German pattern of helmet, or one very like it.
			K.Helmet = EHelmet::Stahl;
		}
		else if (Tag == TEXT("NOR") || Tag == TEXT("POR") || Tag == TEXT("TUR") || Tag == TEXT("IRL"))
		{
			K.Helmet = EHelmet::Brodie;
		}
		return K;
	}

	/**
	 * A steel helmet: a bowl over the head, then a skirt below it that drops and flares by different amounts
	 * round the head. Base is where the bowl sits (brow level); the direction round the head runs from the
	 * front (0) by the right ear (a quarter turn) to the back (half a turn).
	 */
	static void Helmet(FMiniatureMesh& M, const FVector& Base, double Front, double Back, double Half, double High,
		TFunctionRef<double(double Forward, double Sideways)> Drop, TFunctionRef<double(double Forward, double Sideways)> Flare, const FLinearColor& Colour)
	{
		const int32 Sides = 14, Bowl = 5, Skirt = 2;
		const int32 Rings = Bowl + Skirt + 1;
		const FColor Paint = Colour.ToFColor(true);
		TArray<FVector> Points;
		for (int32 J = 0; J < Rings; ++J)
		{
			for (int32 K = 0; K <= Sides; ++K)
			{
				const double Angle = UE_DOUBLE_TWO_PI * K / Sides, C = FMath::Cos(Angle), S = FMath::Sin(Angle);
				const double Reach = C >= 0.0 ? Front : Back;
				double Out, Up;
				if (J <= Bowl)
				{
					const double Polar = UE_DOUBLE_HALF_PI * J / Bowl;
					Out = FMath::Sin(Polar);
					Up = High * FMath::Cos(Polar);
				}
				else
				{
					const double T = static_cast<double>(J - Bowl) / Skirt;
					Out = 1.0 + Flare(C, S) * T;
					Up = -Drop(C, S) * T;
				}
				Points.Add(Base + At(C * Reach * Out, S * Half * Out, Up));
			}
		}
		const int32 First = M.Vertices.Num();
		const FVector Heart = Base + FVector(0.0, 0.0, High * 0.3);
		for (int32 J = 0; J < Rings; ++J)
		{
			for (int32 K = 0; K <= Sides; ++K)
			{
				// Which way the shell faces here, from its neighbours round the head and up and down it.
				const FVector Round = Points[J * (Sides + 1) + (K + 1) % Sides] - Points[J * (Sides + 1) + (K + Sides - 1) % Sides];
				const FVector Down = Points[FMath::Min(J + 1, Rings - 1) * (Sides + 1) + K] - Points[FMath::Max(J - 1, 0) * (Sides + 1) + K];
				FVector Normal = (Round ^ Down).GetSafeNormal();
				if (J == 0 || Normal.IsNearlyZero())
				{
					Normal = FVector::UpVector;
				}
				else if ((Normal | (Points[J * (Sides + 1) + K] - Heart)) < 0.0)
				{
					Normal = -Normal;
				}
				M.Add(Points[J * (Sides + 1) + K], Normal, Paint);
			}
		}
		for (int32 J = 0; J + 1 < Rings; ++J)
		{
			for (int32 K = 0; K < Sides; ++K)
			{
				const int32 A = First + J * (Sides + 1) + K, B = A + 1, C = A + Sides + 1, D = C + 1;
				const FVector Out = M.Normals[C] + M.Normals[D];
				if (J > 0)
				{
					M.Face(A, B, C, Out);
				}
				M.Face(B, D, C, Out);
				// The underside of the brim, so it is not seen through from below.
				if (J >= Bowl)
				{
					M.Face(A, B, C, -Out);
					M.Face(B, D, C, -Out);
				}
			}
		}
	}

	static TArray<FVector2D> Oval(double X, double Front, double Back, double Half, int32 Sides = 12)
	{
		TArray<FVector2D> Plan;
		for (int32 K = 0; K < Sides; ++K)
		{
			const double C = FMath::Cos(K * UE_DOUBLE_TWO_PI / Sides), S = FMath::Sin(K * UE_DOUBLE_TWO_PI / Sides);
			Plan.Add(FVector2D(X + C * (C >= 0.0 ? Front : Back), S * Half));
		}
		return Plan;
	}

	static void Build(FMiniatureMesh& M, const FKit& K)
	{
		const FLinearColor Flesh = Hex(TEXT("C49A78")), Steel = Hex(TEXT("2E2F31")), Walnut = Hex(TEXT("5E4128"));

		// ---- Legs, the left a little forward. Tall boots come to the knee; otherwise the calf is wrapped or gaitered.
		struct FLeg
		{
			FVector Hip, Knee, Ankle;
		};
		const FLeg Legs[2] = {
			{ At(0.0, -0.09, 0.9), At(0.1, -0.1, 0.5), At(0.12, -0.1, 0.09) },
			{ At(0.0, 0.09, 0.9), At(-0.02, 0.1, 0.49), At(-0.1, 0.11, 0.09) } };
		for (const FLeg& Leg : Legs)
		{
			M.Limb(Leg.Hip, Leg.Knee, 0.096, 0.07, K.Trousers, 8);
			M.Ball(Leg.Knee, FVector(0.14, 0.14, 0.14), K.Trousers, 4, 8);
			if (K.bTallBoots)
			{
				const FVector Top = FMath::Lerp(Leg.Knee, Leg.Ankle, 0.12);
				M.Limb(Leg.Knee, Top, 0.07, 0.066, K.Trousers, 8);
				M.Limb(Top, Leg.Ankle, 0.07, 0.054, K.Boots, 8);
			}
			else
			{
				const FVector Calf = FMath::Lerp(Leg.Knee, Leg.Ankle, 0.5);
				M.Limb(Leg.Knee, Calf, 0.07, 0.06, K.Trousers, 8);
				M.Limb(Calf, Leg.Ankle, 0.062, 0.05, K.Lower, 8);
			}
			// The boot: heel under the ankle, toe forward.
			M.Ball(Leg.Ankle + FVector(-0.075, 0.0, -0.04), FVector(0.285, 0.105, 0.105), K.Boots, 5, 10);
			M.Box(Leg.Ankle + FVector(0.035, 0.0, -0.07), FVector(0.07, 0.085, 0.04), K.Boots * 0.8f);
		}

		// ---- The body: the skirt of the tunic, hips, waist, chest, shoulders.
		M.Stack(FVector::ZeroVector, 1.0, 0.0,
			{ Oval(0.0, 0.115, 0.125, 0.18), Oval(0.0, 0.118, 0.128, 0.178), Oval(0.005, 0.1, 0.105, 0.155), Oval(0.01, 0.128, 0.115, 0.188), Oval(0.0, 0.11, 0.112, 0.205), Oval(0.0, 0.07, 0.08, 0.12) },
			{ 0.84, 0.98, 1.08, 1.28, 1.41, 1.47 }, K.Tunic, true);
		// The belt.
		M.Stack(FVector::ZeroVector, 1.0, 0.0, { Oval(0.005, 0.108, 0.113, 0.163), Oval(0.005, 0.108, 0.113, 0.163) }, { 1.05, 1.1 }, K.Straps, true);
		if (K.bChestPouches)
		{
			for (const double Hand : { -1.0, 1.0 })
			{
				M.Box(At(0.125, Hand * 0.085, 1.2), FVector(0.07, 0.1, 0.19), K.Straps);
			}
		}
		else
		{
			for (const double Hand : { -1.0, 1.0 })
			{
				M.Box(At(0.115, Hand * 0.08, 1.06), FVector(0.05, 0.1, 0.09), K.Straps);
			}
		}

		// ---- Neck and head.
		M.Limb(At(0.0, 0.0, 1.45), At(0.012, 0.0, 1.54), 0.055, 0.05, Flesh, 8);
		M.Ball(At(0.015, 0.0, 1.635), FVector(0.195, 0.158, 0.23), Flesh, 6, 10);
		// A face: the nose, the eyes under the brow, the mouth, the jaw; and the ears.
		const FLinearColor Shadow = Hex(TEXT("5A4030")), Lip = Hex(TEXT("9A6A58"));
		M.Limb(At(0.105, 0.0, 1.655), At(0.125, 0.0, 1.615), 0.01, 0.017, Flesh, 5);
		M.Box(At(0.1, 0.0, 1.572), FVector(0.012, 0.046, 0.007), Lip);
		M.Ball(At(0.07, 0.0, 1.555), FVector(0.09, 0.1, 0.07), Flesh, 4, 8);
		for (const double Hand : { -1.0, 1.0 })
		{
			M.Box(At(0.1, Hand * 0.033, 1.642), FVector(0.01, 0.02, 0.009), Shadow);
			M.Ball(At(0.01, Hand * 0.082, 1.63), FVector(0.03, 0.02, 0.055), Flesh, 3, 6);
			// The chin strap, from the helmet by each ear to the chin.
			M.Limb(At(0.03, Hand * 0.083, 1.66), At(0.07, Hand * 0.04, 1.535), 0.006, 0.006, K.Straps, 4);
			// Pockets on the chest and the skirt of the tunic, each with its flap.
			M.Box(At(0.124, Hand * 0.095, 1.3), FVector(0.012, 0.085, 0.095), K.Tunic * 0.94f);
			M.Box(At(0.13, Hand * 0.095, 1.335), FVector(0.012, 0.09, 0.03), K.Tunic * 0.86f);
			M.Box(At(0.112, Hand * 0.1, 0.94), FVector(0.012, 0.1, 0.11), K.Tunic * 0.94f);
			M.Box(At(0.118, Hand * 0.1, 0.985), FVector(0.012, 0.105, 0.03), K.Tunic * 0.86f);
		}
		// The collar, and the buttons down the front.
		M.Stack(FVector::ZeroVector, 1.0, 0.0, { Oval(0.005, 0.078, 0.082, 0.09), Oval(0.008, 0.066, 0.07, 0.07) }, { 1.44, 1.5 }, K.Tunic * 0.8f, true);
		const double ButtonOut[5] = { 0.116, 0.129, 0.139, 0.134, 0.121 };
		for (int32 Button = 0; Button < 5; ++Button)
		{
			M.Disc(At(ButtonOut[Button], 0.0, 1.12 + Button * 0.065), At(1.0, 0.0, 0.0), 0.009, Hex(TEXT("8A8A80")), 6);
		}

		// ---- Arms, holding the rifle across the body: the right hand at the small of the stock by the hip,
		// the left under the fore-end in front of the left shoulder.
		const FVector RightHand = At(0.15, 0.1, 1.05), LeftHand = At(0.2, -0.13, 1.32);
		const FVector RightShoulder = At(0.0, 0.2, 1.385), LeftShoulder = At(0.0, -0.2, 1.385);
		const FVector RightElbow = At(-0.04, 0.27, 1.12), LeftElbow = At(0.1, -0.275, 1.14);
		M.Ball(RightShoulder, FVector(0.118, 0.118, 0.118), K.Tunic, 4, 8);
		M.Ball(LeftShoulder, FVector(0.118, 0.118, 0.118), K.Tunic, 4, 8);
		M.Ball(RightElbow, FVector(0.098, 0.098, 0.098), K.Tunic, 4, 8);
		M.Ball(LeftElbow, FVector(0.098, 0.098, 0.098), K.Tunic, 4, 8);
		M.Limb(RightShoulder, RightElbow, 0.058, 0.048, K.Tunic, 8);
		M.Limb(RightElbow, RightHand, 0.048, 0.036, K.Tunic, 8);
		M.Limb(LeftShoulder, LeftElbow, 0.058, 0.048, K.Tunic, 8);
		M.Limb(LeftElbow, LeftHand, 0.048, 0.036, K.Tunic, 8);
		M.Ball(RightHand, FVector(0.085, 0.085, 0.085), Flesh, 4, 8);
		M.Ball(LeftHand, FVector(0.085, 0.085, 0.085), Flesh, 4, 8);

		// ---- The rifle, through both hands: butt low on the right, muzzle up past the left shoulder.
		const FVector Along = (LeftHand - RightHand).GetSafeNormal();
		const FVector Butt = RightHand - Along * 0.3, Muzzle = RightHand + Along * (K.Rifle - 0.3);
		M.Limb(Butt, RightHand, 0.045, 0.028, Walnut, 6);
		M.Limb(RightHand, RightHand + Along * (K.Rifle * 0.5), 0.028, 0.02, Walnut, 6);
		M.Rod(RightHand + Along * 0.05, Muzzle, 0.011, Steel, 5);
		if (K.bBayonet)
		{
			M.Rod(Muzzle, Muzzle + Along * 0.4, 0.006, Hex(TEXT("9A9A98")), 4);
		}

		// ---- What he carries.
		if (K.bPack)
		{
			M.Box(At(-0.175, 0.0, 1.25), FVector(0.12, 0.3, 0.32), K.Pack);
			M.Rod(At(-0.175, -0.16, 1.43), At(-0.175, 0.16, 1.43), 0.045, K.Pack * 0.9f, 8);
		}
		if (K.bCanister)
		{
			// The gas-mask canister slung across the small of the back, the bread bag and the water bottle on the right hip.
			M.Rod(At(-0.15, -0.15, 1.0), At(-0.17, 0.12, 1.03), 0.06, K.HelmetPaint, 8);
			M.Box(At(-0.09, 0.16, 0.97), FVector(0.14, 0.07, 0.17), Hex(TEXT("6E6A52")));
			M.Ball(At(-0.13, 0.2, 0.96), FVector(0.1, 0.07, 0.15), Hex(TEXT("4A3A2A")), 4, 8);
			// The entrenching tool on the left hip.
			M.Box(At(-0.05, -0.19, 0.9), FVector(0.03, 0.02, 0.3), K.Straps);
		}
		if (K.bRoll)
		{
			// The rolled greatcoat, worn over the left shoulder and across to the right hip.
			const FVector Middle = At(0.0, 0.0, 1.21);
			const FVector Slant = (At(0.0, -0.2, 1.46) - At(0.0, 0.2, 0.96)).GetSafeNormal(), Fore = At(1.0, 0.0, 0.0);
			FVector Previous = FVector::ZeroVector;
			const int32 Turns = 10;
			for (int32 Step = 0; Step <= Turns; ++Step)
			{
				const double Angle = UE_DOUBLE_TWO_PI * Step / Turns;
				const FVector Here = Middle + Slant * (FMath::Cos(Angle) * 0.33) + Fore * (FMath::Sin(Angle) * 0.165);
				if (Step > 0)
				{
					M.Limb(Previous, Here, 0.048, 0.048, K.Pack, 6);
				}
				Previous = Here;
			}
		}

		// ---- The helmet.
		const FVector Brow = At(0.015, 0.0, 1.675);
		switch (K.Helmet)
		{
		case EHelmet::Stahl:
			Helmet(M, Brow, 0.128, 0.14, 0.118, 0.115,
				[](double C, double) { return C > 0.35 ? 0.022 : FMath::Lerp(0.085, 0.022, FMath::Clamp((C + 0.1) / 0.45, 0.0, 1.0)); },
				[](double C, double) { return C > 0.35 ? 0.16 : 0.15; }, K.HelmetPaint);
			break;
		case EHelmet::Brodie:
			Helmet(M, Brow + FVector(0.0, 0.0, 0.035), 0.12, 0.12, 0.112, 0.075,
				[](double, double) { return 0.016; }, [](double, double) { return 0.42; }, K.HelmetPaint);
			break;
		case EHelmet::Soviet:
			Helmet(M, Brow, 0.126, 0.136, 0.118, 0.118,
				[](double C, double) { return C > 0.3 ? 0.03 : 0.055; }, [](double C, double) { return C > 0.3 ? 0.13 : 0.09; }, K.HelmetPaint);
			break;
		case EHelmet::Adrian:
			Helmet(M, Brow + FVector(0.0, 0.0, 0.01), 0.122, 0.128, 0.114, 0.1,
				[](double, double) { return 0.025; }, [](double C, double) { return FMath::Abs(C) > 0.4 ? 0.26 : 0.14; }, K.HelmetPaint);
			M.Limb(Brow + At(0.09, 0.0, 0.075), Brow + At(-0.09, 0.0, 0.085), 0.012, 0.014, K.HelmetPaint, 5);
			M.Limb(Brow + At(-0.03, 0.0, 0.105), Brow + At(0.05, 0.0, 0.1), 0.014, 0.012, K.HelmetPaint, 5);
			break;
		case EHelmet::Round:
			Helmet(M, Brow, 0.125, 0.132, 0.118, 0.118,
				[](double, double) { return 0.04; }, [](double, double) { return 0.035; }, K.HelmetPaint);
			break;
		case EHelmet::Eastern:
			Helmet(M, Brow, 0.124, 0.13, 0.117, 0.112,
				[](double, double) { return 0.032; }, [](double, double) { return 0.085; }, K.HelmetPaint);
			break;
		default:
			Helmet(M, Brow, 0.127, 0.137, 0.12, 0.12,
				[](double C, double) { return C > 0.3 ? 0.03 : 0.05; }, [](double C, double) { return C > 0.3 ? 0.12 : 0.04; }, K.HelmetPaint);
			break;
		}
	}
}

void FMiniatureMesh::Soldier(const FString& Tag, const FVector& Where, double Metre, double Turn)
{
	static TMap<FString, FMiniatureMesh> Built;
	FMiniatureMesh* Model = Built.Find(Tag);
	if (!Model)
	{
		FMiniatureMesh Fresh;
		// A little dust on his boots.
		Fresh.DustFoot = 0.0;
		Fresh.DustTop = 0.5;
		Fresh.DustAmount = 0.4f;
		SoldierParts::Build(Fresh, SoldierParts::KitFor(Tag));
		UE_LOG(LogTheater, Log, TEXT("Soldier %s built: %d points, %d triangles"), *Tag, Fresh.Vertices.Num(), Fresh.Triangles.Num() / 3);
		Model = &Built.Add(Tag, MoveTemp(Fresh));
	}
	Stamp(*Model, Where, Metre, Turn);
}
