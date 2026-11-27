#include "Battle/LandGeneralEye.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace LandGeneralEye
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;
		constexpr double Rad = Pi / 180.0;

		/** One step of a critically damped spring of rate W toward Want (X and its swing carried). */
		void Spring(double& X, double& Swing, double Want, double W, double Dt)
		{
			const double Off = X - Want;
			const double Mix = Swing + Off * W;
			const double Decay = std::exp(-W * Dt);
			X = Want + (Off + Mix * Dt) * Decay;
			Swing = (Swing - Mix * W * Dt) * Decay;
		}

		double WrapNear(double A, double Near)
		{
			while (A - Near > Pi) A -= 2.0 * Pi;
			while (A - Near < -Pi) A += 2.0 * Pi;
			return A;
		}
	}

	double Eye::Ground(double X, double Y) const
	{
		if (Here.Height)
		{
			const double Z = Here.Height(X, Y);
			if (Z == Z) return Z;
		}
		return Rec ? Rec->Ground.HeightAt(X, Y) : 0.0;
	}

	double Eye::CropAt(double X, double Y) const
	{
		if (!Here.Crop) return 0.0;
		// (the corn stands where it stands: kept by the half-metre square for a couple of seconds, the scene asked
		// once, not a hundred times a frame)
		const long long Key = (static_cast<long long>(std::floor(X * 2.0)) << 32) ^ static_cast<long long>(static_cast<unsigned int>(static_cast<int>(std::floor(Y * 2.0))));
		const auto It = Crops.find(Key);
		if (It != Crops.end() && Clock - It->second.second < 2.0) return It->second.first;
		double C = Here.Crop(X, Y);
		C = C == C ? std::clamp(C, 0.0, 3.0) : 0.0;
		if (Crops.size() > 16384) Crops.clear();
		Crops[Key] = std::make_pair(C, Clock);
		return C;
	}

	bool Eye::Clear(const Vec& At, double R, bool bFree) const
	{
		// (Free is the film's ground-level word, a house's floor or a bank's hedge whatever the height: asked only
		// within FreeUnder of the ground; Solid has the things' own shapes)
		++Asked;
		const double G = Ground(At.X, At.Y);
		if (At.Z > G + Set.Tallest) return true;
		// (the point itself first: most points looked at and refused are inside the thing)
		if (Here.Solid && Here.Solid(At, -1) != 0) return false;
		const Vec Round[] = { Vec(1, 0, 0), Vec(-1, 0, 0), Vec(0, 1, 0), Vec(0, -1, 0), Vec(0, 0, -0.6), Vec(0, 0, 0.6) };
		if (Here.Free && At.Z < G + (bFree ? Set.FreeUnder : Set.FreeHard))
		{
			if (!Here.Free(At)) return false;
			for (const Vec& D : Round)
			{
				if (!Here.Free(At + D * R)) return false;
			}
		}
		if (!Here.Solid) return true;
		// (things kept R off; men only ManClear: the view may stand among the men, never in one; no man stands this
		// high over the ground, even on a tank)
		const double Rm = std::min(R, Set.ManClear);
		const bool bMen = R > Rm && At.Z < G + 4.5;
		for (const Vec& D : Round)
		{
			const int What = Here.Solid(At + D * R, -1);
			if (What == 2 || (What == 1 && R <= Rm)) return false;
			if (bMen && Here.Solid(At + D * Rm, -1) == 1) return false;
		}
		return true;
	}

	double Eye::ArmAt(double Zoom) const
	{
		const double Far = Set.Highest / std::sin(Set.TiltFar * Rad);
		return Set.NearArm * std::pow(Far / Set.NearArm, std::clamp(Zoom, 0.0, 1.0));
	}

	double Eye::TiltAuto(double Zoom) const
	{
		return (Set.TiltNear + (Set.TiltFar - Set.TiltNear) * std::clamp(Zoom, 0.0, 1.0)) * Rad;
	}

	double Eye::HillNone() const
	{
		// (over every tilt the view takes by itself and its soft meeting: a low look's tilt up here holds nothing
		// down at any zoom)
		return (std::max(Set.TiltNear, Set.TiltFar) + Set.TiltSoft + 5.0) * Rad;
	}

	double Eye::TiltAt(double Zoom, double Cap) const
	{
		const double Auto = TiltAuto(Zoom);
		const double Soft = Set.TiltSoft * Rad;
		// (no low look, or its tilt still clear over the automatic one here: the automatic tilt itself, to the last
		// bit)
		if (Cap >= Auto + Soft) return Auto;
		// (the low look: the view tilts no steeper than its tilt. What the automatic tilt stands over it is taken
		// off, met softly, a quarter-square across TiltSoft either side, so the zoom never turns a corner where
		// the two meet: by a man's shoulder, where the automatic tilt is the lower, the view is the automatic one
		// still)
		const double D = Auto - Cap;
		const double Over = Soft <= 0.0 ? std::max(D, 0.0) : D >= Soft ? D : (D + Soft) * (D + Soft) / (4.0 * Soft);
		return Auto - Over;
	}

	bool Eye::HillShows(double Zoom, double Cap) const
	{
		return TiltAuto(Zoom) - TiltAt(Zoom, Cap) > 0.25 * Rad;
	}

	double Eye::HillRest() const
	{
		// (where the low look's own hardest push brings it to rest from the swing it has: no flatter than it goes,
		// none where that is clear over every tilt)
		return std::clamp(Hill + SwingHill * std::abs(SwingHill) / (2.0 * Set.TiltPush * Rad), Set.LowTilt * Rad, HillNone());
	}

	double Eye::TiltByItself(double Zoom, double CapDegrees) const
	{
		return TiltAt(Zoom, CapDegrees * Rad) / Rad;
	}

	double Eye::AimAt(double Zoom) const
	{
		const double U = 1.0 - std::clamp(Zoom, 0.0, 1.0);
		return Set.NearAim * U * U;
	}

	double Eye::UpAt(double Zoom) const
	{
		return UpAt(Zoom, Hill);
	}

	double Eye::UpAt(double Zoom, double Cap) const
	{
		return AimAt(Zoom) + ArmAt(Zoom) * std::sin(TiltAt(Zoom, Cap));
	}

	double Eye::TiltOwn(double Zoom, double Own, double Cap) const
	{
		// (his tilt over the automatic one, met softly: a quarter-square across TiltSoft either side of it, so the view
		// never turns a corner as he tilts back down to it or as the zoom carries the automatic tilt past his)
		const double Auto = TiltAt(Zoom, Cap);
		const double Soft = Set.TiltSoft * Rad;
		const double D = std::min(Own, Set.TiltMost * Rad) - Auto;
		const double Over = Soft <= 0.0 ? std::max(D, 0.0) : D >= Soft ? D : D <= -Soft ? 0.0 : (D + Soft) * (D + Soft) / (4.0 * Soft);
		return Auto + Over;
	}

	bool Eye::TiltStands(double Zoom, double Own, double Cap) const
	{
		return TiltOwn(Zoom, Own, Cap) - TiltAt(Zoom, Cap) > 0.25 * Rad;
	}

	bool Eye::TiltShows(double Zoom, double Own, double Cap) const
	{
		return TiltStands(Zoom, std::min(Own, TiltMostAt(Zoom, Cap)), Cap);
	}

	double Eye::TiltMostAt(double Zoom, double Cap) const
	{
		// (toward a man's shoulder his tilt is handed back to the automatic one, as the eased tilt can follow: zoomed
		// right in, the view stands with the men and looks along the ground as it always has)
		const double Low = TiltAt(Zoom, Cap) - (Set.TiltSoft + 1.0) * Rad;
		const double U = std::clamp(Zoom / std::max(1e-6, Set.TiltOwnFrom), 0.0, 1.0);
		return Low + (Set.TiltMost * Rad - Low) * U * U * (3.0 - 2.0 * U);
	}

	double Eye::UpOwn(double Zoom, double Own, double Cap) const
	{
		return AimAt(Zoom) + ArmAt(Zoom) * std::sin(TiltOwn(Zoom, Own, Cap));
	}

	double Eye::HillZoom() const
	{
		// (the zoom whose eye stands HillUp over the look-at at the hill view's tilt)
		double Lo = 0.0, Hi = 1.0;
		for (int K = 0; K < 40; ++K)
		{
			const double Mid = 0.5 * (Lo + Hi);
			(UpAt(Mid, Set.HillTilt * Rad) < Set.HillUp ? Lo : Hi) = Mid;
		}
		return 0.5 * (Lo + Hi);
	}

	double Eye::FlyZoom(double Cap) const
	{
		if (Cap >= HillNone()) return FlyS;
		double Lo = 0.0, Hi = 1.0;
		for (int K = 0; K < 40; ++K)
		{
			const double Mid = 0.5 * (Lo + Hi);
			(UpAt(Mid, Cap) < Set.FlyUp ? Lo : Hi) = Mid;
		}
		return 0.5 * (Lo + Hi);
	}

	double Eye::TopZoom() const
	{
		double Lo = 0.0, Hi = 1.0;
		for (int K = 0; K < 40; ++K)
		{
			const double Mid = 0.5 * (Lo + Hi);
			(UpOwn(Mid, Set.TopTilt * Rad) < Set.TopUp ? Lo : Hi) = Mid;
		}
		return 0.5 * (Lo + Hi);
	}

	double Eye::TiltDegrees() const
	{
		return TiltFor(S, Tl) / Rad;
	}

	void Eye::OwnTiltBegin()
	{
		if (bOwnTilt) return;
		bOwnTilt = true;
		Tl = WantTl = TlGoal = TiltAt(S) - Set.TiltSoft * Rad;
		SwingTl = 0.0;
	}

	void Eye::KeepView(int Number)
	{
		if (Number < 0 || Number >= ViewCount) return;
		View& V = Views[Number];
		// (the view is kept where his own hand's last move is bringing it to rest: the view he is getting, a top view
		// just pressed above all, not one half way to it. Its turn, zoom and tilt are his own hand's unless a flight
		// to a kept view or a wait at a hedge has them; its place is his own hand's only where no flight, glide to a
		// point or followed unit carries it. What is not his own hand's is kept as it stands.)
		const bool bOwnRest = FlyTo < 0 && !bBrake;
		const bool bOwnPlace = bOwnRest && Follow < 0 && !bGliding;
		// (his own tilt, the top view's too. On its way back to the automatic tilt by his own hand it is not his own.
		// Where a flight he keeps in the middle of has the turning of it, to its view's tilt or back to the automatic
		// one, it is his own as it stands if it shows on the picture, and else the automatic one: the view kept is
		// the picture he has, never a tilt he cannot see)
		const bool bFlightTilt = FlyTo >= 0 && bOwnTilt && (bCallToAuto || !bToAuto);
		const bool bShows = bFlightTilt && TiltShows(S, Tl);
		V.bKept = true;
		V.X = bOwnPlace ? WantX : Tx;
		V.Y = bOwnPlace ? WantY : Ty;
		V.H = bOwnRest ? WantH : H;
		V.S = bOwnRest ? WantS : S;
		V.bOwn = bFlightTilt ? bShows : bOwnTilt && !bToAuto;
		V.Tl = V.bOwn ? (bOwnRest ? WantTl : Tl) : 0.0;
		// (the low look with it: the tilt his own press or hand is bringing it to rest at; where a flight or a wait at
		// a hedge has the turning of it, the tilt on the picture, and none where that holds nothing down at this zoom:
		// the view kept is the picture he has, never a low look he cannot see. Tells: by a quarter of a degree, as his
		// own tilt stands. Kept the frame after a call, a low look just setting out from clear over the automatic tilt
		// told a ten-thousandth of a degree, and the view was kept with HILL VIEW lit for ever over the automatic
		// picture (review, 26 November).)
		V.Hill = bOwnRest ? WantHill : HillShows(S, Hill) ? Hill : HillNone();
		AtView = Number;
		// (kept anew in the middle of a flight: the flight ends at the view just kept, where it stood at the press,
		// his own tilt with it where it shows, and else handed back to the automatic one; a view kept with no flight
		// on begins none)
		if (FlyTo >= 0)
		{
			FlyTo = Number;
			if (bFlightTilt) bToAuto = !bShows;
			bCallToAuto = false;
		}
	}

	bool Eye::ViewIs(int Number, Vec& OutLook, double& OutHeading, double& OutZoom, double& OutTilt) const
	{
		if (!ViewKept(Number)) return false;
		const View& V = Views[Number];
		OutLook = Vec(V.X, V.Y, 0.0);
		OutHeading = V.H;
		OutZoom = V.S;
		OutTilt = (V.bOwn ? TiltOwn(V.S, std::min(V.Tl, TiltMostAt(V.S, V.Hill)), V.Hill) : TiltAt(V.S, V.Hill)) / Rad;
		return true;
	}

	void Eye::FlightLetGo(bool bPanned, bool bPlace, double ZoomIn, double ZoomLeast)
	{
		if (bPlace)
		{
			// (a critically damped spring wanted at X + swing / rate dies away to it with no overshoot)
			const double Rate = bPanned || !bGliding ? Set.Ease : Set.Glide;
			WantX = Tx + SwingX / Rate;
			WantY = Ty + SwingY / Rate;
		}
		// (the turn, the zoom and his tilt are held to a hardest push while flying: each stops in swing squared over
		// twice that push)
		WantH = H + SwingH * std::abs(SwingH) / (2.0 * Set.TurnPush * Rad);
		WantS = S + SwingS * std::abs(SwingS) / (2.0 * Set.ZoomPush * Set.FlySlower);
		// (his own tilt, where the call and not his own hand had the turning of it, to the view's tilt or back to the
		// automatic one. Rule 21, the picture is the truth: it stays his own only if the picture HAS it as his hand
		// takes the flight, and WILL have it where the taking frame leaves him; else it goes on back to the
		// automatic one and is handed back, never left his own, unseen, under it. Turned back by his own hand
		// before the call, it goes on back as he asked.
		// Has it: TiltStands of the view as it stands this frame, S and Tl, and of nothing the zoom or the tilt
		// have yet to do. The call sets his tilt off just under the automatic one's soft floor as the view stands,
		// and whatever then lowers that floor bares it though nobody has turned it: a notch IN on the taking frame,
		// or a zoom still coasting in from his own wheel or from a flight's way down. Asked of the zoom's rest
		// instead (as it was until the sixth check), a call made while the zoom coasted in and taken a frame later
		// left him eleven degrees of a tilt that neither he nor the flight had turned, and a tilt plainly on the
		// picture was handed back where the zoom coasted out and a notch in took the flight. So too a tilt of his
		// own that the picture did not have as he took the flight (swallowed farther out, or hidden by a man's
		// shoulder, at the call or on the flight's way) is handed back whatever the taking notch would have bared:
		// from the call the flight has the turning of it, and it is his again only once the picture has shown it.
		// (Whether, not how much: what he then keeps is its rest, below, and a tilt still swinging up comes to rest
		// past what the picture showed by its stopping way, several degrees at a flight's speed.)
		// Will have it: TiltShows of the tilt's own rest from the swing it has, at the zoom's own rest (WantS
		// above) less ZoomIn, the notch he took the flight with, which this same frame still has to take off the
		// zoom's wish, and no nearer than ZoomLeast (a unit's card pressed follows the unit, never nearer than
		// FollowNearest, and that too is laid on the wish later in this frame). Farther out the automatic tilt is
		// steeper and swallows his; right in by a man's shoulder his is handed toward it (TiltMostAt); so none is
		// left on that he cannot see where he is going.
		// What he keeps is the tilt as it comes to rest, Rest. Toward a man's shoulder the eased tilt is already
		// being pressed down (TlGoal), so a take on the last of the way down to a view kept there keeps what the
		// shoulder has left of his tilt as the take finds it, not the tilt the view was kept with: less than a
		// flight left to end would give him back on a later notch out, and nothing once the picture has lost it.)
		// (the low look's tilt, where the call had the turning of it: let go where its own push stops it, as the
		// turn and the zoom are, so the picture comes to rest under his hand and does not swing on to the kept view's
		// look. Rule 21 again: it stays on, HILL VIEW lit, only where it will still hold the view down where the zoom
		// comes to rest; a low look that would tell on nothing there is none. What his tilt comes to rest over,
		// below, is the tilt the view will take by itself with it.)
		const double ZoomRest = std::clamp(std::max(WantS - ZoomIn, ZoomLeast), 0.0, 1.0);
		if (Hill != WantHill || SwingHill != 0.0)
		{
			const double HillRest = std::max(Hill + SwingHill * std::abs(SwingHill) / (2.0 * Set.TiltPush * Rad * Set.FlySlower), Set.LowTilt * Rad);
			WantHill = HillShows(ZoomRest, HillRest) ? HillRest : HillNone();
		}
		if (bOwnTilt && (bCallToAuto || !bToAuto))
		{
			const double Rest = Tl + SwingTl * std::abs(SwingTl) / (2.0 * Set.TiltPush * Rad * Set.FlySlower);
			bToAuto = !(TiltStands(S, Tl) && TiltShows(ZoomRest, Rest, WantHill));
			WantTl = bToAuto ? TiltAt(ZoomRest, WantHill) - (Set.TiltSoft + 1.0) * Rad : Rest;
		}
		bCallToAuto = false;
		FlyTo = -1;
	}

	void Eye::CallView(int Number)
	{
		if (Number < 0 || Number >= ViewCount) return;
		if (!Views[Number].bKept)
		{
			KeepView(Number);
			return;
		}
		const View& V = Views[Number];
		WantX = V.X;
		WantY = V.Y;
		WantH = V.H;
		WantS = V.S;
		bGliding = true;
		bFlying = true;
		Follow = -1;
		// (the tilt it was kept with: his own, or back to tilting with the zoom; the top view's second press has
		// nothing to go back to once he has flown elsewhere, nor has the hill view's; the hill view comes on or goes
		// as the view was kept)
		bTop = false;
		WantHill = V.Hill;
		bHillBack = false;
		bGoS = false;
		if (V.bOwn)
		{
			OwnTiltBegin();
			bToAuto = false;
			bCallToAuto = false;
			WantTl = V.Tl;
		}
		else if (bOwnTilt)
		{
			// (turned back by this call, or by a call before it that this one follows: not by his own hand)
			bCallToAuto = bCallToAuto || !bToAuto;
			bToAuto = true;
		}
		else bCallToAuto = false;
		AtView = FlyTo = Number;
	}

	void Eye::HillWish(const Input& In)
	{
		if (!In.bHillView) return;
		if (WantHill < HillNone())
		{
			// (pressed again, or pressed with a low look of his own hand's on: back to the zoom and the tilt it had
			// before the first press; where a kept view or his own hand and not his press brought the low look on, the
			// view tilts with the zoom again where it stands. Under the top view the look alone is changed: the top
			// view's own second press goes back as it would have.)
			WantHill = HillNone();
			if (!bTop && bHillBack)
			{
				WantS = GoS = HillWasS;
				bGoS = true;
				if (bHillWasOwn)
				{
					OwnTiltBegin();
					bToAuto = false;
					WantTl = HillWasTl;
				}
				else if (bOwnTilt) bToAuto = true;
			}
			// (no view to go back to: a tilt of his own that stood over the low look is his still only where the
			// picture will have it with none. Rule 21, the picture is the truth: left his own under the automatic tilt,
			// unseen, it came out of nowhere at his next notch in, eight degrees of it (review, 26 November).)
			else if (!bTop && bOwnTilt && !bToAuto && !TiltShows(WantS, WantTl, HillNone())) bToAuto = true;
			bHillBack = false;
		}
		else
		{
			// (on: to the hill, HillUp over what he looks at, whatever the zoom was; his own tilt is handed back, so the
			// view comes to the hill's own low look, and the top view is put out. A second press goes back to the zoom
			// and the tilt he had: those from before the top view, where it was on. A tilt the top view took over a
			// low look that has since gone is his to go back to only where the picture would have it with none.)
			bHillBack = true;
			HillWasS = bTop ? TopWasS : WantS;
			bHillWasOwn = bTop ? bTopWasOwn && (TopWasHill >= HillNone() || TiltShows(TopWasS, TopWasTl, HillNone())) : bOwnTilt && !bToAuto;
			HillWasTl = bTop ? TopWasTl : WantTl;
			bTop = false;
			if (bOwnTilt) bToAuto = true;
			WantHill = Set.HillTilt * Rad;
			WantS = GoS = HillZoom();
			bGoS = true;
		}
		// (the zoom held to its fastest and hardest through the swing, as with the top view: a press may ask two
		// thirds of the zoom at once)
		bHillSwing = true;
	}

	void Eye::TiltWish(const Input& In)
	{
		const double Soft = Set.TiltSoft * Rad;
		LowKey = 0.0;
		auto Own = [this]() { OwnTiltBegin(); };
		if (In.bTopView)
		{
			if (bTop)
			{
				// (pressed again: back to the zoom and the tilt it had, or to tilting with the zoom. His tilt as it was
				// under the low look it stood over; the hill view pressed under the top view since, on or off, it is
				// his again only where the picture will have it under the low look there is now: else back to
				// tilting with the zoom, never a tilt of his own he cannot see.)
				bTop = false;
				WantS = GoS = TopWasS;
				bGoS = PressKept();
				if (bTopWasOwn && (TopWasHill == WantHill || TiltShows(TopWasS, TopWasTl, WantHill))) WantTl = TopWasTl;
				else bToAuto = true;
			}
			else
			{
				bTopWasOwn = bOwnTilt && !bToAuto;
				TopWasTl = WantTl;
				TopWasS = WantS;
				TopWasHill = WantHill;
				Own();
				bToAuto = false;
				bTop = true;
				// (almost straight down, from about TopUp up, or from as high as it already was)
				WantTl = Set.TopTilt * Rad;
				WantS = GoS = std::max(WantS, TopZoom());
				bGoS = PressKept();
			}
		}
		// (nothing of his own on the picture to take off or to add to: no tilt of his own, or one tilted all the way
		// back down)
		const bool bByItself = !bTop && (!bOwnTilt || bToAuto || WantTl <= TiltAt(WantS, WantHill) - Soft - Rad);
		if (In.Tilt < 0.0 && bByItself && Set.bLowByHand)
		{
			// (and on down past the tilt the view takes by itself, flatter, as far as LowTilt: the low look by his
			// own hand (Brian, 26 November: "god mode ... a free camera mode where i can move where every i want").
			// It is the tilt the hill view sets that his hand lowers, from where the view stands, so a press always
			// moves it; HILL VIEW lights, and its press, or his tilt back up, puts the view back to tilting with the
			// zoom. A second press of HILL VIEW has no zoom of a first to go back to: where his hand brings the low look
			// on from none. A hill view his press brought on is the hill view still, however far his hand has taken its
			// look down or back up since: its second press goes back to the zoom and the tilt from before the first
			// (the Mac's check, 27 November 2026: pressed, his keys held to the lowest look, pressed again, and the
			// view stayed at the hill's zoom, half way back).)
			// (From where it is wanted, or lower where it stands lower: where its own push would bring it to rest.
			// Going, by a press or a call, its wish is none while it still stands far down: lowered from the wish, his
			// key held down let it run on up for ten degrees before it turned (review, 26 November).)
			if (bOwnTilt) bToAuto = true;
			if (WantHill >= HillNone()) bHillBack = false;
			const double Cur = std::min(WantHill, HillRest());
			const double From = std::min(Cur, TiltAt(WantS, Cur) + Soft);
			WantHill = std::max(From + In.Tilt * Rad, std::min(From, Set.LowTilt * Rad));
			LowKey = In.Tilt * Rad;
		}
		else if (In.Tilt > 0.0 && bByItself && Set.bLowByHand && WantHill < HillNone())
		{
			// (back up out of a low look: its tilt is raised until it holds nothing down where the view is coming
			// to rest, and then there is none: the view tilts with the zoom again, and his hand's next tilt up is
			// his own tilt as ever. One line under his hand from flat to straight down.)
			// (From where it is wanted, or higher where it still stands higher on its way down to it, as above: the
			// hill view pressed and his key held up at once, it ran on down for twenty degrees first.)
			if (bOwnTilt) bToAuto = true;
			const double Cur = std::max(WantHill, HillRest());
			const double From = std::min(Cur, TiltAt(WantS, Cur) + Soft);
			WantHill = From + In.Tilt * Rad;
			if (WantHill >= TiltAuto(WantS) + Soft)
			{
				// (through: the low look is none, and the line goes on as his own tilt from the tilt his key had
				// reached, not from under the automatic tilt's floor, seven degrees back down the line; the view
				// brings his tilt up under the picture as the low look leaves it, in Update)
				const double Line = WantHill;
				WantHill = HillNone();
				Own();
				bToAuto = false;
				WantTl = std::min(Line, Set.TiltMost * Rad);
				// (the hill view's press has nothing left to go back from: its memory is let go where it could next
				// be read, as his hand brings a low look on from none, above)
			}
		}
		else if (In.Tilt != 0.0)
		{
			Own();
			bTop = false;
			bToAuto = false;
			// (from where the view is tilted, never from under the automatic tilt's floor, so a press always moves it;
			// tilted all the way back down, the view goes back to tilting with the zoom)
			const double Floor = TiltAt(WantS, WantHill) - Soft - Rad;
			WantTl = std::min(std::max(WantTl, Floor) + In.Tilt * Rad, Set.TiltMost * Rad);
			if (In.Tilt < 0.0 && WantTl <= Floor)
			{
				WantTl = Floor;
				bToAuto = true;
			}
		}
		// (to the tilt the view will take by itself where it is coming to rest: with the low look wanted)
		if (bToAuto) WantTl = TiltAt(WantS, WantHill) - Soft - Rad;
		// (the top view keeps its tilt wanted, whatever a wait at a thing let go of: it gets there once clear)
		if (bTop) WantTl = Set.TopTilt * Rad;
	}

	void Eye::Index()
	{
		if (!Rec || Rec->Tracks.size() == TracksSeen) return;
		TracksSeen = Rec->Tracks.size();
		std::map<int, std::vector<int>> Of;
		for (size_t I = 0; I < Rec->Tracks.size(); ++I)
		{
			if (!Rec->Tracks[I].bTwin) Of[Rec->Tracks[I].Block].push_back(static_cast<int>(I));
		}
		ByBlock.assign(Of.begin(), Of.end());
	}

	bool Eye::BlockAt(int Block, double Battle, double& OutX, double& OutY) const
	{
		auto It = std::lower_bound(ByBlock.begin(), ByBlock.end(), Block, [](const std::pair<int, std::vector<int>>& P, int B) { return P.first < B; });
		if (It == ByBlock.end() || It->first != Block) return false;
		double Sx = 0.0, Sy = 0.0;
		int N = 0;
		for (const int I : It->second)
		{
			const LandFeed::Track& Tr = Rec->Tracks[static_cast<size_t>(I)];
			if (Tr.At.empty() || Tr.Start() > Battle || (Tr.Fell >= 0.0 && Tr.Fell <= Battle) || (Tr.Out >= 0.0 && Tr.Out <= Battle)) continue;
			const LandFeed::Spot P = LandFeed::SpotAt(Tr, Battle);
			Sx += P.X;
			Sy += P.Y;
			++N;
		}
		if (N == 0) return false;
		OutX = Sx / N;
		OutY = Sy / N;
		return true;
	}

	void Eye::ClampWanted()
	{
		const double M = Set.Margin;
		const double W = Rec ? Rec->Ground.W : 0.0, Ht = Rec ? Rec->Ground.H : 0.0;
		if (W > 0.0) WantX = std::clamp(WantX, -M, W + M);
		if (Ht > 0.0) WantY = std::clamp(WantY, -M, Ht + M);
		WantS = std::clamp(WantS, 0.0, 1.0);
	}

	bool Eye::Begin(const LandFeed::Recording& InRecording, const LandEye::World& InWorld, const Vec& LookAtIn, double Heading, double Up,
		const Settings& InSettings)
	{
		Rec = &InRecording;
		Here = InWorld;
		Set = InSettings;
		TracksSeen = static_cast<size_t>(-1);
		ByBlock.clear();
		Index();
		// (no low look on: the zooms below are the automatic view's)
		Hill = WantHill = HillNone();
		SwingHill = 0.0;
		bGoS = false;
		GoS = 1.0;
		FollowPt = FollowPtLeft = PtWasId = -1;
		PtVx = PtVy = PtWasX = PtWasY = 0.0;
		bHillBack = bHillWasOwn = bHillSwing = false;
		HillWasS = 1.0;
		HillWasTl = 0.0;
		TopWasHill = HillNone();
		// The zoom whose eye stands Up over the look-at (the height grows with the zoom).
		double Lo = 0.0, Hi = 1.0;
		for (int K = 0; K < 40; ++K)
		{
			const double Mid = 0.5 * (Lo + Hi);
			(UpAt(Mid) < Up ? Lo : Hi) = Mid;
		}
		WantX = Tx = LookAtIn.X;
		WantY = Ty = LookAtIn.Y;
		WantS = S = 0.5 * (Lo + Hi);
		// (and the zoom a flight to a kept view climbs to before it crosses the field)
		Lo = 0.0;
		Hi = 1.0;
		for (int K = 0; K < 40; ++K)
		{
			const double Mid = 0.5 * (Lo + Hi);
			(UpAt(Mid) < Set.FlyUp ? Lo : Hi) = Mid;
		}
		FlyS = 0.5 * (Lo + Hi);
		WantH = H = Heading;
		ClampWanted();
		Tx = WantX;
		Ty = WantY;
		S = WantS;
		SwingX = SwingY = SwingH = SwingS = 0.0;
		Lift = LiftSwing = 0.0;
		TurnHeld = 0.0;
		SweepLift = SweepLiftSwing = 0.0;
		SweepClear = 1e9;
		SweepHeld = -1.0;
		SweepHeldAt = LowKey = 0.0;
		ArmZWas = ArmVelWas = LiftPushWas = 0.0;
		ArmSeen = 0;
		PushX = PushY = PushSwingX = PushSwingY = 0.0;
		bBrake = false;
		BrakeNeed = BrakeFor = StuckFor = 0.0;
		for (AsideMemo& M : Memo) M.When = -1e9;
		Tops.clear();
		Crops.clear();
		bGliding = false;
		Follow = -1;
		Tl = WantTl = SwingTl = TlGoal = 0.0;
		bOwnTilt = bToAuto = bCallToAuto = bTop = bTopWasOwn = false;
		for (View& V : Views) V = View();
		AtView = FlyTo = -1;
		bFlying = false;
		for (int K = 0; K < HandKinds; ++K)
		{
			Quiet[K] = 1.0;
			bLeft[K] = false;
		}
		bFirst = true;
		Now = LandEye::Pose();
		return true;
	}

	Eye::Kept Eye::Keep() const
	{
		Kept K;
		K.GroundEased = GroundEased;
		K.GroundSwing = GroundSwing;
		K.AimZ = AimZ;
		K.PushX = PushX;
		K.PushY = PushY;
		K.PushSwingX = PushSwingX;
		K.PushSwingY = PushSwingY;
		K.Lift = Lift;
		K.LiftSwing = LiftSwing;
		K.NeedWas = NeedWas;
		K.ArmZWas = ArmZWas;
		K.ArmVelWas = ArmVelWas;
		K.LiftPushWas = LiftPushWas;
		K.ArmSeen = ArmSeen;
		K.Hurried = Hurried;
		K.bBrake = bBrake;
		K.BrakeNeed = BrakeNeed;
		K.BrakeFor = BrakeFor;
		K.StuckFor = StuckFor;
		return K;
	}

	void Eye::Restore(const Kept& K)
	{
		GroundEased = K.GroundEased;
		GroundSwing = K.GroundSwing;
		AimZ = K.AimZ;
		PushX = K.PushX;
		PushY = K.PushY;
		PushSwingX = K.PushSwingX;
		PushSwingY = K.PushSwingY;
		Lift = K.Lift;
		LiftSwing = K.LiftSwing;
		NeedWas = K.NeedWas;
		ArmZWas = K.ArmZWas;
		ArmVelWas = K.ArmVelWas;
		LiftPushWas = K.LiftPushWas;
		ArmSeen = K.ArmSeen;
		Hurried = K.Hurried;
		bBrake = K.bBrake;
		BrakeNeed = K.BrakeNeed;
		BrakeFor = K.BrakeFor;
		StuckFor = K.StuckFor;
	}

	int Eye::Place(double Dt, bool bHold, Vec& OutEye, Vec& OutLook)
	{
		// On the arm, then over the ground, the corn and any thing in its way.
		const double ArmNow = ArmAt(S), TiltNow = TiltFor(S, Tl);
		const Vec F2(std::sin(H), std::cos(H), 0.0);
		{
			// (the ground under the look-at, taken over a patch the size of the view and eased, so the view does not
			// bob over every bank, furrow and shell hole it slides across)
			const double R = std::clamp(0.08 * ArmNow * std::cos(TiltNow), 0.0, 25.0);
			double G = Ground(Tx, Ty);
			if (R > 0.5) G = 0.2 * (G + Ground(Tx + R, Ty) + Ground(Tx - R, Ty) + Ground(Tx, Ty + R) + Ground(Tx, Ty - R));
			if (bFirst) { GroundEased = G; GroundSwing = 0.0; }
			else Spring(GroundEased, GroundSwing, G, Set.GroundEase, Dt);
		}
		AimZ = GroundEased + AimAt(S);
		const Vec Look(Tx, Ty, AimZ);
		Vec Eye0 = Look - F2 * (ArmNow * std::cos(TiltNow)) + Vec(0.0, 0.0, ArmNow * std::sin(TiltNow));
		// (how hard the arm itself is taking the eye up or down this frame, metres a second squared: its height with
		// no lift against the two frames before; nothing on the first frames, or where no time has gone)
		double ArmPush = 0.0;
		if (bFirst || Dt <= 1e-4)
		{
			ArmZWas = Eye0.Z;
			ArmVelWas = 0.0;
			ArmSeen = bFirst ? 1 : std::min(ArmSeen, 1);
		}
		else
		{
			const double ArmVel = (Eye0.Z - ArmZWas) / Dt;
			if (ArmSeen >= 2) ArmPush = (ArmVel - ArmVelWas) / Dt;
			ArmZWas = Eye0.Z;
			ArmVelWas = ArmVel;
			ArmSeen = ArmSeen >= 1 ? 2 : 1;
		}
		// (what the eye keeps clear of: the wanted clearance, and a nearer hard one it never comes inside, so the eased
		// lift has room to lag a frame or two without a snap)
		auto FloorAt = [this](const Vec& At, bool bHard = false)
		{
			const Vec Round[] = { Vec(0, 0, 0), Vec(0.8, 0, 0), Vec(-0.8, 0, 0), Vec(0, 0.8, 0), Vec(0, -0.8, 0) };
			double Grounds[5];
			double Highest = -1e9;
			for (int K = 0; K < 5; ++K)
			{
				Grounds[K] = Ground(At.X + Round[K].X, At.Y + Round[K].Y);
				Highest = std::max(Highest, Grounds[K]);
			}
			// (well over the tallest corn there is, the corn is not asked)
			const double Least = bHard ? Set.HardGround : Set.GroundClear, Over = bHard ? Set.HardOverCrop : Set.OverCrop;
			if (At.Z > Highest + 3.0 + Over + 0.5) return Highest + Least;
			double Floor = -1e9;
			for (int K = 0; K < 5; ++K)
			{
				Floor = std::max(Floor, Grounds[K] + std::max(Least, CropAt(At.X + Round[K].X, At.Y + Round[K].Y) + Over));
			}
			return Floor;
		};
		const double Want = Set.ThingClear, Hard = Set.HardClear;
		// (a thing in the way, or the ground or the corn too near: the whole view rises over it, eye and look-at
		// together, as a crane would carry it, so the picture is lifted and never tipped; a tall thing near a low eye,
		// a tree's trunk under its crown or a house's wall, is stepped round instead, the view carried aside)
		auto LiftWithin = [&](const Vec& At, double Most, double& Out)
		{
			const double Base = std::max(0.0, FloorAt(At) - At.Z);
			// (half a metre at a time near, a metre further up)
			for (double L = 0.0; L <= Most + 1e-9; L += L < 4.0 ? 0.5 : 1.0)
			{
				if (Clear(Vec(At.X, At.Y, At.Z + Base + L), Want))
				{
					// (to a tenth, so the eased lift is fed a slope, not a staircase)
					while (L - 0.1 >= -1e-9 && Clear(Vec(At.X, At.Y, At.Z + Base + L - 0.1), Want)) L -= 0.1;
					Out = Base + std::max(0.0, L);
					return true;
				}
			}
			Out = Base + Most;
			return false;
		};
		const double Reach = std::max(Set.LiftLow, 0.5 * (Eye0.Z - Ground(Eye0.X, Eye0.Y)));
		// (open sky over a low eye: no crown or eaves over it, so it can always rise straight up; a view near the
		// ground never stands under a tree, it steps out from under first)
		auto SkyOver = [&](const Vec& At)
		{
			const double Top = Ground(At.X, At.Y) + Set.Tallest;
			for (const double Up : { 1.5, 3.0, 5.0, 8.0, 12.0, 18.0 })
			{
				if (At.Z + Up > Top) break;
				if (!Clear(At + Vec(0.0, 0.0, Up), Hard + 0.2, false)) return false;
			}
			return true;
		};
		// (a step aside from a tall thing at a point: the nearest place round it a short lift clears under open sky,
		// keeping to the side already stepped to; Off is the step the point already has)
		// (the eye barely moves between frames: a step found or not found a moment ago near the same place is taken
		// again, the found one checked once more, so the search runs about once in a fifth of a second)
		auto AsideAt = [&](const Vec& At, const Vec& Off, Vec& OutD, double& OutL)
		{
			for (const AsideMemo& M : Memo)
			{
				if (Clock - M.When > 0.2 || (M.At - At).Length() > 0.4) continue;
				if (!M.bOk) return false;
				double L = 0.0;
				if (LiftWithin(At + M.D, 2.0, L) && SkyOver(At + M.D + Vec(0.0, 0.0, L)))
				{
					OutD = M.D;
					OutL = L;
					return true;
				}
			}
			double Best = 1e9;
			bool bFound = false;
			auto Remember = [&](bool bOk)
			{
				AsideMemo& M = Memo[MemoNext];
				MemoNext = (MemoNext + 1) % static_cast<int>(sizeof(Memo) / sizeof(Memo[0]));
				M.At = At;
				M.D = OutD;
				M.When = Clock;
				M.bOk = bOk;
				return bOk;
			};
			for (const double Rr : { 1.0, 2.0, 3.0, 4.5, 6.0 })
			{
				for (int K = 0; K < 12; ++K)
				{
					const double A = K * Pi / 6.0;
					const Vec D(std::sin(A) * Rr, std::cos(A) * Rr, 0.0);
					double L = 0.0;
					// (a column shut at the short lift's top is shut below it too, near enough: one look, not five)
					if (!Clear(At + D + Vec(0.0, 0.0, std::max(0.0, FloorAt(At + D) - At.Z) + 2.0), Want)) continue;
					if (!LiftWithin(At + D, 2.0, L)) continue;
					const double Cost = Rr + 2.0 * L + 0.3 * std::hypot(Off.X + D.X - PushX, Off.Y + D.Y - PushY);
					if (Cost < Best && SkyOver(At + D + Vec(0.0, 0.0, L)))
					{
						Best = Cost;
						OutD = D;
						OutL = L;
						bFound = true;
					}
				}
				if (bFound) return Remember(true);
			}
			return Remember(false);
		};
		// (over a tall thing that cannot be stepped round, a tree standing in a hedge: how high the view must go, in
		// coarse steps from where a short lift failed)
		// (a thing's top stays where it is: kept by the metre square for a moment, as the tent looks at the same
		// trees frame after frame)
		auto TopOver = [&](const Vec& At, double From)
		{
			const long long Key = (static_cast<long long>(std::floor(At.X)) << 32) ^ static_cast<long long>(static_cast<unsigned int>(static_cast<int>(std::floor(At.Y))));
			const auto It = Tops.find(Key);
			if (It != Tops.end() && Clock - It->second.second < 0.5 && It->second.first - At.Z >= From + 1.5 - 1e-9) return std::min(Set.LiftMost, It->second.first - At.Z);
			double Top = Set.LiftMost;
			for (double L = From + 1.5; L <= Set.LiftMost + 1e-9; L += 1.5)
			{
				if (Clear(Vec(At.X, At.Y, At.Z + L), Want))
				{
					Top = L;
					break;
				}
			}
			if (Tops.size() > 4096) Tops.clear();
			Tops[Key] = std::make_pair(At.Z + Top, Clock);
			return Top;
		};
		double Need = 0.0;
		Vec PushWant;
		const bool bLow = Eye0.Z < Ground(Eye0.X, Eye0.Y) + Set.Tallest + 2.0;
		// (a low look on, or wanted: the free view's own rules for a far eye among the trees run; never without one)
		const bool bLowLook = Hill < HillNone() || WantHill < HillNone();
		if (bHold)
		{
			// (held where it was: the step aside as it is, and the lift there)
			PushWant = Vec(PushX, PushY, 0.0);
			if (bLow && !LiftWithin(Eye0 + PushWant, Reach, Need)) LiftWithin(Eye0 + PushWant, Set.LiftMost, Need);
		}
		else if (bLow && (!LiftWithin(Eye0, Reach, Need) || !SkyOver(Eye0 + Vec(0.0, 0.0, Need))))
		{
			if (!AsideAt(Eye0, Vec(), PushWant, Need))
			{
				// (no step aside found: stay where the view has stepped to, and rise there)
				PushWant = Vec(PushX, PushY, 0.0);
				LiftWithin(Eye0 + PushWant, Set.LiftMost, Need);
			}
		}
		else if (bLow)
		{
			// (the lift is shaped like a tent over each thing round the eye, sloping Slope m a metre: the eye rises
			// before it reaches a hedge whichever way it comes, by a slide, a turn or a zoom as well as a pan)
			for (const double Rr : { 1.5, 3.0, 4.5, 6.0 })
			{
				if (Rr * Set.Slope <= 0.0) break;
				for (int K = 0; K < 8; ++K)
				{
					const double A = (K + 0.5 * (Rr == 3.0 || Rr == 6.0)) * Pi / 4.0;
					double L = 0.0;
					const Vec At = Eye0 + Vec(std::sin(A) * Rr, std::cos(A) * Rr, 0.0);
					if (!LiftWithin(At, Reach, L)) L = TopOver(At, L);
					Need = std::max(Need, L - Set.Slope * Rr);
				}
			}
		}
		// (what the things here, under and beside the eye, ask of the lift, before any look ahead)
		const double NeedHere = Need;
		// (The step aside's own way. Where it is wanted may lie across a thing from where the view has stepped to, a
		// place round a trunk found on the trunk's far side. With the free view taken or a low look on, the way
		// there is marched along a quarter of a metre at a time, at the height the eye has, and the step goes no
		// further than the last place clear of the near clearance, found to a few centimetres: it slows into that
		// place as into any other, and the looks ahead take it to be going there. Sent on to where it was wanted,
		// it ran into the trunk at 9 m a second and stopped dead in a frame; see where the eye is placed, below.
		// A build that sets nothing sends it on as it did.)
		const bool bStepWay = (bLowLook || Set.Freed()) && !bFirst && !bHold;
		auto StepTo = [&](const Vec& To)
		{
			const Vec From(PushX, PushY, 0.0), Way = To - From;
			const double Len = Way.Length();
			if (!bStepWay || Len < 0.05) return To;
			auto Open = [&](double D)
			{
				const Vec At = Eye0 + From + Way * (D / Len) + Vec(0.0, 0.0, Lift);
				return At.Z >= FloorAt(At, true) && Clear(At, Hard, false);
			};
			const double Most = std::min(Len, 3.0);
			double Lo = 0.0, Hi = -1.0;
			for (double D = std::min(0.25, Most);; D = std::min(Most, D + 0.25))
			{
				if (!Open(D))
				{
					Hi = D;
					break;
				}
				Lo = D;
				if (D >= Most - 1e-9) break;
			}
			if (Hi < 0.0) return To;
			for (int K = 0; K < 3; ++K)
			{
				const double Mid = 0.5 * (Lo + Hi);
				if (Open(Mid)) Lo = Mid;
				else Hi = Mid;
			}
			return From + Way * (Lo / Len);
		};
		Vec PushGo = StepTo(PushWant);
		// (how fast and hard the lift may climb: by the eye's distance to what it looks at, so a view near the ground
		// climbs a hedge gently and a high one is not held back)
		const double Far = std::max(ArmNow, 1.0);
		const double Fastest = std::max(1.5, Set.LiftSpeed * Far), Hardest = Set.LiftPush * Far;
		// (the way up shut over the eye, under a crown or eaves: the lift reaches no higher than that, however fast it
		// may climb; in Sedan's woods a turn under a crown ran on into the next tree and stopped dead in a frame)
		double Ceiling = 1e9;
		if (bLow && !bFirst && !bHold)
		{
			const Vec Column = Eye0 + Vec(PushX, PushY, 0.0);
			for (double Up = 0.5; Up <= 2.0 + 1e-9; Up += 0.5)
			{
				if (!Clear(Column + Vec(0.0, 0.0, Lift + Up), Hard, false))
				{
					Ceiling = Lift + Up - 0.5;
					break;
				}
			}
		}
		// (the lift it could reach in Tau seconds at the most, a little short of it)
		auto Reachable = [&](double Tau)
		{
			const double V0 = std::clamp(LiftSwing, 0.0, Fastest);
			const double T1 = std::min(Tau, (Fastest - V0) / Hardest);
			return std::min(Ceiling, Lift + 0.8 * (V0 * T1 + 0.5 * Hardest * T1 * T1 + Fastest * (Tau - T1)));
		};
		// (the way the look-at, the zoom, the turn and the tilts are going, each no further than where it is wanted: the
		// springs stop there, so a zoom coming in is not carried on into the ground)
		auto Toward = [](double X, double V, double W, double Tau) { return X + std::clamp(V * Tau, std::min(0.0, W - X), std::max(0.0, W - X)); };
		// (and the low look's key held down: the low look is wanted only as far ahead as its spring trails his key, a
		// third of a second of its travel, yet it will go on down as long as the key is held. Looked along at the rate
		// it goes, or the key's where it is still gathering, as far as its end. Far out the arm's eye comes down 230 m
		// a second under that key: seen a third of a second off, the trees it comes down among braked the whole view
		// in mid air, 85 m up, and the lift was thrown up 28 m to meet them.)
		const double LowAhead = bLowLook && LowKey < 0.0 && Dt > 1e-4 ? std::min(SwingHill, LowKey / Dt) : 0.0;
		const double LowEnd = std::min(Hill, Set.LowTilt * Rad);
		// (how high the arm's eye will stand so far on, with no lift, and how far back from what it looks at)
		auto ArmAhead = [&](double Tau, double& OutUp, double& OutFlat)
		{
			const double Sa = std::clamp(Toward(S, SwingS, WantS, Tau), 0.0, 1.0);
			const double A = ArmAt(Sa), Ta = TiltFor(Sa, Toward(Tl, SwingTl, TlGoal, Tau), LowAhead != 0.0 ? std::max(LowEnd, Hill + LowAhead * Tau) : Toward(Hill, SwingHill, WantHill, Tau));
			OutUp = GroundEased + AimAt(Sa) + A * std::sin(Ta);
			OutFlat = A * std::cos(Ta);
		};
		// (a low look's turn that would sweep the arm's eye fast among the trees' height, Settings::LowSweep: its way
		// round looked along, at the height the eye would have with no rise and at the height it has now. A thing on
		// the first: the view is to rise over everything on the way. How far the second is clear holds the turn.)
		// (And a turn held while his tilt key or his zoom brings the arm's eye DOWN among the trees' height: there the
		// way round is looked along where the eye is coming to, 1.6 s on, and the rise is asked from the start, all of
		// it, so the view comes down no lower than over everything on its way and is never met by a tree as it
		// arrives. Left to the looks ahead, a wood the eye came down into at 290 m a second sideways was seen a tree
		// in three, a fifth of a second off: the turn was braked from 75 degrees a second at 813, let go at 900, and
		// held again.)
		bool bSweep = false;
		if (!bFirst && !bHold && Dt > 1e-4)
		{
			double RiseTo = 0.0;
			SweepClear = 1e9;
			const double Flat = std::max(1.0, ArmNow * std::cos(TiltNow));
			const double Rate = std::max(std::abs(TurnHeld), std::abs(SwingH));
			double Base = Eye0.Z, FlatBase = Flat;
			if (bLowLook && !bFlying)
			{
				for (const double Tau : { 0.4, 0.8, 1.2, 1.6 })
				{
					double Up = 0.0, Back = 0.0;
					ArmAhead(Tau, Up, Back);
					if (Up < Base)
					{
						Base = Up;
						FlatBase = std::max(1.0, Back);
					}
				}
			}
			const bool bDown = Base < Eye0.Z - 1.0;
			if (!bDown)
			{
				Base = Eye0.Z;
				FlatBase = Flat;
			}
			const bool bLowSoon = bLow || Base < Ground(Eye0.X, Eye0.Y) + Set.Tallest + 2.0;
			if (bLowLook && bLowSoon && !bFlying && Set.LowSweep > 0.0 && Set.AheadStep > 0.0 && Rate * std::max(Flat, FlatBase) > Set.LowSweep)
			{
				bSweep = true;
				const double Dir = TurnHeld != 0.0 ? (TurnHeld > 0.0 ? 1.0 : -1.0) : SwingH > 0.0 ? 1.0 : -1.0;
				const int Most = std::max(1, Set.AheadMost);
				const double Way = std::min(Rate * Flat * 1.6, Most * Set.AheadStep);
				const int Looks = std::clamp(static_cast<int>(std::ceil(Way / Set.AheadStep)), 1, Most);
				// (each look reaches half way to the next, so nothing stands unseen between two)
				const double Round = std::max(Want, 0.5 * Way / Looks);
				const bool bSame = std::abs(Lift - Need) < 0.5;
				// (and at the height the rise has asked so far, which the eased lift is still coming up to: the rise is
				// enough once the way is clear THERE. Asked of the height the eye had, the rise went on toward over
				// everything for as long as the lift lagged behind it: 14 m up over a farm's yard that 3.2 m cleared.)
				const double RiseAsk = std::max(Lift, SweepLift);
				const bool bSameAsk = RiseAsk - Lift < 0.5;
				double Over = Ground(Eye0.X, Eye0.Y), FirstBase = 1e9, FirstNow = 1e9, FirstAsk = 1e9;
				for (int K = 1; K <= Looks; ++K)
				{
					const double Along = Way * K / Looks, Ha = H + Dir * Along / Flat;
					const Vec At = Vec(Tx + PushX, Ty + PushY, Eye0.Z) - Vec(std::sin(Ha), std::cos(Ha), 0.0) * Flat;
					Over = std::max(Over, Ground(At.X, At.Y));
					const double Floor = std::max(0.0, FloorAt(At) - At.Z);
					if (bDown)
					{
						// (coming down: the same turn of the way round, where the eye will be, with no rise; further
						// back there the looks stand further apart, and each reaches as much further)
						const Vec Low = Vec(Tx + PushX, Ty + PushY, Base) - Vec(std::sin(Ha), std::cos(Ha), 0.0) * FlatBase;
						Over = std::max(Over, Ground(Low.X, Low.Y));
						if (FirstBase > 1e8 && !Clear(Vec(Low.X, Low.Y, Low.Z + std::max(0.0, FloorAt(Low) - Low.Z)), Round * std::max(1.0, FlatBase / Flat))) FirstBase = Along;
					}
					else if (FirstBase > 1e8 && !Clear(Vec(At.X, At.Y, At.Z + std::max(Need, Floor)), Round)) FirstBase = Along;
					// (coming down, the first was looked at where the eye is coming to, not where it is)
					if (FirstNow > 1e8 && (bSame && !bDown ? FirstBase < 1e8 : !Clear(Vec(At.X, At.Y, At.Z + std::max(Lift, Floor)), Round))) FirstNow = Along;
					if (FirstAsk > 1e8 && (bSameAsk ? FirstNow < 1e8 : !Clear(Vec(At.X, At.Y, At.Z + std::max(RiseAsk, Floor)), Round))) FirstAsk = Along;
				}
				if (FirstBase < 1e8)
				{
					// (up toward over everything while a thing still stands on the way at the height asked so far; once
					// the way is clear there, no higher than that, kept while the turn goes on)
					// (coming down, all of it, over everything where the eye is coming to: what clears is found once
					// the arm's eye is there)
					const double All = std::clamp(Over + Set.Tallest + 2.0 - Base, 0.0, Set.LiftMost);
					if (bDown || FirstAsk < 1e8) SweepHeld = -1.0;
					else if (SweepHeld < 0.0)
					{
						// (and the ask stops there: it is only asked, the lift's own spring eases what the picture
						// sees; left its swing, 39 m/s by then toward over everything, it ran on 7 m past)
						SweepHeld = std::min(All, RiseAsk + 0.5);
						SweepHeldAt = Eye0.Z;
						SweepLiftSwing = std::min(SweepLiftSwing, 0.0);
					}
					// (kept as a height, not as a rise: his tilt key taken down while he turns brings the arm's eye
					// down under it, and the rise asked grows by as much. Kept as a rise, found clear while the arm's
					// eye still stood 8 m higher, it was 8 m short a second later: the lift settled to it, down among
					// the tree tops, and the turn was held again from 75 degrees a second to 8, with no key touched.)
					RiseTo = SweepHeld < 0.0 ? All : std::clamp(SweepHeld + SweepHeldAt - Eye0.Z, 0.0, All);
				}
				SweepClear = FirstNow;
			}
			if (RiseTo <= 0.0) SweepHeld = -1.0;
			Spring(SweepLift, SweepLiftSwing, RiseTo, RiseTo > SweepLift ? Set.SweepRise : Set.SweepSettle, Dt);
			if (RiseTo <= 0.0 && SweepLift < 0.01 && std::abs(SweepLiftSwing) < 0.01) SweepLift = SweepLiftSwing = 0.0;
		}
		// (How high that rise may take the eye: it is asked ahead of any need, so never past the Ceiling, and under a
		// crown no further than the way up is open at the wanted clearance. Left to rest inside it, half a metre
		// under the leaves, the rule that keeps a settling view out of a crown asked the whole way up through it,
		// and the stuck rule would cut for a tree not yet met.)
		double SweepRoof = Ceiling;
		if ((bSweep || SweepLift > 0.0) && bLow && !bFirst && !bHold)
		{
			const Vec Column = Eye0 + Vec(PushX, PushY, 0.0);
			for (double Up = 0.0; Up <= 2.5 + 1e-9; Up += 0.25)
			{
				if (!Clear(Column + Vec(0.0, 0.0, Lift + Up), Want))
				{
					SweepRoof = std::min(SweepRoof, Lift + std::max(0.0, Up - 0.25));
					break;
				}
			}
		}
		bool bBrakeNow = false;
		double BrakeNeedNow = 0.0;
		if (!bFirst && !bHold && Dt > 1e-4)
		{
			// (and where the arm is carrying the eye over the next 1.6 s, so it climbs at most Climb m/s before it gets
			// there, as an operator sees the hedge coming and stands up; a tall thing coming is stepped round early)
			// (the way the look-at, the zoom and the turn are going, each no further than where it is wanted: the
			// springs stop there, so a zoom coming in is not carried on into the ground)
			const double WantHNear = WrapNear(WantH, H);
			auto ArmEyeAt = [&](double Tau)
			{
				const double Xa = Toward(Tx, SwingX, WantX, Tau), Ya = Toward(Ty, SwingY, WantY, Tau);
				const double Ha = Toward(H, SwingH, WantHNear, Tau);
				double Up = 0.0, Flat = 0.0;
				ArmAhead(Tau, Up, Flat);
				return Vec(Xa, Ya, Up) - Vec(std::sin(Ha), std::cos(Ha), 0.0) * Flat;
			};
			// (a view coming down from over everything, the top view let go or a kept view called from high: where it
			// will be is looked at while it is still up there, so the lift is waiting when the eye arrives. Looked at
			// only once the eye itself was among the trees' height, a fall of 100 m a second from the top view to a
			// hill view over a tree line was braked in three frames: 2,205 deg/s/s as the picture sees it)
			// (Only with a low look on, or where the scene has taken the free view: a build that sets nothing and never
			// lowers the look comes down as it always did, to the last bit.)
			const Vec End = ArmEyeAt(1.6);
			const bool bComing = !bLow && (bLowLook || Set.Freed()) && End.Z < Ground(End.X, End.Y) + Set.Tallest + 2.0;
			// (the reach of a look ahead never more than a low eye's own: the highest eye that is low has this much)
			const double ReachAhead = std::min(Reach, std::max(Set.LiftLow, 0.5 * (Set.Tallest + 2.0)));
			if ((bLow || bComing) && (End - Eye0).Length() > 0.05)
			{
				// (and where the lift cannot climb in time, within the time the view takes to stop, it brakes)
				const double StopIn = 2.0 / (bGliding ? Set.Glide : std::min({ Set.Ease, Set.ZoomEase, Set.TurnEase, bOwnTilt || bHillSwing ? Set.TiltEase : Set.Ease })) + 0.15;
				for (int K = 1; K <= 16; ++K)
				{
					const double Tau = 0.1 * K;
					// (and the step aside as it is going there)
					const Vec Ahead = ArmEyeAt(Tau) + Vec(Toward(PushX, PushSwingX, PushGo.X, Tau), Toward(PushY, PushSwingY, PushGo.Y, Tau), 0.0);
					double L = 0.0;
					bool bTall = false;
					if (!LiftWithin(Ahead, ReachAhead, L))
					{
						Vec D;
						if (AsideAt(Ahead, PushWant, D, L) && (PushWant + D).Length() <= Set.PushMost)
						{
							PushWant = PushWant + D;
							PushGo = StepTo(PushWant);
						}
						else L = TopOver(Ahead, L);
						bTall = true;
					}
					Need = std::max(Need, L - Set.Climb * Tau);
					if (Tau <= StopIn && L - 0.4 > Reachable(Tau))
					{
						bBrakeNow = true;
						BrakeNeedNow = std::max(BrakeNeedNow, L);
					}
					// (and where the eye will stand by then, stepped aside and lifted only as far as the springs are
					// carrying it now, inside the near clearance: it would stop dead there in a frame (a turn under a
					// crown in Sedan's woods, 3,382 deg/s/s); it brakes now instead)
					// (With a low look on, a far eye's lift is fast: it is taken where its own spring will have brought
					// it by then, at the most what it can reach, not only where it is going now. Taken as it is going,
					// a pan far out over hedges braked at each, a few metres of lift the spring had in a tenth of a
					// second, and went on, several times a second.)
					double LiftBy = Toward(Lift, LiftSwing, std::max(Lift, L), Tau);
					if (bLowLook)
					{
						const double E = Set.RiseRate * Tau;
						LiftBy = std::max(LiftBy, std::min(Reachable(Tau), Lift + (std::max(Lift, L) - Lift) * (1.0 - (1.0 + E) * std::exp(-E))));
					}
					if (Tau <= StopIn && !bBrakeNow && !Clear(Ahead + Vec(0.0, 0.0, LiftBy), Hard, false))
					{
						bBrakeNow = true;
						BrakeNeedNow = std::max(BrakeNeedNow, L);
					}
					if (bTall) break;
				}
			}
		}
		if (bBrake && (Lift >= BrakeNeed - 0.3 || BrakeFor > Set.BrakeLongest))
		{
			// (climbed, or waited long enough: on again; after a long wait, not braking again for a second)
			BrakeFor = BrakeFor > Set.BrakeLongest ? -1.0 : 0.0;
			bBrake = false;
		}
		if (bBrakeNow && !bFirst && BrakeFor >= 0.0)
		{
			BrakeNeed = bBrake ? std::max(BrakeNeed, BrakeNeedNow) : BrakeNeedNow;
			bBrake = true;
		}
		if (bBrake) Need = std::max(Need, BrakeNeed);
		// (gliding fast and low, as a drone flies: lifted with its speed, so the trees on its way are under it, not met in
		// a frame between two looks ahead; it settles as the glide slows. Never past the Ceiling over it: under a crown the
		// glide's own lift would leave the lift short of its need, and the stuck rule would cut for nothing seen; the
		// pre-check's read of 6240956e)
		if (bLow && bGliding && !bFirst) Need = std::max(Need, std::min({ Set.LiftMost, Set.GlideLift * std::hypot(SwingX, SwingY), Ceiling }));
		// (and the rise a low look's fast turn asks: over what stands on its way round, no further than the way up is
		// open either)
		if (!bFirst && SweepLift > 0.0) Need = std::max(Need, std::min(SweepLift, SweepRoof));
		const double PushWasX = PushX, PushWasY = PushY;
		if (bFirst)
		{
			PushX = PushWant.X;
			PushY = PushWant.Y;
			PushSwingX = PushSwingY = 0.0;
		}
		else if (bHold) PushSwingX = PushSwingY = 0.0;
		else
		{
			Spring(PushX, PushSwingX, PushGo.X, Set.PushRate, Dt);
			Spring(PushY, PushSwingY, PushGo.Y, Set.PushRate, Dt);
		}
		Vec Eye1 = Eye0 + Vec(PushX, PushY, 0.0);
		// (coming down after a hedge, never down through a tree's crown or a roof: the view settles only as far as the
		// way down is open, and stays over the thing until it is passed)
		auto UpFrom = [&](double L, double R)
		{
			const double From = L;
			for (; L <= Set.LiftMost + 1e-9; L += 0.5)
			{
				if (Clear(Eye1 + Vec(0.0, 0.0, L), R, R >= Want))
				{
					// (and no higher than it must: back down in tenths toward where it began)
					while (L - 0.1 >= From && Clear(Eye1 + Vec(0.0, 0.0, L - 0.1), R, R >= Want)) L -= 0.1;
					return L;
				}
			}
			return Set.LiftMost;
		};
		if (!bFirst && Need < Lift)
		{
			if (!Clear(Eye1 + Vec(0.0, 0.0, Lift), Want)) Need = std::max(Need, UpFrom(Lift, Want));
			else
			{
				double L = Lift;
				while (L - 0.5 >= Need && Clear(Eye1 + Vec(0.0, 0.0, L - 0.5), Want)) L -= 0.5;
				if (L - 0.5 >= Need) Need = L;
			}
		}
		const double LiftBefore = Lift, SwingBefore = LiftSwing;
		NeedWas = Need;
		Hurried = 0;
		if (bFirst) Lift = Need;
		else
		{
			// (eased, and never faster or harder than the picture bears)
			// (and never up into a thing: it climbs only as far as the way up is open, a little short of it)
			double Goal = Need;
			if (Need > Lift + 0.05)
			{
				for (double Up = 0.5; Up <= 2.0 + 1e-9 && Lift + Up - 0.5 < Need; Up += 0.5)
				{
					if (!Clear(Eye1 + Vec(0.0, 0.0, Lift + Up), Hard, false))
					{
						Goal = std::min(Need, Lift + Up - 0.5);
						break;
					}
				}
			}
			// (shut in under leaves or eaves with nowhere to step: a moment, then a cut to where it should be)
			StuckFor = Goal < Need - 0.5 ? StuckFor + Dt : 0.0;
			// (over a thing while the zoom brings the arm down, the lift must climb as fast as the arm comes down: led
			// by that speed, so the eased lift does not lag behind it into the near clearance)
			if (Goal > 0.05 && Goal >= Need - 1e-9)
			{
				// (and with his own tilt, as fast as the tilt brings it down too; and as fast as the low look does, coming on)
				const double HillOn = Hill + 1e-3 * SwingHill;
				const double ArmDown = bOwnTilt ? -((UpOwn(S + 1e-3 * SwingS, Tl + 1e-3 * SwingTl, HillOn) - UpOwn(S, Tl)) / 1e-3 + GroundSwing)
					: SwingHill != 0.0 ? -((UpAt(S + 1e-3 * SwingS, HillOn) - UpAt(S)) / 1e-3 + GroundSwing)
					: -((UpAt(S + 1e-3) - UpAt(S)) / 1e-3 * SwingS + GroundSwing);
				// (With the free view taken or a low look on: what the things HERE ask is led, all of it; what the
				// look ahead asks over that is led by no more than the things here ask, nothing where they ask
				// nothing; and what a far low turn's rise asks is not led at all, being asked from the start.
				// All that was asked led by all of the arm's fall, a need seen ahead of an eye still far over
				// everything, coming down under his tilt key, threw the lift up by a tenth of a second of that
				// fall, 28 m, and it settled for a second and a half afterwards. With only what the things here ask
				// led, a zoom brought in over a roof left the lift a metre short of what the look ahead asked: the
				// view braked and the eye was thrown up at 1,355 degrees a second squared as the picture sees it
				// (Normandy, 27 November). A build that sets nothing leads all of it as it did, to the last bit.)
				const double Lead = std::min(ArmDown, Fastest) * 2.0 / Set.RiseRate;
				if (ArmDown > 0.0) Goal = bLowLook || Set.Freed() ? std::max(Goal + (bSweep ? 0.0 : std::min(Lead, NeedHere)), NeedHere > 0.05 ? NeedHere + Lead : 0.0) : Goal + Lead;
			}
			double L = Lift, Sw = LiftSwing;
			// (With a low look on, a lift still swinging up as it comes to the end of the way up under a crown is stopped
			// as firmly as it rose. Eased off at the settle's rate, as a lift coming down is, it coasted on up into
			// the leaves' near clearance, was hurried through them and the picture cut: a far turn's rise begun under
			// a crown, 5 m/s of swing left half a metre under it, the planted wood's bench.)
			const bool bStopUp = bLowLook && Ceiling < 1e8 && LiftSwing > 0.0;
			Spring(L, Sw, Goal, Goal > Lift || bStopUp ? Set.RiseRate : Set.SettleRate, Dt);
			// (With the free view taken or a low look on, the lift's push up gives way to the arm's own push upward,
			// so the two together stay under what the picture bears, RisePush. A top view pressed while the lift was
			// coming down by a tree line: a crown came within reach as the arm swung the eye up, the lift turned at
			// its hardest on top of the flight's own gathering, and the picture saw 1,334 degrees a second squared
			// (Normandy's own hill run at 220.8 s, 27 November; 1,201 to 1,377 on three seeded runs). A build that
			// sets nothing pushes as it did, to the last bit.)
			double PushUp = Hardest;
			if (bLowLook || Set.Freed())
			{
				// (The lift's height goes on by the mean of this frame's swing and the last, so what the picture sees
				// of it this frame is half this frame's push and half the last one's: this frame's is held to what
				// the two leave under the most, and to no more than the arm leaves by itself, so a lift given way
				// one frame does not make it up the next and saw. And, braking at a thing, to the push the arm's
				// stop is about to give: the brake lets each move die away at its own spring's rate, and the stop
				// is at its hardest two frames after the brake is put on, so an arm coming down at 44 m/s under a
				// wheel brought in at a steep tilt of his own is stopped at 395 metres a second squared, 1,100
				// degrees of the picture by itself; the lift, asked at that moment for all it has, pushed the same
				// way: 1,377, and 1,318 with a quarter of its push kept. It never pushes down to make room: where
				// the arm's own push is over the most, the lift's swing is left as it is. Downward the lift keeps
				// all its push: a rise stopped under leaves, above, must stay as firm as it is.)
				const double Most = Set.RisePush * Far;
				const double Stop = bBrake ? -std::max(Set.ZoomEase, Set.TiltEase) * ArmVelWas : ArmPush;
				const double Room = Most - std::max(ArmPush, Stop);
				PushUp = std::clamp(std::min(Room, 2.0 * Room - LiftPushWas), 0.0, Hardest);
			}
			Sw = std::clamp(Sw, LiftSwing - Hardest * Dt, LiftSwing + PushUp * Dt);
			// (a swing already over the fastest, the hurried lift's: with the free view taken or a low look on it is
			// slowed to the fastest no harder than the push, not cut to it in a frame. A low look by a man's
			// shoulder, braked under a tree, was hurried up at 20 m/s and cut to 10 in the next frame: 1,900 to
			// 3,400 degrees a second squared as the picture sees it; review, 26 November. And as near with no low
			// look on, the automatic tilt leaves the eye a hand higher and no further off: the same cut, 2,750 on
			// the bench. A build that sets nothing and never lowers the look cuts it as it did.)
			const double FastNow = bLowLook || Set.Freed() ? std::max(Fastest, std::abs(LiftSwing) - Hardest * Dt) : Fastest;
			Sw = std::clamp(Sw, -FastNow, FastNow);
			Lift += 0.5 * (LiftSwing + Sw) * Dt;
			LiftPushWas = Dt > 1e-4 ? (Sw - LiftSwing) / Dt : 0.0;
			LiftSwing = Sw;
		}
		Vec EyeAt = Eye1 + Vec(0.0, 0.0, Lift);
		// (And the frame's own step, however its way was found: where the step's travel, not the move, would carry
		// the eye inside the near clearance, a thing thinner than the march's stride or a lift come down since,
		// the step waits where it is and the frame is the move's. Taken as a move gone too far, the whole view
		// waited that frame and braked; the frame after, braked, it moved nothing, the step tried again, and so
		// every frame after: no wheel, pan or turn was answered again, 375 seconds to the end of a run on
		// Normandy, 27 November. Only with the free view taken or a low look on: a build that sets nothing waits
		// there as it did, to the last bit.)
		if ((bLowLook || Set.Freed()) && !bFirst && !bHold && (PushX != PushWasX || PushY != PushWasY) && (EyeAt.Z < FloorAt(EyeAt, true) || !Clear(EyeAt, Hard, false)))
		{
			const Vec Stayed = Eye0 + Vec(PushWasX, PushWasY, Lift);
			if (Stayed.Z >= FloorAt(Stayed, true) && Clear(Stayed, Hard, false))
			{
				PushX = PushWasX;
				PushY = PushWasY;
				PushSwingX = PushSwingY = 0.0;
				Eye1 = Eye0 + Vec(PushX, PushY, 0.0);
				EyeAt = Stayed;
			}
		}
		if (EyeAt.Z < FloorAt(EyeAt, true) || !Clear(EyeAt, Hard, false))
		{
			// (the spring is late: inside the near clearance it is hurried up to it, fast but eased; only an eye that
			// would stand in the thing itself, or at the ground, is put clear at once)
			const double Floor = FloorAt(EyeAt, true) - Eye1.Z;
			const double Up = UpFrom(std::max(Lift, Floor), Hard);
			const double Was = Lift;
			Hurried = 1;
			if (!bFirst)
			{
				// (hurried, but its push held under what the picture bears at this distance, HurryPush: at the nearest
				// zoom, a few metres from its look-at, a spring left free lifted the whole view 2 cm in a frame, 1,274
				// degrees a second squared in Kursk; what it cannot climb in time is put clear with a cut, below)
				double L = LiftBefore, Sw = SwingBefore;
				Spring(L, Sw, std::max(Up, Need), Set.HurryRate, Dt);
				const double Push = Set.HurryPush * Far;
				Sw = std::clamp(Sw, SwingBefore - Push * Dt, SwingBefore + Push * Dt);
				Lift = LiftBefore + 0.5 * (SwingBefore + Sw) * Dt;
				LiftPushWas = Dt > 1e-4 ? (Sw - SwingBefore) / Dt : 0.0;
				LiftSwing = Sw;
			}
			Lift = std::max(Lift, Was);
			EyeAt = Eye1 + Vec(0.0, 0.0, Lift);
			// (more than a small step in a frame is a jolt the picture would feel: better a cut)
			if (EyeAt.Z < Ground(EyeAt.X, EyeAt.Y) + 0.25 || !Clear(EyeAt, 0.05, false) || Lift - Was > Set.HurryMost)
			{
				Hurried = 2;
				Lift = Up;
			}
		}
		if (Hurried == 2 || StuckFor > Set.StuckMost)
		{
			// (put clear at once, at the height it wants, and the picture cuts)
			Hurried = 2;
			Lift = UpFrom(std::max(Lift, Need), Want);
			LiftSwing = LiftPushWas = 0.0;
			StuckFor = 0.0;
			bBrake = false;
			EyeAt = Eye1 + Vec(0.0, 0.0, Lift);
		}
		OutEye = EyeAt;
		OutLook = Look + Vec(PushX, PushY, Lift);
		return Hurried;
	}

	LandEye::Pose Eye::Update(const Input& Given, double RealDt, double Battle)
	{
		if (!Rec) return Now;
		Index();
		const double Dt = std::clamp(RealDt, 0.0, 0.25);
		Clock += Dt;
		Asked = 0;
		const Vec Fwd(std::sin(H), std::cos(H), 0.0);
		const Vec Right(std::cos(H), -std::sin(H), 0.0);
		const double Arm = ArmAt(S), Tilt = TiltFor(S, Tl);
		// (the keys' pan by the zoom alone, as fast at his own tilt as at the automatic one: looking straight down the
		// arm's reach across the ground is nothing)
		const double Across = Arm * std::cos(TiltAt(S));

		// What the player asked.
		Input In = Given;
		{
			const bool Moved[HandKinds] = { In.PanRight != 0.0 || In.PanAhead != 0.0, In.SlideRight != 0.0 || In.SlideAhead != 0.0, In.Zoom != 0.0, In.Turn != 0.0,
				In.Tilt != 0.0 };
			for (int K = 0; K < HandKinds; ++K) Quiet[K] = Moved[K] ? 0.0 : Quiet[K] + Dt;
		}
		if (In.KeepView >= 0) KeepView(In.KeepView);
		if (In.CallView >= 0)
		{
			const bool bFlies = ViewKept(In.CallView);
			CallView(In.CallView);
			// (what was moving the view as he called is left out until it stops: a key still held, and a trackpad's
			// slide that runs on after the fingers lift, would else take the view from the call at once)
			for (int K = 0; bFlies && K < HandKinds; ++K) bLeft[K] = bLeft[K] || Quiet[K] < Set.LeftStill;
		}
		for (int K = 0; K < HandKinds; ++K)
		{
			if (Quiet[K] >= Set.LeftStill) bLeft[K] = false;
		}
		if (bLeft[0]) In.PanRight = In.PanAhead = 0.0;
		if (bLeft[1]) In.SlideRight = In.SlideAhead = 0.0;
		if (bLeft[2]) In.Zoom = 0.0;
		if (bLeft[3]) In.Turn = 0.0;
		if (bLeft[4]) In.Tilt = 0.0;
		// (the pointer at the screen's edge pans as ever, but not through a flight to a kept view: it may well rest
		// there as he presses the key)
		const bool bEdge = !In.bLockEdgePan && FlyTo < 0;
		const double Px = std::clamp(In.PanRight + (bEdge ? In.EdgeRight : 0.0), -1.0, 1.0);
		const double Py = std::clamp(In.PanAhead + (bEdge ? In.EdgeAhead : 0.0), -1.0, 1.0);
		const bool bSlide = In.SlideRight != 0.0 || In.SlideAhead != 0.0;
		const bool bPanned = Px != 0.0 || Py != 0.0 || bSlide;
		// (moved by his own hand, he no longer stands at a kept view, and a flight to one is over: its chip goes out)
		// (a point to follow that the scene feeds, one tank or one man of the unit locked on (Brian, 26 November: "or i
		// can skip to individual planes ... do that same concept for the land and sea battles"): taken up when its
		// number is new, followed for as long as it is fed, and left alone once his own pan has let go of it, until
		// the scene names another or stops and begins again)
		const bool bPointFed = In.bFollowAt && In.FollowAtId >= 0;
		if (!bPointFed) FollowPt = FollowPtLeft = -1;
		const bool bPointNew = bPointFed && In.FollowAtId != FollowPt && In.FollowAtId != FollowPtLeft;
		if (bPanned || In.Zoom != 0.0 || In.Turn != 0.0 || In.Tilt != 0.0 || In.bTopView || In.bHillView || In.bGoTo || In.GoBlock >= 0 || bPointNew)
		{
			if (FlyTo >= 0)
			{
				// (a unit's card pressed, with the unit still on the field and no pan of his in the same frame to end
				// the follow: the view will follow it, and the zoom's wish is held no nearer than FollowNearest
				// further down this frame)
				double CardX = 0.0, CardY = 0.0;
				const bool bToUnit = In.GoBlock >= 0 && !bPanned && BlockAt(In.GoBlock, Battle, CardX, CardY);
				FlightLetGo(bPanned, !In.bGoTo && In.GoBlock < 0 && !bPointNew, In.Zoom * Set.ZoomStep, bToUnit || (bPointNew && !bPanned) ? Set.FollowNearest : 0.0);
			}
			AtView = FlyTo = -1;
		}
		if (In.bGoTo)
		{
			WantX = In.GoTo.X;
			WantY = In.GoTo.Y;
			bGliding = true;
			Follow = -1;
			if (FollowPt >= 0) FollowPtLeft = FollowPt;
			FollowPt = -1;
		}
		if (In.GoBlock >= 0)
		{
			Follow = In.GoBlock;
			bGliding = true;
			if (FollowPt >= 0) FollowPtLeft = FollowPt;
			FollowPt = -1;
		}
		if (bPointNew && !In.bGoTo && In.GoBlock < 0)
		{
			FollowPt = In.FollowAtId;
			FollowPtLeft = -1;
			Follow = -1;
			bGliding = true;
		}
		if (In.CallView >= 0 && FollowPt >= 0 && FlyTo >= 0)
		{
			// (a kept view called: the flight has the view)
			FollowPtLeft = FollowPt;
			FollowPt = -1;
		}
		if (bPanned)
		{
			if (FollowPt >= 0) FollowPtLeft = FollowPt;
			FollowPt = -1;
			// (a glide his pan takes, the coast of a flight let go among them, is let go where the pan's own quicker
			// spring brings it to rest: left wanted far ahead at the glide's rate, that spring would rush the look-at
			// on toward it. Not a glide asked for in this same frame.)
			if (bGliding && !In.bGoTo && In.GoBlock < 0)
			{
				WantX = Tx + SwingX / Set.Ease;
				WantY = Ty + SwingY / Set.Ease;
			}
			Follow = -1;
			bGliding = false;
		}
		// (flying to a kept view: its place, turn, zoom and tilt are wanted afresh each frame, so a wait at a hedge on
		// the way, which lets a wish go, does not lose them)
		if (bFlying && FlyTo >= 0)
		{
			const View& V = Views[FlyTo];
			const double Far = std::hypot(V.X - Tx, V.Y - Ty), Round = std::abs(WrapNear(V.H, H) - H);
			// (the low look on or off as the view was kept. Across the field it goes no flatter than the hill view's own
			// tilt, whatever lower look his hand had set here or kept there: almost level, no zoom would lift the eye
			// over the trees it must cross (FlyUp); the rest of the way down is the last of the flight, over its place.
			// With a low look on, now or on the way, the flight climbs to where the eye stands as high over the look-at
			// at the lower tilt)
			const bool bOver = Far < 2.0 && Round < 2.0 * Rad;
			const double Cross = std::max(V.Hill, Set.HillTilt * Rad);
			WantHill = bOver ? V.Hill : Cross;
			const double FlyAt = FlyZoom(std::min(Hill, Cross));
			if (bOver)
			{
				// (over its place and facing its way: down, or up, to its zoom)
				WantX = V.X;
				WantY = V.Y;
				WantH = V.H;
				WantS = V.S;
			}
			else if (S >= FlyAt - 0.03)
			{
				// (high enough to cross over everything: across and round, no lower than that on the way)
				WantX = V.X;
				WantY = V.Y;
				WantH = V.H;
				WantS = std::max(V.S, FlyAt);
				if (Far >= 1.0) bGliding = true;
			}
			else
			{
				// (low down: up first, where it stands and as it faces; a glide or a turn it was in the middle of dies
				// away where its own spring comes to rest, not at a stop)
				WantX = Tx + SwingX / Set.Ease;
				WantY = Ty + SwingY / Set.Ease;
				WantH = H + SwingH * std::abs(SwingH) / (2.0 * Set.TurnPush * Rad);
				WantS = std::max(V.S, FlyAt);
				bGliding = false;
			}
			if (V.bOwn && bOwnTilt)
			{
				bToAuto = false;
				WantTl = V.Tl;
			}
		}
		if (Px != 0.0 || Py != 0.0)
		{
			const double Speed = Set.PanSpeed * Across + Set.PanLeast;
			const Vec Go = (Right * Px + Fwd * Py) * (Speed * Dt);
			WantX += Go.X;
			WantY += Go.Y;
		}
		Vec SlideGo;
		if (bSlide)
		{
			// (the picture's width at the look-at: a slide across the whole picture carries the ground across it)
			const double Fov = Set.FovNear + (Set.FovFar - Set.FovNear) * S;
			const double Wide = 2.0 * Arm * std::tan(0.5 * Fov * Rad) * 16.0 / 9.0;
			const Vec Go = Right * (In.SlideRight * Wide) + Fwd * (In.SlideAhead * Wide * 9.0 / 16.0 / std::max(0.35, std::sin(Tilt)));
			WantX += Go.X;
			WantY += Go.Y;
			if (!bBrake) SlideGo = Go;
		}
		if (Follow >= 0)
		{
			double Bx = 0.0, By = 0.0;
			if (BlockAt(Follow, Battle, Bx, By))
			{
				WantX = Bx;
				WantY = By;
			}
			else Follow = -1;
		}
		if (FollowPt >= 0)
		{
			// (the point's own speed over the ground by real seconds, learnt over PointLearn, and led by: the look-at's
			// spring trails whatever moves steadily by twice its speed over the spring's rate, four metres behind a
			// tank at its pace, a quarter of the picture at the nearest zoom. Wanted that far ahead of the point, the
			// look-at rides on it as a man with a camera keeps a tank in his frame. A point that stops, or a battle
			// paused, lets the lead die away as it was learnt; a point that leaps as no tank drives (PointLeap) has no
			// speed learnt from the leap.)
			const double AtX = In.FollowAt.X, AtY = In.FollowAt.Y;
			if (PtWasId == FollowPt)
			{
				const double Vx = Dt > 1e-6 ? (AtX - PtWasX) / Dt : PtVx, Vy = Dt > 1e-6 ? (AtY - PtWasY) / Dt : PtVy;
				if (std::hypot(Vx, Vy) <= Set.PointLeap)
				{
					const double K = Set.PointLearn > 0.0 ? 1.0 - std::exp(-Dt / Set.PointLearn) : 1.0;
					PtVx += (Vx - PtVx) * K;
					PtVy += (Vy - PtVy) * K;
				}
			}
			else PtVx = PtVy = 0.0;
			PtWasId = FollowPt;
			PtWasX = AtX;
			PtWasY = AtY;
			const double Lead = Set.bPointLead ? 2.0 / (bGliding ? Set.Glide : Set.Ease) : 0.0;
			WantX = AtX + PtVx * Lead;
			WantY = AtY + PtVy * Lead;
		}
		else PtWasId = -1;
		// (a press's zoom, the top view's or the hill view's, there or back: wanted afresh each frame until the view
		// is there, so a wait at a hedge on the way, which lets a wish go, does not lose it and leave the press half
		// done; his own wheel takes it)
		if (bGoS)
		{
			if (In.Zoom != 0.0) bGoS = false;
			else if (!bBrake) WantS = GoS;
		}
		WantS -= In.Zoom * Set.ZoomStep;
		// (following a unit, the view stays a little back from its men: riding with them is the film's; called from
		// nearer than that, it draws back held to the zoom's fastest and hardest, as a flight is, not at a bound)
		if ((Follow >= 0 || FollowPt >= 0) && WantS < Set.FollowNearest)
		{
			WantS = Set.FollowNearest;
			if (bGoS) GoS = std::max(GoS, Set.FollowNearest);
			if (S < Set.FollowNearest - 0.01) bFlying = true;
		}
		WantH += In.Turn * Rad;
		TurnHeld = Dt > 1e-6 ? In.Turn * Rad / Dt : 0.0;
		ClampWanted();
		HillWish(In);
		TiltWish(In);
		if (Rec->Ground.W > 0.0) Tx = std::clamp(Tx, -Set.Margin, Rec->Ground.W + Set.Margin);
		if (Rec->Ground.H > 0.0) Ty = std::clamp(Ty, -Set.Margin, Rec->Ground.H + Set.Margin);

		// Eased toward it.
		const double Tx0 = Tx, Ty0 = Ty, S0 = S, H0 = H, Tl0 = Tl, Hill0 = Hill;
		if (Rec->Ground.W > 0.0) Tx = std::clamp(Tx + SlideGo.X, -Set.Margin, Rec->Ground.W + Set.Margin);
		if (Rec->Ground.H > 0.0) Ty = std::clamp(Ty + SlideGo.Y, -Set.Margin, Rec->Ground.H + Set.Margin);
		if (bFirst)
		{
			Tx = WantX;
			Ty = WantY;
			S = WantS;
			H = WantH;
			Hill = WantHill;
			SwingHill = 0.0;
			Tl = TlGoal = std::min(WantTl, TiltMostAt(S));
		}
		else
		{
			const double Rate = bGliding ? Set.Glide : Set.Ease;
			WantH = WrapNear(WantH, H);
			if (bBrake)
			{
				// (braking at a thing the lift cannot climb in time: each move dies away smoothly, a spring let go at the
				// point it would come to rest, while the player's wishes wait close by)
				WantX = Tx + std::clamp(WantX - Tx, -2.0, 2.0);
				WantY = Ty + std::clamp(WantY - Ty, -2.0, 2.0);
				WantS = S + std::clamp(WantS - S, -0.05, 0.05);
				WantH = H + std::clamp(WantH - H, -10.0 * Rad, 10.0 * Rad);
				Spring(Tx, SwingX, Tx + SwingX / Rate, Rate, Dt);
				Spring(Ty, SwingY, Ty + SwingY / Rate, Rate, Dt);
				Spring(S, SwingS, S + SwingS / Set.ZoomEase, Set.ZoomEase, Dt);
				Spring(H, SwingH, H + SwingH / Set.TurnEase, Set.TurnEase, Dt);
				if (bOwnTilt)
				{
					WantTl = Tl + std::clamp(WantTl - Tl, -5.0 * Rad, 5.0 * Rad);
					Spring(Tl, SwingTl, Tl + SwingTl / Set.TiltEase, Set.TiltEase, Dt);
				}
				// (the low look's swing dies away too; what is wanted of it waits)
				if (SwingHill != 0.0) Spring(Hill, SwingHill, Hill + SwingHill / Set.TiltEase, Set.TiltEase, Dt);
				BrakeFor += Dt;
				++Braked;
			}
			else
			{
				Spring(Tx, SwingX, WantX, Rate, Dt);
				Spring(Ty, SwingY, WantY, Rate, Dt);
				// (a spring held to its fastest and hardest: from a swing already faster, slowed no harder than the push;
				// and never faster than it can still stop from at that push by where it is wanted, so it comes to rest
				// there and does not run on past it, into the end of the zoom's range above all)
				// (bShort: wanted at the end of its range, the zoom's above all, where what runs on past is not eased back
				// but cut off. Slowed from the frame it should be, by the way left at that frame's beginning, a swing
				// comes in a frame's travel late and still going the root of twice the push times that travel: from
				// the zoom's fastest, an eighth of the zoom a second, stopped dead at the farthest zoom from under a
				// steep tilt at 1,469 degrees a second squared (the hill view's second press back to a top view, the
				// steppe's hill run, 26 November). So there it is slowed by what will be left after this frame's
				// travel, and comes in at a frame of the push or less.)
				auto Held = [Dt](double& X, double& Swing, double Want, double W, double Fastest, double Push, bool bShort = false)
				{
					double X1 = X, Sw = Swing;
					Spring(X1, Sw, Want, W, Dt);
					const double Way = std::abs(Want - X);
					const double StopFrom = bShort ? std::sqrt(Push * Push * Dt * Dt + 2.0 * Push * Way) - Push * Dt : std::sqrt(2.0 * Push * Way);
					const double Cap = std::max(std::min(Fastest, StopFrom), std::abs(Swing) - Push * Dt);
					Sw = std::clamp(std::clamp(Sw, Swing - Push * Dt, Swing + Push * Dt), -Cap, Cap);
					X += 0.5 * (Swing + Sw) * Dt;
					Swing = Sw;
				};
				// (flying to a kept view, the turn held too: a call may ask half a turn about the look-at at once; and
				// the zoom and the tilt a little slower than by his own hand, as all three may swing together)
				// (and the hill view's press as a flight: the zoom, the low look and, back to a top view, his tilt)
				const double Fly = bFlying || bHillSwing ? Set.FlySlower : 1.0;
				// (With the scene's switch, the zoom is stopped short wherever it is wanted, not at its range's end alone.
				// Wanted a hair under the end, a top view's zoom, a kept view's, or a flight's over a low look too
				// flat to lift the eye, found by halving, it came in a frame's travel late as before, ran into the end
				// and was cut off there at 1,500 degrees a second squared and more (review, 26 November). Stopped
				// short everywhere, it never has more swing than its push can stop by where it is wanted, so a wish
				// his next notch pulls back nearer cannot carry it into the end either.)
				const bool bZoomEnd = Set.bStopShort;
				const double SwingHWas = SwingH;
				if (bFlying) Held(H, SwingH, WantH, Set.TurnEase, Set.TurnFastest * Rad, Set.TurnPush * Rad);
				else
				{
					Spring(H, SwingH, WantH, Set.TurnEase, Dt);
					// (with a low look on, a turn gathers no harder than TurnGather: the wait at a thing let go
					// with his key still down)
					const double Gain = Set.TurnGather * Rad * Dt;
					if ((Hill < HillNone() || WantHill < HillNone()) && Set.TurnGather > 0.0 && std::abs(SwingH) > std::abs(SwingHWas) + Gain)
					{
						const double Sw = (SwingH > 0.0 ? 1.0 : -1.0) * (std::abs(SwingHWas) + Gain);
						H -= 0.5 * (SwingH - Sw) * Dt;
						SwingH = Sw;
					}
				}
				// (a low look's fast turn among the trees' height: held to the sweep that reaches the first thing on
				// its way round no sooner than SweepTime on, LowSweep at the least, while the view rises over it; slowed
				// to that no harder than the turn's push, and what is wanted of the turn kept as near as the spring
				// trails at that rate, so a key let go stops where it would have)
				// (SweepClear is far wherever the way round was not looked along: with no low look on, never)
				if (!bFlying && Set.LowSweep > 0.0 && SweepClear < 1e8)
				{
					const double Flat = std::max(1.0, ArmAt(S) * std::cos(TiltFor(S, Tl)));
					const double May = std::max(Set.LowSweep, SweepClear / std::max(Set.SweepTime, 0.05)) / Flat;
					const double Most = std::max(May, std::abs(SwingHWas) - Set.TurnPush * Rad * Dt);
					if (std::abs(SwingH) > Most)
					{
						const double Sw = std::clamp(SwingH, -Most, Most);
						H -= 0.5 * (SwingH - Sw) * Dt;
						SwingH = Sw;
					}
					const double Lead = 2.0 * Most / Set.TurnEase;
					WantH = H + std::clamp(WrapNear(WantH, H) - H, -Lead, Lead);
				}
				// (the low look coming on, going or turned by his hand: its tilt eased as his own is, held to the same
				// fastest and hardest. Wanted lower from none, or from clear over the automatic tilt here, it begins
				// where it first tells on the picture, so a press is answered at once and not after it has swung down
				// through tilts that change nothing; going, once clear over the automatic tilt here it tells on nothing
				// more and is none, so the view with none is the automatic one to the last bit)
				// (But under a tilt of his own that is coming to stand clear over the automatic one, the hill view's
				// second press back to a top view or a kept view with his tilt called, the low look tells on nothing
				// once his tilt is there (the picture is his tilt, whatever the view would take by itself under it). So
				// there his tilt goes first, from rest over the low look as it stands, as the top view rises out of the
				// hill view; the low look waits, and is changed unseen once his tilt hides it. Swung together, the two
				// met in the soft meeting at the speed of both, 1,800 degrees a second squared on the steppe's hill run
				// (26 November); the low look sent first, the tilt rose, sank back with the zoom's automatic tilt, and
				// rose again, and the eye came down on the trees between at seventy metres a second.)
				// (And wherever his tilt stands clear over the tilt the view takes by itself, under the low look as it
				// is and as it is wanted, the picture is his tilt to the last bit and the low look tells on nothing,
				// whatever his tilt is wanted to do next: there too it is changed unseen, at once. The hill view
				// pressed off with his tilt high over it and handed back: left to swing, the low look rose as his tilt
				// came down, the two met in the soft meeting, and the picture dipped and came back at 1,468 and 1,933
				// degrees a second squared with no key down (review, 26 November). Changed first, there is one mover:
				// his tilt comes down on the view as it will be.)
				const double SoftNow = Set.TiltSoft * Rad;
				const bool bOwnOver = bOwnTilt && !bToAuto && std::min(WantTl, TiltMostAt(S)) >= TiltAuto(S) + SoftNow + Rad;
				const bool bOwnHides = bOwnTilt && Hill != WantHill && Tl >= std::max(TiltAt(S, Hill), TiltAt(S, WantHill)) + SoftNow;
				// (going under his own key held up: see below)
				const bool bHillByKey = WantHill >= HillNone() && In.Tilt > 0.0 && Set.bLowByHand;
				if (bOwnHides)
				{
					Hill = WantHill;
					SwingHill = 0.0;
				}
				else if (Hill != WantHill && bOwnOver && !bHillByKey)
				{
					if (SwingHill != 0.0) Spring(Hill, SwingHill, Hill + SwingHill / Set.TiltEase, Set.TiltEase, Dt);
				}
				else if (Hill != WantHill || SwingHill != 0.0)
				{
					const double Clear = TiltAuto(S) + SoftNow;
					if (WantHill < Hill && Hill > Clear)
					{
						Hill = std::max(WantHill, Clear);
						SwingHill = std::min(SwingHill, 0.0);
					}
					// (going, it is eased to where it tells on nothing more, Clear, and comes to rest there as his own tilt
					// comes to rest at its floor: swung on through it toward none, the picture's tilt stopped dead inside the
					// soft meeting, TiltFastest to nothing in a twelfth of a second, 800 degrees a second squared and more
					// with the zoom's swing on it. Within a fiftieth of a degree of it the picture has a
					// hundred-thousandth of a degree left of the low look: none.)
					const bool bGoing = WantHill >= HillNone();
					if (bGoing && Hill >= Clear - 0.02 * Rad)
					{
						Hill = WantHill;
						SwingHill = 0.0;
					}
					else
					{
						// (Going under his own key held up, the low look is not slowed toward clear: it goes on at the speed
						// his key gave it, and his own tilt takes the picture over as it leaves (below). Eased to clear
						// there too, the picture slowed to a quarter of his key's speed at the automatic tilt and picked
						// up again, a knot in the one line under his hand (review, 26 November).)
						Held(Hill, SwingHill, !bGoing ? WantHill : bHillByKey ? std::max(Clear, Hill + 2.0 * SwingHill / Set.TiltEase) : Clear, Set.TiltEase,
							Set.TiltFastest * Rad * Fly, Set.TiltPush * Rad * Fly);
						if (!bGoing && std::abs(WantHill - Hill) < 1e-5 && std::abs(SwingHill) < 1e-4)
						{
							Hill = WantHill;
							SwingHill = 0.0;
						}
					}
				}
				if (bOwnTilt)
				{
					// (with his own tilt, the zoom and the tilt eased and never faster or harder than the picture bears:
					// the top view's swing of some fifty degrees and its climb from a man's shoulder take a second or two;
					// his tilt eases toward what he wants as far as the zoom lets it, handed back toward the shoulder)
					Held(S, SwingS, WantS, Set.ZoomEase, Set.ZoomFastest * Fly, Set.ZoomPush * Fly, bZoomEnd);
					TlGoal = std::min(WantTl, TiltMostAt(S));
					bool bCarried = false;
					// (and a tilt of his own wanted up that will NOT stand clear over the automatic one, a degree or two
					// of it, cannot hide the low look: there the low look goes first and his tilt waits just under the
					// picture, more than half a degree of the low look left to travel, then rises from rest as it does
					// from the automatic view. Under the picture: at or under the soft meeting's foot, as it stood or
					// as this frame's zoom and low look have left it.)
					// (Busy: going, while it still tells on the picture, a degree and a half short of clear and a fifth
					// of a degree of it left; the last of its eased way out is seen by nobody, and waited out at half a
					// degree his key held up through the automatic tilt stood still for up to four tenths of a second
					// (review, 26 November). Turned to another tilt, more than half a degree to travel.)
					// (And a tilt of his own wanted DOWN while the low look rises or goes, where it does not stand
					// clear over it (there the low look is changed unseen, above): it does not come down under the
					// rising low look to be lifted again as that passes it. It stands where its own push stops it
					// until the low look is there, then comes down: one mover at a time.)
					{
						const double Under = TiltAt(S) - SoftNow, UnderWas = TiltAt(S0, Hill0) - SoftNow;
						const bool bGoing = WantHill >= HillNone();
						const double Left = bGoing ? TiltAuto(S) + SoftNow - Hill : std::abs(WantHill - Hill);
						const bool bBusy = Hill != WantHill && Left > (bGoing ? 1.5 : 0.5) * Rad;
						const bool bOver = !bToAuto && TlGoal >= TiltAuto(S) + SoftNow + Rad;
						// (Under his own key held up, the low look on its way out at his key's speed: his tilt is carried
						// under it. At the picture's foot until the low look comes into the soft meeting; through the
						// meeting, at the one tilt that keeps the picture ON the low look's own tilt, so the picture goes
						// on up at his key's speed where the low look alone would slow to nothing and his own tilt start
						// from rest; clear of it, his tilt IS the picture, with the speed it was carried at, and the low
						// look is none. Carried only from the foot, where his key's wish ran ahead of the low look by
						// the soft meeting's width or more (eighteen degrees a second and up; the scene's key is fifty):
						// begun inside the meeting, a slow key's, his tilt rises by its own ease as before.)
						auto CarryAt = [&](double Zoom, double Cap)
						{
							const double By = TiltAt(Zoom, Cap);
							return std::min(By - SoftNow + 2.0 * std::sqrt(std::max(0.0, SoftNow * (Cap - By))), TiltMostAt(Zoom, Cap));
						};
						const double Carry = CarryAt(S, Hill), CarryWas = CarryAt(S0, Hill0);
						if (bHillByKey && Hill != WantHill && !bToAuto && std::abs(Tl - CarryWas) < 1e-6 && TlGoal > Carry)
						{
							SwingTl = Dt > 1e-6 ? std::clamp((Carry - Tl) / Dt, 0.0, Set.TiltFastest * Rad) : 0.0;
							Tl = TlGoal = Carry;
							bCarried = true;
						}
						else if (bBusy && !bOver && Tl <= std::max(Under, UnderWas) + 1e-9 && TlGoal > Under)
						{
							Tl = TlGoal = Under;
							SwingTl = 0.0;
						}
						else if (bBusy && WantHill > Hill && TlGoal < Tl && Tl > Under)
						{
							TlGoal = std::max(TlGoal, std::min(Tl, Tl + SwingTl * std::abs(SwingTl) / (2.0 * Set.TiltPush * Rad * Fly)));
						}
					}
					if (!bCarried) Held(Tl, SwingTl, TlGoal, Set.TiltEase, Set.TiltFastest * Rad * Fly, Set.TiltPush * Rad * Fly);
				}
				else if (bFlying || bHillSwing) Held(S, SwingS, WantS, Set.ZoomEase, Set.ZoomFastest * Fly, Set.ZoomPush * Fly, bZoomEnd);
				else Spring(S, SwingS, WantS, Set.ZoomEase, Dt);
				if (BrakeFor < 0.0) BrakeFor = std::min(0.0, BrakeFor + Dt);
			}
			S = std::clamp(S, 0.0, 1.0);
			if (Hill < Set.LowTilt * Rad && Hill < WantHill)
			{
				Hill = std::min(Set.LowTilt * Rad, WantHill);
				SwingHill = 0.0;
			}
			if (bGliding && std::hypot(WantX - Tx, WantY - Ty) < 1.0 && std::hypot(SwingX, SwingY) < 1.0) bGliding = false;
		}
		if (Follow >= 0 && !bGliding && !In.bFollow) Follow = -1;
		H = WrapNear(H, 0.0);
		WantH = WrapNear(WantH, H);

		// Where the eye stands. Where this frame's move would carry it inside the near clearance faster than the lift
		// can climb, the view waits where it was this once, as a man stops at a hedge, while the lift climbs.
		const Kept Before = Keep();
		Vec EyeAt, LookAt3;
		const int Hu = Place(Dt, false, EyeAt, LookAt3);
		if (Hu > 0 && !bFirst)
		{
			// (half the move first, then none: the least stop that keeps it clear)
			const Kept After = Keep();
			const double Tx1 = Tx, Ty1 = Ty, S1 = S, H1 = H, Tl1 = Tl, Hill1 = Hill;
			const double Turned = WrapNear(H1, H0) - H0;
			bool bTook = false;
			for (const double F : { 0.5, 0.0 })
			{
				Restore(Before);
				Tx = Tx0 + F * (Tx1 - Tx0);
				Ty = Ty0 + F * (Ty1 - Ty0);
				S = S0 + F * (S1 - S0);
				H = WrapNear(H0 + F * Turned, 0.0);
				Tl = Tl0 + F * (Tl1 - Tl0);
				Hill = Hill0 + F * (Hill1 - Hill0);
				Vec EyeHeld, LookHeld;
				const int Got = Place(Dt, true, EyeHeld, LookHeld);
				if (Got == 0 || (F == 0.0 && Got < Hu))
				{
					SwingX *= F;
					SwingY *= F;
					SwingS *= F;
					SwingH *= F;
					SwingTl *= F;
					SwingHill *= F;
					if (bOwnTilt) WantTl = Tl + std::clamp(WantTl - Tl, -5.0 * Rad, 5.0 * Rad);
					WantX = Tx + std::clamp(WantX - Tx, -2.0, 2.0);
					WantY = Ty + std::clamp(WantY - Ty, -2.0, 2.0);
					WantS = S + std::clamp(WantS - S, -0.05, 0.05);
					WantH = H + std::clamp(WrapNear(WantH, H) - H, -10.0 * Rad, 10.0 * Rad);
					EyeAt = EyeHeld;
					LookAt3 = LookHeld;
					++Waited;
					// (and it waits until the lift has climbed to what the move wanted)
					if (BrakeFor >= 0.0)
					{
						BrakeNeed = std::max(bBrake ? BrakeNeed : 0.0, std::max(After.NeedWas, After.Lift));
						bBrake = true;
					}
					bTook = true;
					break;
				}
			}
			if (!bTook)
			{
				Restore(After);
				Tx = Tx1;
				Ty = Ty1;
				S = S1;
				H = H1;
				Tl = Tl1;
				Hill = Hill1;
			}
		}
		// (tilted all the way back to the automatic tilt, under its soft floor where his own tilt no longer shows: the
		// view tilts with the zoom again, without a step)
		// (and the zoom come to rest, so its own easing goes back to the automatic one's without a step)
		if (bOwnTilt && bToAuto && !bTop && Tl <= TiltAt(S) - Set.TiltSoft * Rad && std::abs(WantS - S) < 0.005 && std::abs(SwingS) < 0.05)
		{
			bOwnTilt = bToAuto = bCallToAuto = false;
			Tl = WantTl = SwingTl = TlGoal = 0.0;
		}
		// (a press's zoom is there: it is his wheel's again)
		if (bGoS && !bBrake && std::abs(GoS - S) < 0.005 && std::abs(SwingS) < 0.05) bGoS = false;
		// (the hill view's swing over, its tilt and the zoom at rest: the zoom goes back to its own easing, at rest,
		// without a step)
		if (bHillSwing && !bGoS && !bBrake && Hill == WantHill && std::abs(WantS - S) < 0.005 && std::abs(SwingS) < 0.05) bHillSwing = false;
		// (come to rest at the kept view, or wherever his own hand has since taken it, or drawn back from too near a
		// followed unit: the turn and the zoom go back to their own easing, at rest, without a step. Whether the look-at
		// is still gliding is nothing to it: after a unit on the move it glides for as long as the unit does. A flight
		// to a kept view is not over before the tilt is there too: his own at the view's, or, for a view kept without
		// his own tilt, handed back to the automatic one just above)
		if (bFlying && !bBrake && std::abs(WantS - S) < 0.005 && std::abs(SwingS) < 0.05 && std::abs(WrapNear(WantH, H) - H) < 0.5 * Rad
			&& std::abs(SwingH) < 2.0 * Rad
			&& (FlyTo < 0 || (std::hypot(Views[FlyTo].X - Tx, Views[FlyTo].Y - Ty) < 1.5 && std::abs(Views[FlyTo].S - S) < 0.005
				&& std::abs(Views[FlyTo].Hill - Hill) < 0.5 * Rad
				&& (Views[FlyTo].bOwn ? !bOwnTilt || std::abs(std::min(Views[FlyTo].Tl, TiltMostAt(S)) - Tl) < 1.0 * Rad : !bOwnTilt))))
		{
			bFlying = false;
			FlyTo = -1;
			bCallToAuto = false;
		}
		// (carried off the kept view by a unit it follows, by a glide begun before it was kept, or stopped short of it
		// at a hedge: where the view is going is no longer that view, and its chip goes out)
		if (AtView >= 0 && FlyTo < 0)
		{
			const View& V = Views[AtView];
			// (and tilted away from it: his own tilt where it was kept without one, none where it was kept with one,
			// or his own wanted elsewhere than it was kept)
			const bool bOwnNow = bOwnTilt && !bToAuto;
			if (std::hypot(V.X - WantX, V.Y - WantY) > 3.0 || std::abs(V.S - WantS) > 0.02 || std::abs(WrapNear(V.H, WantH) - WantH) > 2.0 * Rad || V.bOwn != bOwnNow
				|| std::abs(V.Hill - WantHill) > 1.0 * Rad
				|| (V.bOwn && std::abs(std::min(V.Tl, TiltMostAt(V.S, V.Hill)) - std::min(WantTl, TiltMostAt(WantS, WantHill))) > 2.0 * Rad))
			{
				AtView = -1;
			}
		}

		Now.Eye = EyeAt;
		Now.Look = LookAt3;
		Now.Fov = Set.FovNear + (Set.FovFar - Set.FovNear) * S;
		Now.Roll = 0.0;
		Now.Shot = LandEye::Kind::Wide;
		Now.Subject = -1;
		// (where the view could not be kept clear smoothly, in a wood, and was put clear at once, the picture cuts)
		Now.bCut = bFirst || Hurried == 2;
		bFirst = false;
		return Now;
	}

	bool Seen(const LandEye::Pose& P, double Aspect, double FarMost, Vec Out[4])
	{
		const double Level = P.Look.Z, Up = P.Eye.Z - Level;
		Vec F = P.Look - P.Eye;
		const double Len = F.Length();
		if (!(Up > 1e-6) || !(FarMost > 0.0) || !(Aspect > 0.0) || !(Len > 1e-9)) return false;
		F = F * (1.0 / Len);
		// (the picture's right: level, a quarter turn clockwise of where it looks over the ground, east where it looks
		// straight down; its top: square to both)
		const double Flat = std::hypot(F.X, F.Y);
		const Vec Right = Flat > 1e-9 ? Vec(F.Y / Flat, -F.X / Flat, 0.0) : Vec(1.0, 0.0, 0.0);
		const Vec Top(Right.Y * F.Z - Right.Z * F.Y, Right.Z * F.X - Right.X * F.Z, Right.X * F.Y - Right.Y * F.X);
		const double TanHigh = std::tan(0.5 * std::clamp(P.Fov, 1.0, 170.0) * Rad), TanWide = TanHigh * Aspect;
		const double Side[4] = { -1.0, 1.0, 1.0, -1.0 }, High[4] = { -1.0, -1.0, 1.0, 1.0 };
		for (int K = 0; K < 4; ++K)
		{
			// (the corner's line, followed down to the level; one at or over the horizon, or that meets the level
			// farther off than FarMost, is put FarMost out the way it goes over the ground)
			const Vec D = F + Right * (Side[K] * TanWide) + Top * (High[K] * TanHigh);
			const double Over = std::hypot(D.X, D.Y);
			const double Off = D.Z < -1e-9 ? std::min(FarMost, Up * Over / -D.Z) : FarMost;
			Out[K] = Over > 1e-12 ? Vec(P.Eye.X + D.X / Over * Off, P.Eye.Y + D.Y / Over * Off, Level) : Vec(P.Eye.X, P.Eye.Y, Level);
		}
		return true;
	}
}
