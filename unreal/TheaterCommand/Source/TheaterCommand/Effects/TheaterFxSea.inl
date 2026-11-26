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

	int Battle::DeckDrench(double Now, const Vec& ColumnAt, double CalibreMm, const Vec& Toward, double DeckZ, int ShipId)
	{
		const auto Sound = [](double V, double Most) { return V == V && V >= -Most && V <= Most; };
		if (!Sound(Now, 1.0e9) || !RealPlace(ColumnAt) || !RealPlace(Toward) || !Sound(DeckZ, 1.0e4))
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
		const Vec Way = Level * (1.0 / Gap);
		const Vec Side(-Way.Y, Way.X, 0.0);
		const Vec Foot(ColumnAt.X, ColumnAt.Y, Height(ColumnAt.X, ColumnAt.Y));
		// (at most two at once on her: by her number, or by where they land)
		const int Most = static_cast<int>(SafeNum(Num(P + "most", 2.0), 1.0, 8.0, 2.0));
		Drenches.erase(std::remove_if(Drenches.begin(), Drenches.end(), [Now](const DrenchFall& F) { return !(F.Ends > Now); }), Drenches.end());
		const Vec Lands = Foot + Way * (0.5 * (Gap + Reach));
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
		const double Big = std::clamp(std::sqrt(H / 60.0), 0.35, 1.2);
		const int N = static_cast<int>(std::round(SafeNum(Num(P + "puffs", 26.0), 0.0, 120.0, 26.0) * Big));
		const double CrownR = CrownK * H;
		const Rgb WaterCol = Picture("grounds.water.spout.colour", Rgb::FromPicture(240, 244, 246));
		const Rgb ShadowCol = Picture("grounds.water.spout.shadow", Rgb::FromPicture(150, 166, 180));
		const Rgb FoamCol = Picture("grounds.water.spout.foam_colour", Rgb::FromPicture(206, 222, 220));
		const Rgb Shade = Picture(P + "shade", ShadowCol);
		const double ShadeK = SafeNum(Num(P + "shade_k", 0.7), 0.0, 1.0, 0.7);
		const double Alpha = SafeNum(Num(P + "alpha", 0.85), 0.0, 1.0, 0.85);
		const double PieceR = SafeNum(Num(P + "piece_r", 0.045), 0.005, 0.3, 0.045) * H; // (a 16-inch's 2.7 m lumps)
		const double SheetR = SafeNum(Num(P + "sheet_r", 0.06), 0.005, 0.3, 0.06) * H;
		int Laid = 0;
		double LastDown = Now;
		for (int I = 0; I < N; ++I)
		{
			// (one lump of the head: from high in its crown, slipping off toward her as the column stands, to a place on
			// her deck between her near edge and the reach, the nearer the likelier)
			const double Up = D.U(0.7, 1.0);
			const double Across = D.Bell();
			const double OutK = D.U(0.0, 0.35);
			const Vec From = Foot + ZUp * (Up * H) + Side * (Across * 0.6 * CrownR) + Way * (OutK * CrownR);
			const double Far = D.U();
			const double Land = Gap + (Reach - Gap) * std::pow(Far, 1.4);
			const double Spread = D.Bell();
			Vec To = Foot + Way * Land + Side * (Spread * 0.7 * CrownR);
			To.Z = DeckZ;
			const double Drop = std::max(From.Z - DeckZ, 0.5);
			const double Tf = std::sqrt(2.0 * Drop / std::max(G, 0.1));
			const double Late = D.U(0.35, 0.95);
			Bit B;
			B.Born = Now + RiseS + HangS * Late;
			B.From = From;
			// (level, so as to be over its place on the deck when it gets down: the wind takes it on, WindK of it)
			B.Vel = Vec((To.X - From.X) / Tf, (To.Y - From.Y) / Tf, 0.0);
			B.Gravity = G;
			B.Windage = WindK;
			B.Life = Tf;
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
			// (where it lands, a sheet of white water spreading on the deck and draining away)
			const double Down = B.Born + Tf;
			LastDown = std::max(LastDown, Down + Settle);
			const Vec Where_ = To + Vec(Wind.X, Wind.Y, 0.0) * (WindK * Tf);
			Bit S;
			S.Form = Bit::Shape::Flat;
			S.Born = Down;
			S.From = Vec(Where_.X, Where_.Y, DeckZ + 0.08);
			S.Vel = Way * D.U(0.5, 2.0);
			S.Drag = 1.2;
			S.Life = Settle * D.U(0.6, 1.0);
			S.R0 = SheetR * 0.5;
			S.R1 = SheetR * D.U(1.1, 1.6);
			S.Grow = 0.3;
			S.Stretch = D.U(1.2, 1.8);
			S.Spin = std::atan2(Way.Y, Way.X) + D.U(-0.4, 0.4);
			S.Col = FoamCol;
			S.Col2 = FoamCol;
			S.Alpha = Alpha * D.U(0.45, 0.7);
			S.FadeIn = 0.05;
			S.Hold = 0.3;
			S.Heart = 0.6;
			S.Inner = 0.3;
			S.Rim = 0.0;
			S.Rough = 0.3;
			S.ShadeK = 0.0;
			S.LumpSeed = static_cast<uint32_t>(D.U() * 4294967295.0);
			S.Turn = D.U(0.0, 2.0 * Pi);
			Bits.push_back(S);
			++Laid;
			if (I % 2 == 0)
			{
				// (and a low burst of spray thrown back up off the deck where it struck)
				Bit Y;
				Y.Born = Down;
				Y.From = Vec(Where_.X, Where_.Y, DeckZ + 0.3);
				const double Ya = D.Bell();
				const double Yb = D.U(1.5, 3.5);
				Y.Vel = Side * (Ya * 1.5) + ZUp * Yb + Way * 1.0;
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
		// (her axes made square: up first, then her bow square to it, then port square to both)
		ShipPose Pose = PoseIn;
		Pose.Up = PoseIn.Up.Unit(ZUp);
		const Vec Fw = PoseIn.Fore - Pose.Up * PoseIn.Fore.Dot(Pose.Up);
		Pose.Fore = Fw.Unit(Vec(1.0, 0.0, 0.0));
		Pose.Left = Pose.Up.Cross(Pose.Fore).Unit(Vec(0.0, 1.0, 0.0));
		Pose.Long = SafeNum(PoseIn.Long, 1.0, 400.0, 100.0);
		Pose.HalfBeam = SafeNum(PoseIn.HalfBeam, 0.5, 30.0, 8.0);
		Pose.Coaming = SafeNum(PoseIn.Coaming, 0.0, 3.0, 0.0);
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
		// (where along the edge)
		const double Along = D.U() * std::max(W.Length, 0.0);
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
			B.Stretch = bAwash ? 1.6 : 2.6;
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
		if (D.U() < (bAwash ? 0.8 : 0.5))
		{
			// (the foam it leaves: on the sea by her side under the sheet, or sliding inboard across her deck)
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
		if (Riders.empty())
		{
			return; // (no ship carries anything: as ever)
		}
		// (a ship not told of for 5 s is let go)
		Riders.erase(std::remove_if(Riders.begin(), Riders.end(), [Now](const Rider& R) { return !(Now - R.At <= 5.0) && !(R.At > Now); }), Riders.end());
		const Rider* Last = nullptr;
		for (Bit& B : Bits)
		{
			if (B.DeckShip == 0)
			{
				continue;
			}
			const int Ship = static_cast<int>(B.DeckShip) - 1;
			const Rider* Her = Last && Last->Ship == Ship ? Last : RiderOf(Ship);
			if (!Her)
			{
				// (her pose gone: what she carried goes with her)
				B.Life = std::min(B.Life, std::max(Now - B.Born, 0.0));
				B.DeckShip = 0;
				continue;
			}
			Last = Her;
			const double Age = std::max(Now - B.Born, 0.0);
			Vec L = B.DeckP;
			if (B.DeckMode == 1)
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
				continue; // (the deck's brass: stepped in its own part, DeckCases)
			}
			B.From = ShipToWorld(Her->Pose, L);
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
