#include "Campaign/BargainLayer.h"

#include "Campaign/BargainLook.h"
#include "Campaign/CampaignCameraPawn.h"
#include "Campaign/CampaignMapActor.h"
#include "Campaign/CampaignMapData.h"
#include "Campaign/CampaignPlayerController.h"
#include "Campaign/CampaignSession.h"
#include "Campaign/CampaignSounds.h"
#include "Campaign/CampaignView.h"
#include "Campaign/EdgeSeals.h"
#include "Campaign/TheaterSpeed.h"
#include "TheaterCommand.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "CoreGlobals.h" // (GFrameCounter)
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "ImageUtils.h"
#include "InputCoreTypes.h"
#include "Input/CursorReply.h"
#include "Input/Reply.h"
#include "Layout/Geometry.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#include <cmath>
#include <string>
#include <vector>

// (Release 2: a neutral's bargains and the volunteer legions on the campaign map; the build contract,
// production/r2-bargains-legions/contract.md, D.7.) The engine's side of them: see BargainLayer.h. Where everything
// stands and what it says is Bargains' (BargainLook.h, plain C++ with its own tests); this file only asks for the book,
// turns the look's draw lists into Slate draws and the lane and the tint into mesh sections, and answers the pointer.
// Its internals live in BargainLayer::Impl, never in an unnamed namespace, so that a unity build cannot mix them with
// another file's helpers of the same name (EdgeNewsLayer's FSealPen, the HUD's Serif).

namespace BargainLayer
{
	namespace Impl
	{
		// -----------------------------------------------------------------------------------------------------------
		// Colours: sRGB in, linear out (FLinearColor(FColor) applies the sRGB curve), never the bytes over 255

		FLinearColor InkOf(uint32 Srgb, float Alpha = 1.0f)
		{
			FLinearColor Ink(FColor(static_cast<uint8>((Srgb >> 16) & 0xFF), static_cast<uint8>((Srgb >> 8) & 0xFF), static_cast<uint8>(Srgb & 0xFF)));
			Ink.A = Alpha;
			return Ink;
		}

		FLinearColor InkOf(const Bargains::FInk& Ink)
		{
			return InkOf(Ink.Srgb, Ink.A);
		}

		/** A vertex's colour for the M_Highlight ink: linear light in the bytes, as the table's own ribbons are coloured (ToFColor(false)). */
		FColor VertexOf(uint32 Srgb, float Alpha)
		{
			return InkOf(Srgb, Alpha).ToFColor(false);
		}

		uint32 SrgbOf(const FColor& Colour)
		{
			return (static_cast<uint32>(Colour.R) << 16) | (static_cast<uint32>(Colour.G) << 8) | static_cast<uint32>(Colour.B);
		}

		// -----------------------------------------------------------------------------------------------------------
		// Text: the book is UTF-8

		FString FromUtf8(const std::string& Text)
		{
			return Text.empty() ? FString() : FString(UTF8_TO_TCHAR(Text.c_str()));
		}

		std::string ToUtf8(const FString& Text)
		{
			const FTCHARToUTF8 Bytes(*Text);
			return std::string(Bytes.Get(), static_cast<size_t>(Bytes.Length()));
		}

		// -----------------------------------------------------------------------------------------------------------
		// The kit's two faces, read as the HUD reads them (CampaignHUD.cpp's Serif and Typed, which are that file's own):
		// PT Serif and Courier Prime from Content/Fonts, else the engine's plain faces.

		FSlateFontInfo SerifFace(const TCHAR* Weight, float Size, int32 Spacing)
		{
			static TSharedPtr<FStandaloneCompositeFont> Face;
			static bool bLooked = false;
			if (!bLooked)
			{
				bLooked = true;
				const FString Own = FPaths::ProjectContentDir() / TEXT("Fonts");
				const FString System = TEXT("/System/Library/Fonts/Supplemental/PTSerif.ttc");
				auto Add = [](FStandaloneCompositeFont& Into, const TCHAR* Name, const FString& File, int32 SubFace)
				{
					FTypefaceEntry& Entry = Into.DefaultTypeface.Fonts.Emplace_GetRef(FName(Name));
					Entry.Font = FFontData(File, EFontHinting::Default, EFontLoadingPolicy::LazyLoad, SubFace);
				};
				if (FPaths::FileExists(Own / TEXT("PTSerif-Regular.ttf")) && FPaths::FileExists(Own / TEXT("PTSerif-Italic.ttf")) && FPaths::FileExists(Own / TEXT("PTSerif-Bold.ttf")))
				{
					Face = MakeShared<FStandaloneCompositeFont>();
					Add(*Face, TEXT("Regular"), Own / TEXT("PTSerif-Regular.ttf"), 0);
					Add(*Face, TEXT("Italic"), Own / TEXT("PTSerif-Italic.ttf"), 0);
					Add(*Face, TEXT("Bold"), Own / TEXT("PTSerif-Bold.ttf"), 0);
				}
				else if (FPaths::FileExists(System))
				{
					// (in the system's collection the faces stand in this order: regular, italic, bold italic, bold)
					Face = MakeShared<FStandaloneCompositeFont>();
					Add(*Face, TEXT("Regular"), System, 0);
					Add(*Face, TEXT("Italic"), System, 1);
					Add(*Face, TEXT("Bold"), System, 3);
				}
			}
			if (!Face.IsValid())
			{
				FSlateFontInfo Plain = FCoreStyle::GetDefaultFontStyle(Weight, Size);
				Plain.LetterSpacing = Spacing;
				return Plain;
			}
			FSlateFontInfo Info(Face, Size, FName(Weight));
			Info.LetterSpacing = Spacing;
			return Info;
		}

		FSlateFontInfo TypedFace(float Size, bool bBold, int32 Spacing)
		{
			static TSharedPtr<FStandaloneCompositeFont> Face;
			static bool bLooked = false;
			if (!bLooked)
			{
				bLooked = true;
				const FString Own = FPaths::ProjectContentDir() / TEXT("Fonts");
				if (FPaths::FileExists(Own / TEXT("CourierPrime-Regular.ttf")) && FPaths::FileExists(Own / TEXT("CourierPrime-Bold.ttf")))
				{
					Face = MakeShared<FStandaloneCompositeFont>();
					auto Add = [](FTypeface& Into, const TCHAR* Name, const FString& File)
					{
						FTypefaceEntry& Entry = Into.Fonts.Emplace_GetRef(FName(Name));
						Entry.Font = FFontData(File, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
					};
					Add(Face->DefaultTypeface, TEXT("Regular"), Own / TEXT("CourierPrime-Regular.ttf"));
					Add(Face->DefaultTypeface, TEXT("Bold"), Own / TEXT("CourierPrime-Bold.ttf"));
					// (the middle dot and the en dash are Courier Prime's own; anything else it lacks is cut from the engine's face)
					const FString Plain = FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf");
					if (FPaths::FileExists(Plain))
					{
						Add(Face->FallbackTypeface.Typeface, TEXT("Regular"), Plain);
						Add(Face->FallbackTypeface.Typeface, TEXT("Bold"), Plain);
					}
				}
			}
			if (!Face.IsValid())
			{
				FSlateFontInfo Plain = FCoreStyle::GetDefaultFontStyle(TEXT("Mono"), Size);
				Plain.LetterSpacing = Spacing;
				return Plain;
			}
			FSlateFontInfo Info(Face, Size, FName(bBold ? TEXT("Bold") : TEXT("Regular")));
			Info.LetterSpacing = Spacing;
			return Info;
		}

		/** A face of the look as Slate's: its size in layer units is pixels at 96 to the inch, Slate's points are 72 (x 0.75). */
		FSlateFontInfo FontOf(const Bargains::FFace& Face)
		{
			const float Points = static_cast<float>(Face.Size * 0.75);
			if (Face.Family == Bargains::EFamily::Courier)
			{
				return TypedFace(Points, Face.Style == Bargains::EStyle::Bold, Face.Spacing);
			}
			const TCHAR* Weight = Face.Style == Bargains::EStyle::Bold ? TEXT("Bold") : Face.Style == Bargains::EStyle::Italic ? TEXT("Italic") : TEXT("Regular");
			return SerifFace(Weight, Points, Face.Spacing);
		}

		/** The look's measure: Slate's own, in layer units (the font measure works at a scale of one). */
		Bargains::FPt MeasureText(std::string_view Text, const Bargains::FFace& Face)
		{
			if (Text.empty())
			{
				return Bargains::FPt{ 0.0, 0.0 };
			}
			if (!FSlateApplication::IsInitialized())
			{
				return Bargains::FPt{ static_cast<double>(Text.size()) * Face.Size * 0.55, Face.Size * 1.25 };
			}
			const FString Words = FromUtf8(std::string(Text));
			const FVector2D Size = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Words, FontOf(Face));
			return Bargains::FPt{ Size.X, Size.Y };
		}

		// -----------------------------------------------------------------------------------------------------------
		// Brushes: the goods signs (Content/Goods, white on clear, tinted) and the flags (the map's, painted by key)

		const FSlateBrush* GoodsBrush(const FString& Key)
		{
			static TMap<FString, TSharedPtr<FSlateBrush>> Kept;
			if (const TSharedPtr<FSlateBrush>* Found = Kept.Find(Key))
			{
				return Found->Get();
			}
			TSharedPtr<FSlateBrush> Brush;
			if (UTexture2D* Texture = FImageUtils::ImportFileAsTexture2D(FPaths::ProjectContentDir() / TEXT("Goods") / (Key + TEXT(".png"))))
			{
				Texture->AddToRoot();
				Brush = MakeShared<FSlateBrush>();
				Brush->SetResourceObject(Texture);
				Brush->ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
				Brush->DrawAs = ESlateBrushDrawType::Image;
			}
			Kept.Add(Key, Brush);
			return Brush.Get();
		}

		// -----------------------------------------------------------------------------------------------------------
		// A draw of the look, turned once into what Slate takes (its words and face made, its brush's key kept), so that
		// painting it each frame makes nothing new

		struct FPaintDraw
		{
			Bargains::EDraw Kind = Bargains::EDraw::Box;
			FVector2D At = FVector2D::ZeroVector;
			FVector2D Size = FVector2D::ZeroVector;
			FLinearColor Fill = FLinearColor::Transparent;
			FLinearColor Edge = FLinearColor::Transparent;
			float EdgeW = 0.0f;
			float Radius = 0.0f;
			FString Text;
			FSlateFontInfo Font;
			FString Key;
			FVector2D A = FVector2D::ZeroVector;
			FVector2D B = FVector2D::ZeroVector;
			TArray<FVector2D> Points;
		};

		void ToPaint(const std::vector<Bargains::FDraw>& In, TArray<FPaintDraw>& Out)
		{
			Out.Reset(static_cast<int32>(In.size()));
			for (const Bargains::FDraw& Draw : In)
			{
				FPaintDraw& Paint = Out.AddDefaulted_GetRef();
				Paint.Kind = Draw.Kind;
				Paint.At = FVector2D(Draw.At.X, Draw.At.Y);
				Paint.Size = FVector2D(Draw.At.W, Draw.At.H);
				Paint.Fill = InkOf(Draw.Fill);
				Paint.Edge = InkOf(Draw.Edge);
				Paint.EdgeW = static_cast<float>(Draw.EdgeW);
				Paint.Radius = static_cast<float>(Draw.Radius);
				if (Draw.Kind == Bargains::EDraw::Text)
				{
					Paint.Text = FromUtf8(Draw.Text);
					Paint.Font = FontOf(Draw.Font);
				}
				Paint.Key = FromUtf8(Draw.Key);
				Paint.A = FVector2D(Draw.A.X, Draw.A.Y);
				Paint.B = FVector2D(Draw.B.X, Draw.B.Y);
				for (const Bargains::FPt& Point : Draw.Points)
				{
					Paint.Points.Add(FVector2D(Point.X, Point.Y));
				}
			}
		}

		/**
		 * The discs, rings and bars the pieces are made of (EdgeNewsLayer's FSealPen, which is that file's own). MakeBox
		 * fills a rounded box with the TINT it is given and never reads the brush's own colour (only the outline takes
		 * OutlineSettings.Color), so each fill passes its own ink as the tint; a hollow ring's fill is the ink at 1/255.
		 */
		struct FPen
		{
			FSlateWindowElementList& Out;
			const FGeometry& On;

			void Disc(int32 Layer, double MidX, double MidY, double Radius, const FLinearColor& Ink) const
			{
				const FSlateRoundedBoxBrush Round(Ink, static_cast<float>(Radius));
				FSlateDrawElement::MakeBox(Out, Layer, On.ToPaintGeometry(FVector2D(2.0 * Radius, 2.0 * Radius), FSlateLayoutTransform(FVector2D(MidX - Radius, MidY - Radius))), &Round, ESlateDrawEffect::None, Round.TintColor.GetSpecifiedColor());
			}

			void Ring(int32 Layer, double MidX, double MidY, double Radius, double Width, const FLinearColor& Ink) const
			{
				const FLinearColor Hollow = Ink.CopyWithNewOpacity(Bargains::Kit::Hollow);
				const FSlateRoundedBoxBrush Round(Hollow, static_cast<float>(Radius), Ink, static_cast<float>(Width));
				FSlateDrawElement::MakeBox(Out, Layer, On.ToPaintGeometry(FVector2D(2.0 * Radius, 2.0 * Radius), FSlateLayoutTransform(FVector2D(MidX - Radius, MidY - Radius))), &Round, ESlateDrawEffect::None, Hollow);
			}

			/** A bar with rounded ends, Long by Thick, its middle at (MidX, MidY), turned Degrees clockwise. */
			void Bar(int32 Layer, double MidX, double MidY, double Long, double Thick, double Degrees, const FLinearColor& Ink) const
			{
				const FSlateRoundedBoxBrush Round(Ink, static_cast<float>(Thick * 0.5));
				const FGeometry Turned = On.MakeChild(FVector2D(Long, Thick), FSlateLayoutTransform(FVector2D(MidX - Long * 0.5, MidY - Thick * 0.5)), FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(static_cast<float>(Degrees)))), FVector2D(0.5, 0.5));
				FSlateDrawElement::MakeBox(Out, Layer, Turned.ToPaintGeometry(), &Round, ESlateDrawEffect::None, Round.TintColor.GetSpecifiedColor());
			}

			/** A stroke from A to B, Thick wide with round ends (no MakeLines: its points would be a new array each frame). */
			void Stroke(int32 Layer, const FVector2D& A, const FVector2D& B, double Thick, const FLinearColor& Ink) const
			{
				const FVector2D Way = B - A;
				const double Long = Way.Size();
				if (Long < 1e-6 || Thick <= 0.0)
				{
					return;
				}
				Bar(Layer, (A.X + B.X) * 0.5, (A.Y + B.Y) * 0.5, Long + Thick, Thick, FMath::RadiansToDegrees(std::atan2(Way.Y, Way.X)), Ink);
			}
		};

		const FSlateBrush* FlagBrushOf(const FString& Key);

		/**
		 * Paints a list of draws, moved by Offset, one layer each so that their order holds (or all on the one layer given,
		 * bOneLayer, for draws that never cross: the plate line, inside the town plates' own two layers); returns the next
		 * free layer.
		 */
		int32 PaintDraws(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const TArray<FPaintDraw>& Draws, const FVector2D& Offset, bool bOneLayer = false)
		{
			const FPen Pen{ Out, Geometry };
			for (const FPaintDraw& Draw : Draws)
			{
				const FVector2D At = Draw.At + Offset;
				switch (Draw.Kind)
				{
				case Bargains::EDraw::Box:
				{
					if (Draw.Size.X <= 0.0 || Draw.Size.Y <= 0.0)
					{
						break;
					}
					if (Draw.EdgeW > 0.0f)
					{
						const FSlateRoundedBoxBrush Round(Draw.Fill, Draw.Radius, Draw.Edge, Draw.EdgeW);
						FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(Draw.Size, FSlateLayoutTransform(At)), &Round, ESlateDrawEffect::None, Draw.Fill);
					}
					else
					{
						const FSlateRoundedBoxBrush Round(Draw.Fill, Draw.Radius);
						FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(Draw.Size, FSlateLayoutTransform(At)), &Round, ESlateDrawEffect::None, Draw.Fill);
					}
					break;
				}
				case Bargains::EDraw::Text:
					if (!Draw.Text.IsEmpty())
					{
						FSlateDrawElement::MakeText(Out, Layer, Geometry.ToPaintGeometry(Draw.Size, FSlateLayoutTransform(At)), Draw.Text, Draw.Font, ESlateDrawEffect::None, Draw.Fill);
					}
					break;
				case Bargains::EDraw::Sign:
					if (const FSlateBrush* Sign = GoodsBrush(Draw.Key))
					{
						FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(Draw.Size, FSlateLayoutTransform(At)), Sign, ESlateDrawEffect::None, Draw.Fill);
					}
					break;
				case Bargains::EDraw::Flag:
					if (const FSlateBrush* Flag = FlagBrushOf(Draw.Key))
					{
						FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(Draw.Size, FSlateLayoutTransform(At)), Flag, ESlateDrawEffect::None, FLinearColor::White);
					}
					break;
				case Bargains::EDraw::Line:
					Pen.Stroke(Layer, Draw.A + Offset, Draw.B + Offset, Draw.EdgeW, Draw.Edge);
					break;
				case Bargains::EDraw::Poly:
					for (int32 Each = 1; Each < Draw.Points.Num(); ++Each)
					{
						Pen.Stroke(Layer, Draw.Points[Each - 1] + Offset, Draw.Points[Each] + Offset, Draw.EdgeW, Draw.Edge);
					}
					break;
				}
				if (!bOneLayer)
				{
					++Layer;
				}
			}
			return bOneLayer ? Layer + 1 : Layer;
		}

		// -----------------------------------------------------------------------------------------------------------
		// What the layer keeps

		/** A lane laid on the table, and its marks on the screen this frame. */
		struct FLaneLaid
		{
			Bargains::FLaneShape Shape;
			TArray<FPaintDraw> Crate; // the crate's draws with its middle at the origin
			FString Tag; // "to Britain"
			FVector2D TagSize = FVector2D::ZeroVector;
			FSlateFontInfo TagFont;
			FText Hover; // the cut's words or the crate's
			Bargains::FPathList OnScreen; // its pieces' points on the screen, seller first (ExitTagOfPieces')
			FVector2D CrateAt = FVector2D::ZeroVector;
			FVector2D RingAt = FVector2D::ZeroVector;
			FVector2D TagAt = FVector2D::ZeroVector;
			bool bCrate = false;
			bool bRing = false;
			bool bTag = false;
		};

		/** A seal at a capital, this frame. */
		struct FSealLaid
		{
			bool bShown = false;
			bool bOnCard = false; // the open card's seal, standing on the card's top-right corner as its close (call 6)
			FVector2D At = FVector2D::ZeroVector; // its disc's middle
			FText Tip;
		};

		/** A sender's capital plate line (the town plate's) and our own capital plate where the town has none. */
		struct FPlateLaid
		{
			int32 Capital = INDEX_NONE;
			Bargains::FPlateLineLay Lay;
			TArray<FPaintDraw> Line; // the line, its top left at the origin
			uint64 DrawnFrame = 0; // the frame STownPlates last drew it on the capital's plate
			Bargains::FOwnPlate Own; // our own plate, laid for a capital at the origin
			TArray<FPaintDraw> OwnDraws;
			FVector2D OwnAt = FVector2D::ZeroVector;
			bool bOwn = false;
		};

		struct FTrayLaid
		{
			int32 Corps = INDEX_NONE;
			FString Flag;
			FText Line;
			FText Tip;
		};

		/** A slot for every seal the look can return (it caps them itself): no seal is left unlaid, unpainted or unpressable. */
		constexpr int32 SealSlots = Bargains::MostSeals;
		static_assert(SealSlots >= Bargains::MostSeals, "a seal slot for every seal LookAt can return");
		constexpr int32 LaneSlots = Bargains::MostLanes;
		/** The map's pixel (RebuildHighlight's, of the 1600 by 1000 view) to a pixel of the 1440 x 900 picture. */
		constexpr double PictureTick = 1000.0 / 900.0;
		/** How high over the ground the tint, the paper casing and the dashes lie, in map units (the table's lanes lie at 0.12 to 0.3). */
		constexpr double TintLift = 0.2;
		constexpr double CasingLift = 0.28;
		constexpr double DashLift = 0.32;

		struct FMeshBuild
		{
			TArray<FVector> Vertices;
			TArray<int32> Triangles;
			TArray<FVector> Normals;
			TArray<FVector2D> UVs;
			TArray<FColor> Colors;
			TArray<FProcMeshTangent> Tangents;

			void Reset()
			{
				Vertices.Reset();
				Triangles.Reset();
				Normals.Reset();
				UVs.Reset();
				Colors.Reset();
				Tangents.Reset();
			}

			int32 Add(const FVector& Position, const FColor& Colour)
			{
				Vertices.Add(Position);
				Normals.Add(FVector::UpVector);
				UVs.Add(FVector2D::ZeroVector);
				Colors.Add(Colour);
				Tangents.Emplace(FVector(0.0, 1.0, 0.0), false);
				return Vertices.Num() - 1;
			}

			/** A triangle wound so that its face looks up, whichever order the corners came in (as the table's own buffers wind theirs). */
			void Face(int32 A, int32 B, int32 C)
			{
				const FVector Up = (Vertices[B] - Vertices[C]) ^ (Vertices[A] - Vertices[C]);
				Triangles.Add(A);
				Triangles.Add(Up.Z >= 0.0 ? B : C);
				Triangles.Add(Up.Z >= 0.0 ? C : B);
			}
		};

		struct FState
		{
			TWeakObjectPtr<ACampaignPlayerController> Player;
			std::string Json;
			Bargains::FBook Book;
			Bargains::FLook Look;
			Bargains::FOpenCard Open;
			bool bFar = false;
			int32 Turn = -1;
			double RunAt = -1000.0; // when this month's crates set off
			bool bRunOwed = false; // a new month: the run starts the first time the map is seen

			// The table's meshes: made on first use, the map their outer.
			TWeakObjectPtr<ACampaignMapActor> MeshMap;
			TWeakObjectPtr<UProceduralMeshComponent> Mesh;
			TWeakObjectPtr<UMaterialInstanceDynamic> Ink;
			FMeshBuild Build;
			Bargains::FStrip Strip;
			std::vector<Bargains::FPath> Rings;
			std::vector<Bargains::FTintShape> Tints;
			double LaidAt = -1.0; // the eye's distance the meshes were laid for

			std::vector<FLaneLaid> Lanes;
			FSealLaid Seals[SealSlots];
			int32 HotSeal = INDEX_NONE;
			int32 CardSeal = INDEX_NONE; // the slot of the seal on the card this frame, or INDEX_NONE
			std::vector<FPlateLaid> Plates;
			std::vector<FTrayLaid> Trays;

			// The screen, as the layer last measured it.
			double Px = 0.0; // layer units a picture pixel
			double LaidPx = 0.0; // the Px the words and the card were laid at
			double PxPerUnit = 1.0; // the screen's pixels a layer unit
			double Eye = 0.0; // the eye's distance

			// The one card.
			Bargains::FCard Card;
			TArray<FPaintDraw> CardDraws;
			std::string CardRow;
			bool bCardLaid = false;
			Bargains::FPlaced Placed;
			bool bPlaced = false;
			Bargains::FRect WasCard; // where it stood before it was last placed: the keep-off's last paint may still hold it there
			int32 HotButton = INDEX_NONE;

			// Kept between frames so that laying again makes nothing new.
			TArray<FSlateRect> KeepOff;
			TArray<FBox2D> PlateBoxes;
			std::vector<Bargains::FRect> Frame;
			std::vector<Bargains::FRect> TownPlates;
			std::vector<Bargains::FRect> Avoid;
			std::vector<Bargains::FPt> Laid;

			TMap<FString, TSharedPtr<FSlateBrush>> Flags;
			TWeakObjectPtr<ACampaignMapActor> FlagsMap;
		};

		FState& State()
		{
			static FState Kept;
			return Kept;
		}

		const FSlateBrush* FlagBrushOf(const FString& Key)
		{
			FState& S = State();
			ACampaignPlayerController* Player = S.Player.Get();
			ACampaignMapActor* Map = Player ? Player->GetMap() : nullptr;
			if (!Map || Key.IsEmpty())
			{
				return nullptr;
			}
			if (S.FlagsMap.Get() != Map)
			{
				// (a new table: its flags are new pictures)
				S.Flags.Reset();
				S.FlagsMap = Map;
			}
			if (const TSharedPtr<FSlateBrush>* Found = S.Flags.Find(Key))
			{
				return Found->Get();
			}
			UTexture2D* Picture = Map->GetFlagTexture(Key);
			if (!Picture)
			{
				return nullptr; // (not ready: asked again next time)
			}
			TSharedPtr<FSlateBrush> Brush = MakeShared<FSlateBrush>();
			Brush->SetResourceObject(Picture);
			Brush->ImageSize = FVector2D(96.0f, 64.0f);
			Brush->DrawAs = ESlateBrushDrawType::Image;
			S.Flags.Add(Key, Brush);
			return Brush.Get();
		}

		/** The map has the screen and nothing is laid over it that the pieces would stand under (as the edge seals ask). */
		bool MapClear(const ACampaignPlayerController* Player)
		{
			return Player && Player->IsMapShown() && !Player->IsFrameHidden() && !Player->IsWatching() && !Player->IsPageOpen()
				&& !Player->GetFaceOff().bVisible && !Player->GetWarPrompt().bVisible && Player->GetAnnouncement() == nullptr;
		}

		double EyeOf(const ACampaignPlayerController* Player)
		{
			const ACampaignCameraPawn* Eye = Player ? Cast<ACampaignCameraPawn>(Player->GetPawn()) : nullptr;
			return Eye ? static_cast<double>(Eye->GetDistance()) : 0.0;
		}

		double LayPx(const FState& S)
		{
			return S.Px > 0.0 ? S.Px : 1.0; // (before the layer has measured the screen: a picture pixel a unit)
		}

		const FTrayLaid* TrayOf(int32 Corps)
		{
			const FState& S = State();
			if (!S.Book.bOn || Corps == INDEX_NONE)
			{
				return nullptr;
			}
			for (const FTrayLaid& Tray : S.Trays)
			{
				if (Tray.Corps == Corps)
				{
					return &Tray;
				}
			}
			return nullptr;
		}

		FPlateLaid* PlateOf(int32 Region)
		{
			FState& S = State();
			if (!S.Book.bOn || S.bFar || Region == INDEX_NONE)
			{
				return nullptr;
			}
			for (FPlateLaid& Plate : S.Plates)
			{
				if (Plate.Capital == Region)
				{
					return &Plate;
				}
			}
			return nullptr;
		}

		// -----------------------------------------------------------------------------------------------------------
		// The table: the lanes and the tints as ribbons on the ground

		UProceduralMeshComponent* MeshOf(FState& S, ACampaignMapActor* Map)
		{
			if (S.MeshMap.Get() != Map)
			{
				S.Mesh.Reset();
				S.Ink.Reset();
				S.MeshMap = Map;
			}
			if (UProceduralMeshComponent* Kept = S.Mesh.Get())
			{
				return Kept;
			}
			UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(Map, MakeUniqueObjectName(Map, UProceduralMeshComponent::StaticClass(), TEXT("BargainLanes")));
			Mesh->SetupAttachment(Map->GetRootComponent());
			Mesh->bUseAsyncCooking = false;
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Mesh->SetCastShadow(false);
			Mesh->SetTranslucentSortPriority(1); // (over the ground's washes and the table's own lanes)
			Mesh->RegisterComponent();
			S.Mesh = Mesh;
			// (the highlight's ink, as the table's own lanes take it: each point carries its own colour, at full brightness)
			if (UMaterialInterface* Highlight = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Campaign/Materials/M_Highlight.M_Highlight")))
			{
				UMaterialInstanceDynamic* Ink = UMaterialInstanceDynamic::Create(Highlight, Map);
				Ink->SetScalarParameterValue(TEXT("Brightness"), 1.0f);
				S.Ink = Ink;
			}
			else
			{
				UE_LOG(LogTheater, Warning, TEXT("Material M_Highlight is missing: the bargains' lanes are not drawn."));
			}
			return Mesh;
		}

		/** One strip as triangles: Outer[k] and Inner[k] across it, each side in its own colour. */
		void AddStrip(FMeshBuild& Build, const ACampaignMapActor& Map, const Bargains::FStrip& Strip, double Lift, const FColor& OuterInk, const FColor& InnerInk)
		{
			const size_t Count = FMath::Min(Strip.Outer.size(), Strip.Inner.size());
			int32 LastOuter = INDEX_NONE, LastInner = INDEX_NONE;
			for (size_t K = 0; K < Count; ++K)
			{
				const int32 Outer = Build.Add(Map.SurfacePointOver(Strip.Outer[K].X, Strip.Outer[K].Y, Lift), OuterInk);
				const int32 Inner = Build.Add(Map.SurfacePointOver(Strip.Inner[K].X, Strip.Inner[K].Y, Lift), InnerInk);
				if (LastOuter != INDEX_NONE)
				{
					Build.Face(LastOuter, LastInner, Outer);
					Build.Face(LastInner, Outer, Inner);
				}
				LastOuter = Outer;
				LastInner = Inner;
			}
		}

		void Commit(FState& S, UProceduralMeshComponent* Mesh, int32 Section)
		{
			if (S.Build.Triangles.Num() == 0)
			{
				return;
			}
			Mesh->CreateMeshSection(Section, S.Build.Vertices, S.Build.Triangles, S.Build.Normals, S.Build.UVs, S.Build.Colors, S.Build.Tangents, false);
			if (UMaterialInstanceDynamic* Ink = S.Ink.Get())
			{
				Mesh->SetMaterial(Section, Ink);
			}
		}

		/** Section 0, the tints: the seller's outer border, coasts and all, as the regions it holds now draw it. */
		void LayTints(FState& S, ACampaignMapActor* Map, UProceduralMeshComponent* Mesh, double Pixel)
		{
			const FCampaignMapData& Data = Map->GetData();
			const FCampaignSession* Session = S.Player.IsValid() ? S.Player->GetSession() : nullptr;
			const TArray<FString>* Holders = Session ? &Session->GetView().RegionHolder : nullptr;
			S.Tints.resize(S.Look.Tints.size());
			S.Build.Reset();
			for (size_t Each = 0; Each < S.Look.Tints.size(); ++Each)
			{
				const Bargains::FTintLook& Tint = S.Look.Tints[Each];
				Bargains::FTintShape& Shape = S.Tints[Each];
				if (Shape.Chains.empty())
				{
					const FString Seller = FromUtf8(Tint.Seller);
					S.Rings.clear();
					for (const FCampaignRegion& Region : Data.Regions)
					{
						const FString& Holder = Holders && Holders->IsValidIndex(Region.Id) && !(*Holders)[Region.Id].IsEmpty() ? (*Holders)[Region.Id] : Region.Owner;
						if (Holder != Seller)
						{
							continue;
						}
						for (const TArray<FVector2D>& Ring : Region.Polygons)
						{
							Bargains::FPath& Path = S.Rings.emplace_back();
							Path.reserve(static_cast<size_t>(Ring.Num()));
							for (const FVector2D& Point : Ring)
							{
								Path.push_back(Bargains::FPt{ Point.X, Point.Y });
							}
						}
					}
				}
				Bargains::LayTint(S.Rings, Pixel, Shape);
				const FColor Border = VertexOf(Tint.Ink, static_cast<float>(Tint.Alpha)), Inside = VertexOf(Tint.Ink, 0.0f);
				for (const Bargains::FStrip& Strip : Shape.Strips)
				{
					AddStrip(S.Build, *Map, Strip, TintLift, Border, Inside);
				}
			}
			Commit(S, Mesh, 0);
		}

		/** Sections 1 and 2, the lanes: the paper casing under, then the dashes from the seller and the cut's two ticks. */
		void LayLanes(FState& S, ACampaignMapActor* Map, UProceduralMeshComponent* Mesh, double Pixel)
		{
			const FCampaignMapData& Data = Map->GetData();
			const double MinX = Data.WorldMinX, MapW = Data.bWorld ? Data.WorldWidth : 0.0;
			S.Lanes.resize(S.Look.Lanes.size());
			for (size_t Each = 0; Each < S.Look.Lanes.size(); ++Each)
			{
				Bargains::LayLane(S.Book.Rows[static_cast<size_t>(S.Look.Lanes[Each].Row)], Pixel, MinX, MapW, S.Lanes[Each].Shape);
			}
			S.Build.Reset();
			const FColor Paper = VertexOf(Bargains::Kit::Paper200, Bargains::Lane::CasingA);
			for (const FLaneLaid& Lane : S.Lanes)
			{
				for (size_t Strip = 0; Strip < Lane.Shape.CasingCount && Strip < Lane.Shape.Casing.size(); ++Strip)
				{
					AddStrip(S.Build, *Map, Lane.Shape.Casing[Strip], CasingLift, Paper, Paper);
				}
			}
			Commit(S, Mesh, 1);
			S.Build.Reset();
			for (size_t Each = 0; Each < S.Lanes.size(); ++Each)
			{
				const FLaneLaid& Lane = S.Lanes[Each];
				const FColor Holder = VertexOf(S.Look.Lanes[Each].Ink, 1.0f);
				for (size_t Dash = 0; Dash < Lane.Shape.Dashes.Num(); ++Dash)
				{
					Bargains::Casing(Lane.Shape.Dashes[Dash], Bargains::Lane::DashW * 0.5 * Pixel, S.Strip);
					AddStrip(S.Build, *Map, S.Strip, DashLift, Holder, Holder);
				}
				if (S.Look.Lanes[Each].bCut && Lane.Shape.Gap.bOk)
				{
					for (const Bargains::FTick& Tick : Lane.Shape.Ticks)
					{
						const Bargains::FPath Across = { Tick.A, Tick.B };
						Bargains::Casing(Across, Bargains::Lane::TickW * 0.5 * Pixel, S.Strip);
						AddStrip(S.Build, *Map, S.Strip, DashLift, Holder, Holder);
					}
				}
			}
			Commit(S, Mesh, 2);
		}

		/**
		 * Lays the lanes and the tints for the eye at Eye: section 0 the tints (the border at its alpha, fading to nothing
		 * 13 px in), 1 the paper casings, 2 the dashes and the cut's ticks in the holder's colour. Nothing is made where
		 * nothing is drawn: with both rules off, or no lane and no tint, there is no mesh.
		 */
		void LayMeshes(FState& S, ACampaignMapActor* Map, double Eye)
		{
			THEATER_SPEED_SCOPE(BargainMeshes);
			S.LaidAt = Eye;
			const bool bAny = S.Book.bOn && (!S.Look.Lanes.empty() || !S.Look.Tints.empty());
			if (!Map || !bAny)
			{
				if (UProceduralMeshComponent* Kept = S.Mesh.Get())
				{
					Kept->ClearAllMeshSections();
				}
				return;
			}
			const double Pixel = Bargains::PixelOf(Eye, Map->UnitScale, 1.0, PictureTick);
			UProceduralMeshComponent* Mesh = MeshOf(S, Map);
			Mesh->ClearAllMeshSections();
			LayTints(S, Map, Mesh, Pixel);
			LayLanes(S, Map, Mesh, Pixel);
		}

		/**
		 * The book is the same and only the card open (or the eye's side of far) changed: the lanes stand as they are (the
		 * look's lanes follow the book alone), and only the tints, section 0, are laid again, at the eye the lanes were laid
		 * for (the zoom's own re-lay follows as it would). A press on a seal near the ground no longer lays every dash again.
		 */
		void LayTintsAgain(FState& S, ACampaignMapActor* Map)
		{
			if (!Map || !S.Book.bOn || S.LaidAt <= 0.0 || S.MeshMap.Get() != Map)
			{
				LayMeshes(S, Map, S.Eye > 0.0 ? S.Eye : EyeOf(S.Player.Get())); // (nothing laid yet for this map: all of it is)
				return;
			}
			if (!S.Mesh.IsValid() && S.Look.Tints.empty())
			{
				return; // (no mesh was made, and none is wanted)
			}
			THEATER_SPEED_SCOPE(BargainMeshes);
			UProceduralMeshComponent* Mesh = MeshOf(S, Map);
			Mesh->ClearMeshSection(0);
			LayTints(S, Map, Mesh, Bargains::PixelOf(S.LaidAt, Map->UnitScale, 1.0, PictureTick));
		}

		// -----------------------------------------------------------------------------------------------------------
		// The look, and the words laid at the screen's measure

		/** Lays what carries words or a size (the crates, the tags, the plate lines, our own plates, the card) at Px. */
		void LayWords(FState& S)
		{
			const double Px = LayPx(S);
			S.LaidPx = Px;
			const Bargains::FMeasure Measure = &MeasureText;
			std::vector<Bargains::FDraw> Draws;
			for (size_t Each = 0; Each < S.Lanes.size() && Each < S.Look.Lanes.size(); ++Each)
			{
				FLaneLaid& Lane = S.Lanes[Each];
				Draws.clear();
				if (S.Look.Lanes[Each].bCrate)
				{
					Bargains::CrateDraws(Bargains::FPt{ 0.0, 0.0 }, S.Look.Lanes[Each].Ink, Px, Draws);
				}
				ToPaint(Draws, Lane.Crate);
				const Bargains::FFace Face = Bargains::TagFace(Px);
				const Bargains::FPt Size = MeasureText(S.Look.Lanes[Each].Tag, Face);
				Lane.TagSize = FVector2D(Size.X, Size.Y);
				Lane.TagFont = FontOf(Face);
			}
			ACampaignPlayerController* Player = S.Player.Get();
			const ACampaignMapActor* Map = Player ? Player->GetMap() : nullptr;
			for (FPlateLaid& Plate : S.Plates)
			{
				const Bargains::FPlateLook* Look = nullptr;
				for (const Bargains::FPlateLook& Each : S.Look.Plates)
				{
					if (Each.Capital == Plate.Capital)
					{
						Look = &Each;
						break;
					}
				}
				if (!Look)
				{
					continue;
				}
				Plate.Lay = Bargains::LayPlateLine(Look->Line, Measure, Px);
				Draws.clear();
				Bargains::DrawPlateLine(Plate.Lay, Bargains::FPt{ 0.0, 0.0 }, Look->Line, Px, Draws);
				ToPaint(Draws, Plate.Line);
				const FString Name = Map && Map->GetData().Regions.IsValidIndex(Plate.Capital) ? Map->GetData().Regions[Plate.Capital].Name : FString();
				Bargains::LayOwnPlate(ToUtf8(Name), Look->Line, Bargains::FPt{ 0.0, 0.0 }, Measure, Px, Plate.Own);
				ToPaint(Plate.Own.Draws, Plate.OwnDraws);
			}
			// The card, where one is open: laid again from the book, and placed again where its size has changed.
			const Bargains::FRow* Row = S.Open.IsOpen() ? S.Book.Row(S.Open.Row()) : nullptr;
			const Bargains::FPt Was = S.Card.Size;
			S.bCardLaid = Row && Bargains::LayCard(*Row, S.Book, Measure, Px, S.Card);
			if (S.bCardLaid)
			{
				ToPaint(S.Card.Draws, S.CardDraws);
			}
			else
			{
				S.CardDraws.Reset();
			}
			if (!S.bCardLaid || S.CardRow != S.Open.Row() || FMath::Abs(Was.X - S.Card.Size.X) > 0.5 || FMath::Abs(Was.Y - S.Card.Size.Y) > 0.5)
			{
				S.bPlaced = false;
				S.Placed = Bargains::FPlaced();
			}
			S.CardRow = S.Open.Row();
			S.HotButton = INDEX_NONE;
		}

		/** What the look is asked with: the nations' colours, the eye's distance, the card open, and the corps the player sees as legions. */
		Bargains::FLookIn LookInOf(const FState& S)
		{
			ACampaignPlayerController* Player = S.Player.Get();
			ACampaignMapActor* Map = Player ? Player->GetMap() : nullptr;
			const FCampaignMapData* Data = Map ? &Map->GetData() : nullptr;
			Bargains::FLookIn In;
			In.SrgbOf = [Data](std::string_view Tag) -> uint32_t
			{
				const FCampaignNation* Nation = Data ? Data->FindNation(FromUtf8(std::string(Tag))) : nullptr;
				return Nation ? SrgbOf(Nation->Color) : 0x808080u;
			};
			In.bFar = S.bFar;
			In.OpenRow = S.Open.Row();
			// (a legion's tray pieces only for a corps the player's own picture shows as one: the bridge's vol, sent for his
			// own, an ally's, or a corps known exactly; never the flag or true strength of one not identified)
			const FCampaignSession* Session = Player ? Player->GetSession() : nullptr;
			const TArray<FViewCorps>* Corps = Session ? &Session->GetView().Corps : nullptr;
			In.SeesLegion = [Corps](int Id, std::string_view Home)
			{
				if (!Corps)
				{
					return false;
				}
				for (const FViewCorps& Each : *Corps)
				{
					if (Each.Id == Id)
					{
						return !Each.Vol.IsEmpty() && Each.Vol == FromUtf8(std::string(Home));
					}
				}
				return false;
			};
			return In;
		}

		/** The tray's pieces, from the look's. */
		void LayTrays(FState& S)
		{
			S.Trays.clear();
			for (const Bargains::FTrayLook& Look : S.Look.Trays)
			{
				FTrayLaid& Tray = S.Trays.emplace_back();
				Tray.Corps = Look.Corps;
				Tray.Flag = FromUtf8(Look.Flag);
				Tray.Line = FText::FromString(FromUtf8(Look.Line));
				Tray.Tip = FText::FromString(FromUtf8(Look.Tip));
			}
		}

		/** The tray's pieces again, for the player's picture of the corps as it is now (the book unchanged). */
		void Retray(FState& S)
		{
			Bargains::TraysOf(S.Book, LookInOf(S), S.Look.Trays);
			LayTrays(S);
		}

		/** Asks the look what the map shows of the book (for the eye's distance and the card open), and lays it all. */
		void Relook(FState& S, bool bNewBook = true)
		{
			ACampaignPlayerController* Player = S.Player.Get();
			ACampaignMapActor* Map = Player ? Player->GetMap() : nullptr;
			Bargains::LookAt(S.Book, LookInOf(S), S.Look);

			// (the look's lanes follow the book alone: with the same book, a card opened or closed, or the eye gone far or
			// near, leaves them as laid, and only the tints are laid again. A new book lays them again in the same buffers)
			const bool bLanesStand = !bNewBook && S.LaidAt > 0.0 && S.Lanes.size() == S.Look.Lanes.size();
			if (!bLanesStand)
			{
				S.Lanes.resize(S.Look.Lanes.size());
				for (size_t Each = 0; Each < S.Look.Lanes.size(); ++Each)
				{
					S.Lanes[Each].Tag = FromUtf8(S.Look.Lanes[Each].Tag);
					S.Lanes[Each].Hover = FText::FromString(FromUtf8(S.Look.Lanes[Each].Hover));
				}
			}
			S.Tints.clear(); // (the seller's border is read again from the regions it holds now)
			for (int32 Slot = 0; Slot < SealSlots; ++Slot)
			{
				S.Seals[Slot].bShown = false;
				S.Seals[Slot].bOnCard = false;
				S.Seals[Slot].Tip = static_cast<size_t>(Slot) < S.Look.Seals.size() ? FText::FromString(FromUtf8(S.Look.Seals[static_cast<size_t>(Slot)].Tip)) : FText::GetEmpty();
			}
			S.HotSeal = INDEX_NONE;
			S.CardSeal = INDEX_NONE;
			S.Plates.clear();
			for (const Bargains::FPlateLook& Look : S.Look.Plates)
			{
				if (Look.Capital >= 0 && !Look.Line.empty())
				{
					FPlateLaid& Plate = S.Plates.emplace_back();
					Plate.Capital = Look.Capital;
				}
			}
			LayTrays(S);
			if (bLanesStand)
			{
				LayTintsAgain(S, Map);
			}
			else
			{
				LayMeshes(S, Map, S.Eye > 0.0 ? S.Eye : EyeOf(Player));
			}
			LayWords(S);
		}

		/** The book is off: nothing stands, and the meshes, where any were made, are emptied. */
		void Clear(FState& S)
		{
			S.Book = Bargains::FBook();
			S.Look = Bargains::FLook();
			S.Open.Close();
			S.Lanes.clear();
			S.Tints.clear();
			S.Plates.clear();
			S.Trays.clear();
			for (FSealLaid& Seal : S.Seals)
			{
				Seal = FSealLaid();
			}
			S.HotSeal = INDEX_NONE;
			S.CardSeal = INDEX_NONE;
			S.Card = Bargains::FCard();
			S.CardDraws.Reset();
			S.CardRow.clear();
			S.bCardLaid = false;
			S.bPlaced = false;
			S.HotButton = INDEX_NONE;
			if (UProceduralMeshComponent* Kept = S.Mesh.Get())
			{
				Kept->ClearAllMeshSections();
			}
		}

		bool CardShown()
		{
			const FState& S = State();
			return S.Book.bOn && S.bCardLaid && S.bPlaced && S.Open.Shown(MapClear(S.Player.Get()));
		}

		// -----------------------------------------------------------------------------------------------------------
		// The player's orders and presses

		void Act(const Bargains::FButton& Button)
		{
			FState& S = State();
			ACampaignPlayerController* Player = S.Player.Get();
			FCampaignSession* Session = Player ? Player->GetSession() : nullptr;
			const Bargains::FRow* Row = S.Open.IsOpen() ? S.Book.Row(S.Open.Row()) : nullptr;
			if (!Session || !Row || !Button.bLive)
			{
				return;
			}
			const FString Id = FromUtf8(Row->Id);
			FString Note;
			bool bOk = false;
			switch (Button.Act)
			{
			case Bargains::EAct::Minus:
			case Bargains::EAct::Plus:
				bOk = Session->BargainBid(Id, Button.Pp, Note);
				break;
			case Bargains::EAct::Accept:
				bOk = Session->BargainSell(Id, FromUtf8(Button.Buyer), Note);
				break;
			case Bargains::EAct::Nobody:
				bOk = Session->BargainSell(Id, FString(), Note);
				break;
			case Bargains::EAct::None:
				return;
			}
			// (one sound for the press, as the court's orders: coins for an order taken, a click for one refused; the note
			// is said with none of its own, as SayWhy would add a second click)
			CampaignSounds::Play(Player, bOk ? TEXT("coins") : TEXT("click"));
			if (!Note.IsEmpty())
			{
				Player->SayNote(Note);
			}
			// (the order changed the war: the rules' kept answers were forgotten, and the book is read again)
			BargainLayer::ShowOnMap(Player);
		}

		/** A press on a seal: opens its card (and closes the other), or closes it on its own seal. False where it opens none. */
		bool PressSeal(int32 Slot)
		{
			FState& S = State();
			if (Slot < 0 || static_cast<size_t>(Slot) >= S.Look.Seals.size() || !S.Look.Seals[static_cast<size_t>(Slot)].bOpens)
			{
				return false;
			}
			const int32 Row = S.Look.Seals[static_cast<size_t>(Slot)].Row;
			if (Row < 0 || static_cast<size_t>(Row) >= S.Book.Rows.size())
			{
				return false;
			}
			if (S.Open.Press(S.Book, S.Book.Rows[static_cast<size_t>(Row)].Id))
			{
				Relook(S, false); // (the open card's seller is tinted, and its seal ringed; the lanes stand as laid)
				CampaignSounds::Play(S.Player.Get(), TEXT("click"));
			}
			return true;
		}

		// -----------------------------------------------------------------------------------------------------------
		// The widgets

		enum class ESpot : uint8
		{
			Seal,
			Crate,
			Ring,
		};

		/** Where a spot stands this frame (in the layer's units), or false where it does not. */
		bool SpotRect(ESpot Kind, int32 Slot, FVector2D& OutAt, FVector2D& OutSize)
		{
			const FState& S = State();
			if (!S.Book.bOn)
			{
				return false;
			}
			const double Px = LayPx(S);
			if (Kind == ESpot::Seal)
			{
				if (Slot < 0 || Slot >= SealSlots || !S.Seals[Slot].bShown)
				{
					return false;
				}
				const Bargains::FRect Box = Bargains::SealBox(Bargains::FPt{ S.Seals[Slot].At.X, S.Seals[Slot].At.Y }, Px);
				OutAt = FVector2D(Box.X, Box.Y);
				OutSize = FVector2D(Box.W, Box.H);
				return true;
			}
			if (Slot < 0 || static_cast<size_t>(Slot) >= S.Lanes.size())
			{
				return false;
			}
			const FLaneLaid& Lane = S.Lanes[static_cast<size_t>(Slot)];
			if (Kind == ESpot::Crate && Lane.bCrate)
			{
				OutSize = FVector2D(Bargains::Lane::CrateW + 4.0, Bargains::Lane::CrateH + 4.0) * Px;
				OutAt = Lane.CrateAt - OutSize * 0.5;
				return true;
			}
			if (Kind == ESpot::Ring && Lane.bRing)
			{
				OutSize = FVector2D(Bargains::Lane::RingD + 6.0, Bargains::Lane::RingD + 6.0) * Px;
				OutAt = Lane.RingAt - OutSize * 0.5;
				return true;
			}
			return false;
		}

		FText SpotTip(ESpot Kind, int32 Slot)
		{
			const FState& S = State();
			if (Kind == ESpot::Seal)
			{
				// (none for the seal on the card: it is the card's close there, and the card says what its round is)
				return Slot >= 0 && Slot < SealSlots && S.Seals[Slot].bShown && !S.Seals[Slot].bOnCard ? S.Seals[Slot].Tip : FText::GetEmpty();
			}
			return Slot >= 0 && static_cast<size_t>(Slot) < S.Lanes.size() ? S.Lanes[static_cast<size_t>(Slot)].Hover : FText::GetEmpty();
		}

		/** A seal, a crate or a cut's ring under the pointer: its words as a tip; only a seal's left press is taken. */
		class SBargainSpot : public SLeafWidget
		{
		public:
			SLATE_BEGIN_ARGS(SBargainSpot) {}
			SLATE_END_ARGS()

			void Construct(const FArguments&, ESpot InKind, int32 InSlot)
			{
				Kind = InKind;
				Slot = InSlot;
				const ESpot Mine = InKind;
				const int32 At = InSlot;
				SetVisibility(TAttribute<EVisibility>::CreateLambda([Mine, At]()
				{
					FVector2D Where, Size;
					return SpotRect(Mine, At, Where, Size) ? EVisibility::Visible : EVisibility::Collapsed;
				}));
				SetToolTipText(TAttribute<FText>::CreateLambda([Mine, At]() { return SpotTip(Mine, At); }));
			}

			virtual FVector2D ComputeDesiredSize(float) const override
			{
				FVector2D Where, Size;
				return SpotRect(Kind, Slot, Where, Size) ? Size : FVector2D::ZeroVector;
			}

			virtual int32 OnPaint(const FPaintArgs&, const FGeometry&, const FSlateRect&, FSlateWindowElementList&, int32 LayerId, const FWidgetStyle&, bool) const override
			{
				return LayerId; // (the layer's paint draws it; the spot only answers the pointer)
			}

			virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
			{
				SLeafWidget::OnMouseEnter(MyGeometry, MouseEvent);
				if (Kind == ESpot::Seal)
				{
					State().HotSeal = Slot;
				}
			}

			virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override
			{
				SLeafWidget::OnMouseLeave(MouseEvent);
				if (Kind == ESpot::Seal && State().HotSeal == Slot)
				{
					State().HotSeal = INDEX_NONE;
				}
			}

			virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
			{
				if (Kind != ESpot::Seal || MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
				{
					return FReply::Unhandled(); // (a drag of the map begun on a mark is still the map's)
				}
				return PressSeal(Slot) ? FReply::Handled() : FReply::Unhandled();
			}

			virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override
			{
				const FState& S = State();
				const bool bOpens = Kind == ESpot::Seal && Slot >= 0 && static_cast<size_t>(Slot) < S.Look.Seals.size() && S.Look.Seals[static_cast<size_t>(Slot)].bOpens;
				return bOpens ? FCursorReply::Cursor(EMouseCursor::Hand) : FCursorReply::Unhandled();
			}

		private:
			ESpot Kind = ESpot::Seal;
			int32 Slot = 0;
		};

		// The sealed letter at a capital: EdgeSeals::Look's seal at 0.6, in brass, with the kit's letter (SLineIcon kind 3).
		const FLinearColor SealWax = FLinearColor(FColor::FromHex(TEXT("B08D57")));
		const FLinearColor SealShade = FLinearColor(FColor(120, 92, 52));
		const FLinearColor SealRibbon = FLinearColor(FColor(214, 201, 168));
		const FLinearColor SealRibbonFold = FLinearColor(FColor(150, 132, 96));

		/** The letter's strokes in the icon's unit square (CampaignHUD's SLineIcon, kind 3), and its small seal. */
		constexpr double Letter[][4] = {
			{ 0.13, 0.27, 0.87, 0.27 }, { 0.87, 0.27, 0.87, 0.75 }, { 0.87, 0.75, 0.13, 0.75 }, { 0.13, 0.75, 0.13, 0.27 },
			{ 0.13, 0.27, 0.5, 0.55 }, { 0.5, 0.55, 0.87, 0.27 }, { 0.13, 0.75, 0.4, 0.5 }, { 0.87, 0.75, 0.6, 0.5 },
		};
		constexpr double LetterSize = 22.0; // the unit square, in the seal's units: the letter about 16 by 11, as the edge seals' signs are
		constexpr double LetterMidY = 0.51;

		void PaintSeal(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2D& Centre, double Px, double PxPerUnit, bool bOpens, bool bOpen, bool bHot)
		{
			namespace Look = EdgeSeals::Look;
			const double Scale = Bargains::SealScale * Px * (bHot ? Look::HoverGrow : 1.0);
			const FGeometry Face = Geometry.MakeChild(FVector2D(Look::BoxW, Look::BoxH), FSlateLayoutTransform(static_cast<float>(Scale), Centre - FVector2D(Look::MidX, Look::MidY) * Scale));
			const FPen Pen{ Out, Face };
			const float Shown = bOpens ? 1.0f : 0.7f; // (a seal that opens no card is quieter)
			const double MX = Look::MidX, MY = Look::MidY;
			for (const Look::FTail& Tail : Look::Tails)
			{
				Pen.Bar(Layer, Tail.X, Tail.Y, Tail.Long, Tail.Thick, Tail.Degrees, SealRibbon.CopyWithNewOpacity(Shown));
				Pen.Bar(Layer + 1, Tail.X, Tail.Y, Tail.Long - 2.0, 1.1, Tail.Degrees, SealRibbonFold.CopyWithNewOpacity(0.7f * Shown));
			}
			Pen.Disc(Layer + 2, MX + 1.3, MY + 2.0, Look::DiscR, FLinearColor(0.0f, 0.0f, 0.0f, 0.35f * Shown));
			for (const Look::FRun& Run : Look::Runs)
			{
				const double Turn = FMath::DegreesToRadians(Run.Degrees);
				Pen.Disc(Layer + 3, MX + Run.Out * std::cos(Turn), MY + Run.Out * std::sin(Turn), Run.Radius, SealShade.CopyWithNewOpacity(Shown));
			}
			Pen.Disc(Layer + 4, MX, MY, Look::DiscR, SealShade.CopyWithNewOpacity(Shown));
			Pen.Disc(Layer + 5, MX + Look::LightX, MY + Look::LightY, Look::DiscR - 1.2, SealWax.CopyWithNewOpacity(Shown));
			const double UnitPx = PxPerUnit * Scale; // (the screen's pixels a unit of the seal)
			Pen.Ring(Layer + 6, MX, MY, Look::DiscR - 0.6, Look::Stroke(Look::RimW, UnitPx), InkOf(Bargains::Kit::Brass300, Shown));
			// The letter pressed into the wax, in desk-900, no stroke under 1.5 px.
			const FLinearColor Mark = InkOf(Bargains::Kit::Desk900, 0.85f * Shown);
			const double Thick = Look::Stroke(1.6, UnitPx);
			auto At = [MX, MY](double U, double V) { return FVector2D(MX + (U - 0.5) * LetterSize, MY + (V - LetterMidY) * LetterSize); };
			for (const auto& Cut : Letter)
			{
				Pen.Stroke(Layer + 7, At(Cut[0], Cut[1]), At(Cut[2], Cut[3]), Thick, Mark);
			}
			const FVector2D Wafer = At(0.5, 0.56);
			Pen.Ring(Layer + 7, Wafer.X, Wafer.Y, 0.06 * LetterSize, Thick * 0.8, Mark);
			// Its card open, or the pointer on it: the brass-300 ring round it.
			if (bOpen || bHot)
			{
				Pen.Ring(Layer + 8, MX, MY, Look::DiscR + 2.2, Look::Stroke(1.5, UnitPx), InkOf(Bargains::Kit::Brass300));
			}
		}

		/** Everything the layer paints over the map: the crates, the cut rings, the tags, our own plates, the seals and the card's pin. */
		class SBargainPaint : public SLeafWidget
		{
		public:
			SLATE_BEGIN_ARGS(SBargainPaint) {}
			SLATE_END_ARGS()

			void Construct(const FArguments&)
			{
				SetVisibility(EVisibility::HitTestInvisible);
			}

			virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(8.0, 8.0); }

			virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override
			{
				THEATER_SPEED_SCOPE(PaintBargains);
				const FState& S = State();
				if (!S.Book.bOn)
				{
					return LayerId;
				}
				const double Px = LayPx(S);
				int32 Layer = LayerId;
				const FPen Pen{ OutDrawElements, Geometry };
				// The crates on their lanes, and the brass rings where lanes were cut.
				for (const FLaneLaid& Lane : S.Lanes)
				{
					if (Lane.bCrate)
					{
						Layer = PaintDraws(OutDrawElements, Layer, Geometry, Lane.Crate, Lane.CrateAt);
					}
					if (Lane.bRing)
					{
						const Bargains::FDraw Ring = Bargains::CutRing(Bargains::FPt{ Lane.RingAt.X, Lane.RingAt.Y }, Px);
						Pen.Ring(Layer, Lane.RingAt.X, Lane.RingAt.Y, Ring.At.W * 0.5, Ring.EdgeW, InkOf(Ring.Edge));
						++Layer;
					}
				}
				// The "to Britain" tags where the lanes leave the screen.
				const FLinearColor TagInk = InkOf(Bargains::Kit::Paper200);
				for (const FLaneLaid& Lane : S.Lanes)
				{
					if (Lane.bTag && !Lane.Tag.IsEmpty())
					{
						FSlateDrawElement::MakeText(OutDrawElements, Layer, Geometry.ToPaintGeometry(Lane.TagSize, FSlateLayoutTransform(Lane.TagAt)), Lane.Tag, Lane.TagFont, ESlateDrawEffect::None, TagInk);
					}
				}
				++Layer;
				// Our own plate at a sender's capital the town plates did not draw this frame.
				for (const FPlateLaid& Plate : S.Plates)
				{
					if (Plate.bOwn && Plate.DrawnFrame != GFrameCounter)
					{
						Layer = PaintDraws(OutDrawElements, Layer, Geometry, Plate.OwnDraws, Plate.OwnAt);
					}
				}
				// The seals at the sellers' capitals.
				for (int32 Slot = 0; Slot < SealSlots; ++Slot)
				{
					if (!S.Seals[Slot].bShown || S.Seals[Slot].bOnCard || static_cast<size_t>(Slot) >= S.Look.Seals.size())
					{
						continue; // (the seal on the card is painted by the card, over its paper)
					}
					const Bargains::FSealLook& Seal = S.Look.Seals[static_cast<size_t>(Slot)];
					PaintSeal(OutDrawElements, Layer, Geometry, S.Seals[Slot].At, Px, S.PxPerUnit, Seal.bOpens, Seal.bOpen, S.HotSeal == Slot);
					Layer += 9;
				}
				// The card's pin, from its edge to the seller's capital.
				if (CardShown() && S.Placed.bPin)
				{
					Pen.Stroke(Layer, FVector2D(S.Placed.PinFrom.X, S.Placed.PinFrom.Y), FVector2D(S.Placed.PinTo.X, S.Placed.PinTo.Y), Bargains::PinW * Px, InkOf(Bargains::Kit::Brass300));
					++Layer;
				}
				return Layer;
			}
		};

		/** The layer: lays everything again each frame as the eye moves and the frame folds and unfolds. */
		class SBargainLayer : public SCompoundWidget
		{
		public:
			SLATE_BEGIN_ARGS(SBargainLayer) {}
			SLATE_END_ARGS()

			void Construct(const FArguments&, const TWeakObjectPtr<ACampaignPlayerController>& InPlayer, TFunction<void(TArray<FSlateRect>&)> InKeepOffOf, TFunction<void(TArray<FBox2D>&)> InPlatesOf)
			{
				Player = InPlayer;
				KeepOffOf = MoveTemp(InKeepOffOf);
				PlatesOf = MoveTemp(InPlatesOf);
				const TWeakObjectPtr<ACampaignPlayerController> Watched = InPlayer;
				SetVisibility(TAttribute<EVisibility>::CreateLambda([Watched]()
				{
					return State().Book.bOn && MapClear(Watched.Get()) ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
				}));
				TSharedRef<SOverlay> Marks = SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible);
				Marks->AddSlot()[SNew(SBargainPaint)];
				auto AddSpots = [&Marks](ESpot Kind, int32 Count)
				{
					for (int32 Slot = 0; Slot < Count; ++Slot)
					{
						Marks->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Top)
						.Padding(TAttribute<FMargin>::CreateLambda([Kind, Slot]()
						{
							FVector2D At(0.0, 0.0), Size(0.0, 0.0);
							SpotRect(Kind, Slot, At, Size);
							return FMargin(static_cast<float>(At.X), static_cast<float>(At.Y), 0.0f, 0.0f);
						}))
						[
							SNew(SBargainSpot, Kind, Slot)
						];
					}
				};
				AddSpots(ESpot::Crate, LaneSlots);
				AddSpots(ESpot::Ring, LaneSlots);
				AddSpots(ESpot::Seal, SealSlots);
				ChildSlot
				[
					Marks
				];
			}

			virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
			{
				SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
				THEATER_SPEED_SCOPE(BargainLayer);
				FState& S = State();
				ACampaignPlayerController* Shown = Player.Get();
				ACampaignMapActor* Map = Shown ? Shown->GetMap() : nullptr;
				if (!S.Book.bOn || !Map || !MapClear(Shown) || S.Player.Get() != Shown)
				{
					S.HotSeal = INDEX_NONE;
					return;
				}
				const FVector2D Local = AllottedGeometry.GetLocalSize();
				// (the layer's last paint, in the window's measure: what the keep-off boxes are read in, below. None before its
				// first paint, and then nothing is laid: the marks wait a frame)
				const FGeometry& Painted = GetPaintSpaceGeometry();
				if (Local.X < 1.0 || Local.Y < 1.0 || Painted.GetLocalSize().X < 1.0 || Painted.GetLocalSize().Y < 1.0)
				{
					return;
				}
				// (the map's places come to the screen in the viewport's pixels; the layer is laid in its own units)
				FVector2D View(0.0, 0.0);
				if (GEngine && GEngine->GameViewport)
				{
					GEngine->GameViewport->GetViewportSize(View);
				}
				const FVector2D PerUnit = View.X > 1.0 && View.Y > 1.0 ? FVector2D(View.X / Local.X, View.Y / Local.Y) : FVector2D(1.0, 1.0);
				S.PxPerUnit = PerUnit.Y;
				S.Px = Local.Y / 900.0; // (layer units a pixel of the 1440 x 900 picture)
				const double Px = S.Px;
				const double Now = FPlatformTime::Seconds();

				// The eye: from far off the quiet seals and the plate lines go; on each 8.5% of height the ribbons are laid again.
				S.Eye = EyeOf(Shown);
				const bool bFar = S.Eye >= Bargains::EyeMost;
				if (bFar != S.bFar)
				{
					S.bFar = bFar;
					Relook(S, false); // (the quiet seals and the plate lines come or go; the lanes stand, and the zoom's re-lay follows)
				}
				else if (Bargains::Relay(S.LaidAt, S.Eye))
				{
					LayMeshes(S, Map, S.Eye);
				}
				if (FMath::Abs(Px - S.LaidPx) > 1e-4)
				{
					LayWords(S);
				}
				if (S.bRunOwed)
				{
					S.bRunOwed = false;
					S.RunAt = Now; // (the month's crates set off the first time the map is seen after it opens)
				}

				// What the marks keep off: the frame's pieces and the panels over the map (but the card itself), and the
				// town plates drawn. The boxes come in the window's (paint) measure, as each was last painted (FrameRects,
				// EdgeNewsKeepOff), and are brought into the layer's units by the layer's own last paint, which is in that
				// measure too. Not by AllottedGeometry: Tick is handed the desktop's measure (SWidget::Paint adds the window's
				// place on the desktop before it ticks), so in any window off the desktop's corner every box would move up
				// and to the left by the window's place (the edge layer's fix, 1a22b3ed). The last paint is a frame old; the
				// layer fills the screen and stands still, so the lag is harmless.
				S.Frame.clear();
				if (KeepOffOf)
				{
					KeepOffOf(S.KeepOff);
					for (const FSlateRect& Piece : S.KeepOff)
					{
						const FVector2D Min = Painted.AbsoluteToLocal(FVector2D(Piece.Left, Piece.Top));
						const FVector2D Max = Painted.AbsoluteToLocal(FVector2D(Piece.Right, Piece.Bottom));
						const Bargains::FRect Rect{ Min.X, Min.Y, Max.X - Min.X, Max.Y - Min.Y };
						auto Same = [&Rect](const Bargains::FRect& Card) { return FMath::Abs(Rect.X - Card.X) < 2.0 && FMath::Abs(Rect.Y - Card.Y) < 2.0 && FMath::Abs(Rect.W - Card.W) < 2.0 && FMath::Abs(Rect.H - Card.H) < 2.0; };
						if (S.bPlaced && (Same(S.Placed.Card) || Same(S.WasCard)))
						{
							continue; // (the card is a piece of the frame too: it keeps off neither where it stands nor where it stood a frame ago)
						}
						S.Frame.push_back(Rect);
					}
				}
				S.TownPlates.clear();
				if (PlatesOf)
				{
					PlatesOf(S.PlateBoxes);
					for (const FBox2D& Plate : S.PlateBoxes)
					{
						S.TownPlates.push_back(Bargains::FRect{ Plate.Min.X, Plate.Min.Y, Plate.Max.X - Plate.Min.X, Plate.Max.Y - Plate.Min.Y });
					}
				}
				const FCampaignMapData& Data = Map->GetData();
				auto Project = [Shown, Map, PerUnit](double X, double Y, FVector2D& Out)
				{
					FVector2D Pixel(0.0, 0.0);
					if (!Shown->ProjectWorldLocationToScreen(Map->SurfacePoint(X, Y), Pixel))
					{
						return false; // (behind the eye)
					}
					Out = Pixel / PerUnit;
					return true;
				};
				auto CapitalAt = [&Data, &Project](int32 Region, FVector2D& Out)
				{
					return Data.Regions.IsValidIndex(Region) && Project(Data.Regions[Region].Center.X, Data.Regions[Region].Center.Y, Out);
				};
				const Bargains::FRect Screen{ 0.0, 0.0, Local.X, Local.Y };

				// The card: placed when it opens, again when its capital has moved 24 px or the screen changed size, and again
				// when the frame has moved so that it no longer keeps 16 px off the screen's edges and every panel, or while it
				// keeps off nowhere (call 5: left of the capital where it keeps off, else right; S.Frame is the keep-off, read in
				// paint space above; placing allocates nothing). Before the seals: the open card's seal stands on it.
				const Bargains::FRow* Open = S.bCardLaid ? S.Book.Row(S.Open.Row()) : nullptr;
				FVector2D OpenCapital(0.0, 0.0);
				if (Open && CapitalAt(Open->Capital, OpenCapital))
				{
					Bargains::FPlaceIn In;
					In.Capital = Bargains::FPt{ OpenCapital.X, OpenCapital.Y };
					In.Size = S.Card.Size;
					In.Screen = Screen;
					In.Frame = &S.Frame;
					In.Px = Px;
					if (!S.bPlaced || Bargains::NeedsPlace(S.Placed, In.Capital, Screen, Px) || !S.Placed.bFits || Bargains::KeepOffIntrusion(S.Placed.Card, In) > 0.0)
					{
						S.WasCard = S.bPlaced ? S.Placed.Card : Bargains::FRect{};
						S.Placed = Bargains::PlaceCard(In);
						S.bPlaced = true;
					}
				}
				else if (!S.bCardLaid)
				{
					S.bPlaced = false;
				}

				// Our own plates at the senders' capitals the town plates left without one (none from far off).
				for (FPlateLaid& Plate : S.Plates)
				{
					FVector2D At(0.0, 0.0);
					Plate.bOwn = false;
					if (Plate.DrawnFrame + 1 >= GFrameCounter || Plate.OwnDraws.Num() == 0 || !CapitalAt(Plate.Capital, At))
					{
						continue;
					}
					const Bargains::FRect Box = Plate.Own.Plate.Moved(At.X, At.Y);
					if (Bargains::OwnPlateShown(S.Eye, Box, S.TownPlates, S.Frame))
					{
						Plate.bOwn = true;
						Plate.OwnAt = At;
						S.TownPlates.push_back(Box);
					}
				}

				// The seals: each beside its capital, clear of the frame, the plates and one another.
				S.Avoid.clear();
				S.Avoid.insert(S.Avoid.end(), S.Frame.begin(), S.Frame.end());
				S.Avoid.insert(S.Avoid.end(), S.TownPlates.begin(), S.TownPlates.end());
				S.Laid.clear();
				// (call 6: while the card is shown, its seal leaves the capital, so that the lane's end there shows, and stands
				// on the card's top-right corner as its close; the card paints it and takes its presses, the spot its overhang)
				const bool bCardShown = CardShown();
				S.CardSeal = INDEX_NONE;
				for (int32 Slot = 0; Slot < SealSlots; ++Slot)
				{
					FSealLaid& Seal = S.Seals[Slot];
					Seal.bShown = false;
					Seal.bOnCard = false;
					if (static_cast<size_t>(Slot) >= S.Look.Seals.size())
					{
						continue;
					}
					if (Bargains::SealOnCard(S.Look.Seals[static_cast<size_t>(Slot)], bCardShown))
					{
						const Bargains::FPt Corner = Bargains::CardSealAt(S.Placed.Card);
						Seal.bShown = true;
						Seal.bOnCard = true;
						Seal.At = FVector2D(Corner.X, Corner.Y);
						S.CardSeal = Slot;
						continue;
					}
					const int32 Row = S.Look.Seals[static_cast<size_t>(Slot)].Row;
					FVector2D Capital(0.0, 0.0);
					if (Row < 0 || static_cast<size_t>(Row) >= S.Book.Rows.size() || !CapitalAt(S.Book.Rows[static_cast<size_t>(Row)].Capital, Capital))
					{
						continue;
					}
					Bargains::FPt At;
					if (Bargains::SealAt(Bargains::FPt{ Capital.X, Capital.Y }, Px, S.Frame, S.Avoid, S.Laid, At))
					{
						Seal.bShown = true;
						Seal.At = FVector2D(At.X, At.Y);
						S.Laid.push_back(At);
					}
				}
				if (S.HotSeal != INDEX_NONE && (S.HotSeal >= SealSlots || !S.Seals[S.HotSeal].bShown))
				{
					S.HotSeal = INDEX_NONE;
				}

				// The lanes' marks: the crate on its run, the cut's ring, the tag where the lane leaves the screen.
				const double Share = Bargains::CrateShare(Bargains::CrateRunT(Now - S.RunAt));
				for (size_t Each = 0; Each < S.Lanes.size() && Each < S.Look.Lanes.size(); ++Each)
				{
					FLaneLaid& Lane = S.Lanes[Each];
					const Bargains::FLaneLook& Look = S.Look.Lanes[Each];
					Lane.bCrate = false;
					Lane.bRing = false;
					Lane.bTag = false;
					if (Each >= static_cast<size_t>(LaneSlots) || Lane.Shape.Pieces.Num() == 0)
					{
						continue;
					}
					if (Look.bCrate && Lane.Crate.Num() > 0)
					{
						const Bargains::FPt On = Bargains::PointAt(Lane.Shape.Pieces, Share);
						Lane.bCrate = Project(On.X, On.Y, Lane.CrateAt) && Screen.Contains(Bargains::FPt{ Lane.CrateAt.X, Lane.CrateAt.Y });
					}
					if (Look.bCut && Lane.Shape.Gap.bOk)
					{
						Lane.bRing = Project(Lane.Shape.Gap.Mid.X, Lane.Shape.Gap.Mid.Y, Lane.RingAt) && Screen.Contains(Bargains::FPt{ Lane.RingAt.X, Lane.RingAt.Y });
					}
					if (!Lane.Tag.IsEmpty())
					{
						// (piece by piece: a lane across the world's seam is two, and the tag is laid on one of them, never across the jump)
						Lane.OnScreen.Clear();
						for (size_t Piece = 0; Piece < Lane.Shape.Raw.Num(); ++Piece)
						{
							Bargains::FPath& Seen = Lane.OnScreen.Add();
							for (const Bargains::FPt& Point : Lane.Shape.Raw[Piece])
							{
								FVector2D At(0.0, 0.0);
								if (Project(Point.X, Point.Y, At))
								{
									Seen.push_back(Bargains::FPt{ At.X, At.Y });
								}
							}
						}
						Bargains::FRect Tag;
						if (Bargains::ExitTagOfPieces(Lane.OnScreen, Screen, S.Frame, Bargains::FPt{ Lane.TagSize.X, Lane.TagSize.Y }, Px, Tag))
						{
							Lane.bTag = true;
							Lane.TagAt = FVector2D(Tag.X, Tag.Y);
						}
					}
				}
			}

		private:
			TWeakObjectPtr<ACampaignPlayerController> Player;
			TFunction<void(TArray<FSlateRect>&)> KeepOffOf;
			TFunction<void(TArray<FBox2D>&)> PlatesOf;
		};

		/** The one bid card: the look's draws on paper; a press on a live button gives the order, one on its seal closes it. */
		class SBargainCard : public SLeafWidget
		{
		public:
			SLATE_BEGIN_ARGS(SBargainCard) {}
			SLATE_END_ARGS()

			void Construct(const FArguments&)
			{
				SetVisibility(TAttribute<EVisibility>::CreateLambda([]() { return CardShown() ? EVisibility::Visible : EVisibility::Collapsed; }));
			}

			virtual FVector2D ComputeDesiredSize(float) const override
			{
				const FState& S = State();
				return S.bCardLaid ? FVector2D(S.Card.Size.X, S.Card.Size.Y) : FVector2D::ZeroVector;
			}

			virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override
			{
				THEATER_SPEED_SCOPE(PaintBargainCard);
				const FState& S = State();
				if (!S.bCardLaid)
				{
					return LayerId;
				}
				int32 Layer = PaintDraws(OutDrawElements, LayerId, Geometry, S.CardDraws, FVector2D::ZeroVector);
				// (the button under the pointer: a brass-300 edge round it)
				if (S.HotButton >= 0 && static_cast<size_t>(S.HotButton) < S.Card.Buttons.size())
				{
					const Bargains::FRect& At = S.Card.Buttons[static_cast<size_t>(S.HotButton)].At;
					const FLinearColor Lit = InkOf(Bargains::Kit::Brass300);
					const FLinearColor Hollow = Lit.CopyWithNewOpacity(Bargains::Kit::Hollow);
					const FSlateRoundedBoxBrush Round(Hollow, static_cast<float>(Bargains::Card::KitRadius * LayPx(S)), Lit, 1.0f);
					FSlateDrawElement::MakeBox(OutDrawElements, Layer, Geometry.ToPaintGeometry(FVector2D(At.W, At.H), FSlateLayoutTransform(FVector2D(At.X, At.Y))), &Round, ESlateDrawEffect::None, Hollow);
					++Layer;
				}
				// (call 6: the open card's seal, its disc's middle on the card's top-right corner, over the paper: the card's close)
				if (const Bargains::FSealLook* Seal = CardSealLook())
				{
					PaintSeal(OutDrawElements, Layer, Geometry, FVector2D(S.Card.Size.X, 0.0), LayPx(S), S.PxPerUnit, Seal->bOpens, Seal->bOpen, S.HotSeal == S.CardSeal);
					Layer += 9;
				}
				return Layer;
			}

			virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
			{
				// (the card is paper over the map: no press on it reaches the map)
				if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
				{
					if (OnSeal(MyGeometry, MouseEvent))
					{
						PressSeal(State().CardSeal); // (its own seal closes the card, as at the capital)
						return FReply::Handled();
					}
					const int32 Button = ButtonAt(MyGeometry, MouseEvent);
					if (Button != INDEX_NONE)
					{
						const Bargains::FButton Pressed = State().Card.Buttons[static_cast<size_t>(Button)];
						Act(Pressed);
					}
				}
				return FReply::Handled();
			}

			virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
			{
				FState& S = State();
				S.HotButton = ButtonAt(MyGeometry, MouseEvent);
				if (OnSeal(MyGeometry, MouseEvent))
				{
					S.HotSeal = S.CardSeal;
				}
				else if (S.HotSeal != INDEX_NONE && S.HotSeal == S.CardSeal)
				{
					S.HotSeal = INDEX_NONE;
				}
				return FReply::Unhandled();
			}

			virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override
			{
				SLeafWidget::OnMouseLeave(MouseEvent);
				FState& S = State();
				S.HotButton = INDEX_NONE;
				if (S.HotSeal != INDEX_NONE && S.HotSeal == S.CardSeal)
				{
					S.HotSeal = INDEX_NONE; // (on its overhang, past the card's edge, the seal's spot takes the pointer again)
				}
			}

			virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override
			{
				const FState& S = State();
				const bool bHand = S.HotButton != INDEX_NONE || (S.CardSeal != INDEX_NONE && S.HotSeal == S.CardSeal);
				return bHand ? FCursorReply::Cursor(EMouseCursor::Hand) : FCursorReply::Cursor(EMouseCursor::Default);
			}

		private:
			/** The look of the seal on the card this frame, or nullptr. */
			static const Bargains::FSealLook* CardSealLook()
			{
				const FState& S = State();
				if (S.CardSeal < 0 || S.CardSeal >= SealSlots || !S.Seals[S.CardSeal].bOnCard || static_cast<size_t>(S.CardSeal) >= S.Look.Seals.size())
				{
					return nullptr;
				}
				return &S.Look.Seals[static_cast<size_t>(S.CardSeal)];
			}

			/** True where the pointer is on the card's seal (the part over the paper; its overhang is the seal's own spot). */
			static bool OnSeal(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
			{
				const FState& S = State();
				if (!S.bCardLaid || !CardSealLook())
				{
					return false;
				}
				const FVector2D At = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
				return Bargains::OnCardSeal(Bargains::FPt{ At.X, At.Y }, S.Card.Size, LayPx(S));
			}

			/** The live button under the pointer, or INDEX_NONE. */
			static int32 ButtonAt(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
			{
				const FState& S = State();
				if (!S.bCardLaid)
				{
					return INDEX_NONE;
				}
				const FVector2D At = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
				for (size_t Each = 0; Each < S.Card.Buttons.size(); ++Each)
				{
					const Bargains::FButton& Button = S.Card.Buttons[Each];
					if (Button.bLive && Button.At.Contains(Bargains::FPt{ At.X, At.Y }))
					{
						return static_cast<int32>(Each);
					}
				}
				return INDEX_NONE;
			}
		};
	}

	const Bargains::FBook& Book()
	{
		return Impl::State().Book;
	}

	bool IsOn()
	{
		return Impl::State().Book.bOn;
	}

	void ShowOnMap(ACampaignPlayerController* Player)
	{
		THEATER_SPEED_SCOPE(BargainShow);
		Impl::FState& S = Impl::State();
		if (S.Player.Get() != Player)
		{
			Impl::Clear(S);
			S.Json.clear();
			S.Turn = -1;
			S.Player = Player;
		}
		FCampaignSession* Session = Player ? Player->GetSession() : nullptr;
		if (Session && Session->IsAnswering())
		{
			// (an order of the player's is still being answered on the rules' thread: asking now would wait for it. Its
			// answer shows the war again, and the book is read then)
			return;
		}
		FString Json;
		if (!Session || !Session->BargainsLook(Json) || Json.IsEmpty())
		{
			Json = TEXT("{\"on\":false}"); // (no rules, or rules without the call: nothing of it is shown)
		}
		const FTCHARToUTF8 Bytes(*Json);
		if (S.Json.size() == static_cast<size_t>(Bytes.Length()) && FMemory::Memcmp(S.Json.data(), Bytes.Get(), S.Json.size()) == 0)
		{
			// (the same picture of the bargains: nothing to lay again, but the tray's legions follow the player's picture of
			// the corps, which a new view can change within the month)
			if (S.Book.bOn)
			{
				Impl::Retray(S);
			}
			return;
		}
		S.Json.assign(Bytes.Get(), static_cast<size_t>(Bytes.Length()));
		std::string Error;
		if (!Bargains::ReadBook(S.Json, S.Book, &Error))
		{
			static bool bSaid = false;
			if (!bSaid)
			{
				bSaid = true;
				UE_LOG(LogTheater, Warning, TEXT("Bargains: the rules' look could not be read (%s)"), *Impl::FromUtf8(Error));
			}
		}
		if (!S.Book.bOn)
		{
			Impl::Clear(S);
			return;
		}
		S.Open.Update(S.Book);
		if (S.Book.Turn != S.Turn)
		{
			// (a new month: its crates run once, the first time the map is seen; a game just loaded shows them standing)
			S.bRunOwed = S.Turn >= 0;
			S.RunAt = -1000.0;
			S.Turn = S.Book.Turn;
		}
		S.Eye = Impl::EyeOf(Player);
		S.bFar = S.Eye >= Bargains::EyeMost;
		Impl::Relook(S);
	}

	TSharedRef<SWidget> MakeBargainLayer(const TWeakObjectPtr<ACampaignPlayerController>& Player, TFunction<void(TArray<FSlateRect>&)> KeepOffOf, TFunction<void(TArray<FBox2D>&)> PlatesOf)
	{
		Impl::FState& S = Impl::State();
		S.KeepOff.Reserve(32);
		S.PlateBoxes.Reserve(64);
		S.Frame.reserve(32);
		S.TownPlates.reserve(64);
		S.Avoid.reserve(96);
		S.Laid.reserve(Impl::SealSlots);
		return SNew(Impl::SBargainLayer, Player, MoveTemp(KeepOffOf), MoveTemp(PlatesOf));
	}

	TSharedRef<SWidget> MakeBargainCard(const TWeakObjectPtr<ACampaignPlayerController>& Player)
	{
		return SNew(Impl::SBargainCard);
	}

	FMargin CardPadding()
	{
		const Impl::FState& S = Impl::State();
		return S.bPlaced ? FMargin(static_cast<float>(S.Placed.Card.X), static_cast<float>(S.Placed.Card.Y), 0.0f, 0.0f) : FMargin(0.0f);
	}

	FVector2D PlateLineSize(int32 Region, bool bClose)
	{
		const Impl::FPlateLaid* Plate = bClose ? Impl::PlateOf(Region) : nullptr;
		return Plate && Plate->Line.Num() > 0 ? FVector2D(Plate->Lay.Size.X, Plate->Lay.Size.Y) : FVector2D::ZeroVector;
	}

	void DrawPlateLine(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, int32 Region, const FVector2D& TopLeft)
	{
		Impl::FPlateLaid* Plate = Impl::PlateOf(Region);
		if (!Plate || Plate->Line.Num() == 0)
		{
			return;
		}
		Plate->DrawnFrame = GFrameCounter; // (the town's own plate carries the line: the layer draws no plate of its own there)
		Impl::PaintDraws(Out, Layer, Geometry, Plate->Line, TopLeft, true);
	}

	TSharedRef<SWidget> TrayFlag(int32 CorpsId)
	{
		return SNew(SBox).Padding(FMargin(0.0f, 0.0f, 6.0f, 0.0f)).VAlign(VAlign_Center)
			.Visibility_Lambda([CorpsId]() { return Impl::TrayOf(CorpsId) ? EVisibility::Visible : EVisibility::Collapsed; })
			.ToolTipText_Lambda([CorpsId]() { const Impl::FTrayLaid* Tray = Impl::TrayOf(CorpsId); return Tray ? Tray->Tip : FText::GetEmpty(); })
			[
				SNew(SBox).WidthOverride(static_cast<float>(Bargains::TrayFlagW)).HeightOverride(static_cast<float>(Bargains::TrayFlagH))
				[
					SNew(SImage).Image_Lambda([CorpsId]() -> const FSlateBrush* { const Impl::FTrayLaid* Tray = Impl::TrayOf(CorpsId); return Tray ? Impl::FlagBrushOf(Tray->Flag) : nullptr; })
				]
			];
	}

	TSharedRef<SWidget> TrayLine(int32 CorpsId)
	{
		// (under the card's kind, grade and moves line, and spaced as it is)
		return SNew(SBox).Padding(FMargin(0.0f, 0.0f, 4.0f, 5.0f))
			.Visibility_Lambda([CorpsId]() { return Impl::TrayOf(CorpsId) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			[
				SNew(STextBlock)
				.Text_Lambda([CorpsId]() { const Impl::FTrayLaid* Tray = Impl::TrayOf(CorpsId); return Tray ? Tray->Line : FText::GetEmpty(); })
				.Font(Impl::FontOf(Bargains::TrayLineFace(1.0)))
				.ColorAndOpacity(Impl::InkOf(Bargains::Kit::Brass300))
			];
	}

	void HangPennant(ACampaignMapActor* Map, UProceduralMeshComponent* Cloth, const TArray<FViewCorps>& Corps, double PoleX, double NearY, double TopZ, double FlagWidth, double U)
	{
		if (!Map || !Cloth)
		{
			return;
		}
		FString Home;
		TArray<FString> Allies;
		for (const FViewCorps& Unit : Corps)
		{
			if (Home.IsEmpty())
			{
				Home = Unit.Vol;
			}
			if (!Unit.Ally.IsEmpty())
			{
				Allies.AddUnique(Unit.Ally);
			}
		}
		if (Home.IsEmpty())
		{
			return; // (no volunteer legion marches with this army: always so in release 1)
		}
		UMaterialInterface* Plain = Map->GetFlagMaterial(Impl::FromUtf8(Bargains::FlagKey(Impl::ToUtf8(Home))));
		if (!Plain)
		{
			return;
		}
		// (a triangle, as the picture's: the hoist on the pole and the point out along the cloth, Spain's bands running
		// together to it; in the first slot under the ally pennants. Its shape is Bargains::PennantShape's, tested)
		const Bargains::FPennantShape Shape = Bargains::PennantShape(NearY, TopZ, FlagWidth, Allies.Num(), U);
		TArray<FVector> Corners;
		TArray<FVector2D> Cover;
		for (int32 Each = 0; Each < 3; ++Each)
		{
			Corners.Add(FVector(PoleX, Shape.At[Each].X, Shape.At[Each].Y));
			Cover.Add(FVector2D(Shape.Uv[Each].X, Shape.Uv[Each].Y));
		}
		TArray<FVector> Facing;
		Facing.Init(FVector(-1.0, 0.0, 0.0), 3);
		TArray<FProcMeshTangent> Lie;
		Lie.Init(FProcMeshTangent(FVector(0.0, 1.0, 0.0), false), 3);
		Cloth->CreateMeshSection_LinearColor(4, Corners, { 0, 1, 2 }, Facing, Cover, TArray<FLinearColor>(), Lie, false);
		Cloth->SetMaterial(4, Plain);
	}
}
