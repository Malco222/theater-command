#include "Campaign/CampaignMapActor.h"

#include "Campaign/ArmyPiece.h"
#include "Campaign/CampaignMiniatures.h"
#include "Campaign/TheaterSpeed.h"
#include "TheaterCommand.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"

/*
 * (Release 2: camps you can see. Brian said yes on 22 November 2026; the art director's paintover
 * "r2-camps-you-can-see".) An army that stays put digs in where everybody can see it, in the stages the rules'
 * fieldwork months already count:
 *
 *   0  in the open: it came this month, and stands on bare fields;
 *   1  a month in camp: tents and a vehicle park beside the base, the ground round them trodden into tracks;
 *   2  digging its works: part of a zigzag trench on the side toward the enemy, and no wire yet;
 *   3  dug in, its works finished: the whole trench, a belt of wire before it, gun pits behind it, and the vehicles
 *      under nets.
 *
 * The stage, and the spade tag on the standard that says it in figures, are the piece's own (ArmyPiece.cpp). The camp
 * on the ground is laid here, because only the map knows the ground: where the enemy lies, what is water, town, wood,
 * river or railway, and where the other armies stand with their guns.
 *
 * Everything is set out in base radii from the middle of the base, toward the enemy ("ahead") and across. The ring out
 * to about one and a half radii is left to the dressing that turns with the piece (its lorry and crates, its fire),
 * which may stand on any side of it as the eye goes round; the camp lies beyond that ring, and keeps out of the ground
 * straight behind the base, where an army's guns stand. Each camp is one still model of a few hundred triangles (some
 * eighteen hundred when its works are finished), made again only when its stage, its place, the way it faces or the
 * ground it finds open has changed: never from one frame to the next.
 */

namespace
{
	FLinearColor CampHex(const TCHAR* Hex)
	{
		return FLinearColor(FColor::FromHex(Hex)); // sRGB in, linear out
	}

	/** A way on the map as a way on the table (the map's north is the table's +X and its east the table's +Y: see MapToWorld). */
	FVector CampWorldWay(const FVector2D& MapWay)
	{
		return FVector(-MapWay.Y, MapWay.X, 0.0);
	}

	/** How far a model is turned on the table to lie along a way on the map, in degrees. */
	double CampYaw(const FVector2D& MapWay)
	{
		return FMath::RadiansToDegrees(FMath::Atan2(MapWay.X, -MapWay.Y));
	}

	/**
	 * A point of the trodden ground, as strong as Much. (Release 2, the art director's read of run 2, 22 November: the
	 * art director's earth of the gun lines, about 166, 138, 96, was the colour of the dry fields by Fort Worth, and no
	 * trodden ground showed round the tents at all. A camp's is a darker earth, trodden bare, that shows on dry ground and
	 * on green alike.)
	 */
	int32 CampEarthPoint(FMiniatureMesh& Ground, const FVector& Where, float Much)
	{
		FLinearColor Ink = FLinearColor(FColor(140, 114, 78));
		Ink.A = FMath::Clamp(Much, 0.0f, 1.0f);
		Ground.Vertices.Add(Where);
		Ground.Normals.Add(FVector::UpVector);
		Ground.Colors.Add(Ink.ToFColor(false));
		return Ground.Vertices.Num() - 1;
	}

	/**
	 * How near two stretches on the map come to each other: nothing where they cross. (Release 2, the art director's
	 * read of run 2, 22 November: the trench ran across Fort Worth's railway, though each bend of it stood on open
	 * ground. A stretch between two open points can still cross a railway or a river between them.)
	 */
	double CampGap(const FVector2D& OneFrom, const FVector2D& OneTo, const FVector2D& TwoFrom, const FVector2D& TwoTo)
	{
		auto Turning = [](const FVector2D& LineFrom, const FVector2D& LineTo, const FVector2D& Probe)
		{
			return (LineTo.X - LineFrom.X) * (Probe.Y - LineFrom.Y) - (LineTo.Y - LineFrom.Y) * (Probe.X - LineFrom.X);
		};
		if (Turning(OneFrom, OneTo, TwoFrom) * Turning(OneFrom, OneTo, TwoTo) < 0.0 && Turning(TwoFrom, TwoTo, OneFrom) * Turning(TwoFrom, TwoTo, OneTo) < 0.0)
		{
			return 0.0; // (they cross)
		}
		auto PointOff = [](const FVector2D& Probe, const FVector2D& LineFrom, const FVector2D& LineTo)
		{
			return FMath::PointDistToSegment(FVector(Probe, 0.0), FVector(LineFrom, 0.0), FVector(LineTo, 0.0));
		};
		return FMath::Min(FMath::Min(PointOff(OneFrom, TwoFrom, TwoTo), PointOff(OneTo, TwoFrom, TwoTo)), FMath::Min(PointOff(TwoFrom, OneFrom, OneTo), PointOff(TwoTo, OneFrom, OneTo)));
	}
}

FVector2D ACampaignMapActor::CampFacing(int32 HomeRegion, const FString& Nation, const FVector2D& FromSpot) const
{
	// Whom the nation is at war with, as the rules last said.
	TArray<FString, TInlineAllocator<8>> Foes;
	for (int32 K = 0; K + 1 < CampWars.Num(); K += 2)
	{
		if (CampWars[K] == Nation)
		{
			Foes.AddUnique(CampWars[K + 1]);
		}
		else if (CampWars[K + 1] == Nation)
		{
			Foes.AddUnique(CampWars[K]);
		}
	}
	auto HeldByFoe = [this, &Foes](int32 Province)
	{
		return CampHolders.IsValidIndex(Province) && Foes.Contains(CampHolders[Province]);
	};
	FVector2D Sum = FVector2D::ZeroVector;
	if (Foes.Num() > 0 && Data.Regions.IsValidIndex(HomeRegion))
	{
		// The enemy's ground next door, each province of it pulling its own way; failing that, his nearest ground.
		for (const int32 Next : Data.Regions[HomeRegion].Neighbors)
		{
			if (Data.Regions.IsValidIndex(Next) && HeldByFoe(Next))
			{
				Sum += (Data.Regions[Next].Center - FromSpot).GetSafeNormal();
			}
		}
		if (Sum.IsNearlyZero(0.05))
		{
			double Nearest = FMath::Square(900.0);
			for (const FCampaignRegion& Region : Data.Regions)
			{
				const double Far = FVector2D::DistSquared(Region.Center, FromSpot);
				if (Far < Nearest && Region.Id != HomeRegion && HeldByFoe(Region.Id))
				{
					Nearest = Far;
					Sum = Region.Center - FromSpot;
				}
			}
		}
	}
	if (Sum.IsNearlyZero(0.05))
	{
		// At peace, or with the enemy far off: it faces out, away from its own capital, as its guns do.
		for (const FCampaignRegion& Region : Data.Regions)
		{
			if (Region.bCapital && Region.Owner == Nation && Region.Id != HomeRegion)
			{
				Sum = FromSpot - Region.Center;
				break;
			}
		}
	}
	// (Where even that says nothing, as for the army in the capital itself: north, the far side of the table from
	// the eye as it first looks at it.)
	return Sum.IsNearlyZero(0.05) ? FVector2D(0.0, -1.0) : Sum.GetSafeNormal();
}

void ACampaignMapActor::BuildCamps()
{
	THEATER_SPEED_SCOPE(BuildCamps);
	if (!Data.IsLoaded() || HasParade())
	{
		return;
	}
	// (the box each river runs in, found once: a camp looks only at the rivers whose box comes near it)
	if (CampRiverBoxes.Num() != Land.Rivers.Num())
	{
		CampRiverBoxes.Reset(Land.Rivers.Num());
		for (const FLandRiver& River : Land.Rivers)
		{
			FBox2D Box(ForceInit);
			for (const FVector2D& Bend : River.Course)
			{
				Box += Bend;
			}
			CampRiverBoxes.Add(Box);
		}
	}
	UMaterialInterface* Trampled = GunEarthInk ? static_cast<UMaterialInterface*>(GunEarthInk) : static_cast<UMaterialInterface*>(HighlightMaterial);
	int32 Pitched = 0, Struck = 0, Cramped = 0;
	for (AArmyPiece* Piece : Pieces)
	{
		if (!Piece)
		{
			continue;
		}
		const int32 Stage = Piece->GetCampStage();
		const int32 Home = Piece->GetRegionId();
		if (Stage <= 0 || !Piece->IsArmy() || !Data.Regions.IsValidIndex(Home))
		{
			if (!Piece->GetCampMade().IsEmpty())
			{
				Piece->LayCamp(FString(), nullptr, nullptr, nullptr, nullptr); // (it has marched, or been beaten out of its works)
				++Struck;
			}
			continue;
		}
		const double Grown = Piece->GetActorScale3D().X; // (four fifths in a crowded province, as its base is)
		const double Unit = 1.4 * Grown; // one of the piece's own units, on the map: its figures and its dressing are built to it
		const double Radius = (Piece->GetBaseRadius() / UnitScale + 0.72 * 1.4) * Grown; // its base and the wooden foot round it, on the map
		const double Wu = Unit * UnitScale; // and that unit on the table
		const FVector2D Middle = WorldToMap(Piece->GetRestLocation());
		// Which way the enemy lies, to the nearest five degrees: a camp is not laid again for a hair's change.
		const FVector2D Toward = CampFacing(Home, Piece->GetNationTag(), Middle);
		const int32 Bearing = FMath::RoundToInt32(FMath::RadiansToDegrees(FMath::Atan2(Toward.Y, Toward.X)) / 5.0) * 5;
		const double Turned = FMath::DegreesToRadians(static_cast<double>(Bearing));
		const FVector2D Ahead(FMath::Cos(Turned), FMath::Sin(Turned));
		const FVector2D Abeam(-Ahead.Y, Ahead.X);
		auto At = [&Middle, &Ahead, &Abeam, Radius](double Forward, double Aside)
		{
			return Middle + Ahead * (Forward * Radius) + Abeam * (Aside * Radius);
		};

		// What the camp keeps clear of, near enough to matter: other armies' bases and their guns, the towns round about,
		// their airfields and the quarter where a town's own buildings go up, each as a ring; and the rivers and
		// railways, as lines.
		const double Span = 4.0 * Radius; // (as far as any part of a camp reaches, and a little more)
		TArray<TPair<FVector2D, double>> Rings;
		auto KeepClear = [&Rings, &Middle, Span](const FVector2D& Centre, double Round)
		{
			if (FVector2D::DistSquared(Centre, Middle) < FMath::Square(Span + Round))
			{
				Rings.Emplace(Centre, Round);
			}
		};
		for (const AArmyPiece* Other : Pieces)
		{
			if (Other && Other != Piece)
			{
				KeepClear(WorldToMap(Other->GetRestLocation()), (Other->GetBaseRadius() / UnitScale + 0.72 * 1.4) * Other->GetActorScale3D().X);
			}
		}
		for (const FGunLine& Guns : GunLines)
		{
			if (Guns.Piece.IsValid())
			{
				KeepClear(WorldToMap(Guns.Middle), Guns.Radius / UnitScale);
			}
		}
		{
			TArray<int32, TInlineAllocator<12>> Around;
			Around.Append(Data.Regions[Home].Neighbors);
			Around.Add(Home);
			for (const int32 Near : Around)
			{
				if (Data.Regions.IsValidIndex(Near))
				{
					KeepClear(Data.Regions[Near].Center, TownRadius(Near) + 1.5);
					if (const FVector2D* Field = AirfieldSpots.Find(Near))
					{
						KeepClear(*Field, 6.0);
					}
				}
			}
			if (!CivicInside(Home))
			{
				// (the quarter south and east of the town where its owner puts up its buildings, unless that is turned:
				// see CivicTurn, whose reckoning this is)
				const double Quarter = FMath::DegreesToRadians(CivicTurn(Home));
				const FVector2D Offset(9.0, CivicNear(Home) + 3.6);
				KeepClear(Data.Regions[Home].Center + FVector2D(Offset.X * FMath::Cos(Quarter) - Offset.Y * FMath::Sin(Quarter), Offset.X * FMath::Sin(Quarter) + Offset.Y * FMath::Cos(Quarter)), 8.0);
			}
		}
		TArray<TPair<FVector2D, FVector2D>> Ways;
		for (const FLandRail& Rail : Land.Rails)
		{
			if (Data.Regions.IsValidIndex(Rail.A) && Data.Regions.IsValidIndex(Rail.B) && FVector2D::DistSquared(Rail.At, Middle) < FMath::Square(160.0))
			{
				const FVector2D Stops[3] = { Data.Regions[Rail.A].Center, Rail.At, Data.Regions[Rail.B].Center };
				for (int32 Leg = 0; Leg < 2; ++Leg)
				{
					if (FMath::PointDistToSegment(FVector(Middle, 0.0), FVector(Stops[Leg], 0.0), FVector(Stops[Leg + 1], 0.0)) < Span)
					{
						Ways.Emplace(Stops[Leg], Stops[Leg + 1]);
					}
				}
			}
		}
		for (int32 K = 0; K < Land.Rivers.Num() && K < CampRiverBoxes.Num(); ++K)
		{
			if (!CampRiverBoxes[K].bIsValid || !CampRiverBoxes[K].ExpandBy(Span).IsInside(Middle))
			{
				continue;
			}
			const TArray<FVector2D>& Course = Land.Rivers[K].Course;
			for (int32 Bend = 0; Bend + 1 < Course.Num(); ++Bend)
			{
				if (FMath::PointDistToSegment(FVector(Middle, 0.0), FVector(Course[Bend], 0.0), FVector(Course[Bend + 1], 0.0)) < Span)
				{
					Ways.Emplace(Course[Bend], Course[Bend + 1]);
				}
			}
		}
		// Open ground: land, on the army's own side of any frontier, clear of all that, and not in a wood (where the
		// trees stand, see BuildTrees: an army pitches its camp in the fields).
		auto OwnSide = [this, Home](int32 Under)
		{
			if (Under == Home)
			{
				return true;
			}
			if (CampHolders.IsValidIndex(Under) && CampHolders.IsValidIndex(Home))
			{
				return CampHolders[Under] == CampHolders[Home];
			}
			return Data.Regions[Under].Owner == Data.Regions[Home].Owner;
		};
		auto Wooded = [this](const FVector2D& Where)
		{
			if (Terrain.NX <= 0 || Terrain.NY <= 0 || Where.X < 0.0 || Where.Y < 0.0 || Where.X >= Data.Width || Where.Y >= Data.Height)
			{
				return false; // (no trees are planted beyond the map's own ground)
			}
			const int32 GI = FMath::Clamp(FMath::RoundToInt32(Where.X / Terrain.Cell), 0, Terrain.NX - 1);
			const int32 GJ = FMath::Clamp(FMath::RoundToInt32(Where.Y / Terrain.Cell), 0, Terrain.NY - 1);
			const int32 Painted = Terrain.Index(GI, GJ);
			return Paint.Ground.IsValidIndex(Painted) && Paint.Ground[Painted].A >= 102; // (four tenths wooded: trees stand there)
		};
		auto Open = [&](const FVector2D& Where, double Room)
		{
			const int32 Under = Data.FindRegionAt(Where.X, Where.Y);
			if (!Data.Regions.IsValidIndex(Under) || GroundAt(Where.X, Where.Y) <= 0.0f || !OwnSide(Under))
			{
				return false;
			}
			for (const TPair<FVector2D, double>& Ring : Rings)
			{
				if (FVector2D::DistSquared(Ring.Key, Where) < FMath::Square(Ring.Value + Room))
				{
					return false;
				}
			}
			for (const TPair<FVector2D, FVector2D>& Way : Ways)
			{
				if (FMath::PointDistToSegment(FVector(Where, 0.0), FVector(Way.Key, 0.0), FVector(Way.Value, 0.0)) < Room + 0.6)
				{
					return false;
				}
			}
			return !Wooded(Where);
		};
		// (Release 2, the art director's read of run 2, 22 November: the trench ran across Fort Worth's railway, though
		// every bend of it stood on open ground. A stretch of trench or wire between two open points keeps as clear of
		// railways, rivers, towns and the rest along its whole length.)
		auto ClearRun = [&Rings, &Ways](const FVector2D& RunFrom, const FVector2D& RunTo, double Room)
		{
			for (const TPair<FVector2D, double>& Ring : Rings)
			{
				if (FMath::PointDistToSegment(FVector(Ring.Key, 0.0), FVector(RunFrom, 0.0), FVector(RunTo, 0.0)) < Ring.Value + Room)
				{
					return false;
				}
			}
			for (const TPair<FVector2D, FVector2D>& Way : Ways)
			{
				if (CampGap(RunFrom, RunTo, Way.Key, Way.Value) < Room + 0.6)
				{
					return false;
				}
			}
			return true;
		};

		// The camp's plan, written down as it is made: what it was laid from, so that it is laid again only when that changes.
		FString Made = FString::Printf(TEXT("%d.%d|%d|%.0f,%.0f|%.2f|"), Stage, Piece->GetCampFigure(), Bearing, Middle.X * 4.0, Middle.Y * 4.0, Grown);
		bool bShort = false;

		// The parts that need ground of their own: two lines of tents and the vehicle park, out on the flanks. Each is
		// tried where it is wanted first, then on the other flank, then a little further out, and is left out if no
		// place is open. (Angles from the way to the enemy, in degrees; distances in base radii. Straight behind,
		// from about 145 degrees round, is the guns' ground.)
		struct FCampLot
		{
			bool bOpen = false;
			FVector2D Centre = FVector2D::ZeroVector;
			FVector2D Outward = FVector2D(1.0, 0.0); // from the base out through the lot
			double HalfLong = 0.0, HalfDeep = 0.0; // across the way out, and along it (base radii)
		};
		TArray<FCampLot, TInlineAllocator<3>> Lots;
		auto PitchLot = [&](const TArray<FVector2D>& Tries, double LotLong, double LotDeep)
		{
			FCampLot Lot;
			Lot.HalfLong = LotLong;
			Lot.HalfDeep = LotDeep;
			for (int32 Try = 0; Try < Tries.Num() && !Lot.bOpen; ++Try)
			{
				const double Angle = FMath::DegreesToRadians(Tries[Try].X);
				const FVector2D Out = Ahead * FMath::Cos(Angle) + Abeam * FMath::Sin(Angle);
				const FVector2D Sideways(-Out.Y, Out.X);
				const FVector2D Centre = Middle + Out * (Tries[Try].Y * Radius);
				bool bRoom = Open(Centre, LotDeep * Radius) && Open(Centre + Sideways * (0.8 * LotLong * Radius), LotDeep * Radius) && Open(Centre - Sideways * (0.8 * LotLong * Radius), LotDeep * Radius);
				for (const FCampLot& Other : Lots)
				{
					if (bRoom && Other.bOpen && FVector2D::Distance(Other.Centre, Centre) < (Other.HalfLong + LotLong) * Radius * 0.85)
					{
						bRoom = false;
					}
				}
				if (bRoom)
				{
					Lot.bOpen = true;
					Lot.Centre = Centre;
					Lot.Outward = Out;
					Made.AppendChar(static_cast<TCHAR>(TEXT('a') + Try));
				}
			}
			if (!Lot.bOpen)
			{
				Made.AppendChar(TEXT('-'));
				bShort = true;
			}
			Lots.Add(Lot);
		};
		PitchLot({ FVector2D(-100.0, 2.05), FVector2D(100.0, 2.05), FVector2D(-100.0, 2.55), FVector2D(100.0, 2.55), FVector2D(-72.0, 2.3), FVector2D(72.0, 2.3) }, 0.55, 0.36); // the first lines of tents
		PitchLot({ FVector2D(-130.0, 2.1), FVector2D(130.0, 2.1), FVector2D(-130.0, 2.6), FVector2D(130.0, 2.6), FVector2D(-100.0, 3.0), FVector2D(100.0, 3.0) }, 0.55, 0.36); // the second
		// (Release 2, the art director's read of run 2, 22 November: no vehicle park showed by Fort Worth, for want of open
		// ground where it was looked for. It is looked for further round toward the back, and toward the front, as well.)
		PitchLot({ FVector2D(108.0, 2.1), FVector2D(-108.0, 2.1), FVector2D(108.0, 2.6), FVector2D(-108.0, 2.6), FVector2D(78.0, 2.35), FVector2D(-78.0, 2.35), FVector2D(128.0, 2.9), FVector2D(-128.0, 2.9), FVector2D(62.0, 2.7), FVector2D(-62.0, 2.7) }, 0.55, 0.42); // the vehicle park
		// (and the works keep off the tents and the park, and the net that is spread over it)
		for (const FCampLot& PitchedLot : Lots)
		{
			if (PitchedLot.bOpen)
			{
				Rings.Emplace(PitchedLot.Centre, (PitchedLot.HalfLong + 0.2) * Radius);
			}
		}

		// Finished works: two or three gun pits behind the trench, each where its own ground is open.
		bool bPit[3] = { false, false, false };
		FVector2D PitAt[3] = { FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector };
		if (Stage >= 3)
		{
			for (int32 K = 0; K < 3; ++K)
			{
				const double Aside = (K - 1) * 0.95;
				PitAt[K] = At(1.85 - 0.1 * Aside * Aside, Aside);
				bPit[K] = Open(PitAt[K], 0.25 * Radius);
				Made.AppendChar(bPit[K] ? TEXT('p') : TEXT('-'));
				bShort |= !bPit[K];
			}
		}

		// The trench: fire bays and traverses in a zigzag across the way to the enemy, bowed a little round the base.
		// While it is being dug only the middle of it is there, and more of it for each month's digging.
		// (Release 2, the art director's read of run 2, 22 November: it crossed Fort Worth's railway. Each bend now keeps
		// the whole width of the cut and its bank clear of railways, rivers, towns and the rest; where it cannot, it is
		// tried a little nearer the base and then a little further out, so that the line bends round what is in its way.
		// A stretch between two bends is dug only if it keeps as clear along its whole length, so that where nothing
		// serves the line stops short; and a stretch left on its own, cut off from the rest of the line, is not dug.)
		const double TrenchRoom = 1.1 * Unit; // (the cut and the bank before it, from the line of its bends, and a little more)
		double TrenchHalf = 0.0;
		int32 TrenchBends = 0;
		TArray<FVector2D, TInlineAllocator<16>> Bays;
		TArray<bool, TInlineAllocator<16>> BayOpen;
		TArray<double, TInlineAllocator<16>> BayBend; // how far each bend was moved out from its place (radii; toward the base is less than nothing)
		TArray<bool, TInlineAllocator<16>> SegDug; // whether each stretch from one bend to the next is dug
		if (Stage >= 2)
		{
			const double Dug = Stage >= 3 ? 1.0 : FMath::Clamp(static_cast<double>(FMath::Max(Piece->GetCampFigure(), 2)) / FMath::Max(AArmyPiece::FullWorks, 1), 0.4, 0.8);
			TrenchHalf = 1.75 * Dug;
			TrenchBends = FMath::Max(2, FMath::RoundToInt32(2.0 * TrenchHalf / 0.36));
			const double BendTries[3] = { 0.0, -0.2, 0.2 };
			for (int32 K = 0; K <= TrenchBends; ++K)
			{
				const double Aside = -TrenchHalf + 2.0 * TrenchHalf * K / TrenchBends;
				const double BayForward = 2.4 - 0.18 * FMath::Square(Aside / 1.75) + (K % 2 == 0 ? 0.13 : -0.13);
				Bays.Add(At(BayForward, Aside));
				BayOpen.Add(false);
				BayBend.Add(0.0);
				TCHAR BayMark = TEXT('-');
				for (int32 Try = 0; Try < 3 && !BayOpen.Last(); ++Try)
				{
					const FVector2D Tried = At(BayForward + BendTries[Try], Aside);
					bool bClearOfPits = true; // (a bend moved back toward the base keeps off the gun pits behind the line)
					for (int32 PitK = 0; PitK < 3; ++PitK)
					{
						if (bPit[PitK] && FVector2D::Distance(PitAt[PitK], Tried) < 0.3 * Radius + 0.3 * Unit)
						{
							bClearOfPits = false;
						}
					}
					if (bClearOfPits && Open(Tried, TrenchRoom))
					{
						Bays.Last() = Tried;
						BayOpen.Last() = true;
						BayBend.Last() = BendTries[Try];
						BayMark = static_cast<TCHAR>(TEXT('t') + Try);
					}
				}
				Made.AppendChar(BayMark);
				bShort |= !BayOpen.Last();
			}
			bool bLongRun = false;
			for (int32 K = 0; K < TrenchBends; ++K)
			{
				SegDug.Add(BayOpen[K] && BayOpen[K + 1] && ClearRun(Bays[K], Bays[K + 1], TrenchRoom));
				bLongRun |= K > 0 && SegDug[K] && SegDug[K - 1];
			}
			for (int32 K = 0; K < TrenchBends; ++K)
			{
				if (bLongRun && SegDug[K] && (K == 0 || !SegDug[K - 1]) && (K + 1 == TrenchBends || !SegDug[K + 1]))
				{
					SegDug[K] = false; // (a scrap of one stretch, cut off from the rest of the line)
				}
				Made.AppendChar(SegDug[K] ? TEXT('s') : TEXT('-'));
				bShort |= !SegDug[K];
			}
		}

		// Finished works: a belt of wire before the whole trench, a short way out beyond its bank and as long as what is
		// dug, and a little longer at each end: two rows of pickets, each row half a step along from the other, with a
		// strand along each row and one in a zigzag between them. (Release 2, the art director's read of run 2, 22
		// November: "wire as a belt in front of the whole line, not a short piece beside it". It stood on an arc of its
		// own further out, and found open ground along only a short piece of it.) A picket stands only before a stretch
		// that is dug, where its own ground is open, and a strand is strung only where it keeps clear all along.
		TArray<FVector2D, TInlineAllocator<32>> Stakes;
		TArray<bool, TInlineAllocator<32>> StakeOpen;
		TArray<bool, TInlineAllocator<32>> ZigStrung, RowStrung; // from each picket to the next (in the other row) and to the next but one (in its own)
		if (Stage >= 3 && TrenchBends > 0)
		{
			const double WireRoom = 0.35 * Unit;
			const double BeltBack = 0.13 + 1.35 * Unit / Radius, BeltDeep = 0.55 * Unit / Radius; // (radii: out past the bank's foot, and from one row to the other)
			const double BeltReach = TrenchHalf + 0.12;
			const double SegWide = 2.0 * TrenchHalf / TrenchBends;
			const int32 Pickets = FMath::Max(2, FMath::RoundToInt32(2.0 * BeltReach / 0.16));
			for (int32 K = 0; K <= Pickets; ++K)
			{
				const double Aside = -BeltReach + 2.0 * BeltReach * K / Pickets;
				const int32 BeltSeg = FMath::Clamp(FMath::FloorToInt32((Aside + TrenchHalf) / SegWide), 0, TrenchBends - 1);
				const double BeltBent = FMath::Max(0.0, FMath::Max(BayBend[BeltSeg], BayBend[BeltSeg + 1])); // (further out where the trench there was bent out)
				Stakes.Add(At(2.4 - 0.18 * FMath::Square(Aside / 1.75) + BeltBack + BeltBent + (K % 2) * BeltDeep, Aside));
				StakeOpen.Add(SegDug[BeltSeg] && Open(Stakes.Last(), WireRoom));
				Made.AppendChar(StakeOpen.Last() ? TEXT('w') : TEXT('-'));
				bShort |= !StakeOpen.Last();
			}
			for (int32 K = 0; K < Stakes.Num(); ++K)
			{
				ZigStrung.Add(K + 1 < Stakes.Num() && StakeOpen[K] && StakeOpen[K + 1] && ClearRun(Stakes[K], Stakes[K + 1], WireRoom));
				RowStrung.Add(K + 2 < Stakes.Num() && StakeOpen[K] && StakeOpen[K + 2] && ClearRun(Stakes[K], Stakes[K + 2], WireRoom));
				Made.AppendChar(ZigStrung.Last() ? (RowStrung.Last() ? TEXT('x') : TEXT('z')) : (RowStrung.Last() ? TEXT('r') : TEXT('-')));
			}
		}
		if (Made == Piece->GetCampMade())
		{
			continue; // (nothing it is laid from has changed)
		}

		// ---- The models. Every point is where it stands on the table, on the ground under it.
		FMiniatureMesh Kit, Trodden;
		auto On = [this](const FVector2D& Where, double Over) { return SurfacePointOver(Where.X, Where.Y, Over); };
		FRandomStream Jitter(static_cast<int32>(GetTypeHash(Piece->GetNationTag()) % 100003u) * 31 + Home * 977 + 5);
		const FLinearColor Livery = FMiniatureMesh::VehicleColour(Piece->GetNationTag());
		// (Release 2, the art director's read of run 2, 22 November: the tents olive canvas, about 96, 98, 62 where the sun
		// is on them, not white; the trench a dark cut, about 55, 45, 32 inside, and its bank the colour of the earth it
		// was thrown up from, not a pale wall. The earth of the bank is the spoil's own, as lit about 166, 140, 96.)
		const FLinearColor TentCloth[3] = { CampHex(TEXT("545636")), CampHex(TEXT("4E5333")), CampHex(TEXT("5A5939")) };
		const FLinearColor TentEnds = CampHex(TEXT("474A2F"));
		const FLinearColor Sandbag = CampHex(TEXT("C2B280")), SandbagDark = CampHex(TEXT("A99B6C"));
		const FLinearColor SpoilDark = CampHex(TEXT("8C7650")), BankInside = CampHex(TEXT("75603F")), Cut = CampHex(TEXT("2B2219"));
		const FLinearColor Picket = CampHex(TEXT("3A342C")), Wire = CampHex(TEXT("55585C")), Barrel = CampHex(TEXT("2A2B2E"));

		// The trodden ground: a worn band out to each lot from the edge of the base, and the ground under it worn
		// in a patch. Soft at every edge, and nothing at its ends.
		auto Track = [&](const FVector2D& TrackFrom, const FVector2D& TrackTo, double HalfWide, float Much)
		{
			const double Length = FVector2D::Distance(TrackFrom, TrackTo);
			if (Length < 0.5)
			{
				return;
			}
			const FVector2D Along = (TrackTo - TrackFrom) / Length;
			const FVector2D Side(-Along.Y, Along.X);
			const int32 Steps = FMath::Max(3, FMath::CeilToInt32(Length / (0.8 * Unit)));
			const double Across[4] = { -1.0, -0.45, 0.45, 1.0 };
			int32 Before[4] = { INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE };
			for (int32 Step = 0; Step <= Steps; ++Step)
			{
				const double Through = static_cast<double>(Step) / Steps;
				const float Fade = static_cast<float>(FMath::SmoothStep(0.0, 1.0, FMath::Min(Step, Steps - Step) / 1.5));
				int32 Row[4] = { INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE };
				for (int32 K = 0; K < 4; ++K)
				{
					const FVector2D Where = TrackFrom + (TrackTo - TrackFrom) * Through + Side * (Across[K] * HalfWide);
					Row[K] = CampEarthPoint(Trodden, On(Where, 0.07), (K == 1 || K == 2) ? Much * Fade : 0.0f);
				}
				if (Step > 0)
				{
					for (int32 K = 0; K < 3; ++K)
					{
						Trodden.Face(Before[K], Before[K + 1], Row[K], FVector::UpVector);
						Trodden.Face(Before[K + 1], Row[K + 1], Row[K], FVector::UpVector);
					}
				}
				for (int32 K = 0; K < 4; ++K)
				{
					Before[K] = Row[K];
				}
			}
		};
		auto Patch = [&](const FVector2D& PatchAt, const FVector2D& PatchOut, double AcrossReach, double AlongReach, float Much)
		{
			const FVector2D Sideways(-PatchOut.Y, PatchOut.X);
			const int32 Hub = CampEarthPoint(Trodden, On(PatchAt, 0.07), Much);
			const int32 Sides = 12;
			int32 Inner[Sides + 1], Outer[Sides + 1];
			for (int32 K = 0; K <= Sides; ++K)
			{
				const double Angle = UE_DOUBLE_TWO_PI * K / Sides;
				const FVector2D Edge = Sideways * (FMath::Cos(Angle) * AcrossReach) + PatchOut * (FMath::Sin(Angle) * AlongReach);
				Inner[K] = CampEarthPoint(Trodden, On(PatchAt + Edge * 0.6, 0.07), Much * 0.85f);
				Outer[K] = CampEarthPoint(Trodden, On(PatchAt + Edge, 0.07), 0.0f);
			}
			for (int32 K = 0; K < Sides; ++K)
			{
				Trodden.Face(Hub, Inner[K], Inner[K + 1], FVector::UpVector);
				Trodden.Face(Inner[K], Outer[K], Outer[K + 1], FVector::UpVector);
				Trodden.Face(Inner[K], Outer[K + 1], Inner[K + 1], FVector::UpVector);
			}
		};
		for (const FCampLot& Lot : Lots)
		{
			if (Lot.bOpen)
			{
				Track(Middle + Lot.Outward * (1.05 * Radius), Lot.Centre - Lot.Outward * (0.5 * Lot.HalfDeep * Radius), 0.16 * Radius, 0.5f);
				Patch(Lot.Centre, Lot.Outward, (Lot.HalfLong + 0.2) * Radius, (Lot.HalfDeep + 0.22) * Radius, 0.34f);
			}
		}

		// Two lines of ridge tents in each lot, four to a line, their ridges along the way out and their ends open
		// toward the base. (The tents of the camp the piece pitches when it rests, at a smaller size: see Rebuild.)
		// (Release 2, the art director's read of run 2, 22 November: white tents in a perfect grid "read as sugar
		// cubes". They are olive canvas now, each of a shade of its own, and a little lower in the ridge; the second
		// line stands half a tent along from the first, and every tent stands a little out of its place and a little
		// turned. Those draws are the army's own, from its nation, its province and where it stands, and the same each
		// time its camp is laid, so the tents do not shift as the months go by and its works grow.)
		const int32 CampSeed = static_cast<int32>(GetTypeHash(Piece->GetNationTag()) % 100003u) * 31 + Home * 977 + FMath::RoundToInt32(Middle.X * 4.0) * 131 + FMath::RoundToInt32(Middle.Y * 4.0) * 17;
		for (int32 Which = 0; Which < 2; ++Which)
		{
			const FCampLot& Lot = Lots[Which];
			if (!Lot.bOpen)
			{
				continue;
			}
			FRandomStream TentLuck(CampSeed + Which * 7919);
			const FVector2D Sideways(-Lot.Outward.Y, Lot.Outward.X);
			const double Turn = CampYaw(Lot.Outward);
			for (int32 Line = 0; Line < 2; ++Line)
			{
				for (int32 K = 0; K < 4; ++K)
				{
					const double TentAcross = (K - 1.5 + (Line == 0 ? -0.25 : 0.25)) * 1.3 + TentLuck.FRandRange(-0.06f, 0.06f);
					const double TentOut = (Line - 0.5) * 1.9 + TentLuck.FRandRange(-0.12f, 0.12f);
					const FLinearColor& Cloth = TentCloth[TentLuck.RandRange(0, 2)];
					const double TentTurn = Turn + TentLuck.FRandRange(-8.0f, 8.0f);
					Kit.House(On(Lot.Centre + Sideways * (TentAcross * Unit) + Lot.Outward * (TentOut * Unit), 0.0), 1.3 * Wu, 1.0 * Wu, 0.06 * Wu, 0.7 * Wu, TentEnds, Cloth, TentTurn);
				}
			}
		}

		// The vehicle park: lorries in three rows, noses out, cabs in the army's paint and loads under canvas. Once the
		// works are finished they are under one net, pegged down at its edges, humped over the rows and mottled.
		const FCampLot& Park = Lots[2];
		if (Park.bOpen)
		{
			const FVector2D Sideways(-Park.Outward.Y, Park.Outward.X);
			if (Stage < 3)
			{
				const double Turn = CampYaw(Park.Outward);
				const FVector Nose = CampWorldWay(Park.Outward);
				const FLinearColor Load = FMath::Lerp(Livery, CampHex(TEXT("8A8261")), 0.5f);
				for (int32 Row = 0; Row < 3; ++Row)
				{
					for (int32 K = 0; K < 4; ++K)
					{
						const FVector Foot = On(Park.Centre + Sideways * ((K - 1.5) * 1.25 * Unit) + Park.Outward * ((Row - 1) * 1.3 * Unit), 0.0);
						Kit.Box(Foot + Nose * (0.42 * Wu) + FVector(0.0, 0.0, 0.27 * Wu), FVector(0.34, 0.5, 0.5) * Wu, Livery, Turn);
						Kit.Box(Foot - Nose * (0.15 * Wu) + FVector(0.0, 0.0, 0.23 * Wu), FVector(0.78, 0.54, 0.42) * Wu, Load, Turn);
					}
				}
			}
			else
			{
				const int32 NetAcross = 8, NetAlong = 6;
				const double HalfAcross = 2.9 * Unit, HalfAlong = 2.2 * Unit;
				const FLinearColor Mottle[4] = { CampHex(TEXT("3E4A26")), CampHex(TEXT("2F3A1F")), CampHex(TEXT("4A4632")), CampHex(TEXT("36422A")) };
				TArray<FVector, TInlineAllocator<NetAcross * NetAlong>> Knots;
				for (int32 J = 0; J < NetAlong; ++J)
				{
					for (int32 I = 0; I < NetAcross; ++I)
					{
						const double S = 2.0 * I / (NetAcross - 1) - 1.0, T = 2.0 * J / (NetAlong - 1) - 1.0;
						const bool bEdge = I == 0 || J == 0 || I == NetAcross - 1 || J == NetAlong - 1;
						const double Pull = bEdge ? 1.0 - 0.14 * Jitter.FRand() : 1.0; // (a ragged edge)
						const double Hump = bEdge ? 0.0 : 0.62 * (1.0 - FMath::Pow(S, 4.0)) * (1.0 - FMath::Pow(T, 4.0)) * (0.75 + 0.25 * FMath::Cos(3.0 * UE_DOUBLE_PI * T));
						Knots.Add(On(Park.Centre + Sideways * (S * HalfAcross * Pull) + Park.Outward * (T * HalfAlong * Pull), 0.0) + FVector(0.0, 0.0, (0.03 + Hump) * Wu));
					}
				}
				TArray<int32, TInlineAllocator<NetAcross * NetAlong>> Tied;
				for (int32 J = 0; J < NetAlong; ++J)
				{
					for (int32 I = 0; I < NetAcross; ++I)
					{
						const FVector Over = Knots[J * NetAcross + FMath::Min(I + 1, NetAcross - 1)] - Knots[J * NetAcross + FMath::Max(I - 1, 0)];
						const FVector Up = Knots[FMath::Min(J + 1, NetAlong - 1) * NetAcross + I] - Knots[FMath::Max(J - 1, 0) * NetAcross + I];
						FVector Normal = (Over ^ Up).GetSafeNormal();
						Normal = Normal.IsNearlyZero() ? FVector::UpVector : Normal.Z < 0.0 ? -Normal : Normal;
						Tied.Add(Kit.Add(Knots[J * NetAcross + I], Normal, Mottle[(I * 5 + J * 3 + Jitter.RandRange(0, 1)) % 4].ToFColor(true)));
					}
				}
				for (int32 J = 0; J + 1 < NetAlong; ++J)
				{
					for (int32 I = 0; I + 1 < NetAcross; ++I)
					{
						const int32 Here = Tied[J * NetAcross + I], Right = Tied[J * NetAcross + I + 1], Below = Tied[(J + 1) * NetAcross + I], Corner = Tied[(J + 1) * NetAcross + I + 1];
						Kit.Face(Here, Right, Corner, FVector::UpVector);
						Kit.Face(Here, Corner, Below, FVector::UpVector);
					}
				}
			}
		}

		// The trench, run by run of what is dug: the cut a dark band lying in the ground, and before it toward the enemy
		// a low bank of the earth thrown out of it, a hand's breadth at its crest and falling away to nothing at its
		// foot. Each run is one strip, mitred at its corners and laid over the ground a step at a time so that it keeps
		// to it. (Release 2, the art director's read of run 2, 22 November: it was "a pale raised wall", a bank of pale
		// spoil higher than the cut was wide, and a second bank behind, so that the cut hardly showed. It "should be a
		// dark cut ... with a low earth parapet on the front side, flush with the ground, earth-coloured".)
		int32 FirstDug = INDEX_NONE, LastDug = INDEX_NONE;
		for (int32 K = 0; K < SegDug.Num(); ++K)
		{
			if (SegDug[K])
			{
				FirstDug = FirstDug == INDEX_NONE ? K : FirstDug;
				LastDug = K + 1;
			}
		}
		if (FirstDug != INDEX_NONE)
		{
			// (and from the base out to it, the way the men go up to the line)
			Track(At(1.05, 0.0), At(2.15, 0.0), 0.12 * Radius, 0.42f);
		}
		auto SegFront = [&Ahead](const FVector2D& SegFrom, const FVector2D& SegTo)
		{
			// (the side of a stretch that looks toward the enemy)
			const FVector2D SegWay = (SegTo - SegFrom).GetSafeNormal();
			return FVector2D::DotProduct(FVector2D(-SegWay.Y, SegWay.X), Ahead) >= 0.0 ? FVector2D(-SegWay.Y, SegWay.X) : FVector2D(SegWay.Y, -SegWay.X);
		};
		const FColor CutInk = Cut.ToFColor(true), BankInk = SpoilDark.ToFColor(true), BankInsideInk = BankInside.ToFColor(true);
		auto LayTrench = [&](int32 FirstBay, int32 LastBay)
		{
			int32 LastRungs[5] = { INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE };
			for (int32 Bay = FirstBay; Bay < LastBay; ++Bay)
			{
				const FVector2D BayFrom = Bays[Bay], BayTo = Bays[Bay + 1];
				const FVector2D StretchFront = SegFront(BayFrom, BayTo);
				const int32 TrenchSteps = FMath::Max(1, FMath::CeilToInt32(FVector2D::Distance(BayFrom, BayTo) / (1.1 * Unit)));
				for (int32 Step = Bay == FirstBay ? 0 : 1; Step <= TrenchSteps; ++Step)
				{
					const FVector2D TrenchSpot = BayFrom + (BayTo - BayFrom) * (static_cast<double>(Step) / TrenchSteps);
					FVector2D CutSide = StretchFront;
					if (Step == TrenchSteps && Bay + 1 < LastBay)
					{
						// (a corner: across it along the line halfway between the two stretches' fronts, and further out, to keep the width)
						const FVector2D Mitre = (StretchFront + SegFront(BayTo, Bays[Bay + 2])).GetSafeNormal();
						CutSide = Mitre / FMath::Max(FVector2D::DotProduct(Mitre, StretchFront), 0.6);
					}
					const bool bRunEnd = (Bay == FirstBay && Step == 0) || (Bay + 1 == LastBay && Step == TrenchSteps);
					const FVector CutWorld = CampWorldWay(CutSide) * Wu; // (one of the piece's units out from the line, toward the enemy)
					const FVector CutDir = CutWorld.GetSafeNormal();
					const FVector CutFloor = On(TrenchSpot, 0.05);
					const FVector BankFoot = On(TrenchSpot + CutSide * (0.6 * Unit), 0.05);
					const double CrestHigh = (bRunEnd ? 0.02 : 0.12) * Wu; // (the bank runs out to nothing where the trench ends)
					int32 Rungs[5];
					Rungs[0] = Kit.Add(CutFloor - CutWorld * 0.3, FVector::UpVector, CutInk); // the cut, from its back edge
					Rungs[1] = Kit.Add(CutFloor + CutWorld * 0.3, FVector::UpVector, CutInk); // to its front edge
					Rungs[2] = Kit.Add(CutFloor + CutWorld * 0.3, (FVector::UpVector - CutDir * 0.5).GetSafeNormal(), BankInsideInk); // the bank, up from the cut
					Rungs[3] = Kit.Add(BankFoot - CutWorld * 0.1 + FVector(0.0, 0.0, CrestHigh), FVector::UpVector, BankInk); // over its crest
					Rungs[4] = Kit.Add(BankFoot + CutWorld * 0.22, (FVector::UpVector + CutDir * 0.4).GetSafeNormal(), BankInk); // and down to its foot
					if (LastRungs[0] != INDEX_NONE)
					{
						for (const int32 Strip : { 0, 2, 3 })
						{
							Kit.Face(LastRungs[Strip], LastRungs[Strip + 1], Rungs[Strip], FVector::UpVector);
							Kit.Face(LastRungs[Strip + 1], Rungs[Strip + 1], Rungs[Strip], FVector::UpVector);
						}
					}
					for (int32 Copied = 0; Copied < 5; ++Copied)
					{
						LastRungs[Copied] = Rungs[Copied];
					}
				}
			}
		};
		int32 RunStart = INDEX_NONE;
		for (int32 K = 0; K <= SegDug.Num(); ++K)
		{
			if (K < SegDug.Num() && SegDug[K])
			{
				RunStart = RunStart == INDEX_NONE ? K : RunStart;
			}
			else if (RunStart != INDEX_NONE)
			{
				LayTrench(RunStart, K); // (from that bend to this one)
				RunStart = INDEX_NONE;
			}
		}
		if (Stage == 2 && FirstDug != INDEX_NONE)
		{
			// Still being dug: a heap of fresh spoil at each end of what is done.
			for (const int32 End : { FirstDug, LastDug })
			{
				const FVector2D Outward = (Bays[End] - Middle).GetSafeNormal();
				Kit.Ball(On(Bays[End] - Outward * (0.5 * Unit), 0.0) + FVector(0.0, 0.0, 0.12 * Wu), FVector(1.1, 0.8, 0.5) * Wu, SpoilDark, 4, 8);
			}
		}

		// The wire: a picket at each stake, a strand along each row, and the zigzag from row to row between them.
		{
			TArray<FVector, TInlineAllocator<32>> StakeFeet;
			for (int32 K = 0; K < Stakes.Num(); ++K)
			{
				StakeFeet.Add(StakeOpen[K] ? On(Stakes[K], 0.0) : FVector::ZeroVector);
			}
			for (int32 K = 0; K < Stakes.Num(); ++K)
			{
				if (!StakeOpen[K])
				{
					continue;
				}
				Kit.Rod(StakeFeet[K], StakeFeet[K] + FVector(0.0, 0.0, 0.62 * Wu), 0.045 * Wu, Picket, 3);
				if (RowStrung[K])
				{
					Kit.Rod(StakeFeet[K] + FVector(0.0, 0.0, 0.5 * Wu), StakeFeet[K + 2] + FVector(0.0, 0.0, 0.5 * Wu), 0.025 * Wu, Wire, 3);
				}
				if (ZigStrung[K])
				{
					Kit.Rod(StakeFeet[K] + FVector(0.0, 0.0, 0.3 * Wu), StakeFeet[K + 1] + FVector(0.0, 0.0, 0.3 * Wu), 0.025 * Wu, Wire, 3);
				}
			}
		}

		// The gun pits: the floor dug down dark, a ring of sandbags round it open at the back, and a gun with its
		// barrel out over the front toward the enemy.
		{
			const FVector Forward = CampWorldWay(Ahead);
			const double Facing = CampYaw(Ahead);
			const double RingOut = 0.17 * Radius * UnitScale;
			for (int32 K = 0; K < 3; ++K)
			{
				if (!bPit[K])
				{
					continue;
				}
				const FVector Floor = On(PitAt[K], 0.0);
				Kit.Pillar(Floor, 0.13 * Radius * UnitScale, 0.13 * Radius * UnitScale, 0.04 * Wu, Cut, 10);
				for (int32 Sack = 0; Sack < 10; ++Sack)
				{
					if (Sack == 5)
					{
						continue; // (the way in, at the back)
					}
					const double Round = FMath::DegreesToRadians(Sack * 36.0);
					const FVector2D Out = Ahead * FMath::Cos(Round) + Abeam * FMath::Sin(Round);
					Kit.Box(Floor + CampWorldWay(Out) * RingOut + FVector(0.0, 0.0, 0.15 * Wu), FVector(0.34, 0.62, 0.3) * Wu, Sack % 2 == 0 ? Sandbag : SandbagDark, CampYaw(Out));
				}
				Kit.Box(Floor + Forward * (0.02 * Radius * UnitScale) + FVector(0.0, 0.0, 0.24 * Wu), FVector(0.07, 0.5, 0.3) * Wu, Livery, Facing); // the shield
				Kit.Rod(Floor + FVector(0.0, 0.0, 0.26 * Wu), Floor + Forward * (0.24 * Radius * UnitScale) + FVector(0.0, 0.0, 0.3 * Wu), 0.055 * Wu, Barrel, 4);
				Kit.Rod(Floor + FVector(0.0, 0.0, 0.12 * Wu), Floor - Forward * (0.1 * Radius * UnitScale) + FVector(0.0, 0.0, 0.04 * Wu), 0.05 * Wu, Livery, 4); // the trail
			}
		}

		Piece->LayCamp(Made, &Kit, &Trodden, GetPieceMaterial(), Trampled);
		++Pitched;
		Cramped += bShort ? 1 : 0;
	}
	if (Pitched + Struck > 0)
	{
		UE_LOG(LogTheater, Log, TEXT("Camps: %d laid again (%d of them with a part left out for want of open ground), %d struck"), Pitched, Cramped, Struck);
	}
}
