#pragma once

#include "CoreMinimal.h"

/**
 * (Brian, 16 November 2026: "make generic bombers and other units for other countries that don't have unique ones
 * like those listed". The design director's rulings 78 and 79.) The one table the game draws its models from: the art
 * director's, Content/Data/Campaign/unit-table.json. A country shows its own model where it has one, a shared one
 * where the record is plain, and otherwise the generic of that kind in its own paint with its own marking: no
 * country shows another country's machine or soldier. A tag that is not in the table takes the table's default.
 * Where the file cannot be read every answer is "not known", and the game draws as it did before.
 */
namespace UnitTable
{
	/** True once the table has been read and holds something. */
	bool IsLoaded();
	/** The head a country's soldiers wear: "stahl", "brodie", "adrian", "soviet", "pot", "round", "cap", "dome", "peaked", "sun" or "fieldcap". Empty if not known. */
	FString Head(const FString& Tag);
	/** The cloth of its soldiers, the paint of its vehicles and aircraft, the grey of its ships. False if not known. */
	bool Cloth(const FString& Tag, FLinearColor& Out);
	bool Paint(const FString& Tag, FLinearColor& Out);
	bool Hull(const FString& Tag, FLinearColor& Out);
	/** For a country with no marking drawn: the ring of its flag's colours, outer, middle and centre. Empty where its marking is drawn, or not known. */
	TArray<FLinearColor> Ring(const FString& Tag);
	/**
	 * (Brian, 18 November 2026: "have the country logo show on the tanks and planes just like the major nations".)
	 * One layer of a country's own mark, in a square from -1 to 1 with Up toward the top of the mark and Across along
	 * it: a disc (or a ring, where Inner is above nothing), a triangle with its point turned Turn degrees from
	 * straight up, a star of Points points, or a bar.
	 */
	struct FInsigniaLayer
	{
		enum class EShape : uint8 { Ring, Triangle, Star, Bar };
		EShape Shape = EShape::Ring;
		FLinearColor Colour = FLinearColor::White;
		double Inner = 0.0, Outer = 1.0; // a ring's two radii; a star's inner radius as a share of its outer
		double Turn = 0.0;
		int32 Points = 5;
		double Across0 = 0.0, Across1 = 0.0, Up0 = 0.0, Up1 = 0.0; // a bar
	};
	/**
	 * The country's own mark from the art director's table, bottom layer first. Empty where the game draws its mark by
	 * hand, or it has none. Where the table dates a mark (the Commonwealth's wartime roundels, with the red taken out
	 * in the Pacific and in Burma), this is the one that stands in the campaign's month (with one look, the last of
	 * them in every month: see OneLook).
	 */
	TArray<FInsigniaLayer> Insignia(const FString& Tag);
	/**
	 * The stripes a country's aircraft carry on the rudder, in the campaign's month: side by side from the leading
	 * edge (bOutUpright) or one over another from the top. A dominion's are its own once its dated roundel stands
	 * (white and blue with the red taken out, and so on); before that it has none here and wears the British flash
	 * its machines carry. False where the country has none.
	 */
	bool Fin(const FString& Tag, bool& bOutUpright, TArray<FLinearColor>& OutBands);
	/**
	 * Models seen close, on the parade and in a battle, wear a country's own layered mark even where the map keeps the
	 * simpler one the game draws by hand: Sweden's three crowns, which are specks at the map's size (the art
	 * director, 19 November 2026). Whoever sets it puts it back; a model painted so is kept apart.
	 */
	void SetCloseMarks(bool bOn);
	bool CloseMarks();
	/** The campaign's month, told by the map whenever it shows a month (turn 1 is September 1939). */
	void SetTurn(int32 Turn);
	/**
	 * The month the painter has, in months from the year 0: the year times 12 and the month counted from 0 (September
	 * 1939 is 1939 * 12 + 8). An aircraft's paint by date (its model's "schemes") is the one that stands in it.
	 */
	int32 Month();
	/**
	 * An air battle tells the painter its own month (its year, and its month of the year from 1): its aircraft wear
	 * the paint of that month. "-TheaterSeason=" still holds the table at one month of the war, for a picture.
	 */
	void SetMonth(int32 Year, int32 MonthOfYear);
	/**
	 * Which of the country's dated marks stands in the campaign's month: 0 for none, else one more than its place in
	 * the table's list. A painted model is kept under this as well as under its country, so that a model painted
	 * before the mark changed is not handed out after.
	 */
	int32 MarkEra(const FString& Tag);
	/**
	 * (Brian, 26 November 2026: "have the paint be on it regardless of the year".) One look for the whole war. With
	 * tc.Map.OneLook (or "-TheaterOneLook") nothing the table paints changes with the date: a country whose mark the
	 * table dates wears the last of its dated marks from the first month to the last, and its own rudder stripes with
	 * it (MarkEra then answers that one in every month); a machine that comes into service later wears the coat of
	 * the one it follows; and a mark that came in with a date is drawn as the one it followed. The table's "one_look"
	 * says which coats and which marks. Off as it comes: the looks change by the month, as before.
	 */
	bool OneLook();
	/**
	 * With one look on, the coat a sculpted model wears whatever its own file says: the colours laid over the file's,
	 * by the file's numbers for its paints, and the ink of its side number where bOutInk says the table gives one.
	 * False, with nothing given, where one look is off or the table says nothing of the model.
	 */
	bool OneLookCoat(const FString& Model, TMap<int32, FLinearColor>& OutPaints, FLinearColor& OutInk, bool& bOutInk);
	/** With one look on, the mark that is drawn in place of this one, where the table names one; else the mark itself. */
	FString OneLookMark(const FString& Mark);
	/** (Brian, the same day: "make the camoflage reflect their terrian".) The second colour of its camouflage, for the patches. False where it has one coat. */
	bool Paint2(const FString& Tag, FLinearColor& Out);
	/**
	 * Whether the country whitewashes its vehicles on snow (the art director's "winter_wash": the Nordic and Baltic
	 * countries, Hungary, Slovakia, Romania, Mongolia, Manchukuo, Germany and the Soviet Union, by the record).
	 */
	bool WinterWash(const FString& Tag);
	/**
	 * The painter is told that what it paints next stands on snow and is to be washed white: a worn, streaky wash of
	 * about seven parts in ten over the coat of a vehicle or a gun. Whoever sets it puts it back. A washed model is
	 * kept apart from the unwashed one.
	 */
	void SetWash(bool bOn);
	bool Wash();
	/**
	 * Whose layout the country's battleship (its cruiser) takes: its own tag, another country's by the record (Vichy's
	 * battleship is France's, a dominion's cruiser Britain's), or nothing for the plain hull, which is no navy's own.
	 * A small nation's battleship, held only by a great power's captured or stolen plans, is the plain one until the
	 * rules say whose plans it holds. Where the table is not read, the tag itself.
	 */
	FString BattleshipOf(const FString& Tag);
	FString CruiserOf(const FString& Tag);
}
