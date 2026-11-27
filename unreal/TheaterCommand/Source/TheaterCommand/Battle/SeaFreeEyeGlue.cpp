#include "Battle/SeaFreeEyeGlue.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace SeaFreeEye
{
	namespace
	{
		constexpr double Rad = Pi / 180.0;

		double Finite(double V, double Otherwise = 0.0)
		{
			return std::isfinite(V) ? V : Otherwise;
		}

		double SaneAspect(double Aspect)
		{
			return std::isfinite(Aspect) && Aspect > 0.1 ? std::min(Aspect, 10.0) : 16.0 / 9.0;
		}

		const SeaShow::ShipOut* Find(const std::vector<SeaShow::ShipOut>& Ships, int Key)
		{
			for (const SeaShow::ShipOut& Ship : Ships)
			{
				if (Ship.Key == Key)
				{
					return &Ship;
				}
			}
			return nullptr;
		}

		/** Her anchor: her waterline amidships, riding her heave. */
		Vec PlaceOf(const SeaShow::ShipOut& Ship)
		{
			return Vec{ Ship.At.X, Ship.At.Y, Ship.At.Z + Ship.Heave };
		}

		double Dot(const Vec& A, const Vec& B)
		{
			return A.X * B.X + A.Y * B.Y + A.Z * B.Z;
		}

		/** The index of a held key in Hands::KeyBits; -1 for a key that holds nothing. */
		int HeldIndex(KeyName Key)
		{
			switch (Key)
			{
			case KeyName::W: return 0;
			case KeyName::A: return 1;
			case KeyName::S: return 2;
			case KeyName::D: return 3;
			case KeyName::Up: return 4;
			case KeyName::Down: return 5;
			case KeyName::Left: return 6;
			case KeyName::Right: return 7;
			case KeyName::Q: return 8;
			case KeyName::E: return 9;
			default: return -1;
			}
		}

		/** What a held key does, Shift as it stood at its press. */
		uint32_t BitOf(KeyName Key, bool bShift)
		{
			switch (Key)
			{
			case KeyName::W:
			case KeyName::Up: return bShift ? TiltDown : PanAhead;
			case KeyName::S:
			case KeyName::Down: return bShift ? TiltUp : PanBack;
			case KeyName::A:
			case KeyName::Left: return PanLeft;
			case KeyName::D:
			case KeyName::Right: return PanRight;
			case KeyName::Q: return bShift ? GoDown : TurnLeft;
			case KeyName::E: return bShift ? GoUp : TurnRight;
			default: return 0u;
			}
		}

		void HeldAgain(Hands& H)
		{
			H.Bits = 0u;
			for (const uint32_t Bit : H.KeyBits)
			{
				H.Bits |= Bit;
			}
		}

		/** This frame's presses spent (J, C, Tab, the clicks), and its sums (the wheel, pinch, twist, slide); the held
		 * keys stay. */
		void SpentPresses(Hands& H)
		{
			H.bJ = H.bC = H.bClick = H.bDouble = false;
			H.Tab = 0;
		}

		void SpentSums(Hands& H)
		{
			H.Wheel = H.Pinch = H.Twist = H.SlideRight = H.SlideAhead = 0.0;
		}

		void Spent(Hands& H)
		{
			SpentPresses(H);
			SpentSums(H);
		}

		/** The found ship nearest the middle of the picture (eye, look, its horizontal lens, its aspect), only one in it;
		 * -1 none (the lead's default for Q3: J with nothing picked). */
		int NearestInPicture(const std::vector<SeaShow::ShipOut>& Ships, const Vec& Eye, const Vec& Look, double Lens, double Aspect)
		{
			const Vec Ahead = (Look - Eye).Unit();
			if (!(Ahead.Length() > 0.5))
			{
				return -1;
			}
			Vec Right{ Ahead.Y, -Ahead.X, 0.0 };
			Right = Right.Length() > 1e-6 ? Right.Unit() : Vec{ 1.0, 0.0, 0.0 };
			const Vec Up{ Right.Y * Ahead.Z - Right.Z * Ahead.Y, Right.Z * Ahead.X - Right.X * Ahead.Z, Right.X * Ahead.Y - Right.Y * Ahead.X };
			const double Wide = std::tan(0.5 * std::clamp(Finite(Lens, 50.0), 1.0, 170.0) * Rad);
			const double Tall = Wide / SaneAspect(Aspect);
			int Best = -1;
			double BestOff = std::numeric_limits<double>::infinity();
			for (const SeaShow::ShipOut& Ship : Ships)
			{
				if (!MayPick(Ship))
				{
					continue;
				}
				const Vec Way = PlaceOf(Ship) - Eye;
				const double Far = Dot(Way, Ahead);
				if (!(Far > 1.0))
				{
					continue;
				}
				const double X = Dot(Way, Right) / Far, Y = Dot(Way, Up) / Far;
				if (std::abs(X) > Wide || std::abs(Y) > Tall)
				{
					continue;
				}
				const double Off = X * X + Y * Y;
				if (Off < BestOff)
				{
					BestOff = Off;
					Best = Ship.Key;
				}
			}
			return Best;
		}

		/** A lock on a ship he may pick (rechecked now: never a lock, not for one frame, on one he may not) or on a
		 * division with a found member in line; false where there is none. */
		bool LockOn(State& St, const std::vector<SeaShow::ShipOut>& Ships, int Key, int Piece, const Lengths& Long)
		{
			Lock Now;
			if (Key >= 0)
			{
				const SeaShow::ShipOut* Ship = Find(Ships, Key);
				if (!Ship || !MayPick(*Ship))
				{
					return false;
				}
				Now.Key = Key;
				Now.Id = Key;
			}
			else if (Piece >= 0)
			{
				const Division Dv = DivisionOf(Ships, Piece, Long);
				if (Dv.Members.empty())
				{
					return false;
				}
				Now.Piece = Piece;
				Now.Members = Dv.Members;
				Now.Id = St.NextDivisionId++;
			}
			else
			{
				return false;
			}
			St.L = Now;
			// (the pick moves onto what is locked, however it was locked: after a Tab or a J with nothing picked, an older
			// click's pick would otherwise turn the next J into a lock on her instead of the let go he was told of)
			St.PickKey = Now.Key;
			St.PickPiece = Now.Key >= 0 ? -1 : Now.Piece;
			++St.Locks;
			return true;
		}

		void LetGo(State& St)
		{
			if (St.L.On())
			{
				++St.LetGoes;
			}
			St.L = Lock();
		}

		/** Every frame, whoever has the camera: the pick and the lock kept only while their ship may be picked (a pick
		 * dropped, a lock let go, the frame she is lost from sight, starts to sink or is gone), and a division's id renewed
		 * whenever its members change. */
		void Keep(State& St, const std::vector<SeaShow::ShipOut>& Ships, const Lengths& Long)
		{
			if (St.PickKey >= 0)
			{
				const SeaShow::ShipOut* Ship = Find(Ships, St.PickKey);
				if (!Ship || !MayPick(*Ship))
				{
					St.PickKey = -1;
				}
			}
			if (St.PickPiece >= 0 && DivisionOf(Ships, St.PickPiece, Long).Members.empty())
			{
				St.PickPiece = -1;
			}
			if (St.L.Key >= 0)
			{
				const SeaShow::ShipOut* Ship = Find(Ships, St.L.Key);
				if (!Ship || !MayPick(*Ship))
				{
					LetGo(St);
				}
			}
			else if (St.L.Piece >= 0)
			{
				const Division Dv = DivisionOf(Ships, St.L.Piece, Long);
				if (Dv.Members.empty())
				{
					LetGo(St);
				}
				else if (Dv.Members != St.L.Members)
				{
					St.L.Members = Dv.Members;
					St.L.Id = St.NextDivisionId++;
					St.L.bLast = false;
				}
			}
		}

		/** The lock fed to the core: her place now, her speed as the scene moved her over this frame's real seconds
		 * (measured by the core on the first frame), half her drawn length, her heading. */
		void FeedLock(State& St, const std::vector<SeaShow::ShipOut>& Ships, const Lengths& Long, double Dt, const Vec& EyeNow, FreeEye::Input& In)
		{
			if (!St.L.On())
			{
				St.L.bLast = false;
				return;
			}
			Vec At;
			double Clear = 0.0, Heading = 0.0;
			if (St.L.Key >= 0)
			{
				const SeaShow::ShipOut& Ship = *Find(Ships, St.L.Key);
				At = PlaceOf(Ship);
				Clear = 0.5 * Long.DrawnLong(Ship);
				Heading = Ship.Heading * Rad;
			}
			else
			{
				const Division Dv = DivisionOf(Ships, St.L.Piece, Long);
				At = Dv.Middle;
				Clear = Dv.Clear;
				Heading = Dv.Heading;
			}
			// (a lock first fed, or its anchor renewed, with the eye inside her clearance: the clearance grown from where
			// the eye stands, ClearGrows a real second, never a throw of the eye in one frame; from under the core's own
			// soft margin, ClearSoftMargin, which inside it grows the boom some 3.5 m in a frame, core ask 9)
			if (!St.L.bLast || St.L.ClearId != St.L.Id)
			{
				St.L.ClearId = St.L.Id;
				St.L.ClearFrom = std::min(Clear, Finite((EyeNow - At).Length()) / ClearSoftMargin);
				St.L.ClearFor = 0.0;
			}
			else
			{
				St.L.ClearFor += std::max(0.0, Dt);
			}
			In.bLock = true;
			In.AnchorId = St.L.Id;
			In.Anchor = At;
			In.AnchorClear = std::min(Clear, St.L.ClearFrom + ClearGrows * St.L.ClearFor);
			In.AnchorHeading = Heading;
			In.bRide = St.bRide;
			In.bAnchorVel = St.L.bLast && Dt > 0.0;
			In.AnchorVel = In.bAnchorVel ? (At - St.L.Last) * (1.0 / Dt) : Vec{};
			St.L.Last = At;
			St.L.bLast = true;
		}

		double DriveEnd(const Drive& D)
		{
			double End = 0.0;
			for (const DriveLine& Line : D.Lines)
			{
				End = std::max(End, std::max(Line.From, Line.To));
			}
			return End;
		}

		std::string Fixed(double V, int Places)
		{
			char Text[64] = {};
			std::snprintf(Text, sizeof(Text), "%.*f", Places, V);
			return Text;
		}

		/** The drive's step lines and its last line, once it is over or stopped. */
		void SayDrive(State& St, bool bStopped)
		{
			Drive& D = St.D;
			for (size_t K = 0; K < D.Starts.size(); ++K)
			{
				const int Frames = D.StepFrames[K];
				if (Frames == 0)
				{
					continue;
				}
				const double Seconds = D.StepSeconds[K];
				const double Mean = Seconds > 0.0 ? Frames / Seconds : 0.0;
				// (the worst whole second inside the step; a step under a second, its own rate)
				const int Whole = static_cast<int>(std::floor(Seconds + 1e-9));
				double Worst = Mean;
				for (int Second = 0; Second < Whole && Second < static_cast<int>(D.StepSecondFrames[K].size()); ++Second)
				{
					Worst = Second == 0 ? D.StepSecondFrames[K][Second] : std::min(Worst, static_cast<double>(D.StepSecondFrames[K][Second]));
				}
				const double Until = K + 1 < D.Starts.size() ? D.Starts[K + 1] : D.Starts[K] + Seconds;
				St.Said.push_back("Sea free eye: drive step " + std::to_string(K + 1) + " of " + std::to_string(D.Starts.size()) + ", " + Fixed(D.Starts[K], 1) + " to " +
					Fixed(Until, 1) + " s, " + D.StepWords[K] + ": " + std::to_string(Frames) + " frames, " + Fixed(Mean, 1) + " a second over the step, its worst whole second " +
					Fixed(Worst, 0) + ".");
			}
			St.Said.push_back(std::string("Sea free eye: the drive ") + (bStopped ? "stopped by C at " + Fixed(D.T, 1) + " s" : "is over") + "; the eye rose to " +
				Fixed(D.Highest, 0) + " m.");
		}

		/** The drive's hands for this frame (his own, but C, put aside while a drive runs). */
		void DriveHands(State& St, double Dt, bool bMayStart)
		{
			Drive& D = St.D;
			if (!D.bOn || D.bDone)
			{
				return;
			}
			// (his keys are not his while a drive runs, U included; C still gives the camera back and stops it)
			const bool bC = St.H.bC;
			St.H = Hands();
			St.H.bC = bC;
			if (!D.bStarted)
			{
				if (!bMayStart)
				{
					return;
				}
				D.bStarted = true;
				D.T = 0.0;
			}
			else
			{
				D.T += std::max(0.0, Dt);
			}
			const double Now = D.T, Was = D.bStarted && D.StepNow >= 0 ? Now - std::max(0.0, Dt) : -1.0;
			for (const DriveLine& Line : D.Lines)
			{
				if (Line.Hold != 0u && Line.From <= Now && Now < Line.To)
				{
					St.H.Bits |= Line.Hold;
				}
				if (Line.Press != KeyName::Other && Line.From > Was && Line.From <= Now)
				{
					if (Line.Press == KeyName::J)
					{
						St.H.bJ = true;
					}
					else if (Line.Press == KeyName::C)
					{
						St.H.bC = true;
					}
					else if (Line.Press == KeyName::Tab)
					{
						St.H.Tab += Line.bShift ? -1 : 1;
					}
				}
				if (Line.Wheel != 0.0 && Line.To > Line.From)
				{
					const double Over = std::min(Now, Line.To) - std::max(Was, Line.From);
					if (Over > 0.0)
					{
						St.H.Wheel += Line.Wheel * Over / (Line.To - Line.From);
					}
				}
			}
			int Step = -1;
			for (size_t K = 0; K < D.Starts.size(); ++K)
			{
				Step = D.Starts[K] <= Now + DriveSlack ? static_cast<int>(K) : Step;
			}
			D.StepNow = std::max(Step, 0);
		}

		/** The frame counted to its step of the drive, and the drive ended once its last line has passed; the height the
		 * free eye rose to (a frame of the director's, C's own, is not the free eye's). */
		void DriveCount(State& St, double Dt, bool bFree, double EyeZ)
		{
			Drive& D = St.D;
			if (!D.bOn || !D.bStarted || D.bDone || D.StepNow < 0)
			{
				return;
			}
			const size_t K = static_cast<size_t>(D.StepNow);
			++D.StepFrames[K];
			D.StepSeconds[K] += std::max(0.0, Dt);
			// (the whole second the frame ends in, [start + k, start + k + 1), with the same slack as its step: the clock is
			// a sum of the frames' seconds, and a frame ending a hair under a whole second belongs to the next)
			const int Second = static_cast<int>(std::floor(std::max(0.0, D.T - D.Starts[K] + DriveSlack)));
			if (Second >= 0 && Second < 100000)
			{
				if (static_cast<int>(D.StepSecondFrames[K].size()) <= Second)
				{
					D.StepSecondFrames[K].resize(static_cast<size_t>(Second) + 1, 0);
				}
				++D.StepSecondFrames[K][static_cast<size_t>(Second)];
			}
			if (bFree)
			{
				D.Highest = std::max(D.Highest, EyeZ);
			}
			if (D.T >= DriveEnd(D))
			{
				D.bDone = true;
				SayDrive(St, false);
			}
		}

		std::string LowerCase(const std::string& Text)
		{
			std::string Out = Text;
			for (char& C : Out)
			{
				C = (C >= 'A' && C <= 'Z') ? static_cast<char>(C - 'A' + 'a') : C;
			}
			return Out;
		}

		bool Number(const std::string& Word, double& Out)
		{
			if (Word.empty())
			{
				return false;
			}
			char* End = nullptr;
			const double V = std::strtod(Word.c_str(), &End);
			if (!End || *End != '\0' || !std::isfinite(V))
			{
				return false;
			}
			Out = V;
			return true;
		}
	}

	// ---- 1a ---------------------------------------------------------------------------------------------------------

	FreeEye::Settings SeaSettings(double Aspect, double CeilingAt)
	{
		FreeEye::Settings Set;
		Set.FloorClear = FloorClear;
		Set.EyeMost = std::isfinite(CeilingAt) && CeilingAt > 0.0 ? CeilingAt : EyeMost;
		Set.CutBeyond = CutBeyond;
		Set.SteadyZ = SteadyZ;
		Set.bPanKeepsLock = true;
		FitLens(Set, Aspect);
		return Set;
	}

	void FitLens(FreeEye::Settings& Set, double Aspect)
	{
		Set.FovNear = VerticalLens(LensNear, Aspect);
		Set.FovFar = VerticalLens(LensFar, Aspect);
	}

	double Ceiling(bool bDeck, double DeckBase)
	{
		if (!bDeck || !std::isfinite(DeckBase))
		{
			return EyeMost;
		}
		return std::min(EyeMost, std::max(CeilingLeast, DeckBase - UnderDeck));
	}

	// ---- 1h ---------------------------------------------------------------------------------------------------------

	double VerticalLens(double Horizontal, double Aspect)
	{
		return 2.0 * std::atan(std::tan(0.5 * Horizontal * Rad) / SaneAspect(Aspect)) / Rad;
	}

	double HorizontalLens(double Vertical, double Aspect)
	{
		return 2.0 * std::atan(std::tan(0.5 * Vertical * Rad) * SaneAspect(Aspect)) / Rad;
	}

	void EaseLens(double& X, double& Swing, double Want, double Dt)
	{
		if (!(Dt > 0.0) || !std::isfinite(Want))
		{
			return;
		}
		const double Left = Want - X;
		const double Toward = (Left > 0.0 ? 1.0 : Left < 0.0 ? -1.0 : 0.0) * std::min(LensFastest, std::sqrt(2.0 * LensPush * std::abs(Left)));
		Swing = std::clamp(Toward, Swing - LensPush * Dt, Swing + LensPush * Dt);
		Swing = std::clamp(Swing, -LensFastest, LensFastest);
		const double Step = Swing * Dt;
		if (Left == 0.0 || (Step * Left > 0.0 && std::abs(Step) >= std::abs(Left)))
		{
			X = Want;
			Swing = 0.0;
			return;
		}
		X += Step;
	}

	double Lengths::DrawnLong(const SeaShow::ShipOut& Ship) const
	{
		const double Drawn = Of ? Of(Context, Ship.Key) : 0.0;
		return std::isfinite(Drawn) && Drawn > 0.0 ? Drawn : Ship.Long;
	}

	// ---- 1b ---------------------------------------------------------------------------------------------------------

	KeyTaken KeyEvent(Hands& H, KeyName Key, bool bDown, bool bShift, bool bRepeat)
	{
		const int Index = HeldIndex(Key);
		if (Index >= 0)
		{
			if (!bDown)
			{
				H.KeyBits[Index] = 0u;
			}
			else if (!bRepeat || H.KeyBits[Index] == 0u)
			{
				// (latched at the press: a repeat keeps what it does, whatever Shift does meanwhile; a repeat with
				// nothing latched, the keys found held as the screen took them, latches now)
				H.KeyBits[Index] = BitOf(Key, bShift);
			}
			HeldAgain(H);
			return KeyTaken::Mine;
		}
		switch (Key)
		{
		case KeyName::J:
			H.bJ = H.bJ || (bDown && !bRepeat);
			return KeyTaken::Mine;
		case KeyName::C:
			H.bC = H.bC || (bDown && !bRepeat);
			return KeyTaken::Mine;
		case KeyName::Tab:
			H.Tab += bDown ? (bShift ? -1 : 1) : 0;
			return KeyTaken::Mine;
		case KeyName::U:
			return bDown && !bRepeat ? KeyTaken::Under : KeyTaken::Mine;
		default:
			return KeyTaken::NotMine;
		}
	}

	void LetGoKeys(Hands& H)
	{
		for (uint32_t& Bit : H.KeyBits)
		{
			Bit = 0u;
		}
		H.Bits = 0u;
	}

	void ClickAt(Hands& H, const Ray& Along, bool bDouble)
	{
		H.Click = Along;
		H.Click.Way = Along.Way.Unit();
		H.bClick = H.bClick || !bDouble;
		H.bDouble = H.bDouble || bDouble;
	}

	FreeEye::Input InputOf(const Hands& H, double RealDt, double Aspect)
	{
		auto On = [&H](uint32_t Bit) { return (H.Bits & Bit) != 0u ? 1.0 : 0.0; };
		const double Dt = std::clamp(Finite(RealDt), 0.0, 0.25);
		FreeEye::Input In;
		In.PanRight = On(PanRight) - On(PanLeft);
		In.PanAhead = On(PanAhead) - On(PanBack);
		In.Rise = On(GoUp) - On(GoDown);
		In.Turn = (On(TurnRight) - On(TurnLeft)) * TurnRate * Dt + Finite(H.Twist);
		In.Tilt = (On(TiltDown) - On(TiltUp)) * TiltRate * Dt;
		In.Zoom = Finite(H.Wheel) + Finite(H.Pinch);
		In.SlideRight = Finite(H.SlideRight);
		In.SlideAhead = Finite(H.SlideAhead);
		In.Aspect = SaneAspect(Aspect);
		return In;
	}

	bool HandsMove(const Hands& H)
	{
		return H.Bits != 0u || Finite(H.Wheel) != 0.0 || Finite(H.Pinch) != 0.0 || Finite(H.Twist) != 0.0 || Finite(H.SlideRight) != 0.0 || Finite(H.SlideAhead) != 0.0;
	}

	// ---- 1c ---------------------------------------------------------------------------------------------------------

	bool MayPick(const SeaShow::ShipOut& Ship)
	{
		return Ship.bShown && (Ship.Doing == SeaShow::Fate::Line || Ship.Doing == SeaShow::Fate::Limping);
	}

	std::vector<int> TabOrder(const std::vector<SeaShow::ShipOut>& Ships, int MySide)
	{
		std::vector<const SeaShow::ShipOut*> Found;
		for (const SeaShow::ShipOut& Ship : Ships)
		{
			if (MayPick(Ship))
			{
				Found.push_back(&Ship);
			}
		}
		std::stable_sort(Found.begin(), Found.end(), [MySide](const SeaShow::ShipOut* A, const SeaShow::ShipOut* B)
			{
				const int SideA = A->Side == MySide ? 0 : 1, SideB = B->Side == MySide ? 0 : 1;
				if (SideA != SideB) return SideA < SideB;
				const int LineA = A->LineAt < 0 ? 1 : 0, LineB = B->LineAt < 0 ? 1 : 0;
				if (LineA != LineB) return LineA < LineB;
				if (A->LineAt != B->LineAt) return A->LineAt < B->LineAt;
				if (A->Piece != B->Piece) return A->Piece < B->Piece;
				if (A->Slot != B->Slot) return A->Slot < B->Slot;
				return A->Key < B->Key;
			});
		std::vector<int> Keys;
		Keys.reserve(Found.size());
		for (const SeaShow::ShipOut* Ship : Found)
		{
			Keys.push_back(Ship->Key);
		}
		return Keys;
	}

	int TabStep(const std::vector<int>& Order, int From, int Step)
	{
		if (Order.empty() || Step == 0)
		{
			return Order.empty() ? -1 : From;
		}
		const int N = static_cast<int>(Order.size());
		const auto At = std::find(Order.begin(), Order.end(), From);
		if (At == Order.end())
		{
			return Step > 0 ? Order.front() : Order.back();
		}
		const int Here = static_cast<int>(At - Order.begin());
		return Order[static_cast<size_t>(((Here + Step) % N + N) % N)];
	}

	// ---- 1e ---------------------------------------------------------------------------------------------------------

	int PickAlongRay(const std::vector<SeaShow::ShipOut>& Ships, const Ray& Along)
	{
		const Vec Way = Along.Way.Unit();
		if (!Along.From.Finite() || !(Way.Length() > 0.5))
		{
			return -1;
		}
		const double Slack = std::tan(0.6 * Rad);
		int Best = -1;
		double BestT = std::numeric_limits<double>::infinity();
		for (const SeaShow::ShipOut& Ship : Ships)
		{
			if (!MayPick(Ship))
			{
				continue;
			}
			const Vec Mid = PlaceOf(Ship);
			const double Far = (Mid - Along.From).Length();
			const double Spare = std::max(0.5 * Ship.Beam, Far * Slack);
			const double H = Ship.Heading * Rad;
			const Vec Fore{ std::sin(H), std::cos(H), 0.0 }, Side{ std::cos(H), -std::sin(H), 0.0 };
			const Vec O = Along.From - Mid;
			const double Os[3] = { Dot(O, Fore), Dot(O, Side), O.Z };
			const double Ds[3] = { Dot(Way, Fore), Dot(Way, Side), Way.Z };
			const double Lo[3] = { -0.5 * Ship.Long - Spare, -0.5 * Ship.Beam - Spare, -2.0 - Spare };
			const double Hi[3] = { 0.5 * Ship.Long + Spare, 0.5 * Ship.Beam + Spare, 6.0 + 0.12 * Ship.Long + Spare };
			double T0 = 0.0, T1 = std::numeric_limits<double>::infinity();
			bool bHit = true;
			for (int Axis = 0; Axis < 3 && bHit; ++Axis)
			{
				if (std::abs(Ds[Axis]) < 1e-12)
				{
					bHit = Os[Axis] >= Lo[Axis] && Os[Axis] <= Hi[Axis];
					continue;
				}
				double A = (Lo[Axis] - Os[Axis]) / Ds[Axis], B = (Hi[Axis] - Os[Axis]) / Ds[Axis];
				if (A > B)
				{
					std::swap(A, B);
				}
				T0 = std::max(T0, A);
				T1 = std::min(T1, B);
				bHit = T0 <= T1;
			}
			if (bHit && T0 < BestT)
			{
				BestT = T0;
				Best = Ship.Key;
			}
		}
		return Best;
	}

	int DivisionAlongRay(const std::vector<SeaShow::ShipOut>& Ships, const Ray& Along)
	{
		const Vec Way = Along.Way.Unit();
		if (!Along.From.Finite() || !(Way.Z < -1e-9))
		{
			return -1;
		}
		const double T = -Along.From.Z / Way.Z;
		if (!(T > 0.0))
		{
			return -1;
		}
		const double Px = Along.From.X + Way.X * T, Py = Along.From.Y + Way.Y * T;
		std::vector<int> Pieces;
		for (const SeaShow::ShipOut& Ship : Ships)
		{
			if (MayPick(Ship) && Ship.Doing == SeaShow::Fate::Line && std::find(Pieces.begin(), Pieces.end(), Ship.Piece) == Pieces.end())
			{
				Pieces.push_back(Ship.Piece);
			}
		}
		std::sort(Pieces.begin(), Pieces.end());
		int Best = -1;
		double BestD = std::numeric_limits<double>::infinity();
		const Lengths Plain;
		for (const int Piece : Pieces)
		{
			const Division Dv = DivisionOf(Ships, Piece, Plain);
			if (Dv.Members.size() < 2)
			{
				continue;
			}
			double Gaps = 0.0, Near = std::numeric_limits<double>::infinity();
			for (size_t K = 0; K + 1 < Dv.Members.size(); ++K)
			{
				const Vec A = PlaceOf(*Find(Ships, Dv.Members[K])), B = PlaceOf(*Find(Ships, Dv.Members[K + 1]));
				const double Ex = B.X - A.X, Ey = B.Y - A.Y, Long = Ex * Ex + Ey * Ey;
				Gaps += std::sqrt(Long);
				const double U = Long > 0.0 ? std::clamp(((Px - A.X) * Ex + (Py - A.Y) * Ey) / Long, 0.0, 1.0) : 0.0;
				Near = std::min(Near, std::hypot(Px - (A.X + Ex * U), Py - (A.Y + Ey * U)));
			}
			const double Reach = std::min(150.0, 0.5 * Gaps / static_cast<double>(Dv.Members.size() - 1));
			if (Near <= Reach && Near < BestD)
			{
				BestD = Near;
				Best = Piece;
			}
		}
		return Best;
	}

	// ---- 1f ---------------------------------------------------------------------------------------------------------

	Division DivisionOf(const std::vector<SeaShow::ShipOut>& Ships, int Piece, const Lengths& Long)
	{
		Division Out;
		std::vector<const SeaShow::ShipOut*> In;
		for (const SeaShow::ShipOut& Ship : Ships)
		{
			if (Ship.Piece == Piece && MayPick(Ship) && Ship.Doing == SeaShow::Fate::Line)
			{
				In.push_back(&Ship);
			}
		}
		std::stable_sort(In.begin(), In.end(), [](const SeaShow::ShipOut* A, const SeaShow::ShipOut* B) { return A->Slot != B->Slot ? A->Slot < B->Slot : A->Key < B->Key; });
		if (In.empty())
		{
			return Out;
		}
		double Longest = 0.0;
		for (const SeaShow::ShipOut* Ship : In)
		{
			Out.Members.push_back(Ship->Key);
			Out.Middle += PlaceOf(*Ship);
			Longest = std::max(Longest, Long.DrawnLong(*Ship));
		}
		Out.Middle = Out.Middle * (1.0 / static_cast<double>(In.size()));
		Out.Clear = 0.5 * Longest;
		Out.Leader = In.front()->Key;
		Out.Heading = In.front()->Heading * Rad;
		return Out;
	}

	// ---- 1k ---------------------------------------------------------------------------------------------------------

	const char* TourText()
	{
		return "# The Mac check's drive (the sea free eye plan, 1k): real seconds from the drive's start (the bench's first counted\n"
			   "# frame), then what is held or pressed; 95 real seconds.\n"
			   "0 0.5 hold W            # take the camera from the director\n"
			   "0.5 4.5 hold Shift+Q    # down to just over the water\n"
			   "0.5 4.5 wheel 12        # down to just over the water\n"
			   "4.5 12.5 hold W Q       # a low pan over the water, turning\n"
			   "12.5 press Tab          # onto the first found ship of his own\n"
			   "30 press Tab            # Tab to the next\n"
			   "40 press Tab            # Tab to the next\n"
			   "50 press J              # let go\n"
			   "52 80 hold Shift+E      # up to the ceiling\n"
			   "52 80 wheel -20         # up to the ceiling\n"
			   "80 90 hold D            # a fast pan at the top\n"
			   "90 95 hold              # held at the top\n"
			   "95 press C              # the camera back to the director\n";
	}

	bool ParseDrive(const std::string& Text, Drive& Out, std::string& Error)
	{
		Out = Drive();
		size_t At = 0;
		int LineNo = 0;
		while (At <= Text.size())
		{
			size_t End = Text.find('\n', At);
			End = End == std::string::npos ? Text.size() : End;
			std::string Line = Text.substr(At, End - At);
			At = End + 1;
			++LineNo;
			std::string Words;
			const size_t Hash = Line.find('#');
			if (Hash != std::string::npos)
			{
				Words = Line.substr(Hash + 1);
				Line = Line.substr(0, Hash);
				const size_t First = Words.find_first_not_of(" \t\r");
				const size_t Last = Words.find_last_not_of(" \t\r");
				Words = First == std::string::npos ? std::string() : Words.substr(First, Last - First + 1);
			}
			std::vector<std::string> Bits;
			std::string One;
			for (const char C : Line + " ")
			{
				if (C == ' ' || C == '\t' || C == '\r')
				{
					if (!One.empty())
					{
						Bits.push_back(One);
					}
					One.clear();
				}
				else
				{
					One.push_back(C);
				}
			}
			if (Bits.empty())
			{
				if (End >= Text.size())
				{
					break;
				}
				continue;
			}
			DriveLine Step;
			size_t K = 0;
			const std::string Where = "line " + std::to_string(LineNo);
			if (!Number(Bits[K++], Step.From) || Step.From < 0.0)
			{
				Error = Where + ": no time to start at";
				return false;
			}
			Step.To = Step.From;
			double To = 0.0;
			if (K < Bits.size() && Number(Bits[K], To))
			{
				Step.To = To;
				++K;
			}
			if (K >= Bits.size())
			{
				Error = Where + ": hold, press or wheel";
				return false;
			}
			const std::string Verb = LowerCase(Bits[K++]);
			if (Verb == "hold")
			{
				if (!(Step.To > Step.From))
				{
					Error = Where + ": a hold needs its end";
					return false;
				}
				for (; K < Bits.size(); ++K)
				{
					const std::string Key = LowerCase(Bits[K]);
					const bool bShift = Key.rfind("shift+", 0) == 0;
					const std::string Bare = bShift ? Key.substr(6) : Key;
					const KeyName Name = Bare == "w" ? KeyName::W : Bare == "a" ? KeyName::A : Bare == "s" ? KeyName::S : Bare == "d" ? KeyName::D : Bare == "q" ? KeyName::Q
						: Bare == "e" ? KeyName::E : Bare == "up" ? KeyName::Up : Bare == "down" ? KeyName::Down : Bare == "left" ? KeyName::Left : Bare == "right" ? KeyName::Right
						: KeyName::Other;
					if (Name == KeyName::Other)
					{
						Error = Where + ": no key " + Bits[K] + " to hold";
						return false;
					}
					Step.Hold |= BitOf(Name, bShift);
				}
			}
			else if (Verb == "press")
			{
				const std::string Key = K < Bits.size() ? LowerCase(Bits[K++]) : std::string();
				Step.bShift = Key == "shift+tab";
				Step.Press = Key == "j" ? KeyName::J : Key == "c" ? KeyName::C : (Key == "tab" || Step.bShift) ? KeyName::Tab : KeyName::Other;
				if (Step.Press == KeyName::Other || K < Bits.size())
				{
					Error = Where + ": press J, C, Tab or Shift+Tab";
					return false;
				}
				Step.To = Step.From;
			}
			else if (Verb == "wheel")
			{
				if (!(Step.To > Step.From) || K + 1 != Bits.size() || !Number(Bits[K], Step.Wheel))
				{
					Error = Where + ": wheel from to notches";
					return false;
				}
			}
			else
			{
				Error = Where + ": hold, press or wheel, not " + Bits[K - 1];
				return false;
			}
			Step.Words = Words.empty() ? Verb : Words;
			Out.Lines.push_back(Step);
			if (End >= Text.size())
			{
				break;
			}
		}
		if (Out.Lines.empty())
		{
			Error = "no steps";
			return false;
		}
		for (const DriveLine& Line : Out.Lines)
		{
			if (std::find(Out.Starts.begin(), Out.Starts.end(), Line.From) == Out.Starts.end())
			{
				Out.Starts.push_back(Line.From);
			}
		}
		std::sort(Out.Starts.begin(), Out.Starts.end());
		for (const double Start : Out.Starts)
		{
			std::string Said;
			for (const DriveLine& Line : Out.Lines)
			{
				if (Line.From == Start && Said.empty())
				{
					Said = Line.Words;
				}
			}
			Out.StepWords.push_back(Said);
		}
		Out.StepFrames.assign(Out.Starts.size(), 0);
		Out.StepSeconds.assign(Out.Starts.size(), 0.0);
		Out.StepSecondFrames.assign(Out.Starts.size(), std::vector<int>());
		Out.bOn = true;
		return true;
	}

	// ---- 1j ---------------------------------------------------------------------------------------------------------

	Taken Take(SeaLookSwitches::Look& InForce, bool bFullBattle)
	{
		Taken Out;
		auto Named = [](const SeaLookSwitches::Asked& One, const char* Name)
		{
			return One.Index >= 0 && One.Index < SeaLookSwitches::Count && std::string(SeaLookSwitches::All[One.Index].Name) == Name;
		};
		for (SeaLookSwitches::Asked& One : InForce.Given)
		{
			if (Named(One, "TheaterSeaFreeEye="))
			{
				Out.bOn = bFullBattle && One.Value == "1";
				One.Value = Out.bOn ? "1" : "0";
			}
		}
		for (SeaLookSwitches::Asked& One : InForce.Given)
		{
			if (Named(One, "TheaterSeaFreeEyeRide="))
			{
				Out.bRide = Out.bOn && One.Value == "1";
				One.Value = Out.bRide ? "1" : "0";
			}
			else if (Named(One, "TheaterSeaFreeEyeDrive="))
			{
				const std::string Asked = LowerCase(One.Value);
				Out.Drive = !Out.bOn || One.Value.empty() || Asked == "off" ? "off" : Asked == "tour" ? "tour" : One.Value;
				One.Value = Out.Drive;
			}
		}
		return Out;
	}

	// ---- The frame -------------------------------------------------------------------------------------------------

	void GiveBack(State& St)
	{
		if (St.D.bOn && St.D.bStarted && !St.D.bDone && St.D.T + 1e-9 < DriveEnd(St.D))
		{
			St.D.bDone = true;
			SayDrive(St, true);
		}
		if (St.bOn || St.L.On() || St.PickKey >= 0 || St.PickPiece >= 0)
		{
			++St.GiveBacks;
		}
		St.bOwedCut = St.bOwedCut || (St.bOn && St.bWasFree);
		St.bOn = false;
		St.PickKey = St.PickPiece = -1;
		LetGo(St);
	}

	bool UnderPressed(State& St, bool& bOutDive)
	{
		if (St.D.bOn && !St.D.bDone)
		{
			bOutDive = false;
			return false;
		}
		bOutDive = true;
		const bool bHad = St.bOn;
		if (bHad)
		{
			GiveBack(St);
		}
		return bHad;
	}

	bool ScreenKey(State& St, KeyName Key, bool bDown, bool bShift, bool bRepeat, bool bChord, KeyTaken& Out)
	{
		Out = KeyTaken::NotMine;
		if (Key >= KeyName::Other)
		{
			return false;
		}
		const uint32_t Bit = 1u << static_cast<uint32_t>(Key);
		if (bDown)
		{
			// (a chord's press is the campaign's; a key the screen already holds keeps its repeats, Control or not)
			if ((St.ScreenKeys & Bit) == 0u && bChord)
			{
				return false;
			}
			Out = KeyEvent(St.H, Key, true, bShift, bRepeat);
			if (Out != KeyTaken::NotMine)
			{
				St.ScreenKeys |= Bit;
			}
			return Out != KeyTaken::NotMine;
		}
		// (a release: always to the key rule, so nothing stays held in the free eye; Handled only for a key whose press the
		// screen took, so a key the campaign saw go down, it sees come up)
		const bool bTook = (St.ScreenKeys & Bit) != 0u;
		St.ScreenKeys &= ~Bit;
		Out = KeyEvent(St.H, Key, false, bShift, bRepeat);
		return bTook && Out != KeyTaken::NotMine;
	}

	void ScreenLostKeys(State& St)
	{
		LetGoKeys(St.H);
		St.ScreenKeys = 0u;
	}

	std::vector<std::string> TakeSaid(State& St)
	{
		std::vector<std::string> Out;
		Out.swap(St.Said);
		return Out;
	}

	bool EyeOwner(bool bSwitch, bool bTaken, bool bHandDive)
	{
		return bSwitch && bTaken && !bHandDive;
	}

	bool CutNow(bool bFreeNow, bool bEyeCut, bool bPoseCut, bool bShotChanged, bool bMoved, bool bFaceCrossed, bool bWindowChanged)
	{
		if (bFreeNow)
		{
			return bEyeCut || bPoseCut;
		}
		return bEyeCut || (bShotChanged && bMoved) || bFaceCrossed || (bWindowChanged && bMoved);
	}

	bool FunnelsDue(Refresh& R, double RealNow, const Vec& Eye, bool bCut, int Hero)
	{
		const bool bDue = !R.bKnown || bCut || Hero != R.Hero || (Eye - R.Eye).Length() >= 1000.0 || RealNow - R.At >= 4.0 || RealNow < R.At;
		if (bDue)
		{
			R.At = RealNow;
			R.Eye = Eye;
			R.Hero = Hero;
			R.bKnown = true;
		}
		return bDue;
	}

	bool FarWreckDue(Refresh& R, double RealNow, const Vec& Eye, bool bCut, double Moved)
	{
		const bool bDue = !R.bKnown || bCut || (Eye - R.Eye).Length() >= Moved || RealNow - R.At >= 0.5 || RealNow < R.At;
		if (bDue)
		{
			R.At = RealNow;
			R.Eye = Eye;
			R.bKnown = true;
		}
		return bDue;
	}

	double SeaFloor::Floor(double X, double Y) const
	{
		const double Z = Drawn ? Drawn(Context, X, Y) : 0.0;
		return std::isfinite(Z) ? std::max(Z, 0.0) : 0.0;
	}

	Shot Step(State& St, FreeEye::Eye& Free, const Scene& In)
	{
		static const std::vector<SeaShow::ShipOut> NoShips;
		static const Lengths Plain;
		const std::vector<SeaShow::ShipOut>& Ships = In.Ships ? *In.Ships : NoShips;
		const Lengths& Long = In.Long ? *In.Long : Plain;
		const double Dt = std::max(0.0, Finite(In.RealDt));
		const double Aspect = SaneAspect(In.Aspect);
		const int LockWas = St.L.On() ? St.L.Id : -1;
		Shot Out;
		St.bFed = false;

		// The drive's hands (his own put aside), then the picks and the lock kept to ships he may pick (C2).
		DriveHands(St, Dt, In.bDriveMayStart);
		Keep(St, Ships, Long);

		// C: the camera back to the director, and nothing kept.
		Hands& H = St.H;
		if (H.bC)
		{
			GiveBack(St);
		}
		Out.bGiveBack = St.bOwedCut;
		St.bOwedCut = false;
		// The picture on screen now: the last frame's shot, or the director's on the first.
		const Vec SeenEye = St.bLastKnown ? St.LastEye : In.DirEye, SeenLook = St.bLastKnown ? St.LastLook : In.DirLook;
		const double SeenLens = St.bLastKnown ? St.LastLens : In.DirLens;
		auto Finish = [&](bool bFree)
		{
			Out.bFree = bFree;
			Out.Locked = St.L.On() ? St.L.Id : -1;
			Out.Hero = St.L.Key >= 0 ? St.L.Key : St.L.Piece >= 0 ? DivisionOf(Ships, St.L.Piece, Long).Leader : -1;
			Out.bLockChanged = Out.Locked != LockWas;
			St.bWasFree = bFree;
			St.LastEye = bFree ? Out.Eye : In.DirEye;
			St.LastLook = bFree ? Out.Look : In.DirLook;
			St.LastLens = bFree ? Out.Lens : In.DirLens;
			St.bLastKnown = true;
			DriveCount(St, Dt, bFree, St.LastEye.Z);
			return Out;
		};

		// C's own frame is the director's: nothing pressed with it locks or takes the camera (a key still held takes it
		// again from his shot on the next frame, as after a dive).
		if (H.bC)
		{
			Spent(H);
			St.L.bLast = false;
			St.Lens = In.DirLens;
			St.LensSwing = 0.0;
			return Finish(false);
		}

		// His own dive under the water: the director has the camera (U gave the free camera back first, Q9); his presses
		// wait for nothing (held keys stay held).
		if (In.bHandDive)
		{
			Spent(H);
			St.L.bLast = false;
			St.Lens = In.DirLens;
			St.LensSwing = 0.0;
			return Finish(false);
		}

		// The presses: a click picks (a ship along the ray, else a division near where it meets the sea, else nothing);
		// a double click picks and locks; J locks the pick, or lets go, or with neither locks the found ship nearest the
		// middle of the picture if one is in it; Tab steps through the found ships. Each lock is asked of MayPick again.
		bool bLockAsked = false;
		if (H.bClick || H.bDouble)
		{
			const int Key = PickAlongRay(Ships, H.Click);
			const int Piece = Key < 0 ? DivisionAlongRay(Ships, H.Click) : -1;
			St.PickKey = Key;
			St.PickPiece = Piece;
			if (H.bDouble && (Key >= 0 || Piece >= 0))
			{
				bLockAsked = LockOn(St, Ships, Key, Piece, Long) || bLockAsked;
			}
		}
		if (H.bJ)
		{
			const bool bPickNew = (St.PickKey >= 0 && St.L.Key != St.PickKey) || (St.PickPiece >= 0 && St.L.Piece != St.PickPiece);
			if (bPickNew)
			{
				bLockAsked = LockOn(St, Ships, St.PickKey, St.PickKey >= 0 ? -1 : St.PickPiece, Long) || bLockAsked;
			}
			else if (St.L.On())
			{
				LetGo(St);
			}
			else
			{
				const int Nearest = NearestInPicture(Ships, SeenEye, SeenLook, SeenLens, Aspect);
				if (Nearest >= 0)
				{
					bLockAsked = LockOn(St, Ships, Nearest, -1, Long) || bLockAsked;
				}
			}
		}
		if (H.Tab != 0)
		{
			const std::vector<int> Order = TabOrder(Ships, In.MySide);
			int Steps = H.Tab;
			int From = St.L.Key;
			if (St.L.Piece >= 0 && !St.L.Members.empty())
			{
				// (from a division, Tab goes to its first member)
				From = St.L.Members.front();
				Steps -= Steps > 0 ? 1 : -1;
			}
			else if (From < 0)
			{
				From = TabStep(Order, -1, Steps);
				Steps -= Steps > 0 ? 1 : -1;
			}
			const int To = TabStep(Order, From, Steps);
			if (To >= 0)
			{
				bLockAsked = LockOn(St, Ships, To, -1, Long) || bLockAsked;
			}
		}

		// The take: a camera key, the wheel, the trackpad or a lock takes the camera from the director (a click alone or a
		// pick does not).
		if (!St.bOn && (bLockAsked || HandsMove(H)))
		{
			St.bOn = true;
		}
		if (!EyeOwner(true, St.bOn, false))
		{
			Spent(H);
			St.L.bLast = false;
			St.Lens = In.DirLens;
			St.LensSwing = 0.0;
			return Finish(false);
		}

		// Begin from the director's shot this frame (no jump), its lens his; the core's lenses fitted first to hold the
		// director's 50 and 55 across at this window's shape (Q8), and fitted again whenever the shape changes.
		Free.Set.EyeMost = In.CeilingNow;
		bool bBegin = false, bLetDown = false;
		if (!St.bWasFree)
		{
			FitLens(Free.Set, Aspect);
			St.AspectFitted = Aspect;
			// (a shot over the free ceiling, under a low deck, is let down to it still looking at what he looked at, a cut:
			// the core's own let down keeps the angle and so sinks the pivot as far under the sea as the shot stood over
			// the ceiling, out of its box, 1i)
			Vec From = In.DirEye;
			if (From.Z > Free.Set.EyeMost)
			{
				From.Z = Free.Set.EyeMost;
				bLetDown = true;
			}
			Free.Begin(From, In.DirLook, Vec{});
			St.Lens = In.DirLens;
			St.LensSwing = 0.0;
			St.L.bLast = false;
			++St.Takes;
			bBegin = true;
			Out.bTaken = true;
		}
		else if (Aspect != St.AspectFitted)
		{
			FitLens(Free.Set, Aspect);
			St.AspectFitted = Aspect;
		}

		// (the take's own frame is the director's shot exactly: his hands count from the next frame, and so does a lock
		// that took the camera, whose glide onto her would move this frame already; the presses that took it are spent,
		// the wheel's and the trackpad's sums kept for the next frame)
		FreeEye::Input Feed = bBegin ? FreeEye::Input() : InputOf(H, Dt, Aspect);
		Feed.Aspect = Aspect;
		Feed.Frame = Vec{};
		Feed.FrameId = 0;
		Feed.bBox = true;
		Feed.BoxMin = Vec{ -BoxMargin, -BoxMargin, BoxLow };
		Feed.BoxMax = Vec{ std::max(0.0, Finite(In.BoardSize)) + BoxMargin, std::max(0.0, Finite(In.BoardSize)) + BoxMargin, In.CeilingNow };
		if (bBegin)
		{
			St.L.bLast = false;
		}
		else
		{
			FeedLock(St, Ships, Long, Dt, Free.Last().Eye, Feed);
		}
		// A let go (J, her loss, anything that stops the lock) holds the boom where it stands: while locked, her clearance
		// held the boom off her however far his wheel had brought it in; dropped at once, the boom would shrink to his
		// wheel's want round a pivot still at her middle and take the eye down into her hull (1g: the view holds).
		if (!bBegin && Free.Locked() >= 0 && !Feed.bLock && Free.WantZoom() < Free.Zoom())
		{
			const double Kept = std::numeric_limits<double>::quiet_NaN();
			Free.Want(Kept, Kept, Free.Zoom());
		}
		// Rise held under the ceiling: fed no further than the room left over the pivot he wants for the boom he has or
		// wants (its longest, tilted its steepest), so the core's ceiling never has a rise of his to take back: it takes one
		// back by shortening the boom, at a speed set by the frame rate (core ask 13). Against the ceiling a held rise does
		// nothing, and the climb to it is the same on every machine.
		if (Feed.Rise > 0.0)
		{
			const double Arm = std::max(Free.Arm(), Free.ArmOf(Free.WantZoom()));
			const double Up = std::max({ std::sin(Free.Tilt()), std::sin(Free.WantTilt()), 0.0 });
			const double Room = In.CeilingNow - (Free.Ref().Z + Free.WantRel().Z + Free.Lift()) - Arm * Up;
			const double RiseStep = Free.Set.RiseRate * std::max(Free.Arm(), Free.Set.PanArmLeast) * std::min(Dt, 0.25);
			Feed.Rise = RiseStep > 0.0 ? std::clamp(Room / RiseStep, 0.0, Feed.Rise) : 0.0;
		}
		St.Fed = Feed;
		St.bFed = true;
		const FreeEye::Pose P = Free.Update(Feed, Dt, In.Floor);
		SpentPresses(H);
		if (!bBegin)
		{
			SpentSums(H);
		}

		// The lens: the core's (vertical) turned back to the director's horizontal angle at this aspect, eased from his;
		// at a cut, at once; on the take's own frame, his untouched (a take never changes the picture's width).
		const double Want = HorizontalLens(P.Fov, Aspect);
		if (bBegin)
		{
			St.Lens = In.DirLens;
		}
		else if (P.bCut)
		{
			St.Lens = Want;
			St.LensSwing = 0.0;
		}
		else
		{
			EaseLens(St.Lens, St.LensSwing, Want, std::min(Dt, 0.25));
		}
		Out.Eye = P.Eye;
		Out.Look = P.Look;
		Out.Lens = St.Lens;
		Out.bCut = P.bCut || bLetDown;
		return Finish(true);
	}
}
