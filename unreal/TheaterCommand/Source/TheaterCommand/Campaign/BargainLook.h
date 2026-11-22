#pragma once

#include "Campaign/BargainFeed.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

/**
 * (Release 2: a neutral's bargains and the volunteer legions on the campaign map. The art director's look of 22
 * November, "Drawn for the build (07:56 UTC)", r2-bargains-legions-look.png and its source; the build contract,
 * production/r2-bargains-legions/contract.md, section C.)
 *
 * The pure look, with no engine in it: from the bridge's book (BargainFeed.h) what the map draws and where. Which rows
 * get a lane, a cargo crate, a border tint, a sealed-letter sign or a card; every word the pieces say; the bid card laid
 * out box by box; where the card stands beside the seller's capital; the lane's paper casing, its dashes, the cut's gap
 * with its ticks and ring, the crate's run along the lane; the tint's soft inner strip; the sender's capital plate line
 * and the tray's line. BargainLayer (Unreal) only turns these into Slate draws and mesh sections.
 *
 * Units. Sizes are pixels of the 1440 x 900 picture. On the screen a picture pixel is Px layer units (PxOf, about 1.08);
 * every rect, size and font size this file returns for the screen is already in layer units (picture px x Px). On the
 * map a picture pixel is Pixel map units (PixelOf: RebuildHighlight's pixel x Px x TickScale), and the lane and the
 * tint are laid in map units, again when the eye has risen or come down by 8.5% (Relay).
 *
 * Colours are sRGB 0xRRGGBB with an alpha (FInk). Slate and the mesh vertices take them through FLinearColor(FColor),
 * which applies the sRGB curve (LinearOf here does the same, for the tests): never the bytes over 255 as linear.
 * FSlateDrawElement::MakeBox fills with its TINT: a box's fill here is the real colour, to be passed as the tint, and a
 * hollow ring is a box whose fill has alpha 1/255 (Kit::Hollow) so that it is never culled.
 *
 * Nothing here allocates on the paths run every frame once their buffers have grown (PlaceCard, SealAt, ExitTag,
 * CrateShare and PointAt; LayLane and LayTint reuse their shapes' buffers): the test counts it.
 */
namespace Bargains
{
	// ---------------------------------------------------------------------------------------------------------------
	// Colours

	/** An sRGB colour (0xRRGGBB) and its opacity. */
	struct FInk
	{
		uint32_t Srgb = 0;
		float A = 1.0f;
	};

	/** The UI kit's colours (CampaignHUD.cpp's constants of the same names), and the card's own inks from the picture. */
	namespace Kit
	{
		inline constexpr uint32_t Paper200 = 0xE2D7BC;
		inline constexpr uint32_t Paper300 = 0xCDBF9C;
		inline constexpr uint32_t Ink900 = 0x2A2118;
		inline constexpr uint32_t Brass700 = 0x8A6B3B;
		inline constexpr uint32_t Brass300 = 0xE0B872;
		inline constexpr uint32_t Desk900 = 0x17120D;
		inline constexpr uint32_t Desk700 = 0x2D241A; // the kit's dark button (.tc-btn), ACCEPT
		inline constexpr uint32_t Paper100 = 0xEDE6D6; // its label
		inline constexpr uint32_t Ink600 = 0x5C5040; // the kit's quiet button on paper (.tc-paper .tc-btn--quiet): the seller's foot
		inline constexpr uint32_t HeldInk = 0x4A3F30; // the card's italic "held by" line
		inline constexpr uint32_t DimInk = 0x7A6A52; // an unseen figure's "?", and what cannot be pressed
		inline constexpr uint32_t HintInk = 0x5B4E3C; // the card's hint under the rows
		inline constexpr uint32_t Black = 0x000000;
		inline constexpr float Hollow = 1.0f / 255.0f; // the fill of a hollow ring: MakeBox culls a box whose tint is under 1e-4
		inline constexpr float PlateA = 0.88f; // a capital plate the layer draws itself: desk-900 at 88%
		/**
		 * The card's 1 px dark keyline, the game's other paper cards' (the art director's call 4, 22 November 13:50Z): the
		 * date card's paper, CampaignHUD.cpp's PaperBrush (FSlateRoundedBoxBrush(Paper, 5.0f, FLinearColor(0.05f,
		 * 0.035f, 0.02f, 0.9f), 1.0f), set in its Construct and drawn round the date card's SBorder), whose outline is that
		 * linear colour at 0.9, 1.0 wide. Here as the sRGB bytes FLinearColor(FColor) reads back nearest to it (63, 53,
		 * 39: each channel within half a byte's step of 0.05, 0.035 and 0.02), at the same 0.9. No shadow: the card
		 * stands off the map by its keyline alone, as the date card does.
		 */
		inline constexpr uint32_t Keyline = 0x3F3527;
		inline constexpr float KeylineA = 0.9f;
		/**
		 * The kit's --line brown, 4B3D2D: the "Others ?" row's blank flag (call 3) at 50%, a flag-shaped grey silhouette,
		 * no white fill and no outline, so that it never reads as a box to tick. The kit's desk ramp runs desk-950 to
		 * desk-600 (3D3125) and goes on with line and track (5D4C37); it has no desk-500, so the constant is named for the
		 * token it is (the UI director's mend 1).
		 */
		inline constexpr uint32_t Line = 0x4B3D2D;
		inline constexpr float OthersFlagA = 0.5f;
	}

	struct FLinear
	{
		float R = 0.0f;
		float G = 0.0f;
		float B = 0.0f;
		float A = 1.0f;
	};

	/** One sRGB byte as linear light, by the sRGB curve (as FLinearColor(FColor) reads it). */
	float LinearOfByte(uint8_t Byte);

	/** An sRGB colour as linear light, its alpha kept: what FLinearColor(FColor(R, G, B)) gives. */
	FLinear LinearOf(FInk Ink);

	// ---------------------------------------------------------------------------------------------------------------
	// Units

	/** Layer units per picture pixel: (ViewportPx.Y / 900) / (FrameScale x TickScale). */
	double PxOf(double ViewportHigh, double FrameScale, double TickScale);

	/** Map units per picture pixel: RebuildHighlight's pixel (0.00037 x CameraDistance / UnitScale) x Px x TickScale. */
	double PixelOf(double CameraDistance, double UnitScale, double Px, double TickScale);

	/** The lane and the tint are laid again when the eye has risen or come down by RelayStep since they were laid. */
	inline constexpr double RelayStep = 0.085;

	/** True (and LaidAt set to CameraDistance) when the shapes laid at LaidAt should be laid again for CameraDistance. */
	bool Relay(double& LaidAt, double CameraDistance);

	// ---------------------------------------------------------------------------------------------------------------
	// Plain geometry (screen boxes are in layer units, map points in map units)

	struct FRect
	{
		double X = 0.0;
		double Y = 0.0;
		double W = 0.0;
		double H = 0.0;

		double Right() const { return X + W; }
		double Bottom() const { return Y + H; }
		FPt Centre() const { return FPt{ X + W * 0.5, Y + H * 0.5 }; }
		bool Contains(FPt P) const { return P.X >= X && P.X <= X + W && P.Y >= Y && P.Y <= Y + H; }
		bool Overlaps(const FRect& Other) const;
		double Overlap(const FRect& Other) const; // the area the two share
		FRect Moved(double DX, double DY) const { return FRect{ X + DX, Y + DY, W, H }; }
	};

	double Distance(FPt A, FPt B);

	/** Polylines kept between calls: Clear keeps every buffer, Add hands one back emptied, so laying again allocates nothing. */
	struct FPathList
	{
		std::vector<FPath> Paths;
		size_t Count = 0;

		void Clear() { Count = 0; }
		FPath& Add();
		const FPath& operator[](size_t Index) const { return Paths[Index]; }
		size_t Num() const { return Count; }
	};

	// ---------------------------------------------------------------------------------------------------------------
	// Faces and measuring

	enum class EFamily : uint8_t
	{
		Serif, // PT Serif
		Courier, // Courier Prime
	};

	enum class EStyle : uint8_t
	{
		Regular,
		Bold,
		Italic,
	};

	/** A face at a size in layer units (picture px x Px; the kit's Slate faces take it x 0.75 as points), letter spacing in thousandths of an em. */
	struct FFace
	{
		EFamily Family = EFamily::Serif;
		EStyle Style = EStyle::Regular;
		double Size = 12.0;
		int Spacing = 0;
	};

	/** A face of SizePx picture pixels at Px. */
	FFace FaceOf(EFamily Family, EStyle Style, double SizePx, double Px, int Spacing = 0);

	/** How big a line of UTF-8 text is in a face, in layer units (Slate's FontMeasure in the game, a table in the tests). */
	using FMeasure = std::function<FPt(std::string_view Text, const FFace& Face)>;

	/** Text cut into lines no wider than Width at whole words (no dots added), at most MostLines; returns how many. */
	int Wrap(std::string_view Text, const FFace& Face, double Width, const FMeasure& Measure, int MostLines, std::vector<std::string>& Out);

	/**
	 * Text on one line no wider than Width: the whole of it where it fits (true); else cut at the last whole word that
	 * fits with the ellipsis (U+2026) after it, a comma or other stop before the cut dropped; a single word too long for
	 * the line is cut by letters. "" where not even the ellipsis fits.
	 */
	bool FitLine(std::string_view Text, const FFace& Face, double Width, const FMeasure& Measure, std::string& Out);

	// ---------------------------------------------------------------------------------------------------------------
	// Draw lists

	enum class EDraw : uint8_t
	{
		Box, // At, filled with Fill (the MakeBox tint), Edge an outline EdgeW wide, Radius its corners
		Text, // Text in Font, its box At (top left; the size measured), in Fill
		Sign, // a goods sign's picture Key ("goods-ore": Content/Goods/goods-ore.png), in At, tinted Fill
		Flag, // a flag Key (FlagKey: "UK", "ESP_PLAIN"), in At
		Line, // from A to B, EdgeW wide, in Edge
		Poly, // the open polyline Points, EdgeW wide, in Edge
	};

	struct FDraw
	{
		EDraw Kind = EDraw::Box;
		FRect At;
		FInk Fill;
		FInk Edge;
		double EdgeW = 0.0;
		double Radius = 0.0;
		std::string Text;
		FFace Font;
		std::string Key;
		FPt A;
		FPt B;
		std::vector<FPt> Points;
	};

	/** A box filled with Fill (the real colour: MakeBox's tint) and outlined. */
	FDraw BoxDraw(const FRect& At, FInk Fill, FInk Edge, double EdgeW, double Radius);
	/** A hollow ring Diameter across with its line Width: a round box filled at alpha 1/255 and outlined in Ink. */
	FDraw RingDraw(FPt Centre, double Diameter, double Width, FInk Ink);
	FDraw TextDraw(FPt TopLeft, std::string_view Text, const FFace& Font, FInk Ink, const FMeasure& Measure);
	FDraw LineDraw(FPt A, FPt B, double Width, FInk Ink);

	// ---------------------------------------------------------------------------------------------------------------
	// Words (UTF-8; every string the pieces show)

	/** The goods' noun: Ore, Oil, Chrome, Wolfram, Food (beef), Goods (exports, and a sign not drawn yet), Bases (base). */
	std::string_view NounOf(std::string_view Sign);

	/** The goods sign's picture: "goods-<sign>" for the seven drawn (signs/goods-*.svg; Content/Goods/goods-*.png), else goods-exports. */
	std::string SignKey(std::string_view Sign);

	/** ASCII letters in capitals (the card's caps: WOLFRAM). */
	std::string Caps(std::string_view Text);

	/** A figure with one decimal, ".0" dropped: 4.5, 2, 0.7. */
	std::string Figure(double Value);

	/**
	 * Money as the game shows it: production points times the book's scale (budget.json 'scale', a hundred), in whole
	 * numbers, as the treasury, the rules' notes ("Your bid stands: 150 a month") and the browser's bargains panel show
	 * it: 1.5 reads 150, 5.18 reads 518. Bids are kept, and ordered, in points.
	 */
	std::string Money(double Pp, double Scale);

	/**
	 * A nation's name (or its tag where the book has no name for it); its name as it follows another word, where the
	 * book's nations carry one ("held by the United States"; else the name); and its adjective (or its name).
	 */
	std::string NameOf(const FBook& Book, std::string_view Tag);
	std::string TheNameOf(const FBook& Book, std::string_view Tag);
	std::string AdjOf(const FBook& Book, std::string_view Tag);

	/** "held by Britain, 5 of 12 months" (n = clamp(Term - MonthsLeft + 1, 1, Term)), or "no buyer yet". */
	std::string HeldLine(const FRow& Row, const FBook& Book);

	/** "Rival bids show with an agent in Lisbon, or in the bidder's capital." */
	std::string RivalHint(const FRow& Row);

	/** The seal's tip: "Swedish iron ore: bids this month." and "Click to see the bids." where it opens a card, else CanBid. */
	std::string SealTip(const FRow& Row);

	/** "Ore for Germany lost at sea, Bay of Biscay, March" (the cut's sea and month where it has them). */
	std::string CutHover(const FRow& Row, const FBook& Book);

	/** "Ore reached Germany, August: 4.5 supply, 2.6 oil, 1.2 production": non-zero parts only, "production ?" unseen. */
	std::string CrateHover(const FRow& Row, const FBook& Book);

	/** "to Britain": the lane's tag where it leaves the screen. */
	std::string ExitText(const FRow& Row, const FBook& Book);

	/**
	 * The sending country's capital plate line, from its legions (Home): "1 division abroad, Leningrad"; "2 divisions
	 * abroad, Leningrad" (no place where their places differ); "1 division abroad" on its way; "home from Leningrad" in
	 * the month it is recalled. "" for a home with none.
	 */
	std::string PlateLine(const FBook& Book, std::string_view Home);

	/** The tray card's line for a legion's corps: "1 DIV · 88% · TOUR 14 OF 24 MO". */
	std::string TrayLine(const FLegion& Legion);

	/**
	 * The tray card's corner flag's tip: the same words as the line under the title (LegionLine), with a full stop (the UI
	 * director's mend 2): "Spanish volunteers, with the German army, Leningrad front."; for a home with no adjective, as
	 * the line, "Volunteers from Spain, with the German army, Leningrad front.".
	 */
	std::string FlagTip(const FLegion& Legion, const FBook& Book);

	/**
	 * The tray card's line under its title (call 13): what a legion is, from the rules' own words: the home's adjective
	 * and the patron's as the bridge sends them (nations.json adj), and the province it stands in: "Spanish volunteers,
	 * with the German army, Leningrad front" (no front while it is on its way). A home with no adjective: "Volunteers from
	 * Spain, with the German army, Leningrad front".
	 */
	std::string LegionLine(const FLegion& Legion, const FBook& Book);

	// ---------------------------------------------------------------------------------------------------------------
	// Plain period flags (no arms): the pennant, the tray's corner flag and the card's rows

	struct FBand
	{
		uint32_t Srgb = 0;
		int Weight = 1;
	};

	/** A plain flag: its key (object names take no "~"), its bands top to bottom (or left to right where bVertical). */
	struct FPlainFlag
	{
		const char* Key = "";
		bool bVertical = false;
		int Count = 0;
		FBand Bands[3];
	};

	/** Spain red-yellow-red 1:2:1, Portugal green-red 2:3 (upright), Germany black-white-red: nullptr for any other tag. */
	const FPlainFlag* PlainFlagOf(std::string_view Tag);

	/** The flag key the card, the tray and the pennant use: ESP_PLAIN, POR_PLAIN, GER_PLAIN, or the tag itself. */
	std::string FlagKey(std::string_view Tag);

	/** The home's pennant under the host's flag on the legion's pole: this share of the flag's width. */
	inline constexpr double PennantShare = 0.7;
	/** The pennant's hoist (its height at the pole) as a share of its length: the picture's triangle, 16 high on 30 long. */
	inline constexpr double PennantHoist = 16.0 / 30.0;

	/**
	 * The home's pennant as the picture draws it: a triangle on the pole, its hoist at the pole and its point out along the
	 * cloth, so that Spain's 1:2:1 bands run together to the point. Points are in the pole's plane, X along the cloth (the
	 * piece's Y) and Y up (the piece's Z): At[0] the hoist's top, At[1] its foot, At[2] the point; Uv the flag picture's
	 * places for each (V down from its top), one triangle 0, 1, 2. It hangs in the first slot under the ally pennants
	 * (FlagWidth x 0.46 wide, 3:2, 0.3 U apart, three at most), from the pole's near side.
	 */
	struct FPennantShape
	{
		FPt At[3];
		FPt Uv[3];
	};
	FPennantShape PennantShape(double NearY, double TopZ, double FlagWidth, int Allies, double U);

	// ---------------------------------------------------------------------------------------------------------------
	// What the map shows of the book (LookAt runs once per book)

	/**
	 * True when the viewer may open the row's card: it has a round, and the viewer may bid, has bid, sells, has an agent
	 * in the seller's capital, or sees a bid. By the rules (bargains.ts seesBid) every bid sent is the viewer's own, or
	 * seen as the seller, or through an agent in the seller's or that bidder's capital: so an agent in a bidder's capital
	 * opens the card while that country bids. An agent in the holder's capital alone, the holder not bidding, shows no
	 * bid, and its seal stays quiet.
	 */
	bool OpensCard(const FRow& Row);

	/** The viewer's own rows: it buys, it sells, or it has bid in the round. */
	bool IsOwn(const FRow& Row, const FBook& Book);

	/** True when the row's lane is drawn (before the cap of 16): see LookAt. */
	bool DrawsLane(const FRow& Row, const FBook& Book);

	/** True when a drawn lane carries the crate this month: not cut, moved, and something arrived (or the production is unseen). */
	bool CarriesCrate(const FRow& Row);

	inline constexpr int MostLanes = 16;
	inline constexpr int MostQuietSeals = 12;
	/**
	 * Seals in all, the layer's slots (BargainLayer.cpp SealSlots). A grand campaign's first month of bargains has 36
	 * rounds, 29 of them opening a card (October 1939, Britain at the table: fixtures/look-first-UK-October-1939.json).
	 */
	inline constexpr int MostSeals = 48;
	inline constexpr double TintAt = 10.0; // |lean| from which the seller's border is tinted
	inline constexpr double TintStrongAt = 25.0;
	inline constexpr double TintA = 0.13;
	inline constexpr double TintStrongA = 0.22;

	struct FLaneLook
	{
		int Row = -1; // in Book.Rows
		uint32_t Ink = 0; // the holder's colour
		bool bCut = false; // broken at its cut mark
		bool bCrate = false;
		std::string Tag; // "to Britain"
		std::string Hover; // the cut's words, or the crate's ("" for an open lane with no crate)
	};

	struct FTintLook
	{
		std::string Seller;
		int Row = -1;
		uint32_t Ink = 0;
		double Alpha = 0.0; // at the border, fading to 0 13 px inside
	};

	struct FSealLook
	{
		int Row = -1;
		bool bOpens = false; // a press opens its card
		bool bOpen = false; // its card is the one open
		std::string Tip;
	};

	struct FPlateLook
	{
		std::string Home;
		int Capital = -1; // the home's capital region
		std::string Line;
	};

	struct FTrayLook
	{
		int Corps = -1;
		std::string Home;
		std::string Flag; // FlagKey of the home
		std::string Line; // TrayLine
		std::string Tip; // FlagTip
		std::string Legion; // LegionLine, under the title
	};

	struct FLookIn
	{
		std::function<uint32_t(std::string_view Tag)> SrgbOf; // a nation's colour (nations.json; FindNation(tag)->Color)
		bool bFar = false; // the eye is far off: no quiet seals, no plate lines
		std::string OpenRow; // the id of the row whose card is open, or ""
		/**
		 * Whether the viewer's own picture of the war shows that corps as a legion of that home: the view's corps entry
		 * carries vol (the bridge sends it only for a corps of the viewer's own, an ally's, or one known exactly; never
		 * for one not identified or known from a stale report). Unset, or false, and that corps has no tray pieces: the
		 * tray card would otherwise show the flag and true strength of an enemy corps the viewer cannot see.
		 */
		std::function<bool(int Corps, std::string_view Home)> SeesLegion;
	};

	struct FLook
	{
		std::vector<FLaneLook> Lanes; // own rows first, at most MostLanes
		std::vector<FTintLook> Tints; // one per seller
		std::vector<FSealLook> Seals; // the seals that open a card first, then the quiet ones (named rows first)
		std::vector<FPlateLook> Plates;
		std::vector<FTrayLook> Trays;
	};

	/**
	 * What the map shows of the book, for its viewer:
	 * - a lane where the row has a path of 2 points or more, no base, and a buyer, and the buyer is the viewer, the viewer
	 *   sells, the buyer is an enemy on a named row, or the viewer has an agent in either capital; not a cut lane without
	 *   its cut mark; at most 16, own rows first;
	 * - the crate on a drawn lane that is not cut, whose goods moved and something arrived (or the production is unseen);
	 * - a border tint per seller of the viewer's own rows and of the open card's row, where |lean| >= 10: alpha .22 from
	 *   |lean| 25, else .13, in the buyer's colour where its bloc is the lean's side, else Germany's (+) or Britain's (-);
	 * - a sealed-letter sign per row with a round, at most 48: those that open a card first (the open card's own, then
	 *   the rows the viewer bids on, sells, has an agent at the seller of, or sees bids on, then those it may bid on),
	 *   then at most 12 quiet ones (named rows first), none of them from far off;
	 * - a plate line per home with legions (for every viewer, not from far off), and a tray line per legion's corps that
	 *   the viewer's own picture shows as one (In.SeesLegion).
	 */
	void LookAt(const FBook& Book, const FLookIn& In, FLook& Out);

	/** LookAt's tray pieces alone (Out replaced): read again when the viewer's picture of the corps changes in a month. */
	void TraysOf(const FBook& Book, const FLookIn& In, std::vector<FTrayLook>& Out);

	// ---------------------------------------------------------------------------------------------------------------
	// One card open at a time

	/**
	 * The open card: a press on a seal opens its card and closes the other (or closes it, on its own seal). It closes on
	 * a new month and at its round's end; a seller's own round opens itself once a month when no card is open. The card
	 * is shown only while it is open and the edge news would be (EdgeNewsShown's conditions).
	 */
	class FOpenCard
	{
	public:
		const std::string& Row() const { return Open; }
		bool IsOpen() const { return !Open.empty(); }
		bool Shown(bool bEdgeNewsShown) const { return IsOpen() && bEdgeNewsShown; }

		/** A press on the seal of row Id: true where the open card changed. */
		bool Press(const FBook& Book, std::string_view Id);
		void Close() { Open.clear(); }

		/** The book changed: true where the open card changed. */
		bool Update(const FBook& Book);

	private:
		std::string Open;
		int Turn = -1; // the month the book was last seen at
		int AutoTurn = -1; // the month a seller's own round last opened itself (or was closed by hand)
	};

	// ---------------------------------------------------------------------------------------------------------------
	// The bid card

	enum class EAct : uint8_t
	{
		None,
		Minus, // bargainBid(id, Pp): one step down
		Plus, // bargainBid(id, Pp): one step up (Steps[0] where the viewer has not bid)
		Accept, // bargainSell(id, Buyer)
		Nobody, // bargainSell(id, null)
	};

	struct FButton
	{
		EAct Act = EAct::None;
		FRect At; // on the card
		bool bLive = false;
		double Pp = 0.0;
		std::string Buyer;
	};

	enum class ELine : uint8_t
	{
		Own, // the viewer's own bid
		Seen, // a bid the viewer sees
		Others, // the bids it does not see: "Others ?"
	};

	struct FCardLine
	{
		ELine Kind = ELine::Seen;
		std::string Nat; // "" for Others
		std::string Figure; // "?" for Others, the en dash for an own row with no bid yet
		bool bSteps = false; // the own row's - and +
		bool bAccept = false; // the seller's ACCEPT
		double Offer = 0.0;
	};

	/** The card's rows in order: the viewer's own, the bids it sees from the highest, then Others where it does not see them all. */
	void CardLines(const FRow& Row, const FBook& Book, std::vector<FCardLine>& Out);

	/** The steps the own row's - and + send: one step each way; with no bid + sends Steps[0]; false where the button cannot be pressed. */
	bool StepDown(const FRound& Round, double& OutPp);
	bool StepUp(const FRound& Round, double& OutPp);

	/** The card's measures, in picture px (x Px on the screen). Content is inside the 1 px edge and the padding. */
	namespace Card
	{
		inline constexpr double W = 230.0;
		inline constexpr double PadX = 10.0;
		inline constexpr double PadY = 8.0;
		inline constexpr double Edge = 1.0;
		inline constexpr double InnerW = W - 2.0 * (PadX + Edge); // 208
		inline constexpr double Radius = 2.0;
		/** The keyline's width in layer units, not scaled by Px: the date card's 1.0 (Kit::Keyline). */
		inline constexpr double KeylineW = 1.0;
		inline constexpr double HeadH = 34.0;
		inline constexpr double SignSize = 22.0;
		inline constexpr double TextX = 28.0; // the caps, and the seller under them at (28, 17)
		inline constexpr double SellerY = 17.0;
		inline constexpr double RingD = 34.0;
		inline constexpr double RingW = 2.0;
		/** The ring's numeral's ink middle to "MO"'s (the contract's 12.5 and 24.5 in the head); the two stand centred as one. */
		inline constexpr double RingLineStep = 12.0;
		inline constexpr double HeldY = 38.0;
		inline constexpr double RuleY = 60.0;
		inline constexpr double RowsY = 66.0;
		inline constexpr double RowStep = 26.0;
		inline constexpr double RowH = 22.0;
		inline constexpr int MostRows = 6;
		inline constexpr double FlagW = 21.0; // the house's 3:2 (22 x 13 would stretch the flags)
		inline constexpr double FlagH = 14.0;
		inline constexpr double NameX = 27.0;
		inline constexpr double FigureRight = 208.0;
		inline constexpr double FigureRightSteps = 161.0;
		inline constexpr double FigureRightAccept = 140.0;
		inline constexpr double MinusX = 167.0;
		inline constexpr double PlusX = 189.0;
		inline constexpr double StepSize = 19.0;
		inline constexpr double AcceptX = 146.0;
		inline constexpr double AcceptW = 62.0;
		inline constexpr double KitRadius = 4.0; // the kit's button corners (--radius-plate)
		inline constexpr double FootGap = 6.0; // the seller's foot under the hint
		inline constexpr double FootLine = 1.0; // the quiet button's brass-700 underline, 1 under its words
		inline constexpr double HintGap = 5.0;
		inline constexpr int MostCanBidLines = 2;
	}

	/**
	 * Where a line of the kit's faces sets its ink, in ems of its size (Content/Fonts; font_widths.py measures them and the
	 * test holds these to it). Slate sets a line's baseline the face's ascender under its box's top, and the box is the
	 * ascender to the descender high: the figures' ink (0 to 0.59 em over the baseline) stands about 0.075 em above the
	 * box's middle, a pixel at 14. The ring's numeral is centred by its ink (call 1), not by its box.
	 */
	namespace Ink
	{
		inline constexpr double CourierAscent = 0.78125; // hhea ascender / em
		inline constexpr double CourierFigureH = 0.59; // the figures' height over the baseline ("1" 0.592, the rest 0.58-0.59)
		inline constexpr double CourierCapH = 0.58; // the capitals' (OS/2 cap height 0.5796)
	}

	/**
	 * The months the round has left, as its ring would say them: max(1, RoundLen - Months + 1). A round of the rules is
	 * a month (bargains.json round 1), so the ring would only ever say 1, and it is not drawn at 1 (call 2): "held by
	 * Britain, 5 of 12 months" tells the time. Rounds of more than a month show it.
	 */
	int RingMonths(const FRound& Round, const FBook& Book);
	bool RingShown(const FRound& Round, const FBook& Book);

	/** The card's faces at Px. */
	FFace CapsFace(double Px); // Courier Prime bold 12, spacing 80
	FFace SellerFace(double Px); // PT Serif 12
	FFace HeldFace(double Px); // PT Serif italic 12
	FFace NameFace(double Px); // PT Serif 12 (bold for the viewer's own name)
	FFace FigureFace(double Px); // Courier Prime bold 18
	FFace RingFace(double Px); // Courier Prime bold 14
	FFace MoFace(double Px); // Courier Prime bold 9
	FFace StepFace(double Px); // Courier Prime bold 14
	FFace KitButtonFace(double Px); // Courier Prime bold 11, spacing 100: the kit's small button, ACCEPT and the seller's foot
	FFace HintFace(double Px); // PT Serif 11

	/** The card laid out: its size, what is drawn on it in order, and its buttons, all on the card in layer units. */
	struct FCard
	{
		FPt Size;
		std::vector<FDraw> Draws; // the frame (paper-200, the keyline) first, then the rest
		std::vector<FButton> Buttons;
		int Rows = 0; // rows drawn (at most Card::MostRows)
		int More = 0; // rows folded into "+N more"
		int HintLines = 0;
	};

	/** The row's card at Px; false (and Out emptied) where the row has no round. */
	bool LayCard(const FRow& Row, const FBook& Book, const FMeasure& Measure, double Px, FCard& Out);

	// ---------------------------------------------------------------------------------------------------------------
	// Where the card stands

	inline constexpr double CardFrom = 28.0; // from the capital to the card's nearest side
	inline constexpr double ReplaceAfter = 24.0; // the capital moved this far: the card is placed again
	inline constexpr double PinShort = 6.0; // the pin stops this short of the capital
	inline constexpr double PinW = 1.2;
	/** The card stands at least this far (picture px, x Px as every size here) off the screen's edges and every frame piece and panel (call 5). */
	inline constexpr double KeepOff = 16.0;
	/** The capital's height stays this far inside the card's top and bottom, so that the level pin meets a straight side. */
	inline constexpr double PinInset = 12.0;

	struct FPlaceIn
	{
		FPt Capital; // on the screen
		FPt Size; // the card's (FCard::Size)
		FRect Screen; // the map's part of the screen
		/**
		 * What the card keeps KeepOff from: the frame's pieces and the panels over the map (but the card itself), in the
		 * layer's own units. The layer reads them as the edge layer does since 1a22b3ed: absolute boxes as each was last
		 * painted, brought into its units through its own paint-space geometry (GetPaintSpaceGeometry), never through the
		 * desktop measure Tick is handed.
		 */
		const std::vector<FRect>* Frame = nullptr;
		double Px = 1.0;
	};

	struct FPlaced
	{
		FRect Card;
		int Spot = -1; // 0 left of the capital, 1 right of it
		bool bFits = false; // KeepOff from the screen's edges and every frame piece
		double Intrusion = 0.0; // how much of the card lies within the keep-off (an area; 0 where it fits)
		FPt PinFrom; // on the card's edge
		FPt PinTo; // PinShort from the capital
		bool bPin = false;
		FPt LaidFor; // the capital it was placed for
		FPt ScreenSize;
	};

	/** The card of Size left (Spot 0) or right (1) of Capital, CardFrom x Px from it, its middle level with it. */
	FRect SpotBox(int Spot, FPt Capital, FPt Size, double Px);

	/**
	 * How much of Box lies within the keep-off: the area of it outside the screen drawn in by KeepOff x Px, and the areas
	 * it shares with each frame piece grown by KeepOff x Px on every side. 0 where it keeps off them all.
	 */
	double KeepOffIntrusion(const FRect& Box, const FPlaceIn& In);

	/**
	 * Where the card stands (the art director's call 5): CardFrom from the capital with a level pin, on the left where it
	 * keeps KeepOff from the screen's edges and every frame piece, else on the right (the pin mirrored). On either side it
	 * may slide up or down from level with the capital, as little as it must to keep off, while the capital stays
	 * PinInset inside its top and bottom so that the pin stays level. Where neither side keeps off (the no-fit case),
	 * the side that lies least within the keep-off, the left on a tie. Allocates nothing.
	 */
	FPlaced PlaceCard(const FPlaceIn& In);

	/** True when the card must be placed again: it was never placed, the capital moved more than 24 px, or the screen changed size. */
	bool NeedsPlace(const FPlaced& Was, FPt CapitalNow, const FRect& ScreenNow, double Px);

	// ---------------------------------------------------------------------------------------------------------------
	// The lane, the cut and the crate (map units, from the seller to the buyer)

	namespace Lane
	{
		inline constexpr double Dash = 10.0;
		inline constexpr double Gap = 7.0;
		inline constexpr double DashW = 2.6;
		inline constexpr double CasingW = 5.0;
		inline constexpr float CasingA = 0.55f; // paper-200
		inline constexpr double Step = 4.0; // the curve's points, about this far apart
		inline constexpr double CutGap = 24.0;
		inline constexpr double TickLong = 11.0;
		inline constexpr double TickW = 2.0;
		inline constexpr double RingD = 12.0; // brass-300, its line 1.6
		inline constexpr double RingLineW = 1.6;
		inline constexpr double CrateW = 16.0;
		inline constexpr double CrateH = 12.0;
		inline constexpr double CrateRadius = 1.5;
		inline constexpr double CrateEdge = 1.8; // the holder's colour
		inline constexpr double CrateBand = 1.2;
		inline constexpr double CrateRest = 0.4; // where the crate stands: 40% along
		inline constexpr double CrateRun = 1.6; // seconds of its run at the month's end
		inline constexpr double TagInside = 10.0; // the "to Britain" tag, inside the screen's edge
		inline constexpr double TagGap = 24.0; // and this far beside the lane (the picture has 34 to 49; nearer reads as the lane's own)
		inline constexpr double MiterMost = 2.0; // a corner's offset is at most this many half widths
	}

	/**
	 * The path cut where it crosses the world's wrap (a step with |dx| > MapW / 2 is the short way round): each piece runs
	 * to the seam (MinX + k MapW: the map's WorldMinX and WorldWidth), the next on from it on the other side. MapW 0: no wrap.
	 */
	void Unwrap(const FPath& In, double MinX, double MapW, FPathList& Out);

	/** A smooth curve through the points (Catmull-Rom, as ShowLanes draws the sea lanes), a point about every Step. */
	void Densify(const FPath& In, double Step, FPath& Out);

	double LengthOf(const FPath& Path);

	/** The point (and the way, a unit vector) Along map units from the start of the pieces, taken as one line. */
	FPt PointAlong(const FPathList& Pieces, double Along, FPt* Dir = nullptr);

	/** The point at a share (0..1) of the pieces' length. */
	FPt PointAt(const FPathList& Pieces, double Share, FPt* Dir = nullptr);

	/** The part of a path from From to To map units along it. */
	void SubPath(const FPath& Path, double From, double To, FPath& Out);

	/**
	 * A strip along a path, as one triangle strip: Outer[i] and Inner[i] are a pair across it (the casing: its left and
	 * right sides; the tint: the border and 13 px in). A pair for each point of the path, and two at a turn too sharp to
	 * miter (the outside of the turn bevelled), so Outer and Inner are as long as each other but may be longer than the path.
	 */
	struct FStrip
	{
		std::vector<FPt> Outer;
		std::vector<FPt> Inner;
	};

	/**
	 * One strip HalfW each side of the path, mitered (a corner's point at most MiterMost half widths out; beyond that the
	 * outside of the turn is bevelled and the inside held there), its sides never running backwards: one triangle strip,
	 * so its alpha never doubles at a corner.
	 */
	void Casing(const FPath& Path, double HalfW, FStrip& Out);

	/** The dashes (Dash on, Gap off, from the start of the first piece) along the pieces, none inside [SkipFrom, SkipTo]. */
	void Dashes(const FPathList& Pieces, double Dash, double Gap, double SkipFrom, double SkipTo, FPathList& Out);

	/** The cut's gap: Len long, centred on the point of the pieces nearest the sea's mark (moved inside the line at its ends). */
	struct FGap
	{
		bool bOk = false;
		double From = 0.0; // along the pieces
		double To = 0.0;
		FPt A; // the gap's ends, and the way the lane runs there
		FPt B;
		FPt DirA;
		FPt DirB;
		FPt Mid; // the ring's middle
	};
	FGap CutGap(const FPathList& Pieces, FPt Sea, double Len);

	/** The two end ticks, Long across the lane, centred on the gap's ends. */
	struct FTick
	{
		FPt A;
		FPt B;
	};
	void Ticks(const FGap& Gap, double Long, FTick (&Out)[2]);

	/** A lane laid for the map at Pixel map units a picture pixel. */
	struct FLaneShape
	{
		FPathList Raw; // unwrapped
		FPathList Pieces; // and smoothed: the line the crate runs on
		FPathList Kept; // the pieces without the cut's gap: the casing's
		std::vector<FStrip> Casing;
		size_t CasingCount = 0;
		FPathList Dashes;
		double Length = 0.0;
		FGap Gap; // where the lane is cut
		FTick Ticks[2];
		double Pixel = 0.0;
	};

	/** The row's lane (its path, its cut) laid at Pixel map units a picture pixel, on a world wrapping at MinX every MapW (0: no wrap). */
	void LayLane(const FRow& Row, double Pixel, double MinX, double MapW, FLaneShape& Out);

	/** Where the crate stands T (0..1) through its run: 0 at the seller, CrateRest at the end, slowing as it arrives. */
	double CrateShare(double T);

	/** How far through its run the crate is, Seconds after the month's end (0..1 over CrateRun). */
	double CrateRunT(double Seconds);

	/** The crate's draws at its point on the screen: paper box 16 x 12, radius 1.5, edged 1.8 in the holder's colour, a 1.2 band. */
	void CrateDraws(FPt Screen, uint32_t Holder, double Px, std::vector<FDraw>& Out);

	/** The cut's brass ring at its middle on the screen: 12 across, its line 1.6. */
	FDraw CutRing(FPt Screen, double Px);

	/**
	 * Where the "to Britain" tag stands: the lane on the screen (seller to buyer) leaves the screen at its last way out;
	 * the tag (TextSize) stands TagInside within that edge, TagGap beside the lane on the side it heads to (else the
	 * other). False where the lane does not leave the screen, the buyer's end is on it, or the tag would lie in the frame.
	 */
	bool ExitTag(const FPath& OnScreen, const FRect& Screen, const std::vector<FRect>& Frame, FPt TextSize, double Px, FRect& Out);

	/**
	 * ExitTag for a lane in pieces on the screen (LayLane's Raw pieces, projected): a lane across the world map's seam is
	 * two, the seller's to the seam and the buyer's on from the other side. The tag is laid on one piece only, the last
	 * with a point on the screen, never across the jump between them: joined, the seam's two ends made one false step
	 * across the screen, and the tag stood at the wrong edge. A piece that runs into the seam on the screen gets none.
	 */
	bool ExitTagOfPieces(const FPathList& OnScreen, const FRect& Screen, const std::vector<FRect>& Frame, FPt TextSize, double Px, FRect& Out);

	// ---------------------------------------------------------------------------------------------------------------
	// The border tint (map units)

	/** A run of a country's outer border: shared edges between two of its own regions are left out; coasts are kept. */
	struct FChain
	{
		FPath Points;
		bool bClosed = false;
		double Inward = 1.0; // +1: the country lies to the left of the way the chain runs (x right, y as the map's), -1 to the right
	};

	/** The outer border of the seller's regions (each ring one region's outline; shared vertices match to 1/1024). */
	void OuterEdges(const std::vector<FPath>& Rings, std::vector<FChain>& Out);

	/** The strip Width inside a chain: Outer on the border (alpha A), Inner Width in (alpha 0). Mitered, folds kept out. */
	void InnerStrip(const FChain& Chain, double Width, FStrip& Out);

	inline constexpr double TintWidth = 13.0; // picture px

	struct FTintShape
	{
		std::vector<FChain> Chains;
		std::vector<FStrip> Strips;
		double Pixel = 0.0;
	};

	/** The seller's tint laid at Pixel: its outer chains (once) and the 13 px strips inside them. */
	void LayTint(const std::vector<FPath>& Rings, double Pixel, FTintShape& Out);

	// ---------------------------------------------------------------------------------------------------------------
	// The sealed-letter sign at a capital

	inline constexpr double SealScale = 0.6; // of the edge seals' box (EdgeSeals::Look, 34 x 44, the disc 30 across)
	inline constexpr double SealBoxW = 34.0 * SealScale;
	inline constexpr double SealBoxH = 44.0 * SealScale;
	inline constexpr double SealMid = 17.0 * SealScale; // the disc's middle in its box
	inline constexpr double SealApart = 20.0; // two seals' middles at least this far apart
	inline constexpr int SealTries = 4;
	inline constexpr FPt SealOffsets[SealTries] = { { 0.0, -24.0 }, { 24.0, -24.0 }, { -24.0, -24.0 }, { 28.0, 0.0 } };

	/** The seal's box with its disc's middle at Centre. */
	FRect SealBox(FPt Centre, double Px);

	/**
	 * Where a capital's seal stands: the first of the four tries that is SealApart from every seal laid (Laid), clear of
	 * Avoid (the frame, the plates); else the first only clear of the seals and of Frame. False where none is.
	 */
	bool SealAt(FPt Capital, double Px, const std::vector<FRect>& Frame, const std::vector<FRect>& Avoid, const std::vector<FPt>& Laid, FPt& Out);

	/**
	 * What the layer's own plates and the seals step off before the town plates are added (the look review's finding 2):
	 * the frame's pieces, and while the card is shown the bid card's rect and its seal's box on its top-right corner
	 * (SealBox at CardSealAt, at Px), so that they step off the card and the seal's overhang as off any frame piece (the
	 * card's own rect is kept out of the frame for its placing, and the card is opaque and paints its seal over the
	 * layer: a seal or plate under either could be neither seen nor pressed; the review after the passes, finding 2).
	 * Out is refilled; nothing is allocated once it has grown.
	 */
	void AvoidOf(const std::vector<FRect>& Frame, bool bCardShown, const FRect& Card, double Px, std::vector<FRect>& Out);

	/**
	 * The open card's seal while its card is shown (the art director's call 6): it leaves the capital, so that the lane's
	 * end there shows, and its disc's middle stands on the card's top-right corner, where it is the card's close (a
	 * press on it shuts the card, as a press on it at the capital did). When the card shuts it is back at its capital.
	 * Only the open card's seal moves. Its box (SealBox) reaches SealMid x Px over the card's top and right, inside the
	 * card's KeepOff, so it stays on the screen and off the panels; it is clear of the ring and of every button. It is
	 * laid first (the look's first seal), so the other seals keep SealApart from it as from any seal laid, and its box is
	 * in what they step off (AvoidOf).
	 */
	FPt CardSealAt(const FRect& Card);

	/** True for the seal that stands on the card: the open card's own (FSealLook::bOpen), while the card is shown. */
	bool SealOnCard(const FSealLook& Seal, bool bCardShown);

	/** True where a point on the card (its own units, the top left at 0) is on the card's seal: a press there shuts the card. */
	bool OnCardSeal(FPt OnCard, FPt CardSize, double Px);

	// ---------------------------------------------------------------------------------------------------------------
	// The sender's capital plate line, a capital plate of our own, and the tray

	inline constexpr double MenSize = 16.0;
	inline constexpr double MenStroke = 1.8; // px, as the kit's line signs take their weight
	inline constexpr double MenGap = 5.0;
	inline constexpr double EyeMost = 12500.0; // our own capital plate only with the eye under this
	/** The tray's corner flag: the size the other tray cards' flags are (call 11), an ally's corps's in MakeCard, 21 x 14 (look_test reads it there). */
	inline constexpr double TrayFlagW = 21.0;
	inline constexpr double TrayFlagH = 14.0;
	/** The corner flag's gap before the title: TrayFlag's right padding, the 6 MakeCard gives an ally's flag. */
	inline constexpr double TrayFlagGap = 6.0;
	/**
	 * The legion line's left padding on the tray card (the look review's finding 1, option a): under the title's text, as
	 * the picture has it (its title, legion line and Courier line on one left edge), so past the corner flag and its gap
	 * while the flag shows, and flush with the card where it does not. The line shows only where the flag does (the same
	 * TrayOf), so in the game it is always TrayFlagW + TrayFlagGap, 27. The Courier line stays where MakeCard puts it.
	 */
	constexpr double TrayLegionIndent(bool bFlagShown) { return bFlagShown ? TrayFlagW + TrayFlagGap : 0.0; }

	FFace PlateLineFace(double Px); // PT Serif 12, paper-300
	FFace PlateNameFace(double Px); // PT Serif 13, paper-200
	FFace TrayLineFace(double Px); // Courier Prime 13, brass-300
	/**
	 * The tray's legion line under the title: PT Serif 12 in paper-300, the face of the legion's line on its home's plate
	 * (PlateLineFace); the picture's is 13, which is the art director's to rule (NOTES). On the game's one-division card
	 * (355 px at 1441) the line has 312 px under the title's text (TrayLegionIndent): the Blue Division's words measure
	 * 318.1 at 12, cut to "... Leningrad" and the ellipsis, and 344.6 at 13, cut to "... German army" and the ellipsis.
	 */
	FFace TrayLegionFace(double Px);
	FFace TagFace(double Px); // PT Serif 12, paper-200

	/** The men sign (the picture's helmet), Size across with its top left at TopLeft, in brass-300 MenStroke x Px wide. */
	void MenSign(FPt TopLeft, double Size, double Px, std::vector<FDraw>& Out);

	/** The plate line laid from its top left: the men sign, then the words. */
	struct FPlateLineLay
	{
		FPt Size; // what the plate grows by: Wide to at least Size.X (plus its sides), High by Size.Y
		FRect Sign;
		FPt TextAt;
		FPt TextSize;
	};
	FPlateLineLay LayPlateLine(std::string_view Text, const FMeasure& Measure, double Px);

	/** The plate line's draws with its top left at At. */
	void DrawPlateLine(const FPlateLineLay& Lay, FPt At, std::string_view Text, double Px, std::vector<FDraw>& Out);

	/** A capital plate of our own, for a capital whose town has no plate this frame: desk-900 .88, brass edge, the name in Serif 13, the line. */
	struct FOwnPlate
	{
		FRect Plate;
		std::vector<FDraw> Draws;
	};
	void LayOwnPlate(std::string_view Name, std::string_view Line, FPt Capital, const FMeasure& Measure, double Px, FOwnPlate& Out);

	/** Our own plate is drawn only with the eye under EyeMost and clear of the town plates and the frame. */
	bool OwnPlateShown(double Eye, const FRect& Plate, const std::vector<FRect>& Plates, const std::vector<FRect>& Frame);
}
