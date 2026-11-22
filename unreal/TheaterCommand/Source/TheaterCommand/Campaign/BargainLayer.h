#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "Layout/SlateRect.h"
#include "Templates/Function.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Campaign/BargainFeed.h"

class ACampaignMapActor;
class ACampaignPlayerController;
class FSlateWindowElementList;
class SWidget;
class UProceduralMeshComponent;
struct FGeometry;
struct FViewCorps;

/**
 * (Release 2: a neutral's bargains and the volunteer legions on the campaign map. The art director's look of 22
 * November, "Drawn for the build (07:56 UTC)"; the build contract, production/r2-bargains-legions/contract.md, C and D.)
 *
 * The engine's side of them. What is drawn and where is Campaign/BargainLook's (plain C++, tested on its own); the
 * rules' answer is read by Campaign/BargainFeed. This file asks the session for the look once for each new picture of
 * the war (and after the player's own bid or sale), keeps the book, lays the lanes and the border tints as ribbons on the
 * table (meshes of its own, made on first use with the map as their outer), and paints the rest over the map in Slate:
 * the sealed-letter signs at the sellers' capitals, the cargo crates, the brass rings where a lane was cut, the "to
 * Britain" tags, a sender's capital plate where the town has none, and the one bid card open.
 *
 * Both rules are switched off in release 1 (stage.json 'bargains' and 'legions'). Then the look answers {"on":false},
 * the book is empty, no mesh is made, the layer and the card stay collapsed, the plate line takes no room and the tray
 * card's two pieces stay collapsed: the map draws exactly what it drew before.
 *
 * Every function here is called on the game thread. The hooks in the files of others are one line each, marked
 * "R2 bargains hook".
 */
namespace BargainLayer
{
	/** The book last read: empty and off until a war with either rule on. */
	const Bargains::FBook& Book();

	/** True while the book is on (a war with a neutral's bargains or volunteer legions). */
	bool IsOn();

	/**
	 * For each new picture of the war (ACampaignPlayerController::ShowWarRest, after the lanes): asks the rules' look
	 * (a question that changes nothing, kept with the others), reads it and lays the lanes and the tints on the table.
	 * With both rules off it asks once and draws nothing.
	 */
	void ShowOnMap(ACampaignPlayerController* Player);

	/**
	 * The layer over the map: under the edge seals (the HUD's slot after SSkyPlates, before the edge news layer), never
	 * hit itself but for its marks. KeepOffOf fills an array with what the seals and the card keep off (the frame's
	 * pieces and the panels over the map, absolute, as EdgeNewsKeepOff gives them); PlatesOf with the town plates drawn
	 * this frame, in the layer's own units (the HUD's DrawnTownPlates).
	 */
	TSharedRef<SWidget> MakeBargainLayer(const TWeakObjectPtr<ACampaignPlayerController>& Player, TFunction<void(TArray<FSlateRect>&)> KeepOffOf, TFunction<void(TArray<FBox2D>&)> PlatesOf);

	/** The one bid card: an opaque piece over the map (the HUD wraps it in Piece), collapsed while no card is open. */
	TSharedRef<SWidget> MakeBargainCard(const TWeakObjectPtr<ACampaignPlayerController>& Player);

	/** Where the card stands, as its overlay slot's padding (the slot is left and top aligned in the screen's root). */
	FMargin CardPadding();

	/**
	 * STownPlates: the room a capital's plate gives the sender's line under its name ("1 division abroad, Leningrad"),
	 * in the plate's own units; zero for any other town, from far off, and while the rules are off (bClose: the plate is a
	 * capital's and not seen from far off).
	 */
	FVector2D PlateLineSize(int32 Region, bool bClose);

	/** STownPlates: draws that line with its top left at TopLeft, and notes the capital's plate as drawn this frame. */
	void DrawPlateLine(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, int32 Region, const FVector2D& TopLeft);

	/** The tray card: the legion's home flag in its title row (21 x 14), collapsed for any other corps. */
	TSharedRef<SWidget> TrayFlag(int32 CorpsId);

	/** The tray card: the legion's Courier line ("1 DIV · 88% · TOUR 14 OF 24 MO"), collapsed for any other corps. */
	TSharedRef<SWidget> TrayLine(int32 CorpsId);

	/**
	 * AArmyPiece: the home's plain pennant on the standard's pole, under the ally pennants (Cloth's section 4), its
	 * width PennantShare of the flag's. Nothing where no corps of the army is a volunteer legion. TopZ is where the ally
	 * pennants begin (under the plate), NearY the hoist's side of the cloth, U the piece's unit.
	 */
	void HangPennant(ACampaignMapActor* Map, UProceduralMeshComponent* Cloth, const TArray<FViewCorps>& Corps, double PoleX, double NearY, double TopZ, double FlagWidth, double U);
}
