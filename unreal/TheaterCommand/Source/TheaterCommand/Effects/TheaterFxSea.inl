// Release 2, the ten dramatic moments (the owner, 26 November: "i like the debris coming off the planes when hit", "lets
// try to add more visual dramatic things like that", "do 2-11"): the sea's items 9, 10 and 11, as the effects kit's own
// calls. Included once, at the end of TheaterFxCore.cpp, inside namespace TheaterFx, so it shares that file's helpers
// (Mix64, PlaceKey, SafeNum, Clamp01, RealPlace). Nothing here is called by the kit itself: a scene that never calls
// these draws every frame as before, to the byte. No piece here draws on the battle's shared seed (Seeds): each seeds
// its own Dice from its place, a key or the time, one draw a statement.

	void Battle::SeaClear()
	{
		Drenches.clear();
		Riders.clear();
		Washes.clear();
		DeckIds = 0;
		Lanes.clear();
		GunShips.clear();
		DeckCases.clear();
		DeckKill.clear();
		DeckRounds = 0;
		HazeEnds.clear();
		HazeHangs = 0;
		HazeLingers = 0;
		HazeLingerWas = 0;
	}

	double Battle::SeaTable(const char* Name, double CalibreMm, double Usual) const
	{
		// (as SeaSplash reads its table: each number between the two calibres either side, held at the table's ends; up to
		// eight calibres, rising; a calibre that does not rise ends the table)
		const std::string P = "sea_splash.";
		const double Mm = SafeNum(CalibreMm, 20.0, 600.0, 150.0);
		double Cal[8] = {};
		int NCal = 0;
		for (int I = 0; I < 8; ++I)
		{
			const std::string Key = P + "calibre_mm." + std::to_string(I);
			const double C = Num(Key, -1.0);
			if (!Has(Key) || !(C == C) || C <= (NCal > 0 ? Cal[NCal - 1] : 0.0) || C > 2000.0)
			{
				break;
			}
			Cal[NCal++] = C;
		}
		const std::string Of = P + (Name ? Name : "") + ".";
		if (NCal == 0 || !Has(Of + "0"))
		{
			return Usual;
		}
		int Lo = 0;
		while (Lo + 1 < NCal && Cal[Lo + 1] < Mm)
		{
			++Lo;
		}
		const int Hi = std::min(Lo + 1, NCal - 1);
		const double Between = Hi > Lo ? Clamp01((Mm - Cal[Lo]) / (Cal[Hi] - Cal[Lo])) : 0.0;
		const double A = SafeNum(Num(Of + std::to_string(Lo), Usual), 0.0, 1.0e4, Usual);
		const double B = SafeNum(Num(Of + std::to_string(Hi), A), 0.0, 1.0e4, A);
		return Mm < Cal[0] ? A : A + (B - A) * Between;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Item 9 part 4: the column's head falling across her deck

	int Battle::DeckDrench(double Now, const Vec& ColumnAt, double CalibreMm, const Vec& Toward, double DeckZ, int ShipId, double Beam)
	{
		const auto Sound = [](double V, double Most) { return V == V && V >= -Most && V <= Most; };
		if (!Sound(Now, 1.0e9) || !RealPlace(ColumnAt) || !RealPlace(Toward) || !Sound(DeckZ, 1.0e4) || !Sound(Beam, 1.0e4))
		{
			return 0;
		}
		const std::string P = "sea_splash.drench.";
		if (!Has(P + "reach_k") || !Has("grounds.water.spout.colour.0"))
		{
			return 0;
		}
		// (the column as SeaSplash stands it: its height, how long it rises and stands, how fast its top comes down)
		const double H = SafeNum(SeaTable("height_m", CalibreMm, 30.0), 1.0, 150.0, 30.0);
		const double RiseS = SafeNum(SeaTable("rise_s", CalibreMm, 1.5), 0.1, 10.0, 1.5);
		const double HangS = SafeNum(SeaTable("hang_s", CalibreMm, 2.0), 0.0, 20.0, 2.0);
		const double FallS = SafeNum(SeaTable("fall_s", CalibreMm, 3.0), 0.2, 30.0, 3.0);
		const double CrownK = SafeNum(SeaTable("crown_wide", CalibreMm, 0.17), 0.02, 1.0, 0.17);
		// (how far its head falls from it: reach_k of its height; her near edge Gap off, so only so much reaches her)
		const double Reach = SafeNum(Num(P + "reach_k", 0.35), 0.0, 2.0, 0.35) * H;
		const Vec Level(Toward.X, Toward.Y, 0.0);
		const double Gap = Level.Length();
		if (!(Gap < Reach - 0.5) || Gap < 1.0e-3)
		{
			return 0; // (beyond its reach, or a column standing on her edge: no way toward her)
		}
		// (her far edge: by her beam, the scene's or her pose's, so no water comes down past her far side over the sea;
		// what of the head falls beyond her is the column's own fall, back into the sea)
		const Rider* Her = ShipId >= 0 ? RiderOf(ShipId) : nullptr;
		const double BeamM = Beam > 0.5 ? std::min(Beam, 80.0) : (Her ? 2.0 * Her->Pose.HalfBeam : 0.0);
		const double Margin = 0.5;
		const double Near = Gap + Margin;
		const double Far = BeamM > 0.0 ? std::min(Reach, Gap + BeamM - Margin) : Reach;
		if (!(Far > Near + 0.25))
		{
			return 0;
		}
		const Vec Way = Level * (1.0 / Gap);
		const Vec Side(-Way.Y, Way.X, 0.0);
		const Vec Foot(ColumnAt.X, ColumnAt.Y, Height(ColumnAt.X, ColumnAt.Y));
		// (at most two at once on her: by her number, or by where they land)
		const int Most = static_cast<int>(SafeNum(Num(P + "most", 2.0), 1.0, 8.0, 2.0));
		Drenches.erase(std::remove_if(Drenches.begin(), Drenches.end(), [Now](const DrenchFall& F) { return !(F.Ends > Now); }), Drenches.end());
		const Vec Lands = Foot + Way * (0.5 * (Near + Far));
		int Hers = 0;
		for (const DrenchFall& F : Drenches)
		{
			const bool bHers = ShipId >= 0 ? F.Ship == ShipId : (F.At - Lands).Length() < 40.0;
			Hers += bHers ? 1 : 0;
		}
		if (Hers >= Most)
		{
			return 0;
		}
		// (its own dice, from the place: nothing else in the battle moves)
		Dice D(PlaceKey(ColumnAt) * 0xD6E8FEB86659FD93ull ^ 0xD4E7C4ull);
		const double G = 2.0 * H / (FallS * FallS); // (the column's own fall: its top back on the sea in fall_s)
		const double WindK = SafeNum(Num(P + "wind_k", 0.25), 0.0, 1.0, 0.25);
		const double Settle = SafeNum(Num(P + "life", 3.0), 0.3, 30.0, 3.0);
		const Span LetGo = Range(P + "let_go", 0.55, 0.9); // (when in its hang the head's lumps leave the crown: that share of hang_s)
		const double Big = std::clamp(std::sqrt(H / 60.0), 0.35, 1.2);
		// (as many lumps a metre of her deck as a wide ship takes: a narrow one under the same head takes its share of them)
		const double Share = Clamp01((Far - Near) / std::max(Reach - Near, 0.5));
		const int N = std::max(4, static_cast<int>(std::round(SafeNum(Num(P + "puffs", 26.0), 0.0, 200.0, 26.0) * Big * Share)));
		const double CrownR = CrownK * H;
		const Rgb WaterCol = Picture("grounds.water.spout.colour", Rgb::FromPicture(240, 244, 246));
		const Rgb ShadowCol = Picture("grounds.water.spout.shadow", Rgb::FromPicture(150, 166, 180));
		const Rgb FoamCol = Picture("grounds.water.spout.foam_colour", Rgb::FromPicture(206, 222, 220));
		const Rgb Shade = Picture(P + "shade", ShadowCol);
		const double ShadeK = SafeNum(Num(P + "shade_k", 0.7), 0.0, 1.0, 0.7);
		const double Alpha = SafeNum(Num(P + "alpha", 0.85), 0.0, 1.0, 0.85);
		const double PieceR = SafeNum(Num(P + "piece_r", 0.045), 0.005, 0.3, 0.045) * H; // (a 16-inch's 3.6 m lumps)
		const double SheetR = SafeNum(Num(P + "sheet_r", 0.06), 0.005, 0.3, 0.06) * H;
		// (her deck under the head when each lump comes down: with her ridden, where she will be then, by her way through
		// the water; a lump whose place she has steamed past falls on to the sea)
		const Vec HerWay = Her && Her->bVel ? Vec(Her->Vel.X, Her->Vel.Y, 0.0) : Vec();
		const Vec WindL(Wind.X, Wind.Y, 0.0);
		int Laid = 0;
		double LastDown = Now;
		for (int I = 0; I < N; ++I)
		{
			// (one lump of the head: from high in its crown, slipping off toward her as the column stands, to a place on
			// her deck between her near edge and her far one (or the reach), the nearer the likelier)
			const double Up = D.U(0.7, 1.0);
			const double Across = D.Bell();
			const double OutK = D.U(0.0, 0.35);
			const Vec From = Foot + ZUp * (Up * H) + Side * (Across * 0.6 * CrownR) + Way * (OutK * CrownR);
			const double FarU = D.U();
			const double Land = Near + (Far - Near) * std::pow(FarU, 1.4);
			const double Spread = D.Bell();
			const double Drop = std::max(From.Z - DeckZ, 0.5);
			const double Tf = std::sqrt(2.0 * Drop / std::max(G, 0.1));
			// (where the wind takes it as it falls, WindK of it: aimed so that it still comes down between her edges)
			const Vec Drift = WindL * (WindK * Tf);
			Vec To = Foot + Way * Land + Side * (Spread * 0.7 * CrownR);
			const double Lands_ = (To + Drift - Foot).Dot(Way);
			To = To + Way * (std::clamp(Lands_, Near, Far) - Lands_);
			To.Z = DeckZ;
			const Vec Down_ = To + Drift; // (where it comes down, at her deck's height)
			const double Late = LetGo.Pick(D);
			Bit B;
			B.Born = Now + RiseS + HangS * Late;
			B.From = From;
			const double DownAt = B.Born + Tf;
			// (is her deck under it then? Her length, from her pose and her way; without her pose, it is)
			bool bOnHer = true;
			if (Her)
			{
				const Vec Rel = Down_ - (Her->Pose.At + HerWay * (DownAt - Her->At));
				const double Ax = Rel.Dot(Her->Pose.Fore), Ay = Rel.Dot(Her->Pose.Left);
				bOnHer = std::fabs(Ax) <= 0.5 * Her->Pose.Long - 1.0 && std::fabs(Ay) <= Her->Pose.HalfBeam + 0.25;
			}
			// (level, so as to be over its place on the deck when it gets down: the wind takes it on, WindK of it)
			B.Vel = Vec((To.X - From.X) / Tf, (To.Y - From.Y) / Tf, 0.0);
			B.Gravity = G;
			B.Windage = WindK;
			// (off her: on down to the sea, as the column's own water)
			B.Life = bOnHer ? Tf : std::sqrt(2.0 * std::max(From.Z - Foot.Z, 0.5) / std::max(G, 0.1));
			const double Grows = D.U(0.8, 1.25);
			B.R0 = PieceR * 0.7 * Grows;
			B.R1 = PieceR * 1.3 * Grows;
			B.Grow = 0.5;
			B.Along = (Way * 0.35 - ZUp).Unit(ZUp * -1.0);
			B.Stretch = D.U(2.0, 2.8);
			B.StretchFrom = 1.2;
			B.StretchAfter = 0.25 * Tf;
			B.StretchBy = 0.6 * Tf; // (a lump torn off the crown, drawn out to a curtain's strand as it falls)
			B.Col = WaterCol;
			B.Col2 = WaterCol.Mix(FoamCol, 0.3);
			B.Alpha = Alpha * D.U(0.75, 1.0);
			B.FadeIn = 0.15;
			B.Hold = 0.8;
			B.Heart = 0.85;
			B.Inner = 0.6;
			B.Rim = 0.0;
			B.Rough = 0.25;
			B.ShadeK = 1.0;
			B.ShadowCol = Shade;
			B.ShadowK = ShadeK; // (white water against a bright sky is read by its shade, never by more white)
			B.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
			B.Turn = D.U(0.0, 2.0 * Pi);
			Bits.push_back(B);
			++Laid;
			// (where it lands, a sheet of white water spreading on the deck and draining away: in her frame when she is
			// ridden, so it goes with her as she steams on)
			const double SlideU = D.U(0.5, 2.0);
			const double LifeU = D.U(0.6, 1.0);
			const double WideU = D.U(1.1, 1.6);
			const double LongU = D.U(1.2, 1.8);
			const double SpinU = D.U(-0.4, 0.4);
			const double ThickU = D.U(0.45, 0.7);
			const uint32_t SheetSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
			const double SheetTurn = D.U(0.0, 2.0 * Pi);
			if (!bOnHer)
			{
				continue;
			}
			LastDown = std::max(LastDown, DownAt + Settle);
			Bit S;
			S.Form = Bit::Shape::Flat;
			S.Born = DownAt;
			S.From = Vec(Down_.X, Down_.Y, DeckZ + 0.08);
			S.Vel = Way * SlideU;
			S.Drag = 1.2;
			S.Life = Settle * LifeU;
			S.R0 = SheetR * 0.5;
			S.R1 = SheetR * WideU;
			S.Grow = 0.3;
			S.Stretch = LongU;
			S.Spin = std::atan2(Way.Y, Way.X) + SpinU;
			S.Col = FoamCol;
			S.Col2 = FoamCol;
			S.Alpha = Alpha * ThickU;
			S.FadeIn = 0.05;
			S.Hold = 0.3;
			S.Heart = 0.6;
			S.Inner = 0.3;
			S.Rim = 0.0;
			S.Rough = 0.3;
			S.ShadeK = 0.0;
			S.LumpSeed = SheetSeed;
			S.Turn = SheetTurn;
			if (Her)
			{
				// (taken into her frame as it lands, DeckPlace; its slide as her own way across her deck)
				S.DeckShip = static_cast<uint32_t>(ShipId) + 1u;
				S.DeckId = ++DeckIds;
				S.DeckMode = 5;
				S.DeckP = S.From;
				S.DeckV = Vec(S.Vel.Dot(Her->Pose.Fore), S.Vel.Dot(Her->Pose.Left), 0.0);
				S.DeckT = S.Born;
				S.Vel = Vec();
				S.Drag = 0.0;
			}
			Bits.push_back(S);
			++Laid;
			if (I % 2 == 0)
			{
				// (and a low burst of spray thrown back up off the deck where it struck)
				Bit Y;
				Y.Born = DownAt;
				Y.From = Vec(Down_.X, Down_.Y, DeckZ + 0.3);
				const double Ya = D.Bell();
				const double Yb = D.U(1.5, 3.5);
				Y.Vel = Side * (Ya * 1.5) + ZUp * Yb + Way * 1.0 + HerWay;
				Y.Drag = 2.5;
				Y.Gravity = 4.0;
				Y.Windage = 0.5;
				Y.Life = D.U(0.6, 1.1);
				Y.R0 = PieceR * 0.4;
				Y.R1 = PieceR * D.U(0.8, 1.2);
				Y.Grow = 0.4;
				Y.Col = WaterCol;
				Y.Col2 = FoamCol;
				Y.Alpha = Alpha * 0.6;
				Y.FadeIn = 0.03;
				Y.Hold = 0.2;
				Y.Heart = 0.6;
				Y.Inner = 0.5;
				Y.Rim = 0.0;
				Y.ShadeK = 1.0;
				Y.ShadowCol = Shade;
				Y.ShadowK = ShadeK;
				Y.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
				Y.Turn = D.U(0.0, 2.0 * Pi);
				Bits.push_back(Y);
				++Laid;
			}
		}
		DrenchFall F;
		F.Ship = ShipId;
		F.At = Lands;
		F.Ends = LastDown;
		Drenches.push_back(F);
		return Laid;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// A ship's own frame (ShipRide), and item 9 part 5 / item 10 part 6: water running off her deck, or the sea over her
	// low edge (DeckWash). Drawn in her frame and moved with her every frame, as BurnMove moves a fire's lying embers
	// with its seat, but turned and heeled with her as well as carried: a piece keeps its place on her (X her bow, Y
	// her port side, Z up from her waterline) and Draw writes where that is in the world from her pose of the frame.

	Vec Battle::ShipToWorld(const ShipPose& Pose, const Vec& Local)
	{
		return Pose.At + Pose.Fore * Local.X + Pose.Left * Local.Y + Pose.Up * Local.Z;
	}

	const Battle::Rider* Battle::RiderOf(int Ship) const
	{
		for (const Rider& R : Riders)
		{
			if (R.Ship == Ship)
			{
				return &R;
			}
		}
		return nullptr;
	}

	void Battle::ShipRide(double Now, int ShipId, const ShipPose& PoseIn)
	{
		if (ShipId < 0 || !(std::fabs(Now) <= 1.0e9) || !RealPlace(PoseIn.At) || !RealPlace(PoseIn.Fore) || !RealPlace(PoseIn.Up))
		{
			return;
		}
		// (her axes made square: up first, then her bow square to it, then port square to both, on the side Left names:
		// Unreal's world turns the other way from a sum's, so there her port side is Fore x Up, not Up x Fore; the kit keeps
		// whichever side the scene gives as her port, Up x Fore where it gives none)
		ShipPose Pose = PoseIn;
		Pose.Up = PoseIn.Up.Unit(ZUp);
		const Vec Fw = PoseIn.Fore - Pose.Up * PoseIn.Fore.Dot(Pose.Up);
		Pose.Fore = Fw.Unit(Vec(1.0, 0.0, 0.0));
		const Vec Square = Pose.Up.Cross(Pose.Fore).Unit(Vec(0.0, 1.0, 0.0));
		const double Named_ = PoseIn.Left.Dot(Square);
		Pose.Left = Named_ < 0.0 ? Square * -1.0 : Square;
		Pose.Long = SafeNum(PoseIn.Long, 1.0, 400.0, 100.0);
		Pose.HalfBeam = SafeNum(PoseIn.HalfBeam, 0.5, 30.0, 8.0);
		Pose.Coaming = SafeNum(PoseIn.Coaming, 0.0, 3.0, 0.0);
		Pose.Deck = SafeNum(PoseIn.Deck, 0.0, 60.0, 0.0);
		Rider* Her = nullptr;
		for (Rider& R : Riders)
		{
			if (R.Ship == ShipId)
			{
				Her = &R;
				break;
			}
		}
		if (!Her)
		{
			if (Riders.size() >= 64)
			{
				return; // (a scene that names endless ships: no more than a fleet's)
			}
			Riders.push_back(Rider());
			Her = &Riders.back();
			Her->Ship = ShipId;
		}
		// (her way through the water, level, eased over half a second: from her last pose, told within a second)
		const double Dt = Now - Her->At;
		if (Her->Pose.Long > 0.0 && Dt > 1.0e-3 && Dt <= 1.0)
		{
			const Vec Moved = Pose.At - Her->Pose.At;
			const Vec Inst(Moved.X / Dt, Moved.Y / Dt, 0.0);
			Her->Vel = Her->bVel ? Her->Vel + (Inst - Her->Vel) * std::min(1.0, Dt / 0.5) : Inst;
			Her->bVel = true;
		}
		else if (!(Dt >= 0.0 && Dt <= 1.0))
		{
			Her->bVel = false; // (a jump in her clock or a long silence: her way is not known until told again)
			Her->Vel = Vec();
		}
		Her->Pose = Pose;
		Her->At = Now;
		// (her wash's water due by now, born where she is)
		for (Washing& W : Washes)
		{
			if (W.Ship != ShipId)
			{
				continue;
			}
			const double Rate = SafeNum(Num(std::string("sea_splash.runoff.") + (W.Kind == 2 ? "awash." : "") + "sheets_per_s", W.Kind == 2 ? 3.0 : 6.0), 0.0, 100.0, 6.0) * W.Strength * std::max(W.Length, 1.0) / 10.0;
			if (!(Rate > 0.0))
			{
				continue;
			}
			for (int Laid = 0; W.Next <= Now && W.Next < W.Until && Laid < 64; ++Laid)
			{
				// (thinning over its last quarter, never stopping at a stroke)
				const double Left_ = Clamp01((W.Until - W.Next) / std::max(0.25 * (W.Until - W.Start), 1e-3));
				WashLay(W, Pose, W.Next);
				W.Next += 1.0 / (Rate * std::max(Left_, 0.25));
			}
			W.Next = std::max(W.Next, Now - 0.1);
		}
		Washes.erase(std::remove_if(Washes.begin(), Washes.end(), [Now](const Washing& W) { return !(W.Until > Now); }), Washes.end());
	}

	void Battle::ShipRideStop(int ShipId)
	{
		Riders.erase(std::remove_if(Riders.begin(), Riders.end(), [ShipId](const Rider& R) { return R.Ship == ShipId; }), Riders.end());
		Washes.erase(std::remove_if(Washes.begin(), Washes.end(), [ShipId](const Washing& W) { return W.Ship == ShipId; }), Washes.end());
	}

	int Battle::DeckWash(double Now, int ShipId, const Vec* Edge, int Count, double Strength, double Seconds, const char* KindIn)
	{
		const std::string Kind = KindIn ? KindIn : "runoff";
		const std::string P = Kind == "awash" ? "sea_splash.runoff.awash." : "sea_splash.runoff.";
		if (ShipId < 0 || !Edge || Count < 1 || !(std::fabs(Now) <= 1.0e9) || !Has(P + "life_s.0"))
		{
			return 0;
		}
		const Rider* Her = RiderOf(ShipId);
		if (!Her)
		{
			return 0; // (no pose yet: ShipRide first)
		}
		Washing W;
		W.Ship = ShipId;
		W.Kind = Kind == "awash" ? 2 : 1;
		for (int I = 0; I < std::min(Count, 64); ++I)
		{
			if (RealPlace(Edge[I]) && Edge[I].Length() < 500.0)
			{
				W.Edge.push_back(Edge[I]);
			}
		}
		if (W.Edge.empty())
		{
			return 0;
		}
		for (size_t I = 1; I < W.Edge.size(); ++I)
		{
			W.Length += (W.Edge[I] - W.Edge[I - 1]).Length();
		}
		W.Strength = SafeNum(Strength, 0.0, 1.0, 1.0);
		W.Start = Now;
		W.Next = Now;
		W.Until = Now + SafeNum(Seconds, 0.1, 600.0, 3.0);
		// (its own dice: from where she is, her number and the moment, never the battle's shared seed)
		const uint64_t Moment = static_cast<uint64_t>(static_cast<int64_t>(std::floor(Now * 1000.0)));
		W.D = Dice(PlaceKey(ShipToWorld(Her->Pose, W.Edge[0])) ^ Mix64(static_cast<uint64_t>(ShipId) * 0x9E3779B97F4A7C15ull + Moment) ^ 0x3A5Bull);
		int Hers = 0;
		for (const Washing& Each : Washes)
		{
			Hers += Each.Ship == ShipId ? 1 : 0;
		}
		if (Hers >= 4)
		{
			return 0;
		}
		Washes.push_back(W);
		return 1;
	}

	void Battle::DeckWashStop(double Now, int ShipId)
	{
		for (Washing& W : Washes)
		{
			if (W.Ship == ShipId)
			{
				W.Until = std::min(W.Until, Now);
			}
		}
	}

	void Battle::WashLay(Washing& W, const ShipPose& Pose, double T)
	{
		// One piece of her wash, born at T at a place along its edge: for runoff a white sheet pouring over the edge and
		// down her side (DeckMode 1, falling from the edge to her waterline) and, where it meets the sea, a patch of foam
		// lying on the water by her side (2); awash, the sea heaving up over her low edge (1, thrown up and inboard) and
		// foam sliding in across her deck (2). Its dice: the wash's own, one draw a statement
		const bool bAwash = W.Kind == 2;
		const std::string P = bAwash ? "sea_splash.runoff.awash." : "sea_splash.runoff.";
		Dice& D = W.D;
		// (where along the edge: a band, a piece every band_m (1.5 m), each a little off its mark, round and round the
		// edge, so the sheets and their foam lie side by side along it and overlap into one, never scattered flecks)
		const double Band = SafeNum(Num(P + "band_m", 1.5), 0.2, 20.0, 1.5);
		const double Len = std::max(W.Length, 0.0);
		const double Jit = D.U(-0.5, 0.5);
		double Along = W.Cursor + Jit * Band;
		Along = Len > 1.0e-6 ? Along - Len * std::floor(Along / Len) : 0.0;
		W.Cursor = Len > 1.0e-6 ? std::fmod(W.Cursor + Band, Len) : 0.0;
		Vec At = W.Edge[0];
		double Gone = 0.0;
		for (size_t I = 1; I < W.Edge.size(); ++I)
		{
			const double Seg = (W.Edge[I] - W.Edge[I - 1]).Length();
			if (Gone + Seg >= Along && Seg > 1e-6)
			{
				At = W.Edge[I - 1] + (W.Edge[I] - W.Edge[I - 1]) * ((Along - Gone) / Seg);
				break;
			}
			Gone += Seg;
			At = W.Edge[I];
		}
		// (outward, level in her frame: away from her middle line, toward her ends near bow and stern)
		const double HalfLong = 0.5 * Pose.Long;
		const double Ends = Clamp01((std::fabs(At.X) - 0.6 * HalfLong) / std::max(0.4 * HalfLong, 1.0));
		const Vec Outward = Vec(At.X > 0.0 ? Ends : -Ends, At.Y >= 0.0 ? 1.0 : -1.0, 0.0).Unit(Vec(0.0, 1.0, 0.0));
		const Rgb WaterCol = Picture("grounds.water.spout.colour", Rgb::FromPicture(240, 244, 246));
		const Rgb ShadowCol = Picture("grounds.water.spout.shadow", Rgb::FromPicture(150, 166, 180));
		const Rgb FoamCol = Picture("grounds.water.spout.foam_colour", Rgb::FromPicture(206, 222, 220));
		const double Wide = SafeNum(Num(P + "wide_m", bAwash ? 1.6 : 1.2), 0.1, 10.0, 1.2);
		const double Alpha = SafeNum(Num(P + "alpha", 0.7), 0.0, 1.0, 0.7);
		const double Run = SafeNum(Num(P + "run_mps", bAwash ? 1.0 : 1.4), 0.0, 10.0, 1.4);
		const double ShadeK = SafeNum(Num(P + "shade_k", 0.6), 0.0, 1.0, 0.6);
		const double Long = SafeNum(Num(P + "stretch", bAwash ? 1.6 : 2.6), 1.0, 8.0, 2.6); // (a sheet pouring: a streak that many times as long as it is wide)
		const Span LifeS = Range(P + "life_s", bAwash ? 1.2 : 0.8, bAwash ? 2.0 : 1.6);
		const double G = 9.81;
		const uint32_t Her = static_cast<uint32_t>(W.Ship) + 1u;
		auto Water = [&](Bit& B)
		{
			B.Born = T;
			B.DeckShip = Her;
			B.DeckId = ++DeckIds;
			B.DeckT = T;
			B.Windage = 0.0;
			B.Col = WaterCol;
			B.Col2 = WaterCol.Mix(FoamCol, 0.4);
			B.FadeIn = 0.08;
			B.Hold = 0.55;
			B.Heart = 0.8;
			B.Inner = 0.55;
			B.Rim = 0.0;
			B.Rough = 0.3;
			B.ShadeK = 1.0;
			B.ShadowCol = ShadowCol;
			B.ShadowK = ShadeK;
		};
		{
			Bit B;
			Water(B);
			B.DeckMode = 1;
			const double Pace = D.U(0.6, 1.4);
			const double Spill = D.U(0.0, 0.3);
			if (bAwash)
			{
				// (heaving up over her low edge and in across her deck, falling back)
				const double Heave = D.U(0.8, 2.2);
				B.DeckP = At + Outward * Spill;
				B.DeckV = Outward * (-Run * Pace) + Vec(0.0, 0.0, Heave);
				B.DeckG = G;
				B.Life = std::clamp(2.0 * Heave / G + 0.4, 0.4, LifeS.B);
			}
			else
			{
				// (pouring over the edge and down her side to the sea)
				B.DeckP = At + Outward * Spill;
				B.DeckV = Outward * (Run * Pace) + Vec(0.0, 0.0, -0.3);
				B.DeckG = G;
				const double Drop = std::max(At.Z - 0.3, 0.2);
				B.Life = std::clamp(std::sqrt(2.0 * Drop / G), 0.3, LifeS.B);
			}
			const double Size = D.U(0.75, 1.25);
			B.R0 = 0.35 * Wide * Size;
			B.R1 = 0.6 * Wide * Size;
			B.Grow = 0.5;
			B.Stretch = Long;
			B.StretchFrom = 1.0;
			B.StretchAfter = 0.0;
			B.StretchBy = 0.35 * B.Life; // (a sheet drawn out as it pours)
			B.Alpha = Alpha * D.U(0.7, 1.0);
			B.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
			B.Turn = D.U(0.0, 2.0 * Pi);
			B.From = ShipToWorld(Pose, B.DeckP);
			B.Along = (Pose.Up * -1.0).Unit(ZUp);
			Bits.push_back(B);
		}
		{
			// (the foam it leaves, every piece, so it too lies as a band: on the sea by her side under the sheet, or sliding
			// inboard across her deck)
			Bit F;
			Water(F);
			F.Form = Bit::Shape::Flat;
			F.DeckMode = 2;
			const double Off = D.U(0.3, 1.2);
			F.DeckP = bAwash ? At - Outward * Off + Vec(0.0, 0.0, 0.08) : Vec(At.X, At.Y, 0.0) + Outward * Off + Vec(0.0, 0.0, 0.06);
			F.DeckV = Outward * ((bAwash ? -1.0 : 0.5) * Run * D.U(0.5, 1.2));
			F.Born = T + (bAwash ? 0.3 : std::sqrt(2.0 * std::max(At.Z, 0.2) / G));
			F.DeckT = F.Born;
			F.Life = LifeS.Pick(D) * 1.8;
			F.R0 = 0.4 * Wide;
			F.R1 = Wide * D.U(1.0, 1.5);
			F.Grow = 0.4;
			F.Stretch = D.U(1.2, 1.8);
			F.Spin = D.U(0.0, 2.0 * Pi);
			F.Col = FoamCol;
			F.Col2 = FoamCol;
			F.Alpha = Alpha * D.U(0.4, 0.65);
			F.FadeIn = 0.1;
			F.Hold = 0.3;
			F.Heart = 0.6;
			F.Inner = 0.3;
			F.ShadeK = 0.0;
			F.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
			F.Turn = D.U(0.0, 2.0 * Pi);
			F.From = ShipToWorld(Pose, F.DeckP);
			Bits.push_back(F);
		}
	}

	void Battle::DeckPlace(double Now)
	{
		if (Riders.empty() && DeckIds == 0)
		{
			return; // (no ship carries anything, nor ever did: as ever)
		}
		// (a ship not told of for 5 s is let go)
		Riders.erase(std::remove_if(Riders.begin(), Riders.end(), [Now](const Rider& R) { return !(Now - R.At <= 5.0) && !(R.At > Now); }), Riders.end());
		const Rider* Last = nullptr;
		// (the deck's brass: how its cases move, read once a frame; those the caps let go; those still lying, kept count of)
		CaseFeel Feel;
		if (!DeckCases.empty())
		{
			Feel.Mu = std::tan(SafeNum(Num("deck_cases.stick_deg", 2.0), 0.0, 30.0, 2.0) * Pi / 180.0);
			Feel.RollK = SafeNum(Num("deck_cases.roll_k", 0.5), 0.0, 1.0, 0.5);
			Feel.Within = SafeNum(Num("deck_cases.within_m", 40.0), 1.0, 1000.0, 40.0);
			Feel.Gone = std::max(Feel.Within + 1.0, SafeNum(Num("deck_cases.gone_m", 60.0), 1.0, 1000.0, 60.0));
		}
		std::sort(DeckKill.begin(), DeckKill.end());
		std::vector<std::pair<uint32_t, Vec>> Lying;
		for (Bit& B : Bits)
		{
			if (B.DeckShip == 0)
			{
				continue;
			}
			if (B.DeckMode >= 3 && B.DeckMode <= 4 && !DeckKill.empty() && std::binary_search(DeckKill.begin(), DeckKill.end(), B.DeckId))
			{
				// (let go by a cap: it fades out over a second where it lies)
				const double Age_ = std::max(Now - B.Born, 0.0);
				if (B.Life > Age_ + 1.0)
				{
					B.Life = Age_ + 1.0;
					B.Hold = Age_ / B.Life;
				}
			}
			const int Ship = static_cast<int>(B.DeckShip) - 1;
			const Rider* Her = Last && Last->Ship == Ship ? Last : RiderOf(Ship);
			if (!Her)
			{
				// (her pose gone, let go or stopped: what she carried stays where it was last drawn and fades out over a
				// second there, a fade and never a pop; a drench's sheet not yet landed lands in the world, as it was
				// aimed; a piece of hers not yet begun is never seen)
				const double Age_ = Now - B.Born;
				if (B.DeckMode == 5)
				{
					B.From = B.DeckP;
				}
				else if (Age_ < 0.0)
				{
					B.Life = 0.0;
				}
				else if (B.Life > Age_ + 1.0)
				{
					B.Life = Age_ + 1.0;
					B.Hold = std::min(B.Hold, Age_ / B.Life);
				}
				B.DeckShip = 0;
				continue;
			}
			Last = Her;
			const double Age = std::max(Now - B.Born, 0.0);
			Vec L = B.DeckP;
			if (B.DeckMode == 5)
			{
				if (Now < B.Born)
				{
					B.From = B.DeckP; // (still falling to her: where it will land, in the world)
					continue;
				}
				// (landed: taken into her frame where her deck is under it now, and from there run as her foam)
				const Vec Rel = B.DeckP - Her->Pose.At;
				const double Lz = Her->Pose.Deck > 0.0 ? Her->Pose.Deck + 0.08 : Rel.Dot(Her->Pose.Up);
				B.DeckP = Vec(Rel.Dot(Her->Pose.Fore), Rel.Dot(Her->Pose.Left), Lz);
				B.DeckMode = 2;
				L = B.DeckP + B.DeckV * (1.0 - std::exp(-Age));
			}
			else if (B.DeckMode == 1)
			{
				L = B.DeckP + B.DeckV * Age + Vec(0.0, 0.0, -0.5 * B.DeckG * Age * Age);
				B.Along = (Her->Pose.Up * -1.0).Unit(ZUp);
			}
			else if (B.DeckMode == 2)
			{
				L = B.DeckP + B.DeckV * (1.0 - std::exp(-Age));
			}
			else
			{
				DeckCaseStep(B, Her->Pose, Now, Feel);
				if (B.DeckMode == 3 && Now < B.Born + B.Life)
				{
					Lying.emplace_back(B.DeckId, B.From);
				}
				continue;
			}
			B.From = ShipToWorld(Her->Pose, L);
		}
		DeckKill.clear();
		if (!DeckCases.empty())
		{
			// (those no longer lying on her, gone over the side, let go or faded, are no longer counted; those lying keep
			// where they lie, for the cap of all the ships, which lets the farthest from the eye go first)
			std::sort(Lying.begin(), Lying.end(), [](const std::pair<uint32_t, Vec>& A_, const std::pair<uint32_t, Vec>& B_) { return A_.first < B_.first; });
			auto Find = [&Lying](uint32_t Id) -> const Vec*
			{
				const auto It = std::lower_bound(Lying.begin(), Lying.end(), Id, [](const std::pair<uint32_t, Vec>& E, uint32_t K) { return E.first < K; });
				return It != Lying.end() && It->first == Id ? &It->second : nullptr;
			};
			DeckCases.erase(std::remove_if(DeckCases.begin(), DeckCases.end(), [&Find](const CaseRec& C) { return Find(C.Id) == nullptr; }), DeckCases.end());
			for (CaseRec& C : DeckCases)
			{
				if (const Vec* At = Find(C.Id))
				{
					C.At = *At;
				}
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Item 10 part 3: the oil lane. A holed ship under way trailed furnace oil, never fire (fire on the water only where a
	// burning, leaking ship stopped: the scene's OilFire). Plain numbers: points of her track every every_s, each one
	// widening as the oil spreads (w0 + k sqrt(age), a slick's spread by the root of its age), drifting at drift_k of
	// the wind, dark for up to an hour; the scene lays the strip with its own slick paint. No dice: each point's ragged
	// edge and sheen come from its own number.

	void Battle::OilLeak(double Now, int ShipId, const Vec& Hole, double Rate)
	{
		const std::string P = "sea_leak.";
		if (ShipId < 0 || !(std::fabs(Now) <= 1.0e9) || !RealPlace(Hole) || !Has(P + "w0_m"))
		{
			return;
		}
		const double Every = SafeNum(Num(P + "every_s", 2.0), 0.25, 60.0, 2.0);
		const double Gap = SafeNum(Num(P + "gap_m", 1.5), 0.0, 100.0, 1.5);
		const size_t MostDots = static_cast<size_t>(SafeNum(Num(P + "points", 200.0), 8.0, 2000.0, 200.0));
		const size_t MostShips = static_cast<size_t>(SafeNum(Num(P + "ships", 6.0), 1.0, 64.0, 6.0));
		const double DarkS = SafeNum(Num(P + "dark_s", 3600.0), 10.0, 36000.0, 3600.0);
		// (lanes wholly faded are let go)
		Lanes.erase(std::remove_if(Lanes.begin(), Lanes.end(), [&](const Lane& L)
			{
				return !L.bLeaks && (L.Dots.empty() || !(Now - L.Dots.back().Born < DarkS));
			}), Lanes.end());
		Lane* Her = nullptr;
		for (Lane& L : Lanes)
		{
			if (L.Ship == ShipId)
			{
				Her = &L;
				break;
			}
		}
		if (!Her)
		{
			if (Lanes.size() >= MostShips)
			{
				return; // (a seventh leaking ship: refused until a lane has faded)
			}
			Lanes.push_back(Lane());
			Her = &Lanes.back();
			Her->Ship = ShipId;
			Her->Next = Now;
		}
		Her->bLeaks = true;
		Her->Hole = Vec(Hole.X, Hole.Y, 0.0);
		Her->HoleAt = Now;
		Her->Rate = SafeNum(Rate, 0.0, 1.0, 1.0);
		if (Now < Her->Next)
		{
			return;
		}
		Her->Next = std::max(Her->Next + Every, Now); // (a clock that jumps: one point, not the missed ones)
		// (her old points fade out first)
		while (!Her->Dots.empty() && !(Now - Her->Dots.front().Born < DarkS))
		{
			Her->Dots.erase(Her->Dots.begin());
		}
		// (a ship lying stopped: no new point where the last one is; the oil spreads from that one)
		if (!Her->Dots.empty() && (Her->Dots.back().At - Her->Hole).Length() < Gap)
		{
			return;
		}
		LaneDot D;
		D.At = Her->Hole;
		D.Born = Now;
		D.Rate = Her->Rate;
		D.Serial = ++Her->Serials;
		Her->Dots.push_back(D);
		while (Her->Dots.size() > MostDots)
		{
			// (thinned: the point whose going leaves the shortest stretch between its two neighbours, counted shorter the
			// older it is, so the whole lane is kept, finest near her and coarser astern; the oldest and the newest stay)
			size_t Drop = 1;
			double Least = 1.0e300;
			for (size_t I = 1; I + 1 < Her->Dots.size(); ++I)
			{
				const double Span_ = (Her->Dots[I + 1].At - Her->Dots[I - 1].At).Length();
				const double Cost = Span_ / std::sqrt(1.0 + std::max(Now - Her->Dots[I].Born, 0.0) / 60.0);
				if (Cost < Least)
				{
					Least = Cost;
					Drop = I;
				}
			}
			Her->Dots.erase(Her->Dots.begin() + static_cast<std::ptrdiff_t>(Drop));
		}
	}

	void Battle::OilLeakStop(double Now, int ShipId)
	{
		(void)Now;
		for (Lane& L : Lanes)
		{
			if (L.Ship == ShipId)
			{
				L.bLeaks = false;
			}
		}
	}

	int Battle::OilLanes(std::vector<int>& Ships) const
	{
		Ships.clear();
		for (const Lane& L : Lanes)
		{
			Ships.push_back(L.Ship);
		}
		return static_cast<int>(Ships.size());
	}

	int Battle::OilLane(double Now, int ShipId, std::vector<LanePoint>& Strip) const
	{
		Strip.clear();
		const std::string P = "sea_leak.";
		const Lane* Her = nullptr;
		for (const Lane& L : Lanes)
		{
			if (L.Ship == ShipId)
			{
				Her = &L;
				break;
			}
		}
		if (!Her || !Has(P + "w0_m") || !(std::fabs(Now) <= 1.0e9))
		{
			return 0;
		}
		const double W0 = SafeNum(Num(P + "w0_m", 3.0), 0.1, 100.0, 3.0);
		const double K = SafeNum(Num(P + "k_m", 0.7), 0.0, 20.0, 0.7);
		const double MostWide = SafeNum(Num(P + "most_wide_m", 80.0), 1.0, 2000.0, 80.0);
		const double DriftK = SafeNum(Num(P + "drift_k", 0.03), 0.0, 0.2, 0.03);
		const double Dark0 = SafeNum(Num(P + "dark", 0.85), 0.0, 1.0, 0.85);
		const double DarkS = SafeNum(Num(P + "dark_s", 3600.0), 10.0, 36000.0, 3600.0);
		const double DarkPow = SafeNum(Num(P + "dark_pow", 1.5), 0.2, 8.0, 1.5);
		const double Ragged = SafeNum(Num(P + "ragged", 0.25), 0.0, 0.9, 0.25);
		const double Sheen0 = SafeNum(Num(P + "sheen", 0.2), 0.0, 1.0, 0.2);
		const double SheenS = SafeNum(Num(P + "sheen_s", 900.0), 1.0, 36000.0, 900.0);
		const Vec Drift(Wind.X * DriftK, Wind.Y * DriftK, 0.0);
		auto Lot = [&](uint32_t Serial, uint64_t Salt)
		{
			return static_cast<double>(Mix64((static_cast<uint64_t>(ShipId) + 1ull) * 0x9E3779B97F4A7C15ull ^ static_cast<uint64_t>(Serial) * 0xD1B54A32D192ED03ull ^ Salt) >> 11) * (1.0 / 9007199254740992.0);
		};
		auto Point = [&](const Vec& Laid, double Born, double Rate, uint32_t Serial)
		{
			LanePoint L;
			L.Age = std::max(Now - Born, 0.0);
			L.At = Laid + Drift * L.Age;
			L.At.Z = Height(L.At.X, L.At.Y);
			const double Half = 0.5 * std::min(W0 * std::max(Rate, 0.2) + K * std::sqrt(L.Age), MostWide);
			L.Left = Half * (1.0 + Ragged * (2.0 * Lot(Serial, 0x1EF7ull) - 1.0));
			L.Right = Half * (1.0 + Ragged * (2.0 * Lot(Serial, 0x51D3ull) - 1.0));
			L.Dark = Dark0 * std::pow(Clamp01(1.0 - L.Age / DarkS), DarkPow);
			L.Sheen = Sheen0 * Clamp01(1.0 - L.Age / SheenS) * (Lot(Serial, 0x5EE7ull) < 0.4 ? 1.0 : 0.3);
			return L;
		};
		for (const LaneDot& D : Her->Dots)
		{
			if (Now - D.Born < DarkS && D.Born <= Now)
			{
				Strip.push_back(Point(D.At, D.Born, D.Rate, D.Serial));
			}
		}
		if (Her->bLeaks && (Strip.empty() || (Strip.back().At - Her->Hole).Length() > 0.05))
		{
			Strip.push_back(Point(Her->Hole, std::min(Her->HoleAt, Now), Her->Rate, Her->Serials + 1u));
		}
		// (square to the lane at each point: from the points either side)
		for (size_t I = 0; I < Strip.size(); ++I)
		{
			const Vec A = Strip[I > 0 ? I - 1 : I].At, B = Strip[I + 1 < Strip.size() ? I + 1 : I].At;
			const Vec Run(B.X - A.X, B.Y - A.Y, 0.0);
			Strip[I].Across = Vec(-Run.Y, Run.X, 0.0).Unit(I > 0 ? Strip[I - 1].Across : Vec(0.0, 1.0, 0.0));
		}
		return static_cast<int>(Strip.size());
	}

	Battle::LaneLook Battle::OilLook(double Dark, double MidPx, double GrazeDeg) const
	{
		LaneLook L;
		const std::string P = "sea_leak.";
		const double D = SafeNum(Dark, 0.0, 1.0, 0.0);
		const double FloorPx = SafeNum(Num(P + "floor_px", 1.5), 0.1, 20.0, 1.5);
		const double FloorA = SafeNum(Num(P + "floor_alpha", 0.3), 0.0, 1.0, 0.3);
		const double CalmFrom = SafeNum(Num(P + "calm_from_deg", 5.0), 0.0, 90.0, 5.0);
		const double CalmFull = SafeNum(Num(P + "calm_full_deg", 1.0), 0.0, 90.0, 1.0);
		const double CalmA = SafeNum(Num(P + "calm_alpha", 0.2), 0.0, 1.0, 0.2);
		const double Dark0 = SafeNum(Num(P + "dark", 0.55), 0.01, 1.0, 0.55);
		const double Px = SafeNum(MidPx, 0.0, 1.0e7, 0.0);
		// (under the floor: widened to it, fainter by the root of that only, and never under floor_alpha while so dark)
		L.Widen = Px < FloorPx ? FloorPx / std::max(Px, 1.0e-3) : 1.0;
		const double DarkMid = L.Widen > 1.0 ? std::max(D / std::sqrt(L.Widen), std::min(D, FloorA)) : D;
		// (seen low: over to the calm band, eased, full by calm_full_deg)
		const double G = SafeNum(GrazeDeg, -90.0, 90.0, 90.0);
		const double C = CalmFrom > CalmFull ? Clamp01((CalmFrom - G) / (CalmFrom - CalmFull)) : (G <= CalmFull ? 1.0 : 0.0);
		L.Calm = C * C * (3.0 - 2.0 * C);
		L.CalmAlpha = CalmA * std::sqrt(Clamp01(D / Dark0));
		L.Mid = DarkMid + (L.CalmAlpha - DarkMid) * L.Calm;
		L.Shoulder = 0.85 * DarkMid + (0.5 * L.CalmAlpha - 0.85 * DarkMid) * L.Calm;
		return L;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Item 11 part 4: the deck's brass. A light or medium gun's spent cases on her deck, in her own frame (as her wash):
	// thrown from the breech, tumbling down to her deck, then lying there, and rolling to her low side once she heels
	// past deck_cases.stick_deg; stopped by a coaming or over the side where there is none. Its dice are its own: the
	// mount's place on her, the moment and its own count, one draw a statement.

	void Battle::AirGunShip(int Gun, int ShipId)
	{
		GunShips.erase(std::remove_if(GunShips.begin(), GunShips.end(), [Gun](const std::pair<int, int>& G) { return G.first == Gun; }), GunShips.end());
		if (ShipId >= 0 && GunShips.size() < 4096)
		{
			GunShips.emplace_back(Gun, ShipId);
		}
	}

	void Battle::DeckRound(int Gun, size_t FromBit, double T, const Vec& At, const Vec& Way, const char* Kind)
	{
		// (AirGun, a round just fired: is the gun on a ridden ship, with the deck_cases table? Then the aircraft-style cases
		// Spent threw for it (if the eye was near) are taken back, after every draw was made, whatever its kind: none falls
		// through her deck. With a row for its kind, its case goes on her deck)
		int Ship = -1;
		for (const std::pair<int, int>& G : GunShips)
		{
			if (G.first == Gun)
			{
				Ship = G.second;
				break;
			}
		}
		const std::string P = std::string("deck_cases.") + (Kind ? Kind : "") + ".";
		if (Ship < 0 || !RiderOf(Ship) || !Has("deck_cases.within_m"))
		{
			return; // (no deck_cases table at all: every gun as today)
		}
		if (FromBit < Bits.size())
		{
			Bits.erase(std::remove_if(Bits.begin() + static_cast<std::ptrdiff_t>(FromBit), Bits.end(), [](const Bit& B) { return B.Form == Bit::Shape::Plate && !B.bThrown && B.Rise < 0.0; }), Bits.end());
		}
		if (!Has(P + "case_m"))
		{
			return;
		}
		const Vec Level = Vec(Way.X, Way.Y, 0.0).Unit(Vec(1.0, 0.0, 0.0));
		const Vec Across = Level.Cross(ZUp).Unit(Vec(0.0, 1.0, 0.0));
		const double Back = SafeNum(Num(P + "breech_back_m", 1.0), 0.0, 10.0, 1.0);
		const double Side = SafeNum(Num(P + "side", 0.0), -1.0, 1.0, 0.0);
		const Vec Breech = At - Way * Back;
		const Vec Away = Across * Side - Level * (1.0 - std::fabs(Side));
		DeckBrass(T, Ship, Breech, Away, Kind, 1);
	}

	void Battle::DeckBrass(double Now, int ShipId, const Vec& Breech, const Vec& Away, const char* KindIn, int Count)
	{
		const std::string Kind = KindIn ? KindIn : "";
		const std::string R = "deck_cases.";
		const std::string P = R + Kind + ".";
		if (ShipId < 0 || Kind.empty() || Count < 1 || !(std::fabs(Now) <= 1.0e9) || !RealPlace(Breech) || !RealPlace(Away) || !Has(P + "case_m"))
		{
			return;
		}
		const Rider* Her = RiderOf(ShipId);
		if (!Her)
		{
			return; // (no pose for her: ShipRide first)
		}
		const ShipPose& Pose = Her->Pose;
		// (laid only near the eye, within lay_m (90 m): a ship far off, whose cases no one could see, never takes the
		// room of those in view)
		const double LayM = SafeNum(Num(R + "lay_m", 90.0), 1.0, 2000.0, 90.0);
		if (bEyeKnown && (Breech - LastEye).Length() > LayM)
		{
			return;
		}
		const Vec Rel = Breech - Pose.At;
		const Vec Local(Rel.Dot(Pose.Fore), Rel.Dot(Pose.Left), Rel.Dot(Pose.Up));
		if (std::fabs(Local.X) > 0.5 * Pose.Long + 5.0 || std::fabs(Local.Y) > Pose.HalfBeam + 5.0)
		{
			return; // (a breech that is not on her)
		}
		const double CaseM = SafeNum(Num(P + "case_m", 0.3), 0.01, 2.0, 0.3);
		const double CaseD = SafeNum(Num(P + "case_d_m", 0.05), 0.005, 0.5, 0.05);
		const double Drop = SafeNum(Num(P + "drop_m", Num(R + "drop_m", 1.6)), 0.0, 10.0, 1.6);
		const Span Throw = Range(Has(P + "throw_mps.0") ? P + "throw_mps" : R + "throw_mps", 1.0, 2.5);
		const Span Up = Range(Has(P + "up_mps.0") ? P + "up_mps" : R + "up_mps", 0.0, 0.8);
		const Span Lie = Range(R + "lie_s", 600.0, 600.0);
		const Span Tumble = Range(R + "tumble", 10.0, 25.0);
		const double Scatter = SafeNum(Num(R + "scatter_mps", 0.3), 0.0, 5.0, 0.3);
		const size_t MostMount = static_cast<size_t>(SafeNum(Num(R + "most_mount", 40.0), 1.0, 400.0, 40.0));
		const size_t MostAll = static_cast<size_t>(SafeNum(Num(R + "most_lying", 200.0), 1.0, 2000.0, 200.0));
		const Rgb Col = Picture(R + "colour", Rgb::FromPicture(140, 120, 89));
		const Rgb Dark = Picture(R + "shade", Rgb::FromPicture(90, 79, 61));
		const double Glint = SafeNum(Num(R + "glint", 1.5), 0.0, 10.0, 1.5);
		const double LeastPx = SafeNum(Num(R + "least_px", 0.8), 0.0, 10.0, 0.8);
		const double G = 9.81;
		// (the mount: her number and the breech's place on her, to two metres)
		const uint64_t Fx = static_cast<uint64_t>(static_cast<int64_t>(std::floor(Local.X / 2.0))) & 0xFFFFFull;
		const uint64_t Fy = static_cast<uint64_t>(static_cast<int64_t>(std::floor(Local.Y / 2.0))) & 0xFFFFFull;
		const uint64_t Mount = Mix64((static_cast<uint64_t>(ShipId) + 1ull) << 40 ^ Fx << 20 ^ Fy);
		const uint64_t Moment = static_cast<uint64_t>(static_cast<int64_t>(std::floor(Now * 1000.0)));
		++DeckRounds;
		Dice D(Mix64(Mount ^ Moment * 0x9E3779B97F4A7C15ull ^ DeckRounds * 0xD1B54A32D192ED03ull) ^ 0xB4A55ull);
		const Vec AwayL = Vec(Away.Dot(Pose.Fore), Away.Dot(Pose.Left), 0.0).Unit(Vec(-1.0, 0.0, 0.0));
		// (her deck: where her pose tells it, whatever the gun's elevation; else drop_m under the breech. drop_m stays the
		// throw's fall)
		const double DeckAt = Pose.Deck > 0.0 ? Pose.Deck : std::max(Local.Z - Drop, 0.3);
		const uint32_t HerKey = static_cast<uint32_t>(ShipId) + 1u;
		for (int I = 0; I < std::min(Count, 16); ++I)
		{
			// (the caps: the mount's oldest, then of all the ships' the one farthest from the eye (the oldest, the eye not
			// known), let go to fade as this one comes)
			size_t Mine = 0;
			for (const CaseRec& C : DeckCases)
			{
				Mine += C.Mount == Mount ? 1u : 0u;
			}
			while (Mine >= MostMount)
			{
				for (size_t K = 0; K < DeckCases.size(); ++K)
				{
					if (DeckCases[K].Mount == Mount)
					{
						DeckKill.push_back(DeckCases[K].Id);
						DeckCases.erase(DeckCases.begin() + static_cast<std::ptrdiff_t>(K));
						break;
					}
				}
				--Mine;
			}
			while (DeckCases.size() >= MostAll)
			{
				size_t Go = 0;
				if (bEyeKnown)
				{
					double Farthest = -1.0;
					for (size_t K = 0; K < DeckCases.size(); ++K)
					{
						const double Off = (DeckCases[K].At - LastEye).Length();
						if (Off > Farthest)
						{
							Farthest = Off;
							Go = K;
						}
					}
				}
				DeckKill.push_back(DeckCases[Go].Id);
				DeckCases.erase(DeckCases.begin() + static_cast<std::ptrdiff_t>(Go));
			}
			Bit B;
			B.Form = Bit::Shape::Plate;
			B.bThrown = true;
			B.Born = Now;
			B.DeckShip = HerKey;
			B.DeckId = ++DeckIds;
			B.DeckMode = 3;
			B.DeckT = Now;
			B.DeckG = G;
			B.R0 = B.R1 = 0.5 * CaseD;
			B.Stretch = std::max(1.0, CaseM / CaseD);
			B.DeckZ = DeckAt + B.R0; // (its middle, lying on her deck)
			const double Sx = D.Bell();
			const double Sy = D.Bell();
			const double Flung = Throw.Pick(D);
			const double Lift = Up.Pick(D);
			B.DeckP = Local;
			B.DeckV = AwayL * Flung + Vec(Sx * Scatter, Sy * Scatter, Lift);
			// (down to her deck: when its middle is a case's radius above it)
			const double Fall = std::max(Local.Z - B.DeckZ, 0.0);
			const double Tf = std::max((B.DeckV.Z + std::sqrt(B.DeckV.Z * B.DeckV.Z + 2.0 * G * Fall)) / G, 0.05);
			B.Land = Tf;
			B.Land2 = Tf;
			// (tumbling end over end in the air, so many half turns that it lands lying along her deck)
			const double Yaw = D.U(0.0, 2.0 * Pi);
			B.DeckAx = Vec(std::cos(Yaw), std::sin(Yaw), 0.0);
			B.DeckFa = Vec(0.0, 0.0, 1.0);
			const double Spin = Tumble.Pick(D);
			const double Way_ = D.U();
			B.Tumble = (Way_ < 0.5 ? -1.0 : 1.0) * std::max(1.0, std::round(Spin * Tf / Pi)) * Pi / Tf;
			B.Life = Tf + std::max(1.0, Lie.Pick(D));
			B.Hold = std::clamp((B.Life - 1.0) / B.Life, 0.0, 0.999); // (fading over its last second)
			const double Tone = D.U(0.92, 1.06);
			B.Col = Col * Tone;
			B.Col2 = Dark;
			B.Alpha = 1.0;
			B.DeckFull = 1.0;
			B.FadeIn = 0.01;
			B.Heart = 1.0;
			B.Rim = 0.9;
			B.Inner = 0.5;
			B.Glint = Glint;
			B.LeastPx = LeastPx;
			B.LeastKeep = 0.6;
			B.Gravity = 0.0;
			DeckCaseStep(B, Pose, Now, CaseFeel());
			Bits.push_back(B);
			CaseRec C;
			C.Id = B.DeckId;
			C.Ship = ShipId;
			C.Mount = Mount;
			C.Born = Now;
			C.At = B.From;
			DeckCases.push_back(C);
		}
	}

	void Battle::DeckCaseStep(Bit& B, const ShipPose& Pose, double Now, const CaseFeel& Feel)
	{
		// One case of her brass to where it is now, in her frame: in the air (DeckMode 3, before it lands), lying or rolling
		// on her deck (3), or gone over her side (4, falling to the sea and gone there). The kit draws it as any spent case
		// (LayCase): it is a thrown piece whose flight is over at once, set where it is each frame
		const double Age = std::max(Now - B.Born, 0.0);
		Vec L = B.DeckP;
		if (B.DeckMode == 3 && Age < B.Land)
		{
			L = B.DeckP + B.DeckV * Age + Vec(0.0, 0.0, -0.5 * B.DeckG * Age * Age);
			B.DeckT = std::max(Now, B.Born);
		}
		else if (B.DeckMode == 3)
		{
			if (B.DeckG > 0.0)
			{
				// (landed: where it struck, its way along the deck mostly lost)
				B.DeckP = B.DeckP + B.DeckV * B.Land;
				B.DeckP.Z = B.DeckZ;
				B.DeckV = Vec(B.DeckV.X * 0.25, B.DeckV.Y * 0.25, 0.0);
				B.DeckG = 0.0;
				B.DeckT = B.Born + B.Land;
			}
			const double Dt = std::clamp(Now - B.DeckT, 0.0, 0.25);
			B.DeckT = std::max(B.DeckT, Now);
			if (Dt > 0.0)
			{
				// (her deck's slope: gravity along it, in her frame; the case stays put until that is past stick_deg's grip,
				// and rolls with roll_k of it less the grip once it moves)
				const Vec Slope(-9.81 * Pose.Fore.Z, -9.81 * Pose.Left.Z, 0.0);
				const double Grip = Feel.Mu * 9.81 * std::max(Pose.Up.Z, 0.0);
				const double Pull = Slope.Length();
				Vec V(B.DeckV.X, B.DeckV.Y, 0.0);
				const double Speed = V.Length();
				if (Speed < 1.0e-3)
				{
					V = Pull > Grip ? Slope * (Feel.RollK * (Pull - Grip) / Pull * Dt) : Vec();
				}
				else
				{
					const Vec Was = V;
					V = V + (Slope - V * (Grip / Speed)) * (Feel.RollK * Dt);
					if (V.Dot(Was) <= 0.0 && Pull <= Grip)
					{
						V = Vec(); // (slowed to a stop where the deck is level enough to hold it)
					}
				}
				B.DeckV = V;
				B.DeckP = B.DeckP + V * Dt;
				B.DeckP.Z = B.DeckZ;
				// (her edge: the half beam, narrowing toward bow and stern)
				const double HalfLong = 0.5 * Pose.Long;
				const double Ends = 2.0 * B.DeckP.X / std::max(Pose.Long, 1.0);
				const double Edge = Pose.HalfBeam * std::min(1.0, 1.25 * std::sqrt(std::max(0.0, 1.0 - Ends * Ends)));
				const double Rad = B.R0;
				if (std::fabs(B.DeckP.Y) + Rad > Edge || std::fabs(B.DeckP.X) > HalfLong)
				{
					const double Sign = B.DeckP.Y >= 0.0 ? 1.0 : -1.0;
					if (Pose.Coaming >= 2.0 * Rad && std::fabs(B.DeckP.X) <= HalfLong && Edge > Rad)
					{
						B.DeckP.Y = Sign * (Edge - Rad); // (against the coaming: it lies there)
						B.DeckV.Y = 0.0;
					}
					else
					{
						// (over the side)
						B.DeckMode = 4;
						B.DeckV = Vec(B.DeckV.X, B.DeckV.Y + Sign * 0.4, 0.0);
						B.DeckG = 9.81;
						B.DeckT = Now;
					}
				}
			}
			L = B.DeckP;
		}
		if (B.DeckMode == 4)
		{
			const double Over = std::max(Now - B.DeckT, 0.0);
			L = B.DeckP + B.DeckV * Over + Vec(0.0, 0.0, -0.5 * B.DeckG * Over * Over);
			if (L.Z < 0.0)
			{
				B.Life = std::min(B.Life, Age); // (into the sea)
			}
		}
		B.From = ShipToWorld(Pose, L);
		B.At1 = B.From;
		B.At2 = B.From;
		B.Vel = Vec();
		B.Vel2 = Vec();
		B.Gravity = 0.0;
		B.Along = (Pose.Fore * B.DeckAx.X + Pose.Left * B.DeckAx.Y + Pose.Up * B.DeckAx.Z).Unit(Pose.Fore);
		B.Face = (Pose.Fore * B.DeckFa.X + Pose.Left * B.DeckFa.Y + Pose.Up * B.DeckFa.Z).Unit(Pose.Up);
		// (drawn near only: whole within within_m of the eye, gone by gone_m)
		const double Far = bEyeKnown ? (LastEye - B.From).Length() : 0.0;
		B.Alpha = B.DeckFull * Clamp01((Feel.Gone - Far) / std::max(Feel.Gone - Feel.Within, 1.0));
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Item 11 part 3: the AA haze, an overlay alone (battle-fx-sea-haze.json, the sea scene's -TheaterSeaAAHaze=1). A
	// fleet's AA smoke hung about her as a haze through an air attack: the 5-inch's muzzle smoke lingers (its own row,
	// linger.gun.by.ship_aa) and one light-AA smoke puff in smoke_hang_one_in hangs smoke_hang_s (air_guns.aa40, aa25).
	// At most sea_haze.most hang at once. The room is counted as what can be drawn: the screen's (RoomSeen, what the last
	// frame had on the screen and what is made since), not every piece asked for off it. Past sea_haze.shed_from of it
	// (game.max_puffs) those hanging over sea_haze.keep go first, the oldest first, fading out, and none more hang over
	// keep; keep of them are the haze's own share of the room, laid whatever else asks (Draw counts them as kept), so a
	// fleet's full barrage still hangs its haze. Without the overlay's keys every hook here does nothing.

	bool Battle::HazeRoom(double Now)
	{
		const double Most = SafeNum(Num("sea_haze.most", 0.0), 0.0, 1.0e5, 0.0);
		if (!(Most > 0.0))
		{
			return true; // (no cap asked: as the rows say)
		}
		// (counted as hanging at Now: the guns lay their rounds a frame's moments apart and not in their order, so an end
		// is let go only 2 s after it, and those still hanging at this round's own moment are counted)
		HazeEnds.erase(std::remove_if(HazeEnds.begin(), HazeEnds.end(), [Now](double End) { return !(End > Now - 2.0) || End > Now + 600.0; }), HazeEnds.end());
		const double Hanging = static_cast<double>(std::count_if(HazeEnds.begin(), HazeEnds.end(), [Now](double End) { return End > Now; }));
		if (!(Hanging < Most))
		{
			return false;
		}
		const double Keep = SafeNum(Num("sea_haze.keep", 0.0), 0.0, Most, 0.0);
		if (Hanging < Keep)
		{
			return true; // (the haze's own share of the room)
		}
		const double From = SafeNum(Num("sea_haze.shed_from", 0.8), 0.1, 1.0, 0.8);
		return static_cast<double>(RoomSeen()) < From * std::max(1, MaxPuffs);
	}

	void Battle::HazeHang(Bit& B, const AirRow& Row, double T)
	{
		if (Row.HangOneIn <= 0)
		{
			return;
		}
		// (which puff hangs: by the rule's own count, no dice, so nothing else of the gun moves)
		const uint64_t N = ++HazeHangs;
		if ((N - 1) % static_cast<uint64_t>(Row.HangOneIn) != 0 || !HazeRoom(T))
		{
			return;
		}
		const double Lot = static_cast<double>(Mix64(N * 0x9E3779B97F4A7C15ull ^ 0x4A2E5ull) >> 11) * (1.0 / 9007199254740992.0);
		B.Life = Row.HangA + (Row.HangB - Row.HangA) * Lot;
		B.R1 *= Row.HangWide; // (a puff that hangs spreads further as it goes thin)
		B.bHaze = true;
		HazeEnds.push_back(B.Born + B.Life);
	}

	bool Battle::HazeCountIn(const char* Gun)
	{
		// (Muzzle, just before LingerFrom: a ship's AA gun's smoke, with the overlay laid, has the linger rule count on
		// a count of its own, so the rest of the field's staying puffs are those they would have been, and each of her
		// clouds still keeps puffs of its own, not the same ones over and over)
		if (!Gun || std::strcmp(Gun, "ship_aa") != 0 || !Has("sea_haze.most"))
		{
			return false;
		}
		HazeLingerWas = LingerNo;
		LingerNo = HazeLingers ^ 0x5EA4A2Eull;
		return true;
	}

	void Battle::HazeGate(double Now, size_t First, const char* Gun, bool bCounted)
	{
		// (Muzzle, just after LingerFrom: a ship's AA gun's smoke that its row made linger, held to the cap. Each puff
		// past the cap, or with the room past shed_from, goes back to its own short life as laid. The linger rule's count
		// put back as it was: HazeCountIn)
		if (bCounted)
		{
			HazeLingers = LingerNo ^ 0x5EA4A2Eull;
			LingerNo = HazeLingerWas;
		}
		if (!Gun || std::strcmp(Gun, "ship_aa") != 0 || !Has("sea_haze.most"))
		{
			return;
		}
		for (size_t I = First; I < Bits.size(); ++I)
		{
			Bit& B = Bits[I];
			if (!B.bTail)
			{
				continue;
			}
			// (counted at the gun's moment, not the puff's birth a little after it: counted later, those ending between
			// were let go, and one more hung over the cap)
			if (HazeRoom(Now))
			{
				B.bHaze = true;
				HazeEnds.push_back(B.Born + B.Life);
			}
			else
			{
				B.Life = B.LookLife;
				B.Hold = B.LookHold;
				B.bTail = false;
			}
		}
	}

	void Battle::HazeShed(double Now)
	{
		if (HazeEnds.empty())
		{
			return;
		}
		const double From = SafeNum(Num("sea_haze.shed_from", 0.8), 0.1, 1.0, 0.8);
		const double Most = SafeNum(Num("sea_haze.most", 0.0), 0.0, 1.0e5, 0.0);
		const size_t Keep = static_cast<size_t>(SafeNum(Num("sea_haze.keep", 0.0), 0.0, Most, 0.0));
		if (!(static_cast<double>(RoomSeen()) >= From * std::max(1, MaxPuffs)))
		{
			return;
		}
		// (those hanging now, the oldest first: all past keep fade out as a trail's cut piece does)
		std::vector<std::pair<double, size_t>> Hung;
		for (size_t I = 0; I < Bits.size(); ++I)
		{
			const Bit& B = Bits[I];
			if (B.bHaze && Now < B.Born + B.Life && Now < B.CutAt)
			{
				Hung.emplace_back(B.Born, I);
			}
		}
		if (Hung.size() <= Keep)
		{
			return;
		}
		std::sort(Hung.begin(), Hung.end());
		const size_t Shed = Hung.size() - Keep;
		for (size_t K = 0; K < Shed; ++K)
		{
			Bit& B = Bits[Hung[K].second];
			B.CutAt = std::min(B.CutAt, Now + TrailCutFade);
			B.bHaze = false;
		}
		HazeEnds.clear();
		for (size_t K = Shed; K < Hung.size(); ++K)
		{
			const Bit& B = Bits[Hung[K].second];
			HazeEnds.push_back(B.Born + B.Life);
		}
	}
